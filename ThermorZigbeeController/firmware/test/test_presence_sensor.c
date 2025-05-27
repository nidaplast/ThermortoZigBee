/**
 * Test du capteur de présence PIR HC-SR501
 * Validation de la détection et de l'intégration avec le système
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"

#define GPIO_PIR_SENSOR     7
#define PIR_DEBOUNCE_MS     500
#define PIR_TIMEOUT_MS      300000  // 5 minutes

static const char *TAG = "PIR_TEST";

typedef struct {
    bool presence_detected;
    uint32_t last_detection_time;
    uint32_t detection_count;
    bool timeout_active;
} pir_state_t;

static pir_state_t pir_state = {0};
static volatile bool pir_interrupt_flag = false;

// ISR pour détection PIR
static void IRAM_ATTR pir_isr_handler(void* arg) {
    pir_interrupt_flag = true;
}

// Initialisation du capteur PIR
void init_pir_sensor(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << GPIO_PIR_SENSOR),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_POSEDGE  // Détection sur front montant
    };
    
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(gpio_isr_handler_add(GPIO_PIR_SENSOR, pir_isr_handler, NULL));
    
    ESP_LOGI(TAG, "PIR sensor initialized on GPIO %d", GPIO_PIR_SENSOR);
}

// Test de base du capteur
void test_pir_basic(void) {
    ESP_LOGI(TAG, "=== TEST 1: PIR Basic Detection ===");
    ESP_LOGI(TAG, "Move in front of the sensor to trigger detection...");
    
    int previous_state = -1;
    
    for(int i = 0; i < 100; i++) {  // Test pendant 10 secondes
        int current_state = gpio_get_level(GPIO_PIR_SENSOR);
        
        if(current_state != previous_state) {
            ESP_LOGI(TAG, "PIR State changed: %s", 
                     current_state ? "MOTION DETECTED" : "No motion");
            previous_state = current_state;
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// Test avec interruption et debounce
void test_pir_interrupt(void) {
    ESP_LOGI(TAG, "\n=== TEST 2: PIR Interrupt with Debounce ===");
    
    uint32_t last_trigger = 0;
    pir_state.detection_count = 0;
    
    for(int i = 0; i < 200; i++) {  // Test pendant 20 secondes
        if(pir_interrupt_flag) {
            uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
            
            // Debounce
            if(now - last_trigger > PIR_DEBOUNCE_MS) {
                pir_state.detection_count++;
                pir_state.presence_detected = true;
                pir_state.last_detection_time = now;
                
                ESP_LOGI(TAG, "Motion detected! Count: %d, Time: %d ms", 
                         pir_state.detection_count, now);
                
                last_trigger = now;
            }
            
            pir_interrupt_flag = false;
        }
        
        // LED indication
        gpio_set_level(8, pir_state.presence_detected);  // LED sur GPIO8
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    ESP_LOGI(TAG, "Total detections: %d", pir_state.detection_count);
}

// Test timeout présence
void test_pir_timeout(void) {
    ESP_LOGI(TAG, "\n=== TEST 3: PIR Presence Timeout ===");
    ESP_LOGI(TAG, "Testing 30-second timeout after last detection...");
    
    const uint32_t test_timeout = 30000;  // 30 secondes pour le test
    pir_state.presence_detected = false;
    pir_state.timeout_active = false;
    
    while(1) {
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        
        // Vérifier nouvelle détection
        if(gpio_get_level(GPIO_PIR_SENSOR)) {
            if(!pir_state.presence_detected) {
                ESP_LOGI(TAG, "Presence detected - Starting timeout timer");
            }
            pir_state.presence_detected = true;
            pir_state.last_detection_time = now;
            pir_state.timeout_active = true;
        }
        
        // Vérifier timeout
        if(pir_state.timeout_active && pir_state.presence_detected) {
            uint32_t elapsed = now - pir_state.last_detection_time;
            
            if(elapsed > test_timeout) {
                ESP_LOGI(TAG, "Presence timeout! No motion for %d seconds", 
                         test_timeout / 1000);
                pir_state.presence_detected = false;
                pir_state.timeout_active = false;
            } else {
                // Afficher compte à rebours
                if(elapsed % 5000 == 0) {  // Toutes les 5 secondes
                    ESP_LOGI(TAG, "Time until timeout: %d seconds", 
                             (test_timeout - elapsed) / 1000);
                }
            }
        }
        
        // LED indique l'état de présence
        gpio_set_level(8, pir_state.presence_detected);
        
        vTaskDelay(pdMS_TO_TICKS(100));
        
        // Sortir après 60 secondes
        if(now > 60000) break;
    }
}

// Test intégration avec contrôle température
void test_pir_temperature_integration(void) {
    ESP_LOGI(TAG, "\n=== TEST 4: PIR + Temperature Control ===");
    
    float comfort_temp = 21.0f;
    float eco_temp = 18.0f;
    float current_setpoint = eco_temp;
    
    ESP_LOGI(TAG, "Simulating temperature control based on presence");
    ESP_LOGI(TAG, "Comfort: %.1f°C, Eco: %.1f°C", comfort_temp, eco_temp);
    
    for(int i = 0; i < 100; i++) {
        bool motion = gpio_get_level(GPIO_PIR_SENSOR);
        
        if(motion && !pir_state.presence_detected) {
            // Présence détectée - passer en mode confort
            pir_state.presence_detected = true;
            current_setpoint = comfort_temp;
            ESP_LOGI(TAG, "PRESENCE ON - Switching to COMFORT mode (%.1f°C)", 
                     current_setpoint);
        } else if(!motion && pir_state.presence_detected) {
            // Absence détectée - passer en mode éco
            pir_state.presence_detected = false;
            current_setpoint = eco_temp;
            ESP_LOGI(TAG, "PRESENCE OFF - Switching to ECO mode (%.1f°C)", 
                     current_setpoint);
        }
        
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

// Test de sensibilité et portée
void test_pir_sensitivity(void) {
    ESP_LOGI(TAG, "\n=== TEST 5: PIR Sensitivity Analysis ===");
    ESP_LOGI(TAG, "Recording detection pattern for 30 seconds...");
    
    uint32_t detection_times[100] = {0};
    int detection_index = 0;
    uint32_t test_start = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    while(1) {
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        
        if(gpio_get_level(GPIO_PIR_SENSOR) && detection_index < 100) {
            detection_times[detection_index++] = now - test_start;
            ESP_LOGI(TAG, "Detection #%d at %d ms", 
                     detection_index, now - test_start);
            
            // Attendre fin du signal
            while(gpio_get_level(GPIO_PIR_SENSOR)) {
                vTaskDelay(pdMS_TO_TICKS(10));
            }
        }
        
        // Test pendant 30 secondes
        if(now - test_start > 30000) break;
        
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    
    // Analyser les résultats
    ESP_LOGI(TAG, "\n--- Analysis Results ---");
    ESP_LOGI(TAG, "Total detections: %d", detection_index);
    
    if(detection_index > 1) {
        uint32_t min_interval = UINT32_MAX;
        uint32_t max_interval = 0;
        uint32_t total_interval = 0;
        
        for(int i = 1; i < detection_index; i++) {
            uint32_t interval = detection_times[i] - detection_times[i-1];
            if(interval < min_interval) min_interval = interval;
            if(interval > max_interval) max_interval = interval;
            total_interval += interval;
        }
        
        ESP_LOGI(TAG, "Min interval: %d ms", min_interval);
        ESP_LOGI(TAG, "Max interval: %d ms", max_interval);
        ESP_LOGI(TAG, "Avg interval: %d ms", total_interval / (detection_index - 1));
    }
}

// Rapport de test
void print_pir_test_report(void) {
    ESP_LOGI(TAG, "\n");
    ESP_LOGI(TAG, "╔════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║      PIR SENSOR TEST REPORT           ║");
    ESP_LOGI(TAG, "╠════════════════════════════════════════╣");
    ESP_LOGI(TAG, "║ Total Detections : %-18d ║", pir_state.detection_count);
    ESP_LOGI(TAG, "║ Current State    : %-18s ║", 
             pir_state.presence_detected ? "PRESENCE" : "NO PRESENCE");
    ESP_LOGI(TAG, "║ GPIO Pin         : %-18d ║", GPIO_PIR_SENSOR);
    ESP_LOGI(TAG, "║ Debounce Time    : %-15d ms ║", PIR_DEBOUNCE_MS);
    ESP_LOGI(TAG, "╚════════════════════════════════════════╝");
}

void app_main(void) {
    ESP_LOGI(TAG, "╔════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║    PIR HC-SR501 SENSOR TEST SUITE     ║");
    ESP_LOGI(TAG, "╚════════════════════════════════════════╝\n");
    
    // Configuration GPIO LED pour indication visuelle
    gpio_config_t led_conf = {
        .pin_bit_mask = (1ULL << 8),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };
    gpio_config(&led_conf);
    
    // Initialiser le capteur PIR
    init_pir_sensor();
    
    // Attendre stabilisation du capteur (important pour HC-SR501)
    ESP_LOGI(TAG, "Waiting for PIR sensor stabilization (10s)...");
    for(int i = 10; i > 0; i--) {
        ESP_LOGI(TAG, "%d...", i);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    // Exécuter les tests
    test_pir_basic();
    test_pir_interrupt();
    test_pir_timeout();
    test_pir_temperature_integration();
    test_pir_sensitivity();
    
    // Rapport final
    print_pir_test_report();
    
    ESP_LOGI(TAG, "\nAll PIR tests completed!");
    
    // Boucle principale - indication présence
    while(1) {
        bool presence = gpio_get_level(GPIO_PIR_SENSOR);
        gpio_set_level(8, presence);
        
        if(presence) {
            ESP_LOGD(TAG, "Motion active");
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}