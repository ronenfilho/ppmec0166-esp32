#include <stdio.h>

#include "connect.h"
#include "clock.h"
#include "secrets.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_http_server.h"

static void init_server(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    ESP_ERROR_CHECK(httpd_start(&server, &config));

    httpd_uri_t clock_url = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = on_get_clock
    };
    httpd_register_uri_handler(server, &clock_url);
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    wifi_init();
    ESP_ERROR_CHECK(wifi_connect_sta(WIFI_SSID, WIFI_PASSWORD, 20000));

    clock_init();
    init_server();
}
