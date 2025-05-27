#ifndef TEMPERATURE_SENSOR_H
#define TEMPERATURE_SENSOR_H

#include <stdint.h>
#include <stdbool.h>
#include "driver/adc.h"

// Temperature sensor types
typedef enum {
    TEMP_SENSOR_NTC_10K,    // 10K NTC thermistor
    TEMP_SENSOR_NTC_100K,   // 100K NTC thermistor
    TEMP_SENSOR_PT1000,     // PT1000 RTD
    TEMP_SENSOR_DS18B20,    // Digital DS18B20
    TEMP_SENSOR_LM35        // LM35 analog sensor
} temp_sensor_type_t;

// Temperature sensor configuration
typedef struct {
    adc1_channel_t adc_channel;  // ADC channel for analog sensors
    temp_sensor_type_t sensor_type;
    
    // NTC thermistor parameters
    float beta;             // Beta coefficient
    float r_nominal;        // Nominal resistance at T_nominal
    float t_nominal;        // Nominal temperature (°C)
    float r_series;         // Series resistor value
    
    // Calibration
    float offset;           // Temperature offset
    float scale;            // Temperature scale factor
} temp_sensor_config_t;

// Temperature sensor state
typedef struct {
    temp_sensor_config_t config;
    float last_temperature;
    uint32_t last_read_time;
    bool initialized;
    
    // Moving average filter
    float *filter_buffer;
    uint8_t filter_size;
    uint8_t filter_index;
    bool filter_full;
} temp_sensor_t;

// Function prototypes
esp_err_t temperature_sensor_init(temp_sensor_t *sensor, const temp_sensor_config_t *config);
esp_err_t temperature_sensor_deinit(temp_sensor_t *sensor);
float temperature_sensor_read(temp_sensor_t *sensor);
float temperature_sensor_read_filtered(temp_sensor_t *sensor);
esp_err_t temperature_sensor_calibrate(temp_sensor_t *sensor, float offset, float scale);
esp_err_t temperature_sensor_set_filter(temp_sensor_t *sensor, uint8_t filter_size);

// Helper functions
float ntc_resistance_to_temperature(float resistance, float beta, float r_nominal, float t_nominal);
float adc_to_voltage(uint32_t adc_value, uint32_t adc_max, float vref);
float voltage_divider_resistance(float v_out, float v_in, float r_series);

#endif // TEMPERATURE_SENSOR_H