#include <stdio.h>

#include "connect.h"
#include "app_time.h"
#include "link_store.h"
#include "google_auth.h"
#include "watcher.h"
#include "app_console.h"
#include "secrets.h"
#include "nvs_flash.h"
#include "esp_log.h"

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    /* Sem isso, o log do Wi-Fi/TLS invade o prompt interativo. */
    esp_log_level_set("wifi", ESP_LOG_WARN);
    esp_log_level_set("esp-tls", ESP_LOG_WARN);

    wifi_init();
    ESP_ERROR_CHECK(wifi_connect_sta(WIFI_SSID, WIFI_PASSWORD, 20000));

    app_time_sync();
    ESP_ERROR_CHECK(store_init());
    ESP_ERROR_CHECK(google_auth_init());
    ESP_ERROR_CHECK(watcher_init());
    ESP_ERROR_CHECK(app_console_init());
}
