#ifndef THERMOR_UI_H
#define THERMOR_UI_H

#include <stdint.h>
#include <stdbool.h>
#include "button_matrix.h"
#include "ht1621_driver.h"

// Operating modes
typedef enum {
    MODE_COMFORT,     // Comfort temperature
    MODE_ECO,         // Eco temperature (reduced)
    MODE_FROST,       // Frost protection
    MODE_PROG,        // Programming/schedule mode
    MODE_OFF,         // Heater off
    MODE_MAX
} thermor_mode_t;

// UI states
typedef enum {
    UI_STATE_NORMAL,          // Normal operation
    UI_STATE_MENU,            // In menu
    UI_STATE_SET_TEMP,        // Setting temperature
    UI_STATE_SET_TIME,        // Setting time
    UI_STATE_PROG_SCHEDULE,   // Programming schedule
    UI_STATE_LOCKED,          // UI locked
    UI_STATE_ERROR            // Error display
} ui_state_t;

// Menu items
typedef enum {
    MENU_EXIT,
    MENU_SET_COMFORT_TEMP,
    MENU_SET_ECO_TEMP,
    MENU_SET_TIME,
    MENU_PROG_SCHEDULE,
    MENU_PRESENCE_DETECT,
    MENU_WINDOW_DETECT,
    MENU_CHILD_LOCK,
    MENU_RESET,
    MENU_MAX
} menu_item_t;

// Time structure
typedef struct {
    uint8_t hour;
    uint8_t minute;
    uint8_t day_of_week;  // 0-6, 0=Monday
} thermor_time_t;

// Schedule slot
typedef struct {
    thermor_time_t start;
    thermor_mode_t mode;
} schedule_slot_t;

// UI configuration
typedef struct {
    float comfort_temp;         // Comfort temperature setting
    float eco_temp;             // Eco temperature setting
    float frost_temp;           // Frost protection temperature
    float current_temp;         // Current measured temperature
    float target_temp;          // Current target temperature
    thermor_mode_t mode;        // Current operating mode
    bool heating_active;        // Heating element active
    bool presence_detected;     // Presence sensor state
    bool window_open;           // Window detection state
    bool child_lock;            // Child lock enabled
    bool presence_detection_enabled;
    bool window_detection_enabled;
    thermor_time_t current_time;
    schedule_slot_t schedule[7][6];  // 7 days, 6 slots per day
} thermor_config_t;

// UI context
typedef struct {
    thermor_config_t config;
    ui_state_t state;
    menu_item_t menu_selection;
    uint8_t menu_cursor;
    float temp_edit_value;
    thermor_time_t time_edit_value;
    uint8_t prog_day;
    uint8_t prog_slot;
    uint32_t last_activity_time;
    uint32_t display_update_time;
    bool display_blink_state;
} thermor_ui_t;

// Function prototypes
void thermor_ui_init(thermor_ui_t *ui);
void thermor_ui_update(thermor_ui_t *ui);
void thermor_ui_handle_button(thermor_ui_t *ui, button_event_t *event);
void thermor_ui_set_temperature(thermor_ui_t *ui, float temperature);
void thermor_ui_set_mode(thermor_ui_t *ui, thermor_mode_t mode);
void thermor_ui_update_display(thermor_ui_t *ui);
float thermor_ui_get_target_temperature(thermor_ui_t *ui);
void thermor_ui_set_heating_state(thermor_ui_t *ui, bool active);
void thermor_ui_set_presence(thermor_ui_t *ui, bool detected);
void thermor_ui_set_window_state(thermor_ui_t *ui, bool open);
void thermor_ui_update_time(thermor_ui_t *ui, thermor_time_t *time);
void thermor_ui_show_error(thermor_ui_t *ui, const char *error_code);
void thermor_ui_clear_error(thermor_ui_t *ui);
bool thermor_ui_is_locked(thermor_ui_t *ui);

// Menu navigation helpers
void thermor_ui_enter_menu(thermor_ui_t *ui);
void thermor_ui_exit_menu(thermor_ui_t *ui);
void thermor_ui_menu_next(thermor_ui_t *ui);
void thermor_ui_menu_prev(thermor_ui_t *ui);
void thermor_ui_menu_select(thermor_ui_t *ui);

// Temperature editing helpers
void thermor_ui_temp_increase(thermor_ui_t *ui);
void thermor_ui_temp_decrease(thermor_ui_t *ui);
void thermor_ui_temp_confirm(thermor_ui_t *ui);
void thermor_ui_temp_cancel(thermor_ui_t *ui);

// Schedule programming helpers
void thermor_ui_prog_next_day(thermor_ui_t *ui);
void thermor_ui_prog_prev_day(thermor_ui_t *ui);
void thermor_ui_prog_next_slot(thermor_ui_t *ui);
void thermor_ui_prog_prev_slot(thermor_ui_t *ui);
void thermor_ui_prog_edit_slot(thermor_ui_t *ui);

// Utility functions
const char* thermor_ui_get_mode_name(thermor_mode_t mode);
const char* thermor_ui_get_day_name(uint8_t day);
void thermor_ui_format_time(char *buffer, size_t size, thermor_time_t *time);

#endif // THERMOR_UI_H