#include "triac_control.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <math.h>

static const char *TAG = "TriacControl";

// Constants
#define TIMER_GROUP         TIMER_GROUP_0
#define TIMER_IDX           TIMER_0
#define TIMER_DIVIDER       80      // 1 microsecond resolution
#define TRIAC_PULSE_WIDTH   10      // 10 microseconds pulse

// Global state
static triac_config_t g_config;
static triac_state_t g_triacs[MAX_TRIACS];
static SemaphoreHandle_t g_mutex = NULL;
static volatile bool g_zero_cross_detected = false;
static esp_timer_handle_t g_firing_timer = NULL;

// Calculate half-cycle period in microseconds
static uint32_t get_half_cycle_us(uint8_t frequency) {
    return (frequency == 60) ? 8333 : 10000;  // 60Hz: 8.33ms, 50Hz: 10ms
}

// Zero crossing interrupt handler
static void IRAM_ATTR zero_cross_isr(void *arg) {
    g_zero_cross_detected = true;
    
    // Schedule firing for all enabled triacs
    for (int i = 0; i < g_config.num_triacs; i++) {
        if (g_triacs[i].enabled && g_triacs[i].power_level > 0) {
            // Use hardware timer for precise timing
            timer_set_counter_value(TIMER_GROUP, TIMER_IDX, 0);
            timer_set_alarm_value(TIMER_GROUP, TIMER_IDX, g_triacs[i].firing_delay);
            timer_start(TIMER_GROUP, TIMER_IDX);
        }
    }
}

// Timer interrupt for triac firing
static void IRAM_ATTR timer_isr(void *arg) {
    // Clear interrupt
    timer_group_clr_intr_status_in_isr(TIMER_GROUP, TIMER_IDX);
    
    // Fire all triacs that need firing
    for (int i = 0; i < g_config.num_triacs; i++) {
        if (g_triacs[i].enabled && g_triacs[i].power_level > 0) {
            gpio_set_level(g_config.triac_pins[i], 1);
        }
    }
    
    // Schedule pulse end
    ets_delay_us(TRIAC_PULSE_WIDTH);
    
    // Turn off all triacs
    for (int i = 0; i < g_config.num_triacs; i++) {
        gpio_set_level(g_config.triac_pins[i], 0);
    }
}

esp_err_t triac_control_init(const triac_config_t *config) {
    if (!config || config->num_triacs == 0 || config->num_triacs > MAX_TRIACS) {
        ESP_LOGE(TAG, "Invalid configuration");
        return ESP_ERR_INVALID_ARG;
    }
    
    g_config = *config;
    
    // Create mutex
    g_mutex = xSemaphoreCreateMutex();
    if (!g_mutex) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }
    
    // Configure triac output pins
    gpio_config_t io_conf = {
        .pin_bit_mask = 0,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    
    for (int i = 0; i < config->num_triacs; i++) {
        io_conf.pin_bit_mask |= (1ULL << config->triac_pins[i]);
        gpio_set_level(config->triac_pins[i], 0);  // Start with triacs off
    }
    gpio_config(&io_conf);
    
    // Configure zero crossing input
    gpio_config_t zc_conf = {
        .pin_bit_mask = (1ULL << config->zero_cross_pin),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_POSEDGE,  // Trigger on rising edge
    };
    gpio_config(&zc_conf);
    
    // Install GPIO ISR service
    gpio_install_isr_service(0);
    gpio_isr_handler_add(config->zero_cross_pin, zero_cross_isr, NULL);
    
    // Configure hardware timer for phase control
    timer_config_t timer_config = {
        .divider = TIMER_DIVIDER,
        .counter_dir = TIMER_COUNT_UP,
        .counter_en = TIMER_PAUSE,
        .alarm_en = TIMER_ALARM_EN,
        .intr_type = TIMER_INTR_LEVEL,
        .auto_reload = TIMER_AUTORELOAD_DIS,
    };
    timer_init(TIMER_GROUP, TIMER_IDX, &timer_config);
    timer_set_counter_value(TIMER_GROUP, TIMER_IDX, 0);
    timer_enable_intr(TIMER_GROUP, TIMER_IDX);
    timer_isr_register(TIMER_GROUP, TIMER_IDX, timer_isr, NULL, ESP_INTR_FLAG_IRAM, NULL);
    
    // Initialize triac states
    for (int i = 0; i < MAX_TRIACS; i++) {
        g_triacs[i].power_level = 0;
        g_triacs[i].firing_delay = get_half_cycle_us(config->mains_frequency);
        g_triacs[i].enabled = false;
    }
    
    ESP_LOGI(TAG, "Triac control initialized with %d triacs, %dHz mains", 
             config->num_triacs, config->mains_frequency);
    
    return ESP_OK;
}

esp_err_t triac_control_deinit(void) {
    // Disable all triacs
    for (int i = 0; i < g_config.num_triacs; i++) {
        gpio_set_level(g_config.triac_pins[i], 0);
        gpio_reset_pin(g_config.triac_pins[i]);
    }
    
    // Remove ISR
    gpio_isr_handler_remove(g_config.zero_cross_pin);
    gpio_uninstall_isr_service();
    
    // Deinit timer
    timer_disable_intr(TIMER_GROUP, TIMER_IDX);
    timer_deinit(TIMER_GROUP, TIMER_IDX);
    
    // Delete mutex
    if (g_mutex) {
        vSemaphoreDelete(g_mutex);
        g_mutex = NULL;
    }
    
    return ESP_OK;
}

