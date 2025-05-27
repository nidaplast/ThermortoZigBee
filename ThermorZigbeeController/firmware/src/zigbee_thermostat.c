#include "zigbee_thermostat.h"
#include "esp_log.h"
#include "esp_check.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ha/esp_zigbee_ha_standard.h"
#include <string.h>

static const char *TAG = "ZigbeeThermostat";

// Global Zigbee stack config
static esp_zb_cfg_t zb_nwk_cfg = {
    .esp_zb_role = ESP_ZB_DEVICE_TYPE_ED,
    .install_code_policy = INSTALLCODE_POLICY_ENABLE,
    .nwk_cfg = {
        .zczr_cfg = {
            .max_children = MAX_CHILDREN,
        },
    },
};

// Convert float temperature to Zigbee format (0.01°C units)
int16_t float_to_zigbee_temp(float temp) {
    return (int16_t)(temp * 100);
}

// Convert Zigbee temperature to float
float zigbee_temp_to_float(int16_t temp) {
    return (float)temp / 100.0f;
}

// Create thermostat cluster
static esp_zb_cluster_list_t *create_thermostat_cluster_list(zigbee_thermostat_t *device) {
    esp_zb_cluster_list_t *cluster_list = esp_zb_zcl_cluster_list_create();
    
    // Basic cluster
    esp_zb_basic_cluster_cfg_t basic_cfg = {
        .zcl_version = ESP_ZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE,
        .power_source = ESP_ZB_ZCL_BASIC_POWER_SOURCE_DEFAULT_VALUE,
    };
    
    esp_zb_attribute_list_t *basic_cluster = esp_zb_basic_cluster_create(&basic_cfg);
    esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, 
                                  MANUFACTURER_NAME);
    esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, 
                                  MODEL_IDENTIFIER);
    
    // Identify cluster
    esp_zb_identify_cluster_cfg_t identify_cfg = {
        .identify_time = ESP_ZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE,
    };
    esp_zb_attribute_list_t *identify_cluster = esp_zb_identify_cluster_create(&identify_cfg);
    
    // Thermostat cluster
    esp_zb_thermostat_cluster_cfg_t thermostat_cfg = {
        .local_temperature = float_to_zigbee_temp(20.0f),
        .occupied_cooling_setpoint = ESP_ZB_ZCL_THERMOSTAT_OCCUPIED_COOLING_SETPOINT_DEFAULT,
        .occupied_heating_setpoint = float_to_zigbee_temp(20.0f),
        .min_heat_setpoint_limit = float_to_zigbee_temp(5.0f),
        .max_heat_setpoint_limit = float_to_zigbee_temp(30.0f),
        .control_sequence_of_operation = ESP_ZB_ZCL_THERMOSTAT_CONTROL_SEQ_OF_OPERATION_HEATING_ONLY,
        .system_mode = ESP_ZB_ZCL_THERMOSTAT_SYSTEM_MODE_HEAT,
    };
    esp_zb_attribute_list_t *thermostat_cluster = esp_zb_thermostat_cluster_create(&thermostat_cfg);
    
    // Add running state attribute
    uint16_t running_state = 0x0000;  // No demand
    esp_zb_thermostat_cluster_add_attr(thermostat_cluster, 
                                       ESP_ZB_ZCL_ATTR_THERMOSTAT_RUNNING_STATE_ID,
                                       &running_state);
    
    // Occupancy sensing cluster
    esp_zb_occupancy_sensing_cluster_cfg_t occupancy_cfg = {
        .occupancy = ESP_ZB_ZCL_OCCUPANCY_SENSING_OCCUPANCY_UNOCCUPIED,
        .occupancy_sensor_type = ESP_ZB_ZCL_OCCUPANCY_SENSING_OCCUPANCY_SENSOR_TYPE_PIR,
    };
    esp_zb_attribute_list_t *occupancy_cluster = esp_zb_occupancy_sensing_cluster_create(&occupancy_cfg);
    
    // Power configuration cluster
    esp_zb_power_config_cluster_cfg_t power_cfg = {
        .mains_voltage = 2300,  // 230V in 0.1V units
        .mains_frequency = 50,
    };
    esp_zb_attribute_list_t *power_cluster = esp_zb_power_config_cluster_create(&power_cfg);
    
    // Add clusters to list
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_basic_cluster(cluster_list, basic_cluster, 
                    ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_identify_cluster(cluster_list, identify_cluster, 
                    ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_thermostat_cluster(cluster_list, thermostat_cluster, 
                    ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_occupancy_sensing_cluster(cluster_list, occupancy_cluster, 
                    ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_power_config_cluster(cluster_list, power_cluster, 
                    ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));
    
    return cluster_list;
}

// Attribute change handler
esp_err_t zb_attribute_handler(const esp_zb_zcl_set_attr_value_message_t *message) {
    esp_err_t ret = ESP_OK;
    
    ESP_LOGI(TAG, "Attribute change: endpoint=%d, cluster=0x%04x, attribute=0x%04x",
             message->info.dst_endpoint, message->info.cluster, message->attribute.id);
    
    if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_THERMOSTAT) {
        zigbee_thermostat_t *device = (zigbee_thermostat_t *)esp_zb_get_endpoint_user_ctx(message->info.dst_endpoint);
        if (!device || !device->ui) {
            return ESP_ERR_INVALID_STATE;
        }
        
        switch (message->attribute.id) {
            case ESP_ZB_ZCL_ATTR_THERMOSTAT_OCCUPIED_HEATING_SETPOINT_ID: {
                int16_t *setpoint = (int16_t *)message->attribute.data.value;
                float temp = zigbee_temp_to_float(*setpoint);
                
                ESP_LOGI(TAG, "New heating setpoint: %.1f°C", temp);
                
                // Update UI based on current mode
                if (device->ui->config.mode == MODE_COMFORT) {
                    device->ui->config.comfort_temp = temp;
                } else if (device->ui->config.mode == MODE_ECO) {
                    device->ui->config.eco_temp = temp;
                }
                break;
            }
            
            case ESP_ZB_ZCL_ATTR_THERMOSTAT_SYSTEM_MODE_ID: {
                uint8_t *mode = (uint8_t *)message->attribute.data.value;
                if (*mode == ESP_ZB_ZCL_THERMOSTAT_SYSTEM_MODE_OFF) {
                    thermor_ui_set_mode(device->ui, MODE_OFF);
                } else if (*mode == ESP_ZB_ZCL_THERMOSTAT_SYSTEM_MODE_HEAT) {
                    thermor_ui_set_mode(device->ui, MODE_COMFORT);
                }
                break;
            }
            
            default:
                ESP_LOGW(TAG, "Unhandled thermostat attribute: 0x%04x", message->attribute.id);
                break;
        }
    }
    
    return ret;
}

// Action handler callback
esp_err_t zb_action_handler(esp_zb_core_action_callback_id_t callback_id, const void *message) {
    esp_err_t ret = ESP_OK;
    
    switch (callback_id) {
        case ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID:
            ret = zb_attribute_handler((esp_zb_zcl_set_attr_value_message_t *)message);
            break;
            
        case ESP_ZB_CORE_CMD_READ_ATTR_RESP_CB_ID:
            ESP_LOGI(TAG, "Read attribute response received");
            break;
            
        case ESP_ZB_CORE_CMD_REPORT_CONFIG_RESP_CB_ID:
            ESP_LOGI(TAG, "Report configuration response");
            break;
            
        default:
            ESP_LOGW(TAG, "Unhandled action callback: %d", callback_id);
            break;
    }
    
    return ret;
}

// Zigbee signal handler
void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct) {
    uint32_t *p_sg_p = signal_struct->p_app_signal;
    esp_err_t err_status = signal_struct->esp_err_status;
    esp_zb_app_signal_type_t sig_type = *p_sg_p;
    esp_zb_zdo_signal_leave_params_t *leave_params = NULL;
    
    switch (sig_type) {
        case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
            ESP_LOGI(TAG, "Initialize Zigbee stack");
            esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
            break;
            
        case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
        case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
            if (err_status == ESP_OK) {
                ESP_LOGI(TAG, "Device started up in %s factory-reset mode", 
                         sig_type == ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START ? "" : "non");
                if (sig_type == ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START) {
                    ESP_LOGI(TAG, "Start network steering");
                    esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
                } else {
                    ESP_LOGI(TAG, "Device rebooted");
                }
            } else {
                ESP_LOGE(TAG, "Failed to initialize Zigbee stack (status: %d)", err_status);
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
            } else {
                ESP_LOGI(TAG, "Network steering was not successful (status: %d)", err_status);
                esp_zb_scheduler_alarm((esp_zb_callback_t)esp_zb_bdb_start_top_level_commissioning, 
                                      ESP_ZB_BDB_MODE_NETWORK_STEERING, 1000);
            }
            break;
            
        case ESP_ZB_ZDO_SIGNAL_LEAVE:
            leave_params = (esp_zb_zdo_signal_leave_params_t *)esp_zb_app_signal_get_params(p_sg_p);
            if (leave_params->leave_type == ESP_ZB_NWK_LEAVE_TYPE_RESET) {
                ESP_LOGI(TAG, "Reset device");
                zigbee_thermostat_factory_reset();
            }
            break;
            
        default:
            ESP_LOGI(TAG, "ZDO signal: %s (0x%x), status: %s", 
                     esp_zb_zdo_signal_to_string(sig_type), sig_type,
                     esp_err_to_name(err_status));
            break;
    }
}

