#include "ht1621_driver.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "rom/ets_sys.h"

static const char *TAG = "HT1621";

// Global config storage
static ht1621_config_t g_config;

// 7-segment patterns for digits 0-9
const uint8_t DIGIT_PATTERNS[16] = {
    0x3F,  // 0: ABCDEF
    0x06,  // 1: BC
    0x5B,  // 2: ABDEG
    0x4F,  // 3: ABCDG
    0x66,  // 4: BCFG
    0x6D,  // 5: ACDFG
    0x7D,  // 6: ACDEFG
    0x07,  // 7: ABC
    0x7F,  // 8: ABCDEFG
    0x6F,  // 9: ABCDFG
    0x77,  // A: ABCEFG
    0x7C,  // b: CDEFG
    0x39,  // C: ADEF
    0x5E,  // d: BCDEG
    0x79,  // E: ADEFG
    0x71   // F: AEFG
};

// Character patterns A-Z (simplified for 7-segment)
const uint8_t CHAR_PATTERNS[26] = {
    0x77,  // A
    0x7C,  // b
    0x39,  // C
    0x5E,  // d
    0x79,  // E
    0x71,  // F
    0x3D,  // G
    0x76,  // H
    0x06,  // I
    0x1E,  // J
    0x76,  // k (same as H)
    0x38,  // L
    0x15,  // m (simplified)
    0x54,  // n
    0x3F,  // O (same as 0)
    0x73,  // P
    0x67,  // q
    0x50,  // r
    0x6D,  // S (same as 5)
    0x78,  // t
    0x3E,  // U
    0x1C,  // v (simplified)
    0x2A,  // w (simplified)
    0x76,  // X (same as H)
    0x6E,  // y
    0x5B   // Z (same as 2)
};

// Display RAM buffer (32 segments x 4 commons = 16 bytes)
static uint8_t display_ram[16] = {0};

// Delay for bit-banging (microseconds)
#define HT1621_DELAY_US 1

static void ht1621_delay(void) {
    ets_delay_us(HT1621_DELAY_US);
}

static void ht1621_cs_high(void) {
    gpio_set_level(g_config.cs_pin, 1);
}

static void ht1621_cs_low(void) {
    gpio_set_level(g_config.cs_pin, 0);
}

static void ht1621_wr_high(void) {
    gpio_set_level(g_config.wr_pin, 1);
}

static void ht1621_wr_low(void) {
    gpio_set_level(g_config.wr_pin, 0);
}

static void ht1621_data_high(void) {
    gpio_set_level(g_config.data_pin, 1);
}

static void ht1621_data_low(void) {
    gpio_set_level(g_config.data_pin, 0);
}

void ht1621_write_bits(uint8_t data, uint8_t bits) {
    for (int i = bits - 1; i >= 0; i--) {
        ht1621_wr_low();
        if (data & (1 << i)) {
            ht1621_data_high();
        } else {
            ht1621_data_low();
        }
        ht1621_delay();
        ht1621_wr_high();
        ht1621_delay();
    }
}

void ht1621_send_cmd(uint8_t cmd) {
    ht1621_cs_low();
    ht1621_write_bits(0x04, 3);  // Command mode: 100
    ht1621_write_bits(cmd, 9);    // 9-bit command
    ht1621_cs_high();
}

void ht1621_send_addr(uint8_t addr) {
    ht1621_write_bits(addr, 6);   // 6-bit address
}

void ht1621_init(const ht1621_config_t *config) {
    // Store config
    g_config = *config;
    
    // Configure GPIO pins
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << config->cs_pin) | 
                       (1ULL << config->wr_pin) | 
                       (1ULL << config->data_pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    
    // Set initial states
    ht1621_cs_high();
    ht1621_wr_high();
    ht1621_data_high();
    
    // Wait for power stabilization
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Initialize HT1621
    ht1621_send_cmd(HT1621_CMD_SYS_EN);     // Enable system
    ht1621_send_cmd(HT1621_CMD_RC_256K);    // Use internal RC oscillator
    ht1621_send_cmd(HT1621_CMD_BIAS_1_2);   // 1/2 bias, 4 commons
    ht1621_send_cmd(HT1621_CMD_LCD_ON);     // Turn on LCD
    
    // Clear display
    ht1621_clear();
    
    ESP_LOGI(TAG, "HT1621 initialized");
}

void ht1621_send_command(uint8_t command) {
    ht1621_send_cmd(command);
}

void ht1621_write_data(uint8_t address, uint8_t data) {
    ht1621_cs_low();
    ht1621_write_bits(0x05, 3);      // Write mode: 101
    ht1621_send_addr(address);       // 6-bit address
    ht1621_write_bits(data, 4);      // 4-bit data (nibble)
    ht1621_cs_high();
    
    // Update local buffer
    if (address < 32) {
        display_ram[address / 2] = (address & 1) ? 
            (display_ram[address / 2] & 0x0F) | (data << 4) :
            (display_ram[address / 2] & 0xF0) | (data & 0x0F);
    }
}

