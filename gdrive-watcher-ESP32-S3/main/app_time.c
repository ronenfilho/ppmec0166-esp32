#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include "app_time.h"
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "esp_netif_sntp.h"

static const char *TAG = "TIME";

esp_err_t app_time_sync(void)
{
    /* Fuso horario de Brasilia (UTC-3, sem horario de verao atualmente) */
    setenv("TZ", "<-03>3", 1);
    tzset();

    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    esp_netif_sntp_init(&config);

    ESP_LOGI(TAG, "Sincronizando horario via NTP...");
    esp_err_t err = esp_netif_sntp_sync_wait(pdMS_TO_TICKS(10000));
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "Nao foi possivel sincronizar via NTP (timeout)");
        return err;
    }

    char buf[32];
    app_time_format(app_time_now(), buf, sizeof(buf));
    ESP_LOGI(TAG, "Horario sincronizado: %s", buf);
    return ESP_OK;
}

void app_time_format(time_t t, char *buf, size_t len)
{
    struct tm timeinfo;
    localtime_r(&t, &timeinfo);
    strftime(buf, len, "%d/%m/%Y %H:%M:%S", &timeinfo);
}

time_t app_time_now(void)
{
    return time(NULL);
}
