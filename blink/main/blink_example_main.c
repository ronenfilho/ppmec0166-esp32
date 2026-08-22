/* Blink Example (versao simplificada)

   Pisca um LED comum ligado a um GPIO. O pino e o periodo sao
   configuraveis via "idf.py menuconfig" (Example Configuration).

   This example code is in the Public Domain (or CC0 licensed, at your option.)
*/
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "sdkconfig.h"

static const char *TAG = "example";

#define BLINK_GPIO 2

void app_main(void)
{
    ESP_LOGI(TAG, "Configurado para piscar LED no GPIO%d", BLINK_GPIO);

    gpio_reset_pin(BLINK_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);

    bool led_on = false;
    while (1) {
        led_on = !led_on;
        ESP_LOGI(TAG, "LED %s", led_on ? "ON" : "OFF");
        gpio_set_level(BLINK_GPIO, led_on);
        vTaskDelay(pdMS_TO_TICKS(CONFIG_BLINK_PERIOD));
    }
}
