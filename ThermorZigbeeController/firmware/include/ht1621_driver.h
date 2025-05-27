#ifndef HT1621_DRIVER_H
#define HT1621_DRIVER_H

#include <stdint.h>
#include <stdbool.h>
#include "driver/gpio.h"

// HT1621 Commands
#define HT1621_CMD_SYS_DIS   0x00  // System disable
#define HT1621_CMD_SYS_EN    0x01  // System enable
#define HT1621_CMD_LCD_OFF   0x02  // LCD off
#define HT1621_CMD_LCD_ON    0x03  // LCD on
#define HT1621_CMD_RC_256K   0x18  // RC 256K
#define HT1621_CMD_BIAS_1_2  0x29  // 1/2 bias, 4 commons

// Pin definitions
typedef struct {
    gpio_num_t cs_pin;
    gpio_num_t wr_pin;
    gpio_num_t data_pin;
} ht1621_config_t;

// Display segments mapping
typedef struct {
    uint8_t digit[4];      // 4 digits (XX.X°C)
    uint8_t icons;         // Special icons byte
    uint8_t decimal_point; // Decimal point position
} ht1621_display_t;

// Icon definitions (bit positions in icons byte)
#define ICON_COMFORT    (1 << 0)  // House icon
#define ICON_ECO        (1 << 1)  // Moon icon
#define ICON_FROST      (1 << 2)  // Snowflake icon
#define ICON_PROG       (1 << 3)  // Clock icon
#define ICON_LOCK       (1 << 4)  // Padlock icon
#define ICON_PRESENCE   (1 << 5)  // Person icon
#define ICON_WINDOW     (1 << 6)  // Window icon
#define ICON_HEATING    (1 << 7)  // Heating active

// 7-segment digit encoding (0-9, A-F, special chars)
#define SEG_A   0x01
#define SEG_B   0x02
#define SEG_C   0x04
#define SEG_D   0x08
#define SEG_E   0x10
#define SEG_F   0x20
#define SEG_G   0x40
#define SEG_DP  0x80

// Predefined digit patterns
extern const uint8_t DIGIT_PATTERNS[16];
extern const uint8_t CHAR_PATTERNS[26];  // A-Z

// Function prototypes
void ht1621_init(const ht1621_config_t *config);
void ht1621_send_command(uint8_t command);
void ht1621_write_data(uint8_t address, uint8_t data);
void ht1621_write_all(const uint8_t *data, uint8_t length);
void ht1621_clear(void);
void ht1621_display_number(float number, uint8_t decimal_places);
void ht1621_display_text(const char *text);
void ht1621_set_icon(uint8_t icon, bool state);
void ht1621_set_all_icons(uint8_t icons);
void ht1621_update_display(const ht1621_display_t *display);
void ht1621_test_pattern(void);

// Low-level bit-banging functions
void ht1621_write_bits(uint8_t data, uint8_t bits);
void ht1621_send_cmd(uint8_t cmd);
void ht1621_send_addr(uint8_t addr);

#endif // HT1621_DRIVER_H