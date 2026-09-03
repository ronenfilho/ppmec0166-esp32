#include <stdio.h>

#include "connect.h"
#include "toogleLed.h"
#include "mqtt_main.h"
#include "secrets.h"
#include "nvs_flash.h"
#include "esp_log.h"

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    wifi_init();
    ESP_ERROR_CHECK(wifi_connect_sta(WIFI_SSID, WIFI_PASSWORD, 20000));

    toogle_led_init();
    mqtt_app_start();
}
