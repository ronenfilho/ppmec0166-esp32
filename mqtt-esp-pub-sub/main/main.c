#include <stdio.h>

#include "connect.h"
#include "mqtt_test.h"
#include "secrets.h"
#include "nvs_flash.h"
#include "esp_log.h"

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    wifi_init();
    ESP_ERROR_CHECK(wifi_connect_sta(WIFI_SSID, WIFI_PASSWORD, 20000));

    mqtt_test_start();
}
