#include "thermor_ui.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "ThermorUI";

// Constants
#define TEMP_MIN 5.0f
#define TEMP_MAX 30.0f
#define TEMP_STEP 0.5f
#define MENU_TIMEOUT_MS 30000  // 30 seconds
#define BLINK_PERIOD_MS 500

// Mode names
static const char *mode_names[MODE_MAX] = {
    "COMFORT", "ECO", "FROST", "PROG", "OFF"
};

// Day names
static const char *day_names[7] = {
    "MON", "TUE", "WED", "THU", "FRI", "SAT", "SUN"
};

// Menu item names (short for LCD)
static const char *menu_names[MENU_MAX] = {
    "EXIT", "CONF", "ECO ", "TIME", "PROG", "PRES", "WIND", "LOCK", "RST "
};

void thermor_ui_init(thermor_ui_t *ui) {
    memset(ui, 0, sizeof(thermor_ui_t));
    
    // Default temperatures
    ui->config.comfort_temp = 20.0f;
    ui->config.eco_temp = 17.0f;
    ui->config.frost_temp = 7.0f;
    ui->config.current_temp = 20.0f;
    ui->config.target_temp = 20.0f;
    
    // Default mode
    ui->config.mode = MODE_COMFORT;
    ui->state = UI_STATE_NORMAL;
    
    // Default time
    ui->config.current_time.hour = 12;
    ui->config.current_time.minute = 0;
    ui->config.current_time.day_of_week = 0;
    
    // Initialize default schedule (comfort during day, eco at night)
    for (int day = 0; day < 7; day++) {
        // 6:00 - Comfort
        ui->config.schedule[day][0].start.hour = 6;
        ui->config.schedule[day][0].start.minute = 0;
        ui->config.schedule[day][0].mode = MODE_COMFORT;
        
        // 8:00 - Eco (work hours)
        ui->config.schedule[day][1].start.hour = 8;
        ui->config.schedule[day][1].start.minute = 0;
        ui->config.schedule[day][1].mode = MODE_ECO;
        
        // 17:00 - Comfort (evening)
        ui->config.schedule[day][2].start.hour = 17;
        ui->config.schedule[day][2].start.minute = 0;
        ui->config.schedule[day][2].mode = MODE_COMFORT;
        
        // 22:00 - Eco (night)
        ui->config.schedule[day][3].start.hour = 22;
        ui->config.schedule[day][3].start.minute = 0;
        ui->config.schedule[day][3].mode = MODE_ECO;
        
        // Unused slots
        ui->config.schedule[day][4].mode = MODE_OFF;
        ui->config.schedule[day][5].mode = MODE_OFF;
    }
    
    ui->last_activity_time = esp_timer_get_time() / 1000;
    
    ESP_LOGI(TAG, "UI initialized");
}

void thermor_ui_update(thermor_ui_t *ui) {
    uint32_t current_time = esp_timer_get_time() / 1000;
    
    // Check for menu timeout
    if (ui->state == UI_STATE_MENU && 
        (current_time - ui->last_activity_time) > MENU_TIMEOUT_MS) {
        thermor_ui_exit_menu(ui);
    }
    
    // Update display blink state
    if ((current_time - ui->display_update_time) > BLINK_PERIOD_MS) {
        ui->display_update_time = current_time;
        ui->display_blink_state = !ui->display_blink_state;
    }
    
    // Update target temperature based on mode
    switch (ui->config.mode) {
        case MODE_COMFORT:
            ui->config.target_temp = ui->config.comfort_temp;
            break;
        case MODE_ECO:
            ui->config.target_temp = ui->config.eco_temp;
            break;
        case MODE_FROST:
            ui->config.target_temp = ui->config.frost_temp;
            break;
        case MODE_PROG:
            // Get temperature from current schedule
            ui->config.target_temp = thermor_ui_get_target_temperature(ui);
            break;
        case MODE_OFF:
            ui->config.target_temp = 0.0f;
            break;
    }
    
    // Update display
    thermor_ui_update_display(ui);
}

