#include <stdio.h>

#include "connect.h"
#include "toogleLed.h"
#include "clock.h"
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
    /* Edite com o SSID/senha da sua rede antes de compilar (ver README). */
    ESP_ERROR_CHECK(wifi_connect_sta("SEU_SSID_AQUI", "SUA_SENHA_AQUI", 20000));

    clock_init();
    toogle_led_init();
    init_server();
}
