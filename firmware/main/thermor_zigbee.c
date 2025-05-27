/**
 * @file thermor_zigbee.c
 * @brief Main implementation file for Thermor Zigbee Controller
 * @date 2024-01-15
 */

#include "thermor_zigbee.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "driver/gpio.h"
#include "driver/timer.h"
#include "esp_zigbee_core.h"

// Logging tag
static const char *TAG = "thermor_zigbee";

// Global variables
system_config_t g_system_config = {0};
SemaphoreHandle_t g_config_mutex = NULL;
QueueHandle_t g_event_queue = NULL;

// Private variables
static TaskHandle_t main_task_handle = NULL;
static TaskHandle_t temp_task_handle = NULL;
static TaskHandle_t ui_task_handle = NULL;
static TaskHandle_t zigbee_task_handle = NULL;
static esp_timer_handle_t system_timer = NULL;
static nvs_handle_t nvs_handle = 0;

// Forward declarations
static void main_task(void *pvParameters);
static void temperature_task(void *pvParameters);
static void ui_task(void *pvParameters);
static void zigbee_task(void *pvParameters);
static void system_timer_callback(void *arg);
static esp_err_t gpio_init(void);
static esp_err_t load_default_config(void);
static esp_err_t init_nvs(void);

/**
 * @brief Initialize the Thermor Zigbee system
 */
esp_err_t thermor_zigbee_init(void)
{
    esp_err_t ret = ESP_OK;
    
    ESP_LOGI(TAG, "Initializing Thermor Zigbee Controller...");
    
    // Initialize NVS
    ret = init_nvs();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NVS");
        return ret;
    }
    
    // Create synchronization primitives
    g_config_mutex = xSemaphoreCreateMutex();
    if (g_config_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create config mutex");
        return ESP_ERR_NO_MEM;
    }
    
    g_event_queue = xQueueCreate(20, sizeof(system_event_t));
    if (g_event_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create event queue");
        vSemaphoreDelete(g_config_mutex);
        return ESP_ERR_NO_MEM;
    }
    
    // Initialize GPIO pins
    ret = gpio_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize GPIO");
        goto error;
    }
    
    // Load configuration or set defaults
    ret = thermor_config_load();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "No saved config found, loading defaults");
        ret = load_default_config();
        if (ret != ESP_OK) {
            goto error;
        }
    }
    
    // Create system timer
    esp_timer_create_args_t timer_args = {
        .callback = system_timer_callback,
        .arg = NULL,
        .name = "system_timer"
    };
    ret = esp_timer_create(&timer_args, &system_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create system timer");
        goto error;
    }
    
    ESP_LOGI(TAG, "Thermor Zigbee Controller initialized successfully");
    return ESP_OK;
    
error:
    if (g_config_mutex) vSemaphoreDelete(g_config_mutex);
    if (g_event_queue) vQueueDelete(g_event_queue);
    return ret;
}

/**
 * @brief Start the Thermor system
 */
esp_err_t thermor_system_start(void)
{
    BaseType_t ret;
    
    ESP_LOGI(TAG, "Starting Thermor system...");
    
    // Create main task
    ret = xTaskCreate(main_task, "main_task", MAIN_TASK_STACK_SIZE, 
                      NULL, MAIN_TASK_PRIORITY, &main_task_handle);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create main task");
        return ESP_ERR_NO_MEM;
    }
    
    // Create temperature monitoring task
    ret = xTaskCreate(temperature_task, "temp_task", TEMP_TASK_STACK_SIZE,
                      NULL, TEMP_TASK_PRIORITY, &temp_task_handle);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create temperature task");
        return ESP_ERR_NO_MEM;
    }
    
    // Create UI task
    ret = xTaskCreate(ui_task, "ui_task", UI_TASK_STACK_SIZE,
                      NULL, UI_TASK_PRIORITY, &ui_task_handle);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create UI task");
        return ESP_ERR_NO_MEM;
    }
    
    // Create Zigbee task
    ret = xTaskCreate(zigbee_task, "zigbee_task", ZIGBEE_TASK_STACK_SIZE,
                      NULL, ZIGBEE_TASK_PRIORITY, &zigbee_task_handle);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create Zigbee task");
        return ESP_ERR_NO_MEM;
    }
    
    // Start system timer (1 second interval)
    esp_timer_start_periodic(system_timer, 1000000); // 1 second in microseconds
    
    // Set system state to idle
    xSemaphoreTake(g_config_mutex, portMAX_DELAY);
    g_system_config.state = STATE_IDLE;
    xSemaphoreGive(g_config_mutex);
    
    ESP_LOGI(TAG, "Thermor system started successfully");
    return ESP_OK;
}

