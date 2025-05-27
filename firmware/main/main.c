/**
 * @file main.c
 * @brief ThermortoZigBee - Main application entry point
 * 
 * ESP32-C6 Zigbee thermostat controller for Thermor heaters
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_zigbee_core.h"
#include "thermor_zigbee.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    esp_err_t ret;

    // Initialize NVS
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "ThermortoZigBee starting...");
    ESP_LOGI(TAG, "Free heap: %ld bytes", esp_get_free_heap_size());
    
    // Initialize Thermor Zigbee application
    thermor_zigbee_init();
    
    ESP_LOGI(TAG, "Application initialized successfully");
}