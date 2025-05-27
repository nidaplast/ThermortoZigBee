/**
 * @file thermor_zigbee.h
 * @brief Main header file for Thermor Zigbee Controller
 * @date 2024-01-15
 * 
 * ESP32-C6 based Zigbee controller for Thermor Emotion 4 heaters
 * Provides native Zigbee support with advanced heating control
 */

#ifndef THERMOR_ZIGBEE_H
#define THERMOR_ZIGBEE_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_system.h"

// GPIO Pin Definitions (matching KiCad schematic)
#define GPIO_TRIAC_CONTROL      GPIO_NUM_4   // IO4 - Triac control via optocoupler
#define GPIO_ZERO_CROSS        GPIO_NUM_5    // IO5 - Zero crossing detection
#define GPIO_TEMP_SENSOR       GPIO_NUM_6    // IO6 - Temperature sensor (1-Wire or I2C)
#define GPIO_PIR_SENSOR        GPIO_NUM_7    // IO7 - PIR motion sensor input
#define GPIO_LCD_CS            GPIO_NUM_8    // IO8 - HT1621 LCD Chip Select
#define GPIO_LCD_WR            GPIO_NUM_9    // IO9 - HT1621 LCD Write
#define GPIO_LCD_DATA          GPIO_NUM_10   // IO10 - HT1621 LCD Data
#define GPIO_BTN_ROW1          GPIO_NUM_11   // IO11 - Button matrix row 1
#define GPIO_BTN_ROW2          GPIO_NUM_12   // IO12 - Button matrix row 2
#define GPIO_BTN_ROW3          GPIO_NUM_13   // IO13 - Button matrix row 3
#define GPIO_BTN_COL1          GPIO_NUM_18   // IO18 - Button matrix column 1
#define GPIO_BTN_COL2          GPIO_NUM_19   // IO19 - Button matrix column 2
#define GPIO_BTN_COL3          GPIO_NUM_20   // IO20 - Button matrix column 3
#define GPIO_STATUS_LED        GPIO_NUM_21   // IO21 - Status LED
#define GPIO_WINDOW_SENSOR     GPIO_NUM_22   // IO22 - Window open detection
#define GPIO_PRESENCE_OVERRIDE GPIO_NUM_23   // IO23 - Manual presence override

// System Configuration
#define MAIN_TASK_STACK_SIZE    4096
#define MAIN_TASK_PRIORITY      5
#define TEMP_TASK_STACK_SIZE    2048
#define TEMP_TASK_PRIORITY      4
#define UI_TASK_STACK_SIZE      3072
#define UI_TASK_PRIORITY        3
#define ZIGBEE_TASK_STACK_SIZE  4096
#define ZIGBEE_TASK_PRIORITY    6

// Temperature Control Parameters
#define TEMP_MIN_CELSIUS        5.0f
#define TEMP_MAX_CELSIUS        30.0f
#define TEMP_STEP_CELSIUS       0.5f
#define TEMP_HYSTERESIS         0.2f
#define TEMP_SAMPLE_PERIOD_MS   1000
#define TEMP_FILTER_SAMPLES     10

// Power Control Parameters
#define POWER_MIN_PERCENT       0
#define POWER_MAX_PERCENT       100
#define POWER_SOFT_START_MS     2000
#define ZERO_CROSS_TIMEOUT_MS   25    // 50Hz = 20ms period

// UI Configuration
#define BUTTON_DEBOUNCE_MS      50
#define BUTTON_LONG_PRESS_MS    1000
#define LCD_UPDATE_PERIOD_MS    200
#define BACKLIGHT_TIMEOUT_S     30

// Zigbee Configuration
#define ZIGBEE_CHANNEL_MASK     0x07FFF800  // Channels 11-26
#define MANUFACTURER_NAME       "DIY_Smart"
#define MODEL_IDENTIFIER        "THERMOR_ZB_1"
#define DEVICE_VERSION          1

// Operating Modes
typedef enum {
    MODE_OFF = 0,
    MODE_COMFORT,
    MODE_ECO,
    MODE_ANTI_FREEZE,
    MODE_PROGRAM,
    MODE_BOOST,
    MODE_VACATION
} heating_mode_t;

// System States
typedef enum {
    STATE_INIT = 0,
    STATE_IDLE,
    STATE_HEATING,
    STATE_ERROR,
    STATE_PAIRING,
    STATE_UPDATING
} system_state_t;

// Temperature Data Structure
typedef struct {
    float current;
    float target;
    float offset;
    bool valid;
    uint32_t last_update;
} temperature_data_t;

// Power Control Structure
typedef struct {
    uint8_t current_percent;
    uint8_t target_percent;
    bool soft_start_active;
    uint32_t soft_start_begin;
    uint32_t last_zero_cross;
    uint16_t phase_delay_us;
} power_control_t;

// Presence Detection Structure
typedef struct {
    bool pir_detected;
    bool manual_override;
    bool window_open;
    uint32_t last_motion;
    uint32_t absence_timer_min;
} presence_data_t;

// Program Schedule Entry
typedef struct {
    uint8_t hour;
    uint8_t minute;
    heating_mode_t mode;
    float temperature;
} schedule_entry_t;

// System Configuration Structure
typedef struct {
    heating_mode_t mode;
    system_state_t state;
    temperature_data_t temperature;
    power_control_t power;
    presence_data_t presence;
    schedule_entry_t schedule[7][6];  // 7 days, 6 entries per day
    bool child_lock;
    bool adaptive_start;
    bool open_window_detection;
    uint8_t lcd_brightness;
    uint16_t zigbee_short_addr;
} system_config_t;

// Global system configuration
extern system_config_t g_system_config;
extern SemaphoreHandle_t g_config_mutex;
extern QueueHandle_t g_event_queue;

// Event Types
typedef enum {
    EVENT_BUTTON_PRESS,
    EVENT_BUTTON_LONG_PRESS,
    EVENT_TEMP_UPDATE,
    EVENT_PRESENCE_CHANGE,
    EVENT_WINDOW_CHANGE,
    EVENT_ZIGBEE_CMD,
    EVENT_ZERO_CROSS,
    EVENT_TIMER_TICK
} event_type_t;

// Event Structure
typedef struct {
    event_type_t type;
    union {
        uint8_t button_id;
        float temperature;
        bool state;
        uint8_t zigbee_cmd;
    } data;
} system_event_t;

// Function Prototypes

// System Initialization
esp_err_t thermor_zigbee_init(void);
esp_err_t thermor_system_start(void);
void thermor_system_stop(void);

// Configuration Management
esp_err_t thermor_config_save(void);
esp_err_t thermor_config_load(void);
esp_err_t thermor_config_reset(void);

// Temperature Control
float thermor_get_current_temp(void);
float thermor_get_target_temp(void);
esp_err_t thermor_set_target_temp(float temp);
esp_err_t thermor_set_temp_offset(float offset);

// Mode Control
heating_mode_t thermor_get_mode(void);
esp_err_t thermor_set_mode(heating_mode_t mode);
const char* thermor_mode_to_string(heating_mode_t mode);

// Power Control
uint8_t thermor_get_power_percent(void);
esp_err_t thermor_set_power_percent(uint8_t percent);
esp_err_t thermor_enable_soft_start(bool enable);

// Presence Detection
bool thermor_is_presence_detected(void);
esp_err_t thermor_set_presence_override(bool override);
esp_err_t thermor_set_absence_timer(uint32_t minutes);

// Schedule Management
esp_err_t thermor_set_schedule(uint8_t day, uint8_t slot, schedule_entry_t *entry);
esp_err_t thermor_get_schedule(uint8_t day, uint8_t slot, schedule_entry_t *entry);
esp_err_t thermor_clear_schedule(void);

// System Control
esp_err_t thermor_set_child_lock(bool enable);
esp_err_t thermor_set_adaptive_start(bool enable);
esp_err_t thermor_set_window_detection(bool enable);
system_state_t thermor_get_system_state(void);

// Diagnostic Functions
esp_err_t thermor_run_self_test(void);
esp_err_t thermor_get_diagnostics(char *buffer, size_t len);
uint32_t thermor_get_uptime_seconds(void);
uint32_t thermor_get_energy_kwh(void);

// Utility Macros
#define THERMOR_CHECK(x) do {                              \
    esp_err_t __err = (x);                                 \
    if (__err != ESP_OK) {                                \
        ESP_LOGE(TAG, "Error %s at %s:%d",                \
                 esp_err_to_name(__err), __FILE__, __LINE__); \
        return __err;                                      \
    }                                                      \
} while(0)

#define THERMOR_CHECK_GOTO(x, label) do {                  \
    esp_err_t __err = (x);                                 \
    if (__err != ESP_OK) {                                \
        ESP_LOGE(TAG, "Error %s at %s:%d",                \
                 esp_err_to_name(__err), __FILE__, __LINE__); \
        goto label;                                        \
    }                                                      \
} while(0)

#endif // THERMOR_ZIGBEE_H