/**
 * @brief Stop the Thermor system
 */
void thermor_system_stop(void)
{
    ESP_LOGI(TAG, "Stopping Thermor system...");
    
    // Stop system timer
    if (system_timer) {
        esp_timer_stop(system_timer);
    }
    
    // Delete tasks
    if (main_task_handle) vTaskDelete(main_task_handle);
    if (temp_task_handle) vTaskDelete(temp_task_handle);
    if (ui_task_handle) vTaskDelete(ui_task_handle);
    if (zigbee_task_handle) vTaskDelete(zigbee_task_handle);
    
    // Turn off heating
    gpio_set_level(GPIO_TRIAC_CONTROL, 0);
    
    // Save configuration
    thermor_config_save();
    
    ESP_LOGI(TAG, "Thermor system stopped");
}

/**
 * @brief Main control task
 */
static void main_task(void *pvParameters)
{
    system_event_t event;
    TickType_t last_wake_time = xTaskGetTickCount();
    
    ESP_LOGI(TAG, "Main task started");
    
    while (1) {
        // Wait for events with timeout
        if (xQueueReceive(g_event_queue, &event, pdMS_TO_TICKS(100)) == pdTRUE) {
            xSemaphoreTake(g_config_mutex, portMAX_DELAY);
            
            switch (event.type) {
                case EVENT_BUTTON_PRESS:
                    ESP_LOGD(TAG, "Button %d pressed", event.data.button_id);
                    // Handle button press
                    break;
                    
                case EVENT_TEMP_UPDATE:
                    g_system_config.temperature.current = event.data.temperature;
                    g_system_config.temperature.last_update = esp_timer_get_time() / 1000;
                    ESP_LOGD(TAG, "Temperature updated: %.1f°C", event.data.temperature);
                    break;
                    
                case EVENT_PRESENCE_CHANGE:
                    g_system_config.presence.pir_detected = event.data.state;
                    if (event.data.state) {
                        g_system_config.presence.last_motion = esp_timer_get_time() / 1000000;
                    }
                    ESP_LOGD(TAG, "Presence changed: %s", event.data.state ? "detected" : "absent");
                    break;
                    
                case EVENT_WINDOW_CHANGE:
                    g_system_config.presence.window_open = event.data.state;
                    ESP_LOGD(TAG, "Window state: %s", event.data.state ? "open" : "closed");
                    break;
                    
                case EVENT_ZIGBEE_CMD:
                    ESP_LOGD(TAG, "Zigbee command: 0x%02x", event.data.zigbee_cmd);
                    // Handle Zigbee commands
                    break;
                    
                default:
                    break;
            }
            
            xSemaphoreGive(g_config_mutex);
        }
        
        // Perform control logic every 100ms
        xSemaphoreTake(g_config_mutex, portMAX_DELAY);
        
        // Temperature control with hysteresis
        if (g_system_config.temperature.valid) {
            float temp_diff = g_system_config.temperature.target - g_system_config.temperature.current;
            
            if (g_system_config.mode != MODE_OFF) {
                if (temp_diff > TEMP_HYSTERESIS) {
                    // Need heating
                    if (g_system_config.state != STATE_HEATING) {
                        g_system_config.state = STATE_HEATING;
                        ESP_LOGI(TAG, "Starting heating (current: %.1f, target: %.1f)",
                                g_system_config.temperature.current,
                                g_system_config.temperature.target);
                    }
                } else if (temp_diff < -TEMP_HYSTERESIS) {
                    // Temperature reached
                    if (g_system_config.state == STATE_HEATING) {
                        g_system_config.state = STATE_IDLE;
                        ESP_LOGI(TAG, "Target temperature reached");
                    }
                }
            }
        }
        
        // Apply heating control
        if (g_system_config.state == STATE_HEATING && !g_system_config.presence.window_open) {
            // Calculate required power based on temperature difference
            float temp_diff = g_system_config.temperature.target - g_system_config.temperature.current;
            uint8_t power = (uint8_t)(temp_diff * 20); // Simple proportional control
            if (power > 100) power = 100;
            
            g_system_config.power.target_percent = power;
        } else {
            g_system_config.power.target_percent = 0;
        }
        
        xSemaphoreGive(g_config_mutex);
        
        // Delay until next cycle
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(100));
    }
}