void thermor_ui_handle_button(thermor_ui_t *ui, button_event_t *event) {
    ui->last_activity_time = esp_timer_get_time() / 1000;
    
    // Handle child lock
    if (ui->config.child_lock && ui->state != UI_STATE_LOCKED) {
        if (event->button == BUTTON_LOCK && event->event == BUTTON_EVENT_LONG_PRESS) {
            ui->state = UI_STATE_LOCKED;
        }
        return;  // Ignore other buttons when locked
    }
    
    // Handle unlock
    if (ui->state == UI_STATE_LOCKED) {
        if (event->button == BUTTON_LOCK && event->event == BUTTON_EVENT_LONG_PRESS) {
            ui->state = UI_STATE_NORMAL;
        }
        return;
    }
    
    // Handle button based on current state
    switch (ui->state) {
        case UI_STATE_NORMAL:
            switch (event->button) {
                case BUTTON_MODE:
                    if (event->event == BUTTON_EVENT_PRESS) {
                        // Cycle through modes
                        ui->config.mode = (ui->config.mode + 1) % MODE_MAX;
                        ESP_LOGI(TAG, "Mode changed to %s", mode_names[ui->config.mode]);
                    } else if (event->event == BUTTON_EVENT_LONG_PRESS) {
                        thermor_ui_enter_menu(ui);
                    }
                    break;
                    
                case BUTTON_PLUS:
                    if (event->event == BUTTON_EVENT_PRESS || event->event == BUTTON_EVENT_REPEAT) {
                        // Quick temperature adjustment
                        if (ui->config.mode == MODE_COMFORT) {
                            ui->config.comfort_temp += TEMP_STEP;
                            if (ui->config.comfort_temp > TEMP_MAX) {
                                ui->config.comfort_temp = TEMP_MAX;
                            }
                        }
                    }
                    break;
                    
                case BUTTON_MINUS:
                    if (event->event == BUTTON_EVENT_PRESS || event->event == BUTTON_EVENT_REPEAT) {
                        // Quick temperature adjustment
                        if (ui->config.mode == MODE_COMFORT) {
                            ui->config.comfort_temp -= TEMP_STEP;
                            if (ui->config.comfort_temp < TEMP_MIN) {
                                ui->config.comfort_temp = TEMP_MIN;
                            }
                        }
                    }
                    break;
                    
                case BUTTON_PROG:
                    if (event->event == BUTTON_EVENT_PRESS) {
                        ui->config.mode = MODE_PROG;
                    }
                    break;
                    
                case BUTTON_LOCK:
                    if (event->event == BUTTON_EVENT_LONG_PRESS) {
                        ui->config.child_lock = !ui->config.child_lock;
                        ESP_LOGI(TAG, "Child lock %s", ui->config.child_lock ? "enabled" : "disabled");
                    }
                    break;
                    
                default:
                    break;
            }
            break;
            
        case UI_STATE_MENU:
            switch (event->button) {
                case BUTTON_MODE:
                    if (event->event == BUTTON_EVENT_PRESS) {
                        thermor_ui_exit_menu(ui);
                    }
                    break;
                    
                case BUTTON_PLUS:
                    if (event->event == BUTTON_EVENT_PRESS) {
                        thermor_ui_menu_next(ui);
                    }
                    break;
                    
                case BUTTON_MINUS:
                    if (event->event == BUTTON_EVENT_PRESS) {
                        thermor_ui_menu_prev(ui);
                    }
                    break;
                    
                case BUTTON_OK:
                    if (event->event == BUTTON_EVENT_PRESS) {
                        thermor_ui_menu_select(ui);
                    }
                    break;
                    
                default:
                    break;
            }
            break;
            
        case UI_STATE_SET_TEMP:
            switch (event->button) {
                case BUTTON_PLUS:
                    if (event->event == BUTTON_EVENT_PRESS || event->event == BUTTON_EVENT_REPEAT) {
                        thermor_ui_temp_increase(ui);
                    }
                    break;
                    
                case BUTTON_MINUS:
                    if (event->event == BUTTON_EVENT_PRESS || event->event == BUTTON_EVENT_REPEAT) {
                        thermor_ui_temp_decrease(ui);
                    }
                    break;
                    
                case BUTTON_OK:
                    if (event->event == BUTTON_EVENT_PRESS) {
                        thermor_ui_temp_confirm(ui);
                    }
                    break;
                    
                case BUTTON_MODE:
                    if (event->event == BUTTON_EVENT_PRESS) {
                        thermor_ui_temp_cancel(ui);
                    }
                    break;
                    
                default:
                    break;
            }
            break;
            
        // Add other state handlers...
        default:
            break;
    }
}

