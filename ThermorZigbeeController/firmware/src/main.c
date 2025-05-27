#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "driver/adc.h"

// Application headers
#include "ht1621_driver.h"
#include "button_matrix.h"
#include "thermor_ui.h"
#include "zigbee_thermostat.h"
#include "pid_controller.h"
#include "triac_control.h"
#include "temperature_sensor.h"

static const char *TAG = "ThermorMain";

// Pin definitions
#define LCD_CS_PIN          GPIO_NUM_4
#define LCD_WR_PIN          GPIO_NUM_5
#define LCD_DATA_PIN        GPIO_NUM_6

#define BUTTON_ROW1_PIN     GPIO_NUM_7
#define BUTTON_ROW2_PIN     GPIO_NUM_8
#define BUTTON_COL1_PIN     GPIO_NUM_9
#define BUTTON_COL2_PIN     GPIO_NUM_10
#define BUTTON_COL3_PIN     GPIO_NUM_11

#define TRIAC1_PIN          GPIO_NUM_18
#define TRIAC2_PIN          GPIO_NUM_19
#define TRIAC3_PIN          GPIO_NUM_20

#define ZERO_CROSS_PIN      GPIO_NUM_21
#define TEMP_SENSOR_PIN     GPIO_NUM_1  // ADC1_CH0

#define PIR_SENSOR_PIN      GPIO_NUM_2
#define WINDOW_SENSOR_PIN   GPIO_NUM_3

// Global objects
static thermor_ui_t g_ui;
static zigbee_thermostat_t g_zigbee_device;
static QueueHandle_t g_button_queue;
static pid_controller_t g_pid;
static temp_sensor_t g_temp_sensor;

// Task handles
static TaskHandle_t g_ui_task_handle = NULL;
static TaskHandle_t g_control_task_handle = NULL;
static TaskHandle_t g_sensor_task_handle = NULL;

