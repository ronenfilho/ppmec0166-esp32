#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "clock.h"
#include "toogleLed.h"
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "esp_netif_sntp.h"

static const char *TAG = "CLOCK";

static const char PAGE_TEMPLATE[] =
    "<!DOCTYPE html>"
    "<html><head>"
    "<meta charset=\"utf-8\">"
    "<meta http-equiv=\"refresh\" content=\"10\">"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
    "<title>ESP32</title>"
    "<style>"
    "body{font-family:-apple-system,Arial,sans-serif;background:#0f172a;"
    "color:#e2e8f0;display:flex;align-items:center;justify-content:center;"
    "min-height:100vh;margin:0}"
    ".card{background:#1e293b;padding:2rem 2.5rem;border-radius:12px;"
    "box-shadow:0 10px 30px rgba(0,0,0,.4);text-align:center;max-width:360px}"
    "h1{font-size:1.4rem;margin:0 0 .25rem}"
    ".time{font-size:1.8rem;font-weight:600;margin:.5rem 0 1.2rem;"
    "font-variant-numeric:tabular-nums}"
    ".status{display:inline-block;padding:.35rem 1rem;border-radius:999px;"
    "font-weight:600;margin-bottom:1.2rem}"
    ".on{background:#166534;color:#bbf7d0}"
    ".off{background:#374151;color:#d1d5db}"
    ".buttons{display:flex;gap:.75rem;justify-content:center}"
    "button{flex:1;padding:.7rem 0;border:none;border-radius:8px;"
    "font-size:1rem;font-weight:600;cursor:pointer}"
    ".btn-on{background:#22c55e;color:#052e16}"
    ".btn-off{background:#ef4444;color:#450a0a}"
    ".hint{margin-top:1rem;font-size:.75rem;color:#94a3b8}"
    "</style>"
    "</head><body>"
    "<div class=\"card\">"
    "<h1>ESP32 - Ronen</h1>"
    "<div class=\"time\">%s</div>"
    "<div class=\"status %s\">LED %s</div>"
    "<div class=\"buttons\">"
    "<button class=\"btn-on\" onclick=\"setLed(true)\">Ligar</button>"
    "<button class=\"btn-off\" onclick=\"setLed(false)\">Desligar</button>"
    "</div>"
    "<p class=\"hint\">Atualiza sozinha a cada 10s</p>"
    "</div>"
    "<script>"
    "function setLed(on){"
    "fetch('/',{method:'POST',headers:{'Content-Type':'application/json'},"
    "body:JSON.stringify({is_on:on})}).then(()=>location.reload());"
    "}"
    "</script>"
    "</body></html>";

void clock_init(void)
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
        return;
    }

    time_t now = time(NULL);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    char buf[32];
    strftime(buf, sizeof(buf), "%d/%m/%Y %H:%M:%S", &timeinfo);
    ESP_LOGI(TAG, "Horario sincronizado: %s", buf);
}

esp_err_t on_get_clock(httpd_req_t *req)
{
    time_t now = time(NULL);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    char time_str[32];
    strftime(time_str, sizeof(time_str), "%d/%m/%Y %H:%M:%S", &timeinfo);

    bool led_on = toogle_led_get_state();

    char html[2048];
    snprintf(html, sizeof(html), PAGE_TEMPLATE,
             time_str,
             led_on ? "on" : "off",
             led_on ? "Ligado" : "Desligado");

    httpd_resp_set_type(req, "text/html");
    httpd_resp_sendstr(req, html);
    return ESP_OK;
}
