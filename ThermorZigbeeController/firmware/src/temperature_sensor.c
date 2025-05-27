#include "temperature_sensor.h"
#include "esp_log.h"
#include "esp_adc_cal.h"
#include <math.h>
#include <stdlib.h>

static const char *TAG = "TempSensor";

// ADC configuration
#define DEFAULT_VREF    1100        // Default ADC reference voltage in mV
#define ADC_SAMPLES     64          // Number of samples for averaging
#define ADC_WIDTH       ADC_WIDTH_BIT_12
#define ADC_ATTEN       ADC_ATTEN_DB_11  // Full scale 0-3.3V

static esp_adc_cal_characteristics_t adc_chars;

esp_err_t temperature_sensor_init(temp_sensor_t *sensor, const temp_sensor_config_t *config) {
    if (!sensor || !config) {
        return ESP_ERR_INVALID_ARG;
    }
    
    sensor->config = *config;
    sensor->last_temperature = 20.0f;  // Default temperature
    sensor->last_read_time = 0;
    sensor->filter_buffer = NULL;
    sensor->filter_size = 0;
    sensor->filter_index = 0;
    sensor->filter_full = false;
    
    // Configure ADC for analog sensors
    if (config->sensor_type != TEMP_SENSOR_DS18B20) {
        adc1_config_width(ADC_WIDTH);
        adc1_config_channel_atten(config->adc_channel, ADC_ATTEN);
        
        // Characterize ADC
        esp_adc_cal_value_t val_type = esp_adc_cal_characterize(
            ADC_UNIT_1, ADC_ATTEN, ADC_WIDTH, DEFAULT_VREF, &adc_chars);
        
        if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
            ESP_LOGI(TAG, "ADC characterized using Two Point Value");
        } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
            ESP_LOGI(TAG, "ADC characterized using eFuse Vref");
        } else {
            ESP_LOGI(TAG, "ADC characterized using Default Vref");
        }
    }
    
    // Set default calibration if not specified
    if (sensor->config.offset == 0 && sensor->config.scale == 0) {
        sensor->config.offset = 0.0f;
        sensor->config.scale = 1.0f;
    }
    
    sensor->initialized = true;
    ESP_LOGI(TAG, "Temperature sensor initialized");
    
    return ESP_OK;
}

esp_err_t temperature_sensor_deinit(temp_sensor_t *sensor) {
    if (!sensor) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (sensor->filter_buffer) {
        free(sensor->filter_buffer);
        sensor->filter_buffer = NULL;
    }
    
    sensor->initialized = false;
    return ESP_OK;
}

float ntc_resistance_to_temperature(float resistance, float beta, float r_nominal, float t_nominal) {
    // Steinhart-Hart equation simplified form
    // 1/T = 1/T0 + (1/B) * ln(R/R0)
    float t0_kelvin = t_nominal + 273.15f;
    float inv_t = 1.0f / t0_kelvin + (1.0f / beta) * logf(resistance / r_nominal);
    float t_kelvin = 1.0f / inv_t;
    return t_kelvin - 273.15f;  // Convert to Celsius
}

float adc_to_voltage(uint32_t adc_value, uint32_t adc_max, float vref) {
    return (float)adc_value * vref / (float)adc_max;
}

float voltage_divider_resistance(float v_out, float v_in, float r_series) {
    // R_thermistor = R_series * V_out / (V_in - V_out)
    if (v_out >= v_in) {
        return INFINITY;  // Open circuit
    }
    return r_series * v_out / (v_in - v_out);
}

static float read_ntc_temperature(temp_sensor_t *sensor) {
    // Read ADC multiple times for averaging
    uint32_t adc_reading = 0;
    for (int i = 0; i < ADC_SAMPLES; i++) {
        adc_reading += adc1_get_raw(sensor->config.adc_channel);
    }
    adc_reading /= ADC_SAMPLES;
    
    // Convert to voltage
    uint32_t voltage = esp_adc_cal_raw_to_voltage(adc_reading, &adc_chars);
    float v_thermistor = voltage / 1000.0f;  // Convert mV to V
    
    // Calculate thermistor resistance
    float v_supply = 3.3f;  // Supply voltage
    float r_thermistor = voltage_divider_resistance(v_thermistor, v_supply, sensor->config.r_series);
    
    // Convert resistance to temperature
    float temperature = ntc_resistance_to_temperature(
        r_thermistor, 
        sensor->config.beta,
        sensor->config.r_nominal,
        sensor->config.t_nominal
    );
    
    // Apply calibration
    temperature = temperature * sensor->config.scale + sensor->config.offset;
    
    ESP_LOGD(TAG, "ADC: %d, Voltage: %.3fV, Resistance: %.0fΩ, Temp: %.1f°C",
             adc_reading, v_thermistor, r_thermistor, temperature);
    
    return temperature;
}