void thermor_ui_update_display(thermor_ui_t *ui) {
    ht1621_display_t display = {0};
    
    switch (ui->state) {
        case UI_STATE_NORMAL:
        case UI_STATE_LOCKED:
            // Display current temperature
            ht1621_display_number(ui->config.current_temp, 1);
            
            // Set mode icon
            display.icons = 0;
            switch (ui->config.mode) {
                case MODE_COMFORT:
                    display.icons |= ICON_COMFORT;
                    break;
                case MODE_ECO:
                    display.icons |= ICON_ECO;
                    break;
                case MODE_FROST:
                    display.icons |= ICON_FROST;
                    break;
                case MODE_PROG:
                    display.icons |= ICON_PROG;
                    break;
                default:
                    break;
            }
            
            // Set status icons
            if (ui->config.heating_active) {
                display.icons |= ICON_HEATING;
            }
            if (ui->config.presence_detected && ui->config.presence_detection_enabled) {
                display.icons |= ICON_PRESENCE;
            }
            if (ui->config.window_open && ui->config.window_detection_enabled) {
                display.icons |= ICON_WINDOW;
            }
            if (ui->config.child_lock || ui->state == UI_STATE_LOCKED) {
                display.icons |= ICON_LOCK;
            }
            
            ht1621_set_all_icons(display.icons);
            break;
            
        case UI_STATE_MENU:
            // Display menu item
            ht1621_display_text(menu_names[ui->menu_selection]);
            ht1621_set_all_icons(ui->display_blink_state ? ICON_PROG : 0);
            break;
            
        case UI_STATE_SET_TEMP:
            // Display temperature being edited with blinking
            if (ui->display_blink_state) {
                ht1621_display_number(ui->temp_edit_value, 1);
            } else {
                ht1621_clear();
            }
            break;
            
        case UI_STATE_ERROR:
            // Display error code with blinking
            if (ui->display_blink_state) {
                ht1621_display_text("Err");
            } else {
                ht1621_clear();
            }
            break;
            
        default:
            break;
    }
}

void thermor_ui_enter_menu(thermor_ui_t *ui) {
    ui->state = UI_STATE_MENU;
    ui->menu_selection = MENU_EXIT;
    ESP_LOGI(TAG, "Entered menu");
}

void thermor_ui_exit_menu(thermor_ui_t *ui) {
    ui->state = UI_STATE_NORMAL;
    ESP_LOGI(TAG, "Exited menu");
}

void thermor_ui_menu_next(thermor_ui_t *ui) {
    ui->menu_selection = (ui->menu_selection + 1) % MENU_MAX;
}

void thermor_ui_menu_prev(thermor_ui_t *ui) {
    if (ui->menu_selection == 0) {
        ui->menu_selection = MENU_MAX - 1;
    } else {
        ui->menu_selection--;
    }
}

void thermor_ui_menu_select(thermor_ui_t *ui) {
    switch (ui->menu_selection) {
        case MENU_EXIT:
            thermor_ui_exit_menu(ui);
            break;
            
        case MENU_SET_COMFORT_TEMP:
            ui->state = UI_STATE_SET_TEMP;
            ui->temp_edit_value = ui->config.comfort_temp;
            ui->menu_cursor = 0;  // Remember which temp we're editing
            break;
            
        case MENU_SET_ECO_TEMP:
            ui->state = UI_STATE_SET_TEMP;
            ui->temp_edit_value = ui->config.eco_temp;
            ui->menu_cursor = 1;
            break;
            
        case MENU_PRESENCE_DETECT:
            ui->config.presence_detection_enabled = !ui->config.presence_detection_enabled;
            ESP_LOGI(TAG, "Presence detection %s", 
                     ui->config.presence_detection_enabled ? "enabled" : "disabled");
            break;
            
        case MENU_WINDOW_DETECT:
            ui->config.window_detection_enabled = !ui->config.window_detection_enabled;
            ESP_LOGI(TAG, "Window detection %s", 
                     ui->config.window_detection_enabled ? "enabled" : "disabled");
            break;
            
        case MENU_CHILD_LOCK:
            ui->config.child_lock = !ui->config.child_lock;
            ESP_LOGI(TAG, "Child lock %s", ui->config.child_lock ? "enabled" : "disabled");
            break;
            
        // Add other menu handlers...
        default:
            break;
    }
}

