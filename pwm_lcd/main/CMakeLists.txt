idf_component_register(SRCS "pwm_lcd_main.c" "lcd1602.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES driver esp_timer)