// Initialize Zigbee thermostat
esp_err_t zigbee_thermostat_init(zigbee_thermostat_t *device, thermor_ui_t *ui) {
    if (!device || !ui) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memset(device, 0, sizeof(zigbee_thermostat_t));
    device->ui = ui;
    device->endpoint = HA_THERMOSTAT_ENDPOINT;
    
    // Platform config
    esp_zb_platform_config_t config = {
        .radio_config = ESP_ZB_DEFAULT_RADIO_CONFIG(),
        .host_config = ESP_ZB_DEFAULT_HOST_CONFIG(),
    };
    ESP_ERROR_CHECK(esp_zb_platform_config(&config));
    
    // Create endpoint list
    device->ep_list = esp_zb_ep_list_create();
    
    // Create thermostat endpoint
    esp_zb_cluster_list_t *cluster_list = create_thermostat_cluster_list(device);
    esp_zb_endpoint_config_t endpoint_config = {
        .endpoint = device->endpoint,
        .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .app_device_id = ESP_ZB_HA_THERMOSTAT_DEVICE_ID,
        .app_device_version = 0,
    };
    
    esp_zb_ep_list_add_ep(device->ep_list, cluster_list, endpoint_config);
    
    // Register device
    esp_zb_device_register(device->ep_list);
    
    // Set callbacks
    esp_zb_core_action_handler_register(zb_action_handler);
    esp_zb_set_primary_network_channel_set(ESP_ZB_PRIMARY_CHANNEL_MASK);
    
    device->initialized = true;
    ESP_LOGI(TAG, "Zigbee thermostat initialized");
    
    return ESP_OK;
}

