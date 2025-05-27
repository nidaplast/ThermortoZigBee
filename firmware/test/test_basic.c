/**
 * Programme de Test Initial ESP32-C6
 * Test progressif et sécurisé avant connexion au radiateur
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "driver/uart.h"

// Configuration des GPIO pour les tests
#define GPIO_TEST_LED       8    // LED de statut
#define GPIO_TEST_TRIAC1    4    // Sortie triac 1 
#define GPIO_TEST_TRIAC2    5    // Sortie triac 2
#define GPIO_TEST_ZERO      6    // Entrée zero cross
#define GPIO_TEST_PRESENCE  7    // Entrée présence
#define ADC_TEST_TEMP       ADC1_CHANNEL_0  // Canal ADC température

// Configuration UART pour debug
#define UART_NUM            UART_NUM_0
#define UART_TX_PIN         21
#define UART_RX_PIN         20
#define UART_BAUD_RATE      115200

static const char *TAG = "TEST_BASIC";

// Structure pour stocker les résultats des tests
typedef struct {
    bool gpio_ok;
    bool adc_ok;
    bool zero_cross_ok;
    bool memory_ok;
    float test_voltage;
    uint32_t test_duration_ms;
} test_results_t;

static test_results_t results = {0};

// Test 1: Vérification des GPIO en sortie
void test_gpio_outputs(void) {
    ESP_LOGI(TAG, "=== TEST 1: GPIO Outputs ===");
    
    // Configuration des GPIO en sortie
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << GPIO_TEST_LED) | 
                       (1ULL << GPIO_TEST_TRIAC1) | 
                       (1ULL << GPIO_TEST_TRIAC2),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    
    // Test pattern sur les sorties
    ESP_LOGI(TAG, "Testing output pattern...");
    for(int i = 0; i < 3; i++) {
        // LED ON
        gpio_set_level(GPIO_TEST_LED, 1);
        ESP_LOGI(TAG, "LED: ON");
        vTaskDelay(pdMS_TO_TICKS(500));
        
        // TRIAC1 pulse (safe test)
        gpio_set_level(GPIO_TEST_TRIAC1, 1);
        ESP_LOGI(TAG, "TRIAC1: PULSE");
        vTaskDelay(pdMS_TO_TICKS(10)); // 10ms pulse
        gpio_set_level(GPIO_TEST_TRIAC1, 0);
        
        vTaskDelay(pdMS_TO_TICKS(200));
        
        // TRIAC2 pulse
        gpio_set_level(GPIO_TEST_TRIAC2, 1);
        ESP_LOGI(TAG, "TRIAC2: PULSE");
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(GPIO_TEST_TRIAC2, 0);
        
        // LED OFF
        gpio_set_level(GPIO_TEST_LED, 0);
        ESP_LOGI(TAG, "LED: OFF");
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    
    results.gpio_ok = true;
    ESP_LOGI(TAG, "✓ GPIO outputs test PASSED");
}

// Test 2: Vérification des entrées
void test_gpio_inputs(void) {
    ESP_LOGI(TAG, "\n=== TEST 2: GPIO Inputs ===");
    
    // Configuration des GPIO en entrée
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << GPIO_TEST_ZERO) | 
                       (1ULL << GPIO_TEST_PRESENCE),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    
    // Lecture des entrées
    ESP_LOGI(TAG, "Reading inputs for 5 seconds...");
    ESP_LOGI(TAG, "Toggle ZERO_CROSS and PRESENCE inputs to test");
    
    for(int i = 0; i < 50; i++) {
        int zero_state = gpio_get_level(GPIO_TEST_ZERO);
        int presence_state = gpio_get_level(GPIO_TEST_PRESENCE);
        
        ESP_LOGI(TAG, "ZERO_CROSS: %d | PRESENCE: %d", 
                 zero_state, presence_state);
        
        // LED indique l'état des entrées
        gpio_set_level(GPIO_TEST_LED, zero_state || presence_state);
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    ESP_LOGI(TAG, "✓ GPIO inputs test completed");
}

// Test 3: ADC pour capteur température
void test_adc_temperature(void) {
    ESP_LOGI(TAG, "\n=== TEST 3: ADC Temperature ===");
    
    // Configuration ADC
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC_TEST_TEMP, ADC_ATTEN_DB_11);
    
    ESP_LOGI(TAG, "Reading ADC for 10 samples...");
    
    uint32_t adc_sum = 0;
    for(int i = 0; i < 10; i++) {
        uint32_t adc_reading = adc1_get_raw(ADC_TEST_TEMP);
        float voltage = (adc_reading / 4095.0f) * 3.3f;
        
        ESP_LOGI(TAG, "Sample %d: ADC=%d, Voltage=%.3fV", 
                 i+1, adc_reading, voltage);
        
        adc_sum += adc_reading;
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    
    uint32_t adc_avg = adc_sum / 10;
    results.test_voltage = (adc_avg / 4095.0f) * 3.3f;
    
    // Simulation calcul température avec NTC 10k
    float R_REF = 10000.0f;
    float voltage = results.test_voltage;
    float resistance = R_REF * voltage / (3.3f - voltage);
    
    ESP_LOGI(TAG, "Average: ADC=%d, Voltage=%.3fV, R=%.0fΩ", 
             adc_avg, results.test_voltage, resistance);
    
    results.adc_ok = (adc_avg > 100 && adc_avg < 4000);
    ESP_LOGI(TAG, "%s ADC test %s", 
             results.adc_ok ? "✓" : "✗",
             results.adc_ok ? "PASSED" : "FAILED");
}

// Test 4: Interruption Zero Cross (simulation)
static volatile uint32_t zero_cross_count = 0;
static void IRAM_ATTR zero_cross_isr(void* arg) {
    zero_cross_count++;
}

void test_zero_cross_interrupt(void) {
    ESP_LOGI(TAG, "\n=== TEST 4: Zero Cross Interrupt ===");
    
    // Configuration interruption
    gpio_set_intr_type(GPIO_TEST_ZERO, GPIO_INTR_POSEDGE);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(GPIO_TEST_ZERO, zero_cross_isr, NULL);
    
    ESP_LOGI(TAG, "Monitoring zero cross for 5 seconds...");
    ESP_LOGI(TAG, "Expected: ~250 interrupts for 50Hz");
    
    zero_cross_count = 0;
    vTaskDelay(pdMS_TO_TICKS(5000));
    
    uint32_t count = zero_cross_count;
    float frequency = count / 5.0f;
    
    ESP_LOGI(TAG, "Detected %d zero crossings = %.1f Hz", 
             count, frequency);
    
    // Test OK si fréquence entre 45-55 Hz
    results.zero_cross_ok = (frequency > 45.0f && frequency < 55.0f);
    
    ESP_LOGI(TAG, "%s Zero cross test %s", 
             results.zero_cross_ok ? "✓" : "✗",
             results.zero_cross_ok ? "PASSED" : "FAILED");
    
    gpio_isr_handler_remove(GPIO_TEST_ZERO);
}

// Test 5: Mémoire NVS
void test_nvs_storage(void) {
    ESP_LOGI(TAG, "\n=== TEST 5: NVS Storage ===");
    
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || 
        err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    
    // Test écriture/lecture
    nvs_handle_t handle;
    err = nvs_open("test", NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        uint32_t test_value = 0x12345678;
        err = nvs_set_u32(handle, "test_val", test_value);
        
        if (err == ESP_OK) {
            err = nvs_commit(handle);
            
            uint32_t read_value = 0;
            err = nvs_get_u32(handle, "test_val", &read_value);
            
            results.memory_ok = (read_value == test_value);
            ESP_LOGI(TAG, "Write: 0x%08X, Read: 0x%08X", 
                     test_value, read_value);
        }
        
        nvs_close(handle);
    }
    
    ESP_LOGI(TAG, "%s NVS test %s", 
             results.memory_ok ? "✓" : "✗",
             results.memory_ok ? "PASSED" : "FAILED");
}

// Mode interactif pour tests manuels
void interactive_test_mode(void) {
    ESP_LOGI(TAG, "\n=== INTERACTIVE TEST MODE ===");
    ESP_LOGI(TAG, "Commands:");
    ESP_LOGI(TAG, "  1 - Toggle LED");
    ESP_LOGI(TAG, "  2 - Pulse TRIAC1");
    ESP_LOGI(TAG, "  3 - Pulse TRIAC2");
    ESP_LOGI(TAG, "  4 - Read inputs");
    ESP_LOGI(TAG, "  5 - Read ADC");
    ESP_LOGI(TAG, "  q - Quit");
    
    char cmd;
    while(1) {
        if(uart_read_bytes(UART_NUM, &cmd, 1, pdMS_TO_TICKS(100)) > 0) {
            switch(cmd) {
                case '1':
                    gpio_set_level(GPIO_TEST_LED, !gpio_get_level(GPIO_TEST_LED));
                    ESP_LOGI(TAG, "LED toggled");
                    break;
                    
                case '2':
                    gpio_set_level(GPIO_TEST_TRIAC1, 1);
                    vTaskDelay(pdMS_TO_TICKS(10));
                    gpio_set_level(GPIO_TEST_TRIAC1, 0);
                    ESP_LOGI(TAG, "TRIAC1 pulsed");
                    break;
                    
                case '3':
                    gpio_set_level(GPIO_TEST_TRIAC2, 1);
                    vTaskDelay(pdMS_TO_TICKS(10));
                    gpio_set_level(GPIO_TEST_TRIAC2, 0);
                    ESP_LOGI(TAG, "TRIAC2 pulsed");
                    break;
                    
                case '4':
                    ESP_LOGI(TAG, "ZERO: %d, PRESENCE: %d",
                             gpio_get_level(GPIO_TEST_ZERO),
                             gpio_get_level(GPIO_TEST_PRESENCE));
                    break;
                    
                case '5':
                    {
                        uint32_t adc = adc1_get_raw(ADC_TEST_TEMP);
                        float v = (adc / 4095.0f) * 3.3f;
                        ESP_LOGI(TAG, "ADC: %d (%.3fV)", adc, v);
                    }
                    break;
                    
                case 'q':
                case 'Q':
                    ESP_LOGI(TAG, "Exiting interactive mode");
                    return;
            }
        }
        
        // Clignotement LED pour indiquer mode actif
        static int blink = 0;
        if(++blink > 10) {
            gpio_set_level(GPIO_TEST_LED, !gpio_get_level(GPIO_TEST_LED));
            blink = 0;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// Rapport final des tests
void print_test_report(void) {
    ESP_LOGI(TAG, "\n");
    ESP_LOGI(TAG, "╔════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║         TEST REPORT SUMMARY            ║");
    ESP_LOGI(TAG, "╠════════════════════════════════════════╣");
    ESP_LOGI(TAG, "║ GPIO Outputs    : %s                  ║", 
             results.gpio_ok ? "PASS ✓" : "FAIL ✗");
    ESP_LOGI(TAG, "║ ADC Temperature : %s                  ║", 
             results.adc_ok ? "PASS ✓" : "FAIL ✗");
    ESP_LOGI(TAG, "║ Zero Cross Int. : %s                  ║", 
             results.zero_cross_ok ? "PASS ✓" : "FAIL ✗");
    ESP_LOGI(TAG, "║ NVS Memory      : %s                  ║", 
             results.memory_ok ? "PASS ✓" : "FAIL ✗");
    ESP_LOGI(TAG, "╠════════════════════════════════════════╣");
    ESP_LOGI(TAG, "║ Test Voltage    : %.3f V              ║", 
             results.test_voltage);
    ESP_LOGI(TAG, "║ Test Duration   : %lu ms             ║", 
             results.test_duration_ms);
    ESP_LOGI(TAG, "╚════════════════════════════════════════╝");
    
    // LED indication finale
    if(results.gpio_ok && results.adc_ok && results.memory_ok) {
        // Succès : clignotement rapide vert
        for(int i = 0; i < 10; i++) {
            gpio_set_level(GPIO_TEST_LED, i % 2);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    } else {
        // Échec : clignotement lent
        for(int i = 0; i < 6; i++) {
            gpio_set_level(GPIO_TEST_LED, i % 2);
            vTaskDelay(pdMS_TO_TICKS(500));
        }
    }
}

void app_main(void) {
    // Initialisation UART
    uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    uart_param_config(UART_NUM, &uart_config);
    uart_driver_install(UART_NUM, 256, 0, 0, NULL, 0);
    
    ESP_LOGI(TAG, "╔════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║   ESP32-C6 THERMOR TEST PROGRAM V1.0   ║");
    ESP_LOGI(TAG, "║      Safe testing before connection    ║");
    ESP_LOGI(TAG, "╚════════════════════════════════════════╝");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Starting tests in 3 seconds...");
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    uint32_t start_time = xTaskGetTickCount();
    
    // Exécution des tests
    test_gpio_outputs();
    test_gpio_inputs();
    test_adc_temperature();
    test_zero_cross_interrupt();
    test_nvs_storage();
    
    results.test_duration_ms = (xTaskGetTickCount() - start_time) * portTICK_PERIOD_MS;
    
    // Rapport final
    print_test_report();
    
    // Mode interactif
    ESP_LOGI(TAG, "\nPress 'i' for interactive mode or wait...");
    char cmd;
    if(uart_read_bytes(UART_NUM, &cmd, 1, pdMS_TO_TICKS(5000)) > 0) {
        if(cmd == 'i' || cmd == 'I') {
            interactive_test_mode();
        }
    }
    
    ESP_LOGI(TAG, "Test program completed!");
    
    // Boucle principale - clignotement status
    while(1) {
        gpio_set_level(GPIO_TEST_LED, 1);
        vTaskDelay(pdMS_TO_TICKS(1000));
        gpio_set_level(GPIO_TEST_LED, 0);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}