void thermor_ui_temp_increase(thermor_ui_t *ui) {
    ui->temp_edit_value += TEMP_STEP;
    if (ui->temp_edit_value > TEMP_MAX) {
        ui->temp_edit_value = TEMP_MAX;
    }
}

void thermor_ui_temp_decrease(thermor_ui_t *ui) {
    ui->temp_edit_value -= TEMP_STEP;
    if (ui->temp_edit_value < TEMP_MIN) {
        ui->temp_edit_value = TEMP_MIN;
    }
}

void thermor_ui_temp_confirm(thermor_ui_t *ui) {
    if (ui->menu_cursor == 0) {
        ui->config.comfort_temp = ui->temp_edit_value;
        ESP_LOGI(TAG, "Comfort temp set to %.1f°C", ui->config.comfort_temp);
    } else {
        ui->config.eco_temp = ui->temp_edit_value;
        ESP_LOGI(TAG, "Eco temp set to %.1f°C", ui->config.eco_temp);
    }
    ui->state = UI_STATE_MENU;
}

void thermor_ui_temp_cancel(thermor_ui_t *ui) {
    ui->state = UI_STATE_MENU;
}

float thermor_ui_get_target_temperature(thermor_ui_t *ui) {
    if (ui->config.mode != MODE_PROG) {
        return ui->config.target_temp;
    }
    
    // Find current schedule slot
    uint8_t day = ui->config.current_time.day_of_week;
    uint8_t hour = ui->config.current_time.hour;
    uint8_t minute = ui->config.current_time.minute;
    uint16_t current_minutes = hour * 60 + minute;
    
    thermor_mode_t scheduled_mode = MODE_ECO;  // Default
    
    // Find the active slot (last slot before current time)
    for (int slot = 5; slot >= 0; slot--) {
        if (ui->config.schedule[day][slot].mode == MODE_OFF) {
            continue;  // Skip unused slots
        }
        
        uint16_t slot_minutes = ui->config.schedule[day][slot].start.hour * 60 + 
                               ui->config.schedule[day][slot].start.minute;
        
        if (current_minutes >= slot_minutes) {
            scheduled_mode = ui->config.schedule[day][slot].mode;
            break;
        }
    }
    
    // Return temperature based on scheduled mode
    switch (scheduled_mode) {
        case MODE_COMFORT:
            return ui->config.comfort_temp;
        case MODE_ECO:
            return ui->config.eco_temp;
        case MODE_FROST:
            return ui->config.frost_temp;
        default:
            return ui->config.eco_temp;
    }
}

void thermor_ui_set_temperature(thermor_ui_t *ui, float temperature) {
    ui->config.current_temp = temperature;
}

void thermor_ui_set_heating_state(thermor_ui_t *ui, bool active) {
    ui->config.heating_active = active;
}

void thermor_ui_set_presence(thermor_ui_t *ui, bool detected) {
    ui->config.presence_detected = detected;
}

void thermor_ui_set_window_state(thermor_ui_t *ui, bool open) {
    ui->config.window_open = open;
}

const char* thermor_ui_get_mode_name(thermor_mode_t mode) {
    if (mode >= MODE_MAX) {
        return "UNKNOWN";
    }
    return mode_names[mode];
}

bool thermor_ui_is_locked(thermor_ui_t *ui) {
    return ui->state == UI_STATE_LOCKED || ui->config.child_lock;
}