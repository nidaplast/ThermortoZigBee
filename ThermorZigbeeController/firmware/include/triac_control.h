#ifndef TRIAC_CONTROL_H
#define TRIAC_CONTROL_H

#include <stdint.h>
#include <stdbool.h>
#include "driver/gpio.h"

#define MAX_TRIACS 3

// Triac control configuration
typedef struct {
    gpio_num_t triac_pins[MAX_TRIACS];  // GPIO pins for triacs
    uint8_t num_triacs;                  // Number of triacs
    gpio_num_t zero_cross_pin;           // Zero crossing detection pin
    uint16_t max_power_watts;            // Maximum power in watts
    uint8_t mains_frequency;             // Mains frequency (50 or 60 Hz)
} triac_config_t;

// Triac state
typedef struct {
    uint8_t power_level;     // Power level 0-100%
    uint16_t firing_delay;   // Delay in microseconds
    bool enabled;            // Triac enabled state
} triac_state_t;

// Function prototypes
esp_err_t triac_control_init(const triac_config_t *config);
esp_err_t triac_control_deinit(void);
esp_err_t triac_control_set_power(uint8_t power_percent);
esp_err_t triac_control_set_triac_power(uint8_t triac_num, uint8_t power_percent);
uint8_t triac_control_get_power(void);
uint8_t triac_control_get_triac_power(uint8_t triac_num);
esp_err_t triac_control_enable(bool enable);
esp_err_t triac_control_enable_triac(uint8_t triac_num, bool enable);
bool triac_control_is_enabled(void);
uint16_t triac_control_get_actual_power_watts(void);

// Phase control helpers
uint16_t power_to_firing_delay(uint8_t power_percent, uint8_t frequency);
uint8_t firing_delay_to_power(uint16_t delay_us, uint8_t frequency);

#endif // TRIAC_CONTROL_H