static float read_lm35_temperature(temp_sensor_t *sensor) {
    // LM35: 10mV/°C, 0°C = 0V
    uint32_t adc_reading = 0;
    for (int i = 0; i < ADC_SAMPLES; i++) {
        adc_reading += adc1_get_raw(sensor->config.adc_channel);
    }
    adc_reading /= ADC_SAMPLES;
    
    uint32_t voltage = esp_adc_cal_raw_to_voltage(adc_reading, &adc_chars);
    float temperature = voltage / 10.0f;  // 10mV per degree
    
    // Apply calibration
    temperature = temperature * sensor->config.scale + sensor->config.offset;
    
    return temperature;
}

float temperature_sensor_read(temp_sensor_t *sensor) {
    if (!sensor || !sensor->initialized) {
        ESP_LOGE(TAG, "Sensor not initialized");
        return -273.15f;  // Invalid temperature
    }
    
    float temperature = 0.0f;
    
    switch (sensor->config.sensor_type) {
        case TEMP_SENSOR_NTC_10K:
        case TEMP_SENSOR_NTC_100K:
            temperature = read_ntc_temperature(sensor);
            break;
            
        case TEMP_SENSOR_LM35:
            temperature = read_lm35_temperature(sensor);
            break;
            
        case TEMP_SENSOR_DS18B20:
            // TODO: Implement OneWire protocol for DS18B20
            ESP_LOGW(TAG, "DS18B20 not implemented yet");
            temperature = 20.0f;
            break;
            
        case TEMP_SENSOR_PT1000:
            // TODO: Implement PT1000 reading
            ESP_LOGW(TAG, "PT1000 not implemented yet");
            temperature = 20.0f;
            break;
            
        default:
            ESP_LOGE(TAG, "Unknown sensor type");
            return -273.15f;
    }
    
    // Sanity check
    if (temperature < -50.0f || temperature > 150.0f) {
        ESP_LOGW(TAG, "Temperature out of range: %.1f°C", temperature);
        return sensor->last_temperature;  // Return last valid reading
    }
    
    sensor->last_temperature = temperature;
    sensor->last_read_time = esp_timer_get_time() / 1000;
    
    return temperature;
}

float temperature_sensor_read_filtered(temp_sensor_t *sensor) {
    if (!sensor || !sensor->initialized) {
        return -273.15f;
    }
    
    float raw_temp = temperature_sensor_read(sensor);
    
    if (!sensor->filter_buffer || sensor->filter_size == 0) {
        return raw_temp;  // No filtering
    }
    
    // Add to filter buffer
    sensor->filter_buffer[sensor->filter_index] = raw_temp;
    sensor->filter_index = (sensor->filter_index + 1) % sensor->filter_size;
    
    if (!sensor->filter_full && sensor->filter_index == 0) {
        sensor->filter_full = true;
    }
    
    // Calculate average
    float sum = 0.0f;
    int count = sensor->filter_full ? sensor->filter_size : sensor->filter_index;
    
    for (int i = 0; i < count; i++) {
        sum += sensor->filter_buffer[i];
    }
    
    return sum / count;
}

esp_err_t temperature_sensor_calibrate(temp_sensor_t *sensor, float offset, float scale) {
    if (!sensor) {
        return ESP_ERR_INVALID_ARG;
    }
    
    sensor->config.offset = offset;
    sensor->config.scale = scale;
    
    ESP_LOGI(TAG, "Calibration set: offset=%.2f, scale=%.2f", offset, scale);
    return ESP_OK;
}

esp_err_t temperature_sensor_set_filter(temp_sensor_t *sensor, uint8_t filter_size) {
    if (!sensor) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Free existing buffer
    if (sensor->filter_buffer) {
        free(sensor->filter_buffer);
        sensor->filter_buffer = NULL;
    }
    
    if (filter_size == 0) {
        sensor->filter_size = 0;
        return ESP_OK;
    }
    
    // Allocate new buffer
    sensor->filter_buffer = (float *)calloc(filter_size, sizeof(float));
    if (!sensor->filter_buffer) {
        ESP_LOGE(TAG, "Failed to allocate filter buffer");
        return ESP_ERR_NO_MEM;
    }
    
    sensor->filter_size = filter_size;
    sensor->filter_index = 0;
    sensor->filter_full = false;
    
    ESP_LOGI(TAG, "Filter enabled with size %d", filter_size);
    return ESP_OK;
}