void ht1621_write_all(const uint8_t *data, uint8_t length) {
    ht1621_cs_low();
    ht1621_write_bits(0x05, 3);      // Write mode: 101
    ht1621_send_addr(0);             // Start at address 0
    
    // Write consecutive data
    for (int i = 0; i < length && i < 32; i++) {
        ht1621_write_bits(data[i], 4);
    }
    
    ht1621_cs_high();
    
    // Update local buffer
    memcpy(display_ram, data, length > 16 ? 16 : length);
}

void ht1621_clear(void) {
    uint8_t clear_data[32] = {0};
    ht1621_write_all(clear_data, 32);
    memset(display_ram, 0, sizeof(display_ram));
}

void ht1621_display_number(float number, uint8_t decimal_places) {
    // Limit display range
    if (number > 99.9) number = 99.9;
    if (number < -9.9) number = -9.9;
    
    // Convert to integer for processing
    int display_value = (int)(number * 10);
    bool negative = display_value < 0;
    if (negative) display_value = -display_value;
    
    // Extract digits
    uint8_t digits[3];
    digits[2] = display_value % 10;        // Decimal
    digits[1] = (display_value / 10) % 10; // Units
    digits[0] = (display_value / 100) % 10; // Tens
    
    // Clear display first
    uint8_t segment_data[32] = {0};
    
    // Write digits to segment data
    if (negative && digits[0] == 0) {
        // Display minus sign in tens position
        segment_data[0] = SEG_G;  // Minus sign
    } else {
        segment_data[0] = DIGIT_PATTERNS[digits[0]];
    }
    
    segment_data[1] = DIGIT_PATTERNS[digits[1]];
    if (decimal_places > 0) {
        segment_data[1] |= SEG_DP;  // Add decimal point
    }
    segment_data[2] = DIGIT_PATTERNS[digits[2]];
    
    // Update display
    ht1621_write_all(segment_data, 32);
}

void ht1621_display_text(const char *text) {
    uint8_t segment_data[32] = {0};
    int pos = 0;
    
    for (int i = 0; text[i] && pos < 4; i++) {
        char c = text[i];
        uint8_t pattern = 0;
        
        if (c >= '0' && c <= '9') {
            pattern = DIGIT_PATTERNS[c - '0'];
        } else if (c >= 'A' && c <= 'Z') {
            pattern = CHAR_PATTERNS[c - 'A'];
        } else if (c >= 'a' && c <= 'z') {
            pattern = CHAR_PATTERNS[c - 'a'];
        } else if (c == '-') {
            pattern = SEG_G;
        } else if (c == '_') {
            pattern = SEG_D;
        } else if (c == ' ') {
            pattern = 0x00;
        }
        
        if (c != '.') {
            segment_data[pos++] = pattern;
        } else if (pos > 0) {
            segment_data[pos-1] |= SEG_DP;
        }
    }
    
    ht1621_write_all(segment_data, 32);
}

void ht1621_set_icon(uint8_t icon, bool state) {
    // Icons are typically in segments 24-31
    uint8_t icon_addr = 24 + __builtin_ctz(icon);  // Find bit position
    
    if (icon_addr < 32) {
        uint8_t current = display_ram[icon_addr / 2];
        if (icon_addr & 1) {
            current = state ? (current | 0xF0) : (current & 0x0F);
        } else {
            current = state ? (current | 0x0F) : (current & 0xF0);
        }
        ht1621_write_data(icon_addr, state ? 0xF : 0x0);
    }
}

void ht1621_set_all_icons(uint8_t icons) {
    for (int i = 0; i < 8; i++) {
        ht1621_set_icon(1 << i, icons & (1 << i));
    }
}

void ht1621_update_display(const ht1621_display_t *display) {
    uint8_t segment_data[32] = {0};
    
    // Copy digit data
    for (int i = 0; i < 4; i++) {
        segment_data[i] = display->digit[i];
    }
    
    // Add decimal point if needed
    if (display->decimal_point > 0 && display->decimal_point <= 4) {
        segment_data[display->decimal_point - 1] |= SEG_DP;
    }
    
    // Set icons
    ht1621_set_all_icons(display->icons);
    
    // Update main display
    ht1621_write_all(segment_data, 32);
}

void ht1621_test_pattern(void) {
    ESP_LOGI(TAG, "Running display test pattern");
    
    // Test all segments on
    uint8_t all_on[32];
    memset(all_on, 0xFF, sizeof(all_on));
    ht1621_write_all(all_on, 32);
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Test digits 0-9
    for (int i = 0; i < 10; i++) {
        uint8_t test_data[32] = {0};
        test_data[0] = DIGIT_PATTERNS[i];
        test_data[1] = DIGIT_PATTERNS[i];
        test_data[2] = DIGIT_PATTERNS[i];
        test_data[3] = DIGIT_PATTERNS[i];
        ht1621_write_all(test_data, 32);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    
    // Test icons one by one
    for (int i = 0; i < 8; i++) {
        ht1621_clear();
        ht1621_set_icon(1 << i, true);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    
    // Clear display
    ht1621_clear();
    ESP_LOGI(TAG, "Display test complete");
}