/**
 * @brief Temperature monitoring task
 */
static void temperature_task(void *pvParameters)
{
    TickType_t last_wake_time = xTaskGetTickCount();
    
    ESP_LOGI(TAG, "Temperature task started");
    
    while (1) {
        // Read temperature sensor (placeholder - implement actual sensor reading)
        // For now, simulate temperature reading
        float temperature = 20.0f + ((float)(esp_random() % 100) - 50) / 100.0f;
        
        // Send temperature update event
        system_event_t event = {
            .type = EVENT_TEMP_UPDATE,
            .data.temperature = temperature
        };
        xQueueSend(g_event_queue, &event, 0);
        
        // Check PIR sensor
        bool pir_state = gpio_get_level(GPIO_PIR_SENSOR);
        static bool last_pir_state = false;
        if (pir_state != last_pir_state) {
            last_pir_state = pir_state;
            event.type = EVENT_PRESENCE_CHANGE;
            event.data.state = pir_state;
            xQueueSend(g_event_queue, &event, 0);
        }
        
        // Check window sensor
        bool window_state = gpio_get_level(GPIO_WINDOW_SENSOR);
        static bool last_window_state = false;
        if (window_state != last_window_state) {
            last_window_state = window_state;
            event.type = EVENT_WINDOW_CHANGE;
            event.data.state = window_state;
            xQueueSend(g_event_queue, &event, 0);
        }
        
        // Delay until next reading
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(TEMP_SAMPLE_PERIOD_MS));
    }
}

/**
 * @brief UI handling task
 */
static void ui_task(void *pvParameters)
{
    ESP_LOGI(TAG, "UI task started");
    
    while (1) {
        // Handle button scanning and LCD updates
        // Placeholder for actual implementation
        
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

/**
 * @brief Zigbee communication task
 */
static void zigbee_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Zigbee task started");
    
    // Initialize Zigbee stack
    // Placeholder for actual Zigbee implementation
    
    while (1) {
        // Handle Zigbee communication
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/**
 * @brief System timer callback (1 second interval)
 */
static void system_timer_callback(void *arg)
{
    system_event_t event = {
        .type = EVENT_TIMER_TICK
    };
    xQueueSend(g_event_queue, &event, 0);
}

/**
 * @brief Initialize GPIO pins
 */
static esp_err_t gpio_init(void)
{
    gpio_config_t io_conf = {0};
    
    // Configure output pins
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << GPIO_TRIAC_CONTROL) | 
                          (1ULL << GPIO_LCD_CS) |
                          (1ULL << GPIO_LCD_WR) |
                          (1ULL << GPIO_LCD_DATA) |
                          (1ULL << GPIO_STATUS_LED);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);
    
    // Configure input pins with interrupts
    io_conf.intr_type = GPIO_INTR_POSEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << GPIO_ZERO_CROSS);
    io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);
    
    // Configure input pins without interrupts
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.pin_bit_mask = (1ULL << GPIO_TEMP_SENSOR) |
                          (1ULL << GPIO_PIR_SENSOR) |
                          (1ULL << GPIO_BTN_ROW1) |
                          (1ULL << GPIO_BTN_ROW2) |
                          (1ULL << GPIO_BTN_ROW3) |
                          (1ULL << GPIO_WINDOW_SENSOR) |
                          (1ULL << GPIO_PRESENCE_OVERRIDE);
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);
    
    // Configure button columns as open-drain outputs
    io_conf.mode = GPIO_MODE_OUTPUT_OD;
    io_conf.pin_bit_mask = (1ULL << GPIO_BTN_COL1) |
                          (1ULL << GPIO_BTN_COL2) |
                          (1ULL << GPIO_BTN_COL3);
    gpio_config(&io_conf);
    
    // Set initial states
    gpio_set_level(GPIO_TRIAC_CONTROL, 0);
    gpio_set_level(GPIO_STATUS_LED, 0);
    gpio_set_level(GPIO_LCD_CS, 1);
    gpio_set_level(GPIO_LCD_WR, 1);
    
    return ESP_OK;
}