// UI Task - Handle display and button inputs
static void ui_task(void *pvParameters) {
    button_event_t event;
    
    while (1) {
        // Process button events
        if (xQueueReceive(g_button_queue, &event, 0) == pdTRUE) {
            thermor_ui_handle_button(&g_ui, &event);
        }
        
        // Update UI state
        thermor_ui_update(&g_ui);
        
        // Update Zigbee attributes if changed
        static float last_target_temp = 0;
        float current_target = thermor_ui_get_target_temperature(&g_ui);
        if (current_target != last_target_temp) {
            last_target_temp = current_target;
            zigbee_thermostat_update_setpoint(&g_zigbee_device, current_target);
        }
        
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// Control Task - PID control and heating management
static void control_task(void *pvParameters) {
    float last_temp = 0;
    uint32_t last_control_time = 0;
    
    while (1) {
        uint32_t current_time = esp_timer_get_time() / 1000;
        
        // Run PID control every second
        if ((current_time - last_control_time) >= 1000) {
            last_control_time = current_time;
            
            float current_temp = g_ui.config.current_temp;
            float target_temp = thermor_ui_get_target_temperature(&g_ui);
            
            // Check for window open detection
            if (g_ui.config.window_open && g_ui.config.window_detection_enabled) {
                // Force heating off when window is open
                pid_controller_reset(&g_pid);
                triac_control_set_power(0);
                thermor_ui_set_heating_state(&g_ui, false);
                zigbee_thermostat_update_heating_state(&g_zigbee_device, false);
            } else if (target_temp > 0) {
                // Run PID control
                float output = pid_controller_compute(&g_pid, target_temp, current_temp);
                
                // Convert PID output (0-100%) to power level
                uint8_t power_percent = (uint8_t)(output);
                triac_control_set_power(power_percent);
                
                // Update heating state
                bool heating = (power_percent > 0);
                thermor_ui_set_heating_state(&g_ui, heating);
                zigbee_thermostat_update_heating_state(&g_zigbee_device, heating);
                
                // Update power consumption
                uint16_t power_watts = (power_percent * 2000) / 100;  // Assuming 2000W heater
                zigbee_thermostat_update_power(&g_zigbee_device, power_watts);
                
                ESP_LOGD(TAG, "PID: Target=%.1f Current=%.1f Output=%d%%", 
                         target_temp, current_temp, power_percent);
            } else {
                // Heating off
                triac_control_set_power(0);
                thermor_ui_set_heating_state(&g_ui, false);
                zigbee_thermostat_update_heating_state(&g_zigbee_device, false);
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// Sensor Task - Read temperature and other sensors
static void sensor_task(void *pvParameters) {
    float temp_accumulator = 0;
    int temp_samples = 0;
    uint32_t last_report_time = 0;
    
    // Configure PIR sensor
    gpio_config_t pir_conf = {
        .pin_bit_mask = (1ULL << PIR_SENSOR_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&pir_conf);
    
    // Configure window sensor (magnetic contact)
    gpio_config_t window_conf = {
        .pin_bit_mask = (1ULL << WINDOW_SENSOR_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&window_conf);
    
    while (1) {
        uint32_t current_time = esp_timer_get_time() / 1000;
        
        // Read temperature sensor
        float temp = temperature_sensor_read(&g_temp_sensor);
        if (temp > -50.0f) {  // Valid reading
            temp_accumulator += temp;
            temp_samples++;
        }
        
        // Average temperature every second
        if ((current_time - last_report_time) >= 1000 && temp_samples > 0) {
            float avg_temp = temp_accumulator / temp_samples;
            temp_accumulator = 0;
            temp_samples = 0;
            last_report_time = current_time;
            
            // Update UI and Zigbee
            thermor_ui_set_temperature(&g_ui, avg_temp);
            zigbee_thermostat_update_temperature(&g_zigbee_device, avg_temp);
            
            ESP_LOGD(TAG, "Temperature: %.1f°C", avg_temp);
        }
        
        // Read PIR sensor
        bool presence = gpio_get_level(PIR_SENSOR_PIN);
        static bool last_presence = false;
        if (presence != last_presence) {
            last_presence = presence;
            thermor_ui_set_presence(&g_ui, presence);
            zigbee_thermostat_update_occupancy(&g_zigbee_device, presence);
            ESP_LOGI(TAG, "Presence: %s", presence ? "detected" : "none");
        }
        
        // Read window sensor (normally closed contact)
        bool window_open = gpio_get_level(WINDOW_SENSOR_PIN);  // High = open
        static bool last_window_state = false;
        if (window_open != last_window_state) {
            last_window_state = window_open;
            thermor_ui_set_window_state(&g_ui, window_open);
            zigbee_thermostat_update_window_state(&g_zigbee_device, window_open);
            ESP_LOGI(TAG, "Window: %s", window_open ? "open" : "closed");
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// Initialize hardware peripherals
static esp_err_t init_hardware(void) {
    // Initialize LCD
    ht1621_config_t lcd_config = {
        .cs_pin = LCD_CS_PIN,
        .wr_pin = LCD_WR_PIN,
        .data_pin = LCD_DATA_PIN
    };
    ht1621_init(&lcd_config);
    
    // Test LCD
    ht1621_test_pattern();
    
    // Initialize button matrix
    g_button_queue = xQueueCreate(10, sizeof(button_event_t));
    if (!g_button_queue) {
        ESP_LOGE(TAG, "Failed to create button queue");
        return ESP_FAIL;
    }
    
    button_matrix_config_t button_config = {
        .row_pins = {BUTTON_ROW1_PIN, BUTTON_ROW2_PIN},
        .col_pins = {BUTTON_COL1_PIN, BUTTON_COL2_PIN, BUTTON_COL3_PIN},
        .event_queue = g_button_queue
    };
    
    esp_err_t ret = button_matrix_init(&button_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize button matrix");
        return ret;
    }
    
    // Initialize temperature sensor
    temp_sensor_config_t temp_config = {
        .adc_channel = ADC1_CHANNEL_0,
        .sensor_type = TEMP_SENSOR_NTC_10K,
        .beta = 3950,
        .r_nominal = 10000,
        .t_nominal = 25,
        .r_series = 10000
    };
    temperature_sensor_init(&g_temp_sensor, &temp_config);
    
    // Initialize triac control
    triac_config_t triac_config = {
        .triac_pins = {TRIAC1_PIN, TRIAC2_PIN, TRIAC3_PIN},
        .num_triacs = 3,
        .zero_cross_pin = ZERO_CROSS_PIN,
        .max_power_watts = 2000,
        .mains_frequency = 50
    };
    ret = triac_control_init(&triac_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize triac control");
        return ret;
    }
    
    // Initialize PID controller
    pid_config_t pid_config = {
        .kp = 25.0,
        .ki = 0.5,
        .kd = 10.0,
        .output_min = 0.0,
        .output_max = 100.0,
        .sample_time_ms = 1000
    };
    pid_controller_init(&g_pid, &pid_config);
    
    return ESP_OK;
}

void app_main(void) {
    esp_err_t ret;
    
    // Initialize NVS
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    ESP_LOGI(TAG, "Thermor Zigbee Controller starting...");
    
    // Initialize hardware
    ret = init_hardware();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Hardware initialization failed");
        return;
    }
    
    // Initialize UI
    thermor_ui_init(&g_ui);
    
    // Initialize Zigbee
    ret = zigbee_thermostat_init(&g_zigbee_device, &g_ui);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Zigbee initialization failed");
        return;
    }
    
    // Create tasks
    xTaskCreate(ui_task, "ui_task", 4096, NULL, 5, &g_ui_task_handle);
    xTaskCreate(control_task, "control_task", 4096, NULL, 6, &g_control_task_handle);
    xTaskCreate(sensor_task, "sensor_task", 4096, NULL, 4, &g_sensor_task_handle);
    xTaskCreate(zigbee_thermostat_task, "zigbee_task", 4096, &g_zigbee_device, 7, NULL);
    
    ESP_LOGI(TAG, "System initialized successfully");
    
    // Main loop - monitor system health
    while (1) {
        // Print heap info every 30 seconds
        ESP_LOGI(TAG, "Free heap: %d bytes", esp_get_free_heap_size());
        vTaskDelay(pdMS_TO_TICKS(30000));
    }
}