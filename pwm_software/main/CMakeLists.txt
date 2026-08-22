idf_component_register(SRCS "pwm_software_main.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES driver esp_timer)