/**
 * @brief Initialize NVS
 */
static esp_err_t init_nvs(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    return ret;
}

/**
 * @brief Load default configuration
 */
static esp_err_t load_default_config(void)
{
    xSemaphoreTake(g_config_mutex, portMAX_DELAY);
    
    // Set default values
    g_system_config.mode = MODE_ECO;
    g_system_config.state = STATE_INIT;
    g_system_config.temperature.target = 20.0f;
    g_system_config.temperature.offset = 0.0f;
    g_system_config.temperature.valid = false;
    g_system_config.power.current_percent = 0;
    g_system_config.power.target_percent = 0;
    g_system_config.power.soft_start_active = true;
    g_system_config.presence.absence_timer_min = 60;
    g_system_config.child_lock = false;
    g_system_config.adaptive_start = true;
    g_system_config.open_window_detection = true;
    g_system_config.lcd_brightness = 80;
    
    // Clear schedule
    memset(g_system_config.schedule, 0, sizeof(g_system_config.schedule));
    
    xSemaphoreGive(g_config_mutex);
    
    return ESP_OK;
}

/**
 * @brief Save configuration to NVS
 */
esp_err_t thermor_config_save(void)
{
    esp_err_t ret;
    
    ret = nvs_open("thermor", NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle");
        return ret;
    }
    
    xSemaphoreTake(g_config_mutex, portMAX_DELAY);
    
    // Save configuration blob
    ret = nvs_set_blob(nvs_handle, "config", &g_system_config, sizeof(g_system_config));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save config");
    } else {
        ret = nvs_commit(nvs_handle);
        ESP_LOGI(TAG, "Configuration saved");
    }
    
    xSemaphoreGive(g_config_mutex);
    
    nvs_close(nvs_handle);
    return ret;
}

/**
 * @brief Load configuration from NVS
 */
esp_err_t thermor_config_load(void)
{
    esp_err_t ret;
    size_t length = sizeof(g_system_config);
    
    ret = nvs_open("thermor", NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        return ret;
    }
    
    xSemaphoreTake(g_config_mutex, portMAX_DELAY);
    
    ret = nvs_get_blob(nvs_handle, "config", &g_system_config, &length);
    if (ret == ESP_OK && length == sizeof(g_system_config)) {
        ESP_LOGI(TAG, "Configuration loaded");
    } else {
        ret = ESP_ERR_NOT_FOUND;
    }
    
    xSemaphoreGive(g_config_mutex);
    
    nvs_close(nvs_handle);
    return ret;
}

/**
 * @brief Reset configuration to defaults
 */
esp_err_t thermor_config_reset(void)
{
    ESP_LOGI(TAG, "Resetting configuration to defaults");
    
    esp_err_t ret = load_default_config();
    if (ret == ESP_OK) {
        ret = thermor_config_save();
    }
    
    return ret;
}

// Temperature control functions
float thermor_get_current_temp(void)
{
    float temp;
    xSemaphoreTake(g_config_mutex, portMAX_DELAY);
    temp = g_system_config.temperature.current;
    xSemaphoreGive(g_config_mutex);
    return temp;
}

float thermor_get_target_temp(void)
{
    float temp;
    xSemaphoreTake(g_config_mutex, portMAX_DELAY);
    temp = g_system_config.temperature.target;
    xSemaphoreGive(g_config_mutex);
    return temp;
}

