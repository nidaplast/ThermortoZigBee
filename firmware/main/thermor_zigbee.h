/**
 * @file thermor_zigbee.h
 * @brief Thermor Zigbee application header
 */

#ifndef THERMOR_ZIGBEE_H
#define THERMOR_ZIGBEE_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

/* GPIO Definitions */
#define GPIO_ZERO_CROSS     GPIO_NUM_4   // Zero crossing detection
#define GPIO_TRIAC_CTRL     GPIO_NUM_5   // Triac control (via optocoupler)
#define GPIO_TEMP_SENSOR    GPIO_NUM_6   // DS18B20 temperature sensor
#define GPIO_PIR_SENSOR     GPIO_NUM_7   // PIR motion sensor
#define GPIO_BUTTON_MODE    GPIO_NUM_8   // Mode button
#define GPIO_BUTTON_PLUS    GPIO_NUM_9   // Plus button
#define GPIO_BUTTON_MINUS   GPIO_NUM_10  // Minus button
#define GPIO_LED_STATUS     GPIO_NUM_18  // Status LED
#define GPIO_LED_HEATING    GPIO_NUM_19  // Heating indicator LED

/* Thermostat modes */
typedef enum {
    THERMOSTAT_MODE_OFF = 0,
    THERMOSTAT_MODE_COMFORT,
    THERMOSTAT_MODE_ECO,
    THERMOSTAT_MODE_ANTI_FREEZE,
    THERMOSTAT_MODE_BOOST,
    THERMOSTAT_MODE_AUTO
} thermostat_mode_t;

/* Device state structure */
typedef struct {
    float current_temp;          // Current temperature in °C
    float target_temp;           // Target temperature in °C
    uint8_t heating_power;       // Heating power 0-100%
    thermostat_mode_t mode;      // Current operating mode
    bool presence_detected;      // PIR sensor state
    bool window_open;           // Window open detection
    bool heating_active;        // Heating element state
    uint32_t power_consumption; // Current power in Watts
} thermor_state_t;

/* Function prototypes */
esp_err_t thermor_zigbee_init(void);
esp_err_t thermor_set_mode(thermostat_mode_t mode);
esp_err_t thermor_set_temperature(float temperature);
float thermor_get_current_temperature(void);
void thermor_get_state(thermor_state_t *state);

#endif /* THERMOR_ZIGBEE_H */