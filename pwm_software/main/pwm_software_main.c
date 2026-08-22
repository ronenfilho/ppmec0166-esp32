/* Trabalho 01 - PWM em Software

   Gera PWM "em software" (sem usar o periferico LEDC/hardware de PWM do
   ESP32) para acionar um LED. O duty-cycle e recebido via terminal serial:
   digite um numero de 0 a 100 e pressione Enter.

   Temporizacao: a modulacao acontece via esp_timer (interrupcao de alta
   resolucao), reagendado a cada transicao ON/OFF conforme o duty-cycle
   atual. O periodo fixo (1kHz) fica bem acima do limiar de cintilacao
   perceptivel ao olho humano (~60-100 Hz).
*/
#include <stdio.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"

static const char *TAG = "pwm_sw";

#define PWM_GPIO        2      /* LED onboard desta placa */
#define PWM_PERIOD_US   1000   /* 1000us = 1kHz */

static volatile int s_duty_percent = 50; /* 0-100, atualizado pelo terminal serial */
static esp_timer_handle_t s_pwm_timer;
static bool s_led_on = false;

/* Disparado pelo esp_timer a cada transicao. Le o duty-cycle atual, alterna
   o LED e agenda a proxima transicao de acordo com o tempo ligado/desligado
   correspondente. */
static void pwm_timer_callback(void *arg)
{
    int duty = s_duty_percent;

    if (duty <= 0) {
        gpio_set_level(PWM_GPIO, 0);
        s_led_on = false;
        esp_timer_start_once(s_pwm_timer, PWM_PERIOD_US);
        return;
    }
    if (duty >= 100) {
        gpio_set_level(PWM_GPIO, 1);
        s_led_on = true;
        esp_timer_start_once(s_pwm_timer, PWM_PERIOD_US);
        return;
    }

    if (s_led_on) {
        gpio_set_level(PWM_GPIO, 0);
        s_led_on = false;
        esp_timer_start_once(s_pwm_timer, PWM_PERIOD_US * (100 - duty) / 100);
    } else {
        gpio_set_level(PWM_GPIO, 1);
        s_led_on = true;
        esp_timer_start_once(s_pwm_timer, PWM_PERIOD_US * duty / 100);
    }
}

/* Le uma linha do terminal serial (bloqueante), com eco de cada caractere
   digitado, e atualiza o duty-cycle quando Enter e pressionado. */
static void serial_input_task(void *arg)
{
    char line[16];
    size_t len = 0;

    printf("\nDigite o duty-cycle (0-100) e pressione Enter.\n");
    printf("Duty atual: %d%%\n", s_duty_percent);

    while (1) {
        int c = getchar();
        if (c == EOF) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        if (c == '\r' || c == '\n') {
            if (len == 0) {
                continue;
            }
            line[len] = '\0';
            len = 0;

            int value;
            if (sscanf(line, "%d", &value) == 1) {
                if (value < 0) value = 0;
                if (value > 100) value = 100;
                s_duty_percent = value;
                printf("\nDuty-cycle ajustado para %d%%\n", value);
                ESP_LOGI(TAG, "Novo duty-cycle: %d%%", value);
            } else {
                printf("\nEntrada invalida. Digite um numero de 0 a 100.\n");
            }
        } else if (len < sizeof(line) - 1) {
            line[len++] = (char)c;
            putchar(c);
            fflush(stdout);
        }
    }
}

void app_main(void)
{
    gpio_reset_pin(PWM_GPIO);
    gpio_set_direction(PWM_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(PWM_GPIO, 0);

    const esp_timer_create_args_t timer_args = {
        .callback = &pwm_timer_callback,
        .name = "pwm_sw_timer",
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &s_pwm_timer));
    ESP_ERROR_CHECK(esp_timer_start_once(s_pwm_timer, PWM_PERIOD_US));

    xTaskCreate(serial_input_task, "serial_input", 4096, NULL, 5, NULL);
}
