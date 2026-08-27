#include <string.h>

#include "toogleLed.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "cJSON.h"

static const char *TAG = "TOGGLE_LED";

#define LED_GPIO 2 /* LED onboard desta placa (ver ../blink/README.md) */

void toogle_led_init(void)
{
    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_GPIO, 0);
}

esp_err_t on_toggle_led(httpd_req_t *req)
{
    char buf[128];
    size_t recv_size = req->content_len < sizeof(buf) - 1
                            ? req->content_len
                            : sizeof(buf) - 1;

    int len = httpd_req_recv(req, buf, recv_size);
    if (len <= 0)
    {
        if (len == HTTPD_SOCK_ERR_TIMEOUT)
        {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }
    buf[len] = '\0';

    cJSON *json = cJSON_Parse(buf);
    if (json == NULL)
    {
        ESP_LOGE(TAG, "JSON invalido: %s", buf);
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "JSON invalido");
        return ESP_OK;
    }

    cJSON *is_on = cJSON_GetObjectItem(json, "is_on");
    if (!cJSON_IsBool(is_on))
    {
        cJSON_Delete(json);
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "Campo \"is_on\" (bool) obrigatorio");
        return ESP_OK;
    }

    bool led_on = cJSON_IsTrue(is_on);
    gpio_set_level(LED_GPIO, led_on ? 1 : 0);
    ESP_LOGI(TAG, "LED %s via HTTP", led_on ? "ligado" : "desligado");

    cJSON_Delete(json);

    char resp[64];
    snprintf(resp, sizeof(resp), "{\"is_on\":%s}", led_on ? "true" : "false");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, resp);
    return ESP_OK;
}