esp_err_t thermor_set_target_temp(float temp)
{
    if (temp < TEMP_MIN_CELSIUS || temp > TEMP_MAX_CELSIUS) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(g_config_mutex, portMAX_DELAY);
    g_system_config.temperature.target = temp;
    xSemaphoreGive(g_config_mutex);
    
    ESP_LOGI(TAG, "Target temperature set to %.1f°C", temp);
    return ESP_OK;
}

// Mode control functions
heating_mode_t thermor_get_mode(void)
{
    heating_mode_t mode;
    xSemaphoreTake(g_config_mutex, portMAX_DELAY);
    mode = g_system_config.mode;
    xSemaphoreGive(g_config_mutex);
    return mode;
}

esp_err_t thermor_set_mode(heating_mode_t mode)
{
    if (mode > MODE_VACATION) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(g_config_mutex, portMAX_DELAY);
    g_system_config.mode = mode;
    
    // Apply mode-specific settings
    switch (mode) {
        case MODE_OFF:
            g_system_config.temperature.target = TEMP_MIN_CELSIUS;
            break;
        case MODE_COMFORT:
            g_system_config.temperature.target = 21.0f;
            break;
        case MODE_ECO:
            g_system_config.temperature.target = 19.0f;
            break;
        case MODE_ANTI_FREEZE:
            g_system_config.temperature.target = 7.0f;
            break;
        case MODE_BOOST:
            g_system_config.temperature.target = 23.0f;
            break;
        default:
            break;
    }
    
    xSemaphoreGive(g_config_mutex);
    
    ESP_LOGI(TAG, "Mode changed to %s", thermor_mode_to_string(mode));
    return ESP_OK;
}

const char* thermor_mode_to_string(heating_mode_t mode)
{
    switch (mode) {
        case MODE_OFF:         return "OFF";
        case MODE_COMFORT:     return "COMFORT";
        case MODE_ECO:         return "ECO";
        case MODE_ANTI_FREEZE: return "ANTI-FREEZE";
        case MODE_PROGRAM:     return "PROGRAM";
        case MODE_BOOST:       return "BOOST";
        case MODE_VACATION:    return "VACATION";
        default:               return "UNKNOWN";
    }
}

// Power control functions
uint8_t thermor_get_power_percent(void)
{
    uint8_t power;
    xSemaphoreTake(g_config_mutex, portMAX_DELAY);
    power = g_system_config.power.current_percent;
    xSemaphoreGive(g_config_mutex);
    return power;
}

esp_err_t thermor_set_power_percent(uint8_t percent)
{
    if (percent > 100) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(g_config_mutex, portMAX_DELAY);
    g_system_config.power.target_percent = percent;
    xSemaphoreGive(g_config_mutex);
    
    return ESP_OK;
}

// System state function
system_state_t thermor_get_system_state(void)
{
    system_state_t state;
    xSemaphoreTake(g_config_mutex, portMAX_DELAY);
    state = g_system_config.state;
    xSemaphoreGive(g_config_mutex);
    return state;
}

// Diagnostic functions
uint32_t thermor_get_uptime_seconds(void)
{
    return esp_timer_get_time() / 1000000;
}

esp_err_t thermor_get_diagnostics(char *buffer, size_t len)
{
    if (buffer == NULL || len < 256) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(g_config_mutex, portMAX_DELAY);
    
    snprintf(buffer, len,
            "Mode: %s\n"
            "State: %d\n"
            "Current Temp: %.1f°C\n"
            "Target Temp: %.1f°C\n"
            "Power: %d%%\n"
            "Presence: %s\n"
            "Window: %s\n"
            "Uptime: %lu seconds\n",
            thermor_mode_to_string(g_system_config.mode),
            g_system_config.state,
            g_system_config.temperature.current,
            g_system_config.temperature.target,
            g_system_config.power.current_percent,
            g_system_config.presence.pir_detected ? "Yes" : "No",
            g_system_config.presence.window_open ? "Open" : "Closed",
            thermor_get_uptime_seconds());
    
    xSemaphoreGive(g_config_mutex);
    
    return ESP_OK;
}