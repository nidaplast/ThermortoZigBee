/**
 * @file thermor_zigbee.c
 * @brief Thermor Zigbee application implementation
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "driver/gptimer.h"
#include "driver/temperature_sensor.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_zigbee_core.h"
#include "esp_zigbee_endpoint.h"
#include "esp_zigbee_cluster.h"
#include "esp_zigbee_attribute.h"
#include "zcl/esp_zigbee_zcl_thermostat.h"
#include "thermor_zigbee.h"

static const char *TAG = "THERMOR_ZB";

/* Zigbee configuration */
#define INSTALLCODE_POLICY_ENABLE    false
#define ESP_ZB_PRIMARY_CHANNEL_MASK   ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK
#define THERMOR_ENDPOINT_ID          1

/* Temperature control parameters */
#define TEMP_MIN                     5.0f
#define TEMP_MAX                     30.0f
#define TEMP_HYSTERESIS             0.5f
#define PID_KP                       10.0f
#define PID_KI                       0.1f
#define PID_KD                       1.0f

/* Timing parameters */
#define ZERO_CROSS_TIMEOUT_MS        25
#define CONTROL_LOOP_PERIOD_MS       1000
#define ZIGBEE_REPORT_PERIOD_MS      30000

/* Global state */
static thermor_state_t g_thermor_state = {
    .current_temp = 20.0f,
    .target_temp = 20.0f,
    .heating_power = 0,
    .mode = THERMOSTAT_MODE_COMFORT,
    .presence_detected = false,
    .window_open = false,
    .heating_active = false,
    .power_consumption = 0
};

static SemaphoreHandle_t g_state_mutex;
static gptimer_handle_t g_phase_timer;
static esp_timer_handle_t g_control_timer;
static esp_timer_handle_t g_report_timer;

/* PID controller state */
static struct {
    float integral;
    float prev_error;
} g_pid_state = {0};

/* Forward declarations */
static void control_loop_callback(void *arg);
static void zigbee_report_callback(void *arg);
static void IRAM_ATTR zero_cross_isr(void *arg);
static void IRAM_ATTR phase_timer_callback(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_data);
static esp_err_t init_hardware(void);
static esp_err_t init_zigbee(void);
static void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct);
static float calculate_pid_output(float setpoint, float current_value);
static void update_heating_power(uint8_t power);

esp_err_t thermor_zigbee_init(void)
{
    esp_err_t ret;

    // Create mutex for state protection
    g_state_mutex = xSemaphoreCreateMutex();
    if (!g_state_mutex) {
        ESP_LOGE(TAG, "Failed to create state mutex");
        return ESP_ERR_NO_MEM;
    }

    // Initialize hardware
    ret = init_hardware();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Hardware initialization failed");
        return ret;
    }

    // Initialize Zigbee
    ret = init_zigbee();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Zigbee initialization failed");
        return ret;
    }

    // Create control loop timer
    const esp_timer_create_args_t control_timer_args = {
        .callback = control_loop_callback,
        .name = "control_loop"
    };
    ESP_ERROR_CHECK(esp_timer_create(&control_timer_args, &g_control_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(g_control_timer, CONTROL_LOOP_PERIOD_MS * 1000));

    // Create Zigbee report timer
    const esp_timer_create_args_t report_timer_args = {
        .callback = zigbee_report_callback,
        .name = "zigbee_report"
    };
    ESP_ERROR_CHECK(esp_timer_create(&report_timer_args, &g_report_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(g_report_timer, ZIGBEE_REPORT_PERIOD_MS * 1000));

    ESP_LOGI(TAG, "Thermor Zigbee initialized successfully");
    return ESP_OK;
}

static esp_err_t init_hardware(void)
{
    // Configure GPIOs
    gpio_config_t io_conf = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << GPIO_TRIAC_CTRL) | (1ULL << GPIO_LED_STATUS) | (1ULL << GPIO_LED_HEATING),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    // Configure input GPIOs
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << GPIO_PIR_SENSOR) | (1ULL << GPIO_BUTTON_MODE) | 
                           (1ULL << GPIO_BUTTON_PLUS) | (1ULL << GPIO_BUTTON_MINUS);
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);

    // Configure zero crossing interrupt
    io_conf.pin_bit_mask = (1ULL << GPIO_ZERO_CROSS);
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    gpio_config(&io_conf);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(GPIO_ZERO_CROSS, zero_cross_isr, NULL);

    // Configure phase control timer
    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000, // 1MHz, 1us resolution
    };
    ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &g_phase_timer));

    gptimer_alarm_config_t alarm_config = {
        .reload_count = 0,
        .alarm_count = 5000, // Default 5ms delay
        .flags.auto_reload_on_alarm = false,
    };
    ESP_ERROR_CHECK(gptimer_set_alarm_action(g_phase_timer, &alarm_config));

    gptimer_event_callbacks_t cbs = {
        .on_alarm = phase_timer_callback,
    };
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(g_phase_timer, &cbs, NULL));
    ESP_ERROR_CHECK(gptimer_enable(g_phase_timer));

    ESP_LOGI(TAG, "Hardware initialized");
    return ESP_OK;
}

static esp_err_t init_zigbee(void)
{
    esp_zb_cfg_t zb_nwk_cfg = ESP_ZB_ZED_CONFIG();
    esp_zb_init(&zb_nwk_cfg);

    // Create endpoint
    esp_zb_cluster_list_t *cluster_list = esp_zb_zcl_cluster_list_create();
    
    // Basic cluster
    esp_zb_attribute_list_t *basic_cluster = esp_zb_basic_cluster_create(NULL);
    esp_zb_cluster_list_add_basic_cluster(cluster_list, basic_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    
    // Thermostat cluster
    esp_zb_thermostat_cluster_cfg_t thermostat_cfg = {
        .local_temperature = (int16_t)(g_thermor_state.current_temp * 100),
        .occupied_cooling_setpoint = 0x0A28, // Not used
        .occupied_heating_setpoint = (int16_t)(g_thermor_state.target_temp * 100),
        .control_sequence_of_operation = ESP_ZB_ZCL_THERMOSTAT_CONTROL_SEQ_HEATING_ONLY,
        .system_mode = ESP_ZB_ZCL_THERMOSTAT_MODE_HEAT,
    };
    esp_zb_attribute_list_t *thermostat_cluster = esp_zb_thermostat_cluster_create(&thermostat_cfg);
    esp_zb_cluster_list_add_thermostat_cluster(cluster_list, thermostat_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

    // Create endpoint
    esp_zb_endpoint_config_t endpoint_config = {
        .endpoint = THERMOR_ENDPOINT_ID,
        .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .app_device_id = ESP_ZB_HA_THERMOSTAT_DEVICE_ID,
        .app_device_version = 0
    };
    
    esp_zb_ep_list_t *ep_list = esp_zb_ep_list_create();
    esp_zb_ep_list_add_ep(ep_list, cluster_list, endpoint_config);
    
    esp_zb_device_register(ep_list);
    esp_zb_set_primary_network_channel_set(ESP_ZB_PRIMARY_CHANNEL_MASK);
    
    ESP_ERROR_CHECK(esp_zb_start(false));
    esp_zb_main_loop_iteration();

    return ESP_OK;
}

static void IRAM_ATTR zero_cross_isr(void *arg)
{
    // Start phase control timer with calculated delay
    gptimer_start(g_phase_timer);
}

static void IRAM_ATTR phase_timer_callback(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_data)
{
    // Fire triac
    gpio_set_level(GPIO_TRIAC_CTRL, 1);
    // Short pulse to trigger triac
    ets_delay_us(10);
    gpio_set_level(GPIO_TRIAC_CTRL, 0);
    
    // Stop timer until next zero crossing
    gptimer_stop(timer);
}

static void control_loop_callback(void *arg)
{
    xSemaphoreTake(g_state_mutex, portMAX_DELAY);
    
    // Read temperature sensor (simplified - in real implementation use DS18B20 driver)
    // g_thermor_state.current_temp = read_temperature();
    
    // Read PIR sensor
    g_thermor_state.presence_detected = !gpio_get_level(GPIO_PIR_SENSOR);
    
    // Calculate heating power based on mode and PID
    float target_temp = g_thermor_state.target_temp;
    
    // Adjust target based on mode
    switch (g_thermor_state.mode) {
        case THERMOSTAT_MODE_ECO:
            target_temp -= 3.5f;
            break;
        case THERMOSTAT_MODE_ANTI_FREEZE:
            target_temp = 7.0f;
            break;
        case THERMOSTAT_MODE_BOOST:
            target_temp = TEMP_MAX;
            break;
        case THERMOSTAT_MODE_OFF:
            target_temp = 0;
            break;
        default:
            break;
    }
    
    // Apply presence detection
    if (!g_thermor_state.presence_detected && g_thermor_state.mode == THERMOSTAT_MODE_AUTO) {
        target_temp -= 2.0f;
    }
    
    // Window open detection (simplified)
    if (g_thermor_state.window_open) {
        target_temp = 0;
    }
    
    // Calculate PID output
    float pid_output = calculate_pid_output(target_temp, g_thermor_state.current_temp);
    uint8_t power = (uint8_t)(pid_output > 100.0f ? 100 : (pid_output < 0 ? 0 : pid_output));
    
    g_thermor_state.heating_power = power;
    g_thermor_state.heating_active = power > 0;
    
    // Update heating power
    update_heating_power(power);
    
    // Update LEDs
    gpio_set_level(GPIO_LED_HEATING, g_thermor_state.heating_active);
    
    xSemaphoreGive(g_state_mutex);
}

static void zigbee_report_callback(void *arg)
{
    // Report current temperature
    esp_zb_zcl_set_attribute_val(THERMOR_ENDPOINT_ID, 
                                 ESP_ZB_ZCL_CLUSTER_ID_THERMOSTAT,
                                 ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
                                 ESP_ZB_ZCL_ATTR_THERMOSTAT_LOCAL_TEMPERATURE_ID,
                                 (void *)&g_thermor_state.current_temp,
                                 false);
    
    // Report heating demand
    uint8_t heating_demand = g_thermor_state.heating_power;
    esp_zb_zcl_set_attribute_val(THERMOR_ENDPOINT_ID,
                                 ESP_ZB_ZCL_CLUSTER_ID_THERMOSTAT,
                                 ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
                                 ESP_ZB_ZCL_ATTR_THERMOSTAT_PI_HEATING_DEMAND_ID,
                                 &heating_demand,
                                 false);
}

static float calculate_pid_output(float setpoint, float current_value)
{
    float error = setpoint - current_value;
    
    // Proportional term
    float p_term = PID_KP * error;
    
    // Integral term
    g_pid_state.integral += error;
    // Anti-windup
    if (g_pid_state.integral > 100.0f) g_pid_state.integral = 100.0f;
    if (g_pid_state.integral < -100.0f) g_pid_state.integral = -100.0f;
    float i_term = PID_KI * g_pid_state.integral;
    
    // Derivative term
    float d_term = PID_KD * (error - g_pid_state.prev_error);
    g_pid_state.prev_error = error;
    
    return p_term + i_term + d_term;
}

static void update_heating_power(uint8_t power)
{
    // Convert power percentage to phase delay (0-100% -> 10ms-0ms)
    // For 50Hz mains: half period = 10ms
    uint32_t delay_us = (100 - power) * 100; // 0-10000us
    
    if (power == 0) {
        // Disable timer
        gptimer_stop(g_phase_timer);
        gpio_set_level(GPIO_TRIAC_CTRL, 0);
    } else {
        // Update timer alarm value
        gptimer_alarm_config_t alarm_config = {
            .reload_count = 0,
            .alarm_count = delay_us,
            .flags.auto_reload_on_alarm = false,
        };
        gptimer_set_alarm_action(g_phase_timer, &alarm_config);
    }
}

static void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct)
{
    uint32_t *p_sg_p       = signal_struct->p_app_signal;
    esp_err_t err_status   = signal_struct->esp_err_status;
    esp_zb_app_signal_type_t sig_type = *p_sg_p;
    
    switch (sig_type) {
    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
        if (err_status == ESP_OK) {
            ESP_LOGI(TAG, "Device started up in %s factory-reset mode", 
                     sig_type == ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START ? "" : "non");
            if (esp_zb_bdb_is_factory_new()) {
                ESP_LOGI(TAG, "Start network formation");
                esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
            } else {
                ESP_LOGI(TAG, "Device rebooted");
            }
        } else {
            ESP_LOGE(TAG, "Failed to initialize Zigbee stack (status: %s)", 
                     esp_err_to_name(err_status));
        }
        break;
        
    case ESP_ZB_BDB_SIGNAL_STEERING:
        if (err_status == ESP_OK) {
            esp_zb_ieee_addr_t extended_pan_id;
            esp_zb_get_extended_pan_id(extended_pan_id);
            ESP_LOGI(TAG, "Joined network successfully (Extended PAN ID: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, PAN ID: 0x%04hx, Channel:%d)",
                     extended_pan_id[7], extended_pan_id[6], extended_pan_id[5], extended_pan_id[4],
                     extended_pan_id[3], extended_pan_id[2], extended_pan_id[1], extended_pan_id[0],
                     esp_zb_get_pan_id(), esp_zb_get_current_channel());
            gpio_set_level(GPIO_LED_STATUS, 1);
        } else {
            ESP_LOGI(TAG, "Network steering was not successful (status: %s)", esp_err_to_name(err_status));
            esp_zb_scheduler_alarm((esp_zb_callback_t)esp_zb_bdb_start_top_level_commissioning, ESP_ZB_BDB_MODE_NETWORK_STEERING, 1000);
        }
        break;
        
    default:
        ESP_LOGI(TAG, "ZB signal: %s (0x%x), status: %s", esp_zb_app_signal_to_string(sig_type), 
                 sig_type, esp_err_to_name(err_status));
        break;
    }
}

esp_err_t thermor_set_mode(thermostat_mode_t mode)
{
    if (mode > THERMOSTAT_MODE_AUTO) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(g_state_mutex, portMAX_DELAY);
    g_thermor_state.mode = mode;
    xSemaphoreGive(g_state_mutex);
    
    ESP_LOGI(TAG, "Mode set to %d", mode);
    return ESP_OK;
}

esp_err_t thermor_set_temperature(float temperature)
{
    if (temperature < TEMP_MIN || temperature > TEMP_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(g_state_mutex, portMAX_DELAY);
    g_thermor_state.target_temp = temperature;
    xSemaphoreGive(g_state_mutex);
    
    // Update Zigbee attribute
    int16_t temp_zigbee = (int16_t)(temperature * 100);
    esp_zb_zcl_set_attribute_val(THERMOR_ENDPOINT_ID,
                                 ESP_ZB_ZCL_CLUSTER_ID_THERMOSTAT,
                                 ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
                                 ESP_ZB_ZCL_ATTR_THERMOSTAT_OCCUPIED_HEATING_SETPOINT_ID,
                                 &temp_zigbee,
                                 false);
    
    ESP_LOGI(TAG, "Target temperature set to %.1f°C", temperature);
    return ESP_OK;
}

float thermor_get_current_temperature(void)
{
    return g_thermor_state.current_temp;
}

void thermor_get_state(thermor_state_t *state)
{
    if (state) {
        xSemaphoreTake(g_state_mutex, portMAX_DELAY);
        memcpy(state, &g_thermor_state, sizeof(thermor_state_t));
        xSemaphoreGive(g_state_mutex);
    }
}