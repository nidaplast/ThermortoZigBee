#ifndef BUTTON_MATRIX_H
#define BUTTON_MATRIX_H

#include <stdint.h>
#include <stdbool.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// Button matrix configuration
#define BUTTON_MATRIX_ROWS    2
#define BUTTON_MATRIX_COLS    3
#define BUTTON_DEBOUNCE_MS    50
#define BUTTON_LONG_PRESS_MS  1000
#define BUTTON_REPEAT_DELAY_MS 500
#define BUTTON_REPEAT_RATE_MS  100

// Button IDs (mapped to matrix positions)
typedef enum {
    BUTTON_MODE = 0,    // Row 0, Col 0
    BUTTON_PLUS,        // Row 0, Col 1
    BUTTON_MINUS,       // Row 0, Col 2
    BUTTON_PROG,        // Row 1, Col 0
    BUTTON_OK,          // Row 1, Col 1
    BUTTON_LOCK,        // Row 1, Col 2
    BUTTON_MAX
} button_id_t;

// Button events
typedef enum {
    BUTTON_EVENT_PRESS,
    BUTTON_EVENT_RELEASE,
    BUTTON_EVENT_LONG_PRESS,
    BUTTON_EVENT_REPEAT
} button_event_type_t;

// Button event structure
typedef struct {
    button_id_t button;
    button_event_type_t event;
    uint32_t timestamp;
} button_event_t;

// Button matrix configuration
typedef struct {
    gpio_num_t row_pins[BUTTON_MATRIX_ROWS];
    gpio_num_t col_pins[BUTTON_MATRIX_COLS];
    QueueHandle_t event_queue;
    uint32_t debounce_ms;
    uint32_t long_press_ms;
    uint32_t repeat_delay_ms;
    uint32_t repeat_rate_ms;
} button_matrix_config_t;

// Button state tracking
typedef struct {
    bool pressed;
    bool long_press_fired;
    uint32_t press_time;
    uint32_t last_repeat_time;
    uint8_t debounce_count;
} button_state_t;

// Function prototypes
esp_err_t button_matrix_init(const button_matrix_config_t *config);
esp_err_t button_matrix_deinit(void);
void button_matrix_scan_task(void *pvParameters);
bool button_matrix_is_pressed(button_id_t button);
esp_err_t button_matrix_get_event(button_event_t *event, TickType_t timeout);
const char* button_matrix_get_name(button_id_t button);
void button_matrix_enable_repeat(button_id_t button, bool enable);
void button_matrix_set_callback(void (*callback)(button_event_t *event));

// Utility functions
void button_matrix_test(void);
void button_matrix_print_state(void);

#endif // BUTTON_MATRIX_H