uint16_t power_to_firing_delay(uint8_t power_percent, uint8_t frequency) {
    if (power_percent >= 100) {
        return 100;  // Minimum delay for maximum power
    }
    if (power_percent == 0) {
        return get_half_cycle_us(frequency);  // Maximum delay (no firing)
    }
    
    // Convert power percentage to firing angle
    // 0% power = 180° delay, 100% power = 0° delay
    // Using sine wave relationship for better linearity
    float angle_rad = (1.0f - power_percent / 100.0f) * M_PI;
    float delay_ratio = (1.0f + cos(angle_rad)) / 2.0f;
    
    uint32_t half_cycle = get_half_cycle_us(frequency);
    return (uint16_t)(delay_ratio * half_cycle);
}

uint8_t firing_delay_to_power(uint16_t delay_us, uint8_t frequency) {
    uint32_t half_cycle = get_half_cycle_us(frequency);
    
    if (delay_us >= half_cycle) {
        return 0;
    }
    if (delay_us <= 100) {
        return 100;
    }
    
    float delay_ratio = (float)delay_us / half_cycle;
    float angle_rad = acos(2.0f * delay_ratio - 1.0f);
    float power_ratio = 1.0f - (angle_rad / M_PI);
    
    return (uint8_t)(power_ratio * 100);
}

esp_err_t triac_control_set_power(uint8_t power_percent) {
    if (power_percent > 100) {
        power_percent = 100;
    }
    
    xSemaphoreTake(g_mutex, portMAX_DELAY);
    
    // Set same power for all triacs
    for (int i = 0; i < g_config.num_triacs; i++) {
        g_triacs[i].power_level = power_percent;
        g_triacs[i].firing_delay = power_to_firing_delay(power_percent, g_config.mains_frequency);
        
        if (power_percent > 0 && !g_triacs[i].enabled) {
            g_triacs[i].enabled = true;
        } else if (power_percent == 0) {
            g_triacs[i].enabled = false;
            gpio_set_level(g_config.triac_pins[i], 0);
        }
    }
    
    xSemaphoreGive(g_mutex);
    
    ESP_LOGD(TAG, "Power set to %d%% (delay: %dus)", 
             power_percent, g_triacs[0].firing_delay);
    
    return ESP_OK;
}

esp_err_t triac_control_set_triac_power(uint8_t triac_num, uint8_t power_percent) {
    if (triac_num >= g_config.num_triacs) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (power_percent > 100) {
        power_percent = 100;
    }
    
    xSemaphoreTake(g_mutex, portMAX_DELAY);
    
    g_triacs[triac_num].power_level = power_percent;
    g_triacs[triac_num].firing_delay = power_to_firing_delay(power_percent, g_config.mains_frequency);
    
    if (power_percent > 0 && !g_triacs[triac_num].enabled) {
        g_triacs[triac_num].enabled = true;
    } else if (power_percent == 0) {
        g_triacs[triac_num].enabled = false;
        gpio_set_level(g_config.triac_pins[triac_num], 0);
    }
    
    xSemaphoreGive(g_mutex);
    
    return ESP_OK;
}

uint8_t triac_control_get_power(void) {
    xSemaphoreTake(g_mutex, portMAX_DELAY);
    uint8_t power = g_triacs[0].power_level;  // Return first triac power
    xSemaphoreGive(g_mutex);
    return power;
}

uint8_t triac_control_get_triac_power(uint8_t triac_num) {
    if (triac_num >= g_config.num_triacs) {
        return 0;
    }
    
    xSemaphoreTake(g_mutex, portMAX_DELAY);
    uint8_t power = g_triacs[triac_num].power_level;
    xSemaphoreGive(g_mutex);
    return power;
}

esp_err_t triac_control_enable(bool enable) {
    xSemaphoreTake(g_mutex, portMAX_DELAY);
    
    for (int i = 0; i < g_config.num_triacs; i++) {
        if (enable && g_triacs[i].power_level > 0) {
            g_triacs[i].enabled = true;
        } else {
            g_triacs[i].enabled = false;
            gpio_set_level(g_config.triac_pins[i], 0);
        }
    }
    
    xSemaphoreGive(g_mutex);
    return ESP_OK;
}

esp_err_t triac_control_enable_triac(uint8_t triac_num, bool enable) {
    if (triac_num >= g_config.num_triacs) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(g_mutex, portMAX_DELAY);
    
    if (enable && g_triacs[triac_num].power_level > 0) {
        g_triacs[triac_num].enabled = true;
    } else {
        g_triacs[triac_num].enabled = false;
        gpio_set_level(g_config.triac_pins[triac_num], 0);
    }
    
    xSemaphoreGive(g_mutex);
    return ESP_OK;
}

bool triac_control_is_enabled(void) {
    xSemaphoreTake(g_mutex, portMAX_DELAY);
    bool enabled = false;
    
    for (int i = 0; i < g_config.num_triacs; i++) {
        if (g_triacs[i].enabled) {
            enabled = true;
            break;
        }
    }
    
    xSemaphoreGive(g_mutex);
    return enabled;
}

uint16_t triac_control_get_actual_power_watts(void) {
    xSemaphoreTake(g_mutex, portMAX_DELAY);
    
    uint32_t total_power = 0;
    for (int i = 0; i < g_config.num_triacs; i++) {
        if (g_triacs[i].enabled) {
            total_power += (g_triacs[i].power_level * g_config.max_power_watts) / 100;
        }
    }
    
    // Divide by number of triacs if they share the load
    total_power /= g_config.num_triacs;
    
    xSemaphoreGive(g_mutex);
    return (uint16_t)total_power;
}