// Zigbee task
void zigbee_thermostat_task(void *pvParameters) {
    zigbee_thermostat_t *device = (zigbee_thermostat_t *)pvParameters;
    
    // Initialize Zigbee stack
    ESP_ERROR_CHECK(esp_zb_init(&zb_nwk_cfg));
    esp_zb_set_network_ed_timeout(ED_AGING_TIMEOUT);
    esp_zb_set_ed_keep_alive(ED_KEEP_ALIVE);
    
    // Start Zigbee stack
    ESP_ERROR_CHECK(esp_zb_start(false));
    esp_zb_main_loop_iteration();
    
    // This task should not return
    vTaskDelete(NULL);
}

// Update functions
esp_err_t zigbee_thermostat_update_temperature(zigbee_thermostat_t *device, float temp) {
    if (!device || !device->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    device->thermostat.local_temperature = float_to_zigbee_temp(temp);
    
    // Update attribute
    esp_zb_zcl_set_attribute_val(device->endpoint,
                                ESP_ZB_ZCL_CLUSTER_ID_THERMOSTAT,
                                ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
                                ESP_ZB_ZCL_ATTR_THERMOSTAT_LOCAL_TEMPERATURE_ID,
                                &device->thermostat.local_temperature,
                                false);
    
    return ESP_OK;
}

esp_err_t zigbee_thermostat_update_setpoint(zigbee_thermostat_t *device, float setpoint) {
    if (!device || !device->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    device->thermostat.occupied_heating_setpoint = float_to_zigbee_temp(setpoint);
    
    esp_zb_zcl_set_attribute_val(device->endpoint,
                                ESP_ZB_ZCL_CLUSTER_ID_THERMOSTAT,
                                ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
                                ESP_ZB_ZCL_ATTR_THERMOSTAT_OCCUPIED_HEATING_SETPOINT_ID,
                                &device->thermostat.occupied_heating_setpoint,
                                false);
    
    return ESP_OK;
}

esp_err_t zigbee_thermostat_update_heating_state(zigbee_thermostat_t *device, bool heating) {
    if (!device || !device->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    device->thermostat.running_state = heating ? 0x0001 : 0x0000;  // Heat demand bit
    
    esp_zb_zcl_set_attribute_val(device->endpoint,
                                ESP_ZB_ZCL_CLUSTER_ID_THERMOSTAT,
                                ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
                                ESP_ZB_ZCL_ATTR_THERMOSTAT_RUNNING_STATE_ID,
                                &device->thermostat.running_state,
                                false);
    
    return ESP_OK;
}

esp_err_t zigbee_thermostat_update_occupancy(zigbee_thermostat_t *device, bool occupied) {
    if (!device || !device->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    device->occupancy.occupancy = occupied ? 
        ESP_ZB_ZCL_OCCUPANCY_SENSING_OCCUPANCY_OCCUPIED :
        ESP_ZB_ZCL_OCCUPANCY_SENSING_OCCUPANCY_UNOCCUPIED;
    
    esp_zb_zcl_set_attribute_val(device->endpoint,
                                ESP_ZB_ZCL_CLUSTER_ID_OCCUPANCY_SENSING,
                                ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
                                ESP_ZB_ZCL_ATTR_OCCUPANCY_SENSING_OCCUPANCY_ID,
                                &device->occupancy.occupancy,
                                false);
    
    return ESP_OK;
}

esp_err_t zigbee_thermostat_update_mode(zigbee_thermostat_t *device, thermor_mode_t mode) {
    if (!device || !device->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Map Thermor modes to Zigbee system modes
    switch (mode) {
        case MODE_OFF:
            device->thermostat.system_mode = ESP_ZB_ZCL_THERMOSTAT_SYSTEM_MODE_OFF;
            break;
        case MODE_COMFORT:
        case MODE_ECO:
        case MODE_FROST:
        case MODE_PROG:
            device->thermostat.system_mode = ESP_ZB_ZCL_THERMOSTAT_SYSTEM_MODE_HEAT;
            break;
        default:
            return ESP_ERR_INVALID_ARG;
    }
    
    esp_zb_zcl_set_attribute_val(device->endpoint,
                                ESP_ZB_ZCL_CLUSTER_ID_THERMOSTAT,
                                ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
                                ESP_ZB_ZCL_ATTR_THERMOSTAT_SYSTEM_MODE_ID,
                                &device->thermostat.system_mode,
                                false);
    
    return ESP_OK;
}

void zigbee_thermostat_factory_reset(void) {
    ESP_LOGI(TAG, "Performing factory reset");
    esp_zb_factory_reset();
}