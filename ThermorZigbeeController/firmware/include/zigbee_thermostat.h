#ifndef ZIGBEE_THERMOSTAT_H
#define ZIGBEE_THERMOSTAT_H

#include "esp_zigbee_core.h"
#include "esp_err.h"
#include "thermor_ui.h"

// Zigbee configuration
#define INSTALLCODE_POLICY_ENABLE    false
#define ED_AGING_TIMEOUT              ESP_ZB_ED_AGING_TIMEOUT_64MIN
#define ED_KEEP_ALIVE                 3000    // 3000 ms
#define ZIGBEE_CHANNEL                11      // Default channel
#define MAX_CHILDREN                  10

// Zigbee endpoints
#define HA_THERMOSTAT_ENDPOINT        1

// Custom manufacturer info
#define MANUFACTURER_NAME             "DIY_Thermor"
#define MODEL_IDENTIFIER             "THERMOR_ZB_V1"

// Thermostat cluster attributes
typedef struct {
    int16_t local_temperature;           // 0x0000 - Measured temperature (0.01°C)
    int16_t occupied_cooling_setpoint;   // 0x0011 - Not used
    int16_t occupied_heating_setpoint;   // 0x0012 - Target temperature (0.01°C)
    int16_t min_heat_setpoint_limit;     // 0x0015 - Min temp limit
    int16_t max_heat_setpoint_limit;     // 0x0016 - Max temp limit
    uint8_t control_sequence;            // 0x001B - Heating only
    uint8_t system_mode;                 // 0x001C - Off/Heat
    uint8_t running_state;               // 0x0029 - Heat demand
} thermostat_data_t;

// Occupancy sensing attributes
typedef struct {
    uint8_t occupancy;                   // 0x0000 - Occupied/Unoccupied
    uint8_t occupancy_sensor_type;       // 0x0001 - PIR
} occupancy_data_t;

// Power configuration attributes
typedef struct {
    uint16_t mains_voltage;              // 0x0000 - Mains voltage (V)
    uint8_t mains_frequency;             // 0x0001 - 50Hz
} power_config_data_t;

// Custom cluster for Thermor-specific features
#define THERMOR_CUSTOM_CLUSTER_ID     0xFC00

typedef struct {
    uint8_t window_open_detection;       // 0x0000 - Window detection enabled
    uint8_t window_open_state;           // 0x0001 - Window open/closed
    uint8_t presence_detection_enabled;  // 0x0002 - Presence detection enabled
    uint8_t child_lock;                  // 0x0003 - Child lock state
    uint16_t eco_temperature;            // 0x0004 - Eco mode temperature
    uint16_t frost_temperature;          // 0x0005 - Frost protection temp
    uint8_t schedule_enabled;            // 0x0006 - Programming mode enabled
    uint8_t current_mode;                // 0x0007 - Comfort/Eco/Frost/Prog/Off
    uint32_t energy_consumption;         // 0x0008 - Energy counter (Wh)
    uint16_t current_power;              // 0x0009 - Current power (W)
} thermor_custom_data_t;

// Zigbee device context
typedef struct {
    esp_zb_ep_list_t *ep_list;
    thermostat_data_t thermostat;
    occupancy_data_t occupancy;
    power_config_data_t power_config;
    thermor_custom_data_t custom;
    thermor_ui_t *ui;
    uint8_t endpoint;
    bool initialized;
} zigbee_thermostat_t;

// Function prototypes
esp_err_t zigbee_thermostat_init(zigbee_thermostat_t *device, thermor_ui_t *ui);
void zigbee_thermostat_task(void *pvParameters);
esp_err_t zigbee_thermostat_update_temperature(zigbee_thermostat_t *device, float temp);
esp_err_t zigbee_thermostat_update_setpoint(zigbee_thermostat_t *device, float setpoint);
esp_err_t zigbee_thermostat_update_heating_state(zigbee_thermostat_t *device, bool heating);
esp_err_t zigbee_thermostat_update_occupancy(zigbee_thermostat_t *device, bool occupied);
esp_err_t zigbee_thermostat_update_window_state(zigbee_thermostat_t *device, bool open);
esp_err_t zigbee_thermostat_update_mode(zigbee_thermostat_t *device, thermor_mode_t mode);
esp_err_t zigbee_thermostat_update_power(zigbee_thermostat_t *device, uint16_t power);
esp_err_t zigbee_thermostat_report_attributes(zigbee_thermostat_t *device);

// Zigbee callbacks
esp_err_t zb_attribute_handler(const esp_zb_zcl_set_attr_value_message_t *message);
esp_err_t zb_read_attr_handler(esp_zb_zcl_cmd_read_attr_resp_message_t *message);
esp_err_t zb_action_handler(esp_zb_core_action_callback_id_t callback_id, const void *message);
void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct);

// Helper functions
int16_t float_to_zigbee_temp(float temp);
float zigbee_temp_to_float(int16_t temp);
void zigbee_thermostat_factory_reset(void);

#endif // ZIGBEE_THERMOSTAT_H