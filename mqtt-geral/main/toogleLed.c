#include "toogleLed.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "TOGGLE_LED";

#define LED_GPIO 2 /* LED onboard desta placa (ver ../blink/README.md) */

static bool s_led_on = false;

void toogle_led_init(void)
{
    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_GPIO, 0);
    s_led_on = false;
}

void toogle_led_set(bool on)
{
    gpio_set_level(LED_GPIO, on ? 1 : 0);
    s_led_on = on;
    ESP_LOGI(TAG, "LED %s", on ? "ligado" : "desligado");
}

bool toogle_led_get_state(void)
{
    return s_led_on;
}
