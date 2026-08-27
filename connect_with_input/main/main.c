#include <stdio.h>

#include "connect.h"
#include "toogleLed.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_http_server.h"

static void init_server(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    ESP_ERROR_CHECK(httpd_start(&server, &config));

    httpd_uri_t toggle_led_url = {
        .uri = "/",
        .method = HTTP_POST,
        .handler = on_toggle_led
    };
    httpd_register_uri_handler(server, &toggle_led_url);
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    wifi_init();
    ESP_ERROR_CHECK(wifi_connect_sta("labSHP", "labSHP2025", 10000));

    toogle_led_init();
    init_server();
}
