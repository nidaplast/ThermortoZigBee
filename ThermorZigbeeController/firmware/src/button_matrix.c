#include "button_matrix.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "ButtonMatrix";

// Global state
static button_matrix_config_t g_config;
static button_state_t g_button_states[BUTTON_MAX];
static TaskHandle_t g_scan_task_handle = NULL;
static bool g_initialized = false;
static void (*g_callback)(button_event_t *event) = NULL;
static bool g_repeat_enabled[BUTTON_MAX] = {false};

// Button names for debugging
static const char *button_names[BUTTON_MAX] = {
    "MODE", "PLUS", "MINUS", "PROG", "OK", "LOCK"
};

// Matrix position to button ID mapping
static const button_id_t matrix_map[BUTTON_MATRIX_ROWS][BUTTON_MATRIX_COLS] = {
    {BUTTON_MODE, BUTTON_PLUS, BUTTON_MINUS},
    {BUTTON_PROG, BUTTON_OK, BUTTON_LOCK}
};

esp_err_t button_matrix_init(const button_matrix_config_t *config) {
    if (g_initialized) {
        ESP_LOGW(TAG, "Button matrix already initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!config || !config->event_queue) {
        ESP_LOGE(TAG, "Invalid configuration");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Copy configuration
    memcpy(&g_config, config, sizeof(button_matrix_config_t));
    
    // Set default timings if not specified
    if (g_config.debounce_ms == 0) g_config.debounce_ms = BUTTON_DEBOUNCE_MS;
    if (g_config.long_press_ms == 0) g_config.long_press_ms = BUTTON_LONG_PRESS_MS;
    if (g_config.repeat_delay_ms == 0) g_config.repeat_delay_ms = BUTTON_REPEAT_DELAY_MS;
    if (g_config.repeat_rate_ms == 0) g_config.repeat_rate_ms = BUTTON_REPEAT_RATE_MS;
    
    // Configure row pins as outputs
    gpio_config_t row_conf = {
        .pin_bit_mask = 0,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    
    for (int i = 0; i < BUTTON_MATRIX_ROWS; i++) {
        row_conf.pin_bit_mask |= (1ULL << config->row_pins[i]);
        gpio_set_level(config->row_pins[i], 1);  // High = inactive
    }
    gpio_config(&row_conf);
    
    // Configure column pins as inputs with pull-ups
    gpio_config_t col_conf = {
        .pin_bit_mask = 0,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    
    for (int i = 0; i < BUTTON_MATRIX_COLS; i++) {
        col_conf.pin_bit_mask |= (1ULL << config->col_pins[i]);
    }
    gpio_config(&col_conf);
    
    // Initialize button states
    memset(g_button_states, 0, sizeof(g_button_states));
    
    // Enable repeat for PLUS and MINUS buttons by default
    g_repeat_enabled[BUTTON_PLUS] = true;
    g_repeat_enabled[BUTTON_MINUS] = true;
    
    // Create scan task
    xTaskCreate(button_matrix_scan_task, "button_scan", 2048, NULL, 10, &g_scan_task_handle);
    
    g_initialized = true;
    ESP_LOGI(TAG, "Button matrix initialized");
    
    return ESP_OK;
}

esp_err_t button_matrix_deinit(void) {
    if (!g_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Stop scan task
    if (g_scan_task_handle) {
        vTaskDelete(g_scan_task_handle);
        g_scan_task_handle = NULL;
    }
    
    // Reset GPIO pins
    for (int i = 0; i < BUTTON_MATRIX_ROWS; i++) {
        gpio_reset_pin(g_config.row_pins[i]);
    }
    for (int i = 0; i < BUTTON_MATRIX_COLS; i++) {
        gpio_reset_pin(g_config.col_pins[i]);
    }
    
    g_initialized = false;
    ESP_LOGI(TAG, "Button matrix deinitialized");
    
    return ESP_OK;
}

static void send_event(button_id_t button, button_event_type_t event_type) {
    button_event_t event = {
        .button = button,
        .event = event_type,
        .timestamp = esp_timer_get_time() / 1000  // Convert to ms
    };
    
    // Send to queue
    xQueueSend(g_config.event_queue, &event, 0);
    
    // Call callback if registered
    if (g_callback) {
        g_callback(&event);
    }
    
    ESP_LOGD(TAG, "Button %s: %s", button_names[button],
             event_type == BUTTON_EVENT_PRESS ? "PRESS" :
             event_type == BUTTON_EVENT_RELEASE ? "RELEASE" :
             event_type == BUTTON_EVENT_LONG_PRESS ? "LONG_PRESS" : "REPEAT");
}

void button_matrix_scan_task(void *pvParameters) {
    const TickType_t scan_period = pdMS_TO_TICKS(10);  // 10ms scan rate
    uint32_t current_time;
    
    while (1) {
        current_time = esp_timer_get_time() / 1000;  // Convert to ms
        
        // Scan each row
        for (int row = 0; row < BUTTON_MATRIX_ROWS; row++) {
            // Activate current row (low)
            gpio_set_level(g_config.row_pins[row], 0);
            
            // Small delay for signal to settle
            ets_delay_us(10);
            
            // Read columns
            for (int col = 0; col < BUTTON_MATRIX_COLS; col++) {
                button_id_t button = matrix_map[row][col];
                bool is_pressed = (gpio_get_level(g_config.col_pins[col]) == 0);
                button_state_t *state = &g_button_states[button];
                
                // Debounce logic
                if (is_pressed) {
                    if (state->debounce_count < 255) {
                        state->debounce_count++;
                    }
                } else {
                    if (state->debounce_count > 0) {
                        state->debounce_count--;
                    }
                }
                
                // Check for state change after debounce
                bool debounced_state = (state->debounce_count > (g_config.debounce_ms / 10));
                
                if (debounced_state && !state->pressed) {
                    // Button pressed
                    state->pressed = true;
                    state->press_time = current_time;
                    state->last_repeat_time = current_time;
                    state->long_press_fired = false;
                    send_event(button, BUTTON_EVENT_PRESS);
                    
                } else if (!debounced_state && state->pressed) {
                    // Button released
                    state->pressed = false;
                    send_event(button, BUTTON_EVENT_RELEASE);
                    
                } else if (state->pressed && !state->long_press_fired) {
                    // Check for long press
                    if ((current_time - state->press_time) >= g_config.long_press_ms) {
                        state->long_press_fired = true;
                        send_event(button, BUTTON_EVENT_LONG_PRESS);
                        
                        // Start repeat if enabled
                        if (g_repeat_enabled[button]) {
                            state->last_repeat_time = current_time;
                        }
                    }
                    
                } else if (state->pressed && state->long_press_fired && g_repeat_enabled[button]) {
                    // Handle repeat
                    uint32_t repeat_interval = (current_time - state->press_time > 
                                               g_config.long_press_ms + g_config.repeat_delay_ms) ?
                                              g_config.repeat_rate_ms : g_config.repeat_delay_ms;
                    
                    if ((current_time - state->last_repeat_time) >= repeat_interval) {
                        state->last_repeat_time = current_time;
                        send_event(button, BUTTON_EVENT_REPEAT);
                    }
                }
            }
            
            // Deactivate row
            gpio_set_level(g_config.row_pins[row], 1);
        }
        
        vTaskDelay(scan_period);
    }
}

bool button_matrix_is_pressed(button_id_t button) {
    if (button >= BUTTON_MAX) {
        return false;
    }
    return g_button_states[button].pressed;
}

esp_err_t button_matrix_get_event(button_event_t *event, TickType_t timeout) {
    if (!event) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xQueueReceive(g_config.event_queue, event, timeout) == pdTRUE) {
        return ESP_OK;
    }
    
    return ESP_ERR_TIMEOUT;
}

const char* button_matrix_get_name(button_id_t button) {
    if (button >= BUTTON_MAX) {
        return "UNKNOWN";
    }
    return button_names[button];
}

void button_matrix_enable_repeat(button_id_t button, bool enable) {
    if (button < BUTTON_MAX) {
        g_repeat_enabled[button] = enable;
    }
}

void button_matrix_set_callback(void (*callback)(button_event_t *event)) {
    g_callback = callback;
}

void button_matrix_test(void) {
    ESP_LOGI(TAG, "Button matrix test - press buttons to test");
    
    button_event_t event;
    while (1) {
        if (button_matrix_get_event(&event, portMAX_DELAY) == ESP_OK) {
            ESP_LOGI(TAG, "Button %s: %s at %lu ms",
                     button_matrix_get_name(event.button),
                     event.event == BUTTON_EVENT_PRESS ? "PRESS" :
                     event.event == BUTTON_EVENT_RELEASE ? "RELEASE" :
                     event.event == BUTTON_EVENT_LONG_PRESS ? "LONG_PRESS" : "REPEAT",
                     event.timestamp);
            
            // Exit test on LOCK button long press
            if (event.button == BUTTON_LOCK && event.event == BUTTON_EVENT_LONG_PRESS) {
                ESP_LOGI(TAG, "Test mode exit");
                break;
            }
        }
    }
}

void button_matrix_print_state(void) {
    ESP_LOGI(TAG, "Button states:");
    for (int i = 0; i < BUTTON_MAX; i++) {
        ESP_LOGI(TAG, "  %s: %s (debounce=%d)",
                 button_names[i],
                 g_button_states[i].pressed ? "PRESSED" : "RELEASED",
                 g_button_states[i].debounce_count);
    }
}