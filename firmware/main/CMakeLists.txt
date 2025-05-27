idf_component_register(
    SRCS 
        "main.c"
        "thermor_zigbee.c"
    INCLUDE_DIRS "."
    REQUIRES 
        driver 
        nvs_flash 
        esp_system
        esp_timer
        freertos
        esp_zigbee_lib
)