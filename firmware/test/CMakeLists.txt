idf_component_register(
    SRCS "test_basic.c"
    INCLUDE_DIRS "."
    REQUIRES driver nvs_flash esp_system
)