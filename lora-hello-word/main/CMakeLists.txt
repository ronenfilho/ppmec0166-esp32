idf_component_register(SRCS "main.c" "sx1276.c"
                       INCLUDE_DIRS "."
                       REQUIRES esp_driver_spi esp_driver_gpio esp_timer esp_hw_support)
