/* PWM em software + LED + display do duty-cycle num LCD 16x2 (I2C).

   Mesma logica de PWM do projeto pwm_software (esp_timer, sem LEDC), com
   a adicao de um LCD 1602 via I2C mostrando o valor do duty-cycle em
   texto e uma barra de nivel feita de caracteres — um LCD de texto nao
   desenha a forma de onda real, entao essa barra e a aproximacao visual
   possivel.
*/
#include <stdio.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_timer.h"
#include "esp_log.h"

#include "lcd1602.h"

static const char *TAG = "pwm_lcd";

#define PWM_GPIO        2      /* LED onboard desta placa */
#define PWM_PERIOD_US   1000   /* 1000us = 1kHz */

#define LCD_SDA_GPIO    21
#define LCD_SCL_GPIO    22
#define LCD_I2C_ADDR    0x27   /* troque para 0x3F se o LCD nao responder */

static volatile int s_duty_percent = 50; /* 0-100, atualizado pelo terminal serial */
static esp_timer_handle_t s_pwm_timer;
static bool s_led_on = false;
static lcd1602_t s_lcd;

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

/* Mostra o duty-cycle em texto na linha 1 e uma barra de 16 caracteres
   (proporcional ao duty-cycle) na linha 2. */
static void lcd_update_duty(int duty)
{
    char line1[17];
    snprintf(line1, sizeof(line1), "Duty-cycle: %3d%%", duty);

    char line2[17];
    int filled = duty * 16 / 100;
    for (int i = 0; i < 16; i++) {
        line2[i] = (i < filled) ? '#' : '.';
    }
    line2[16] = '\0';

    lcd1602_set_cursor(&s_lcd, 0, 0);
    lcd1602_print(&s_lcd, line1);
    lcd1602_set_cursor(&s_lcd, 0, 1);
    lcd1602_print(&s_lcd, line2);
}

/* Le uma linha do terminal serial (bloqueante), com eco de cada caractere
   digitado, e atualiza o duty-cycle (e o LCD) quando Enter e pressionado. */
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
                lcd_update_duty(value);
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

static void lcd_setup(void)
{
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = -1,
        .sda_io_num = LCD_SDA_GPIO,
        .scl_io_num = LCD_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus;
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &bus));

    /* Scan do barramento I2C: ajuda a confirmar o endereco real do LCD
       (ou detectar problema de fiacao, se nada responder). */
    ESP_LOGI(TAG, "Escaneando barramento I2C...");
    int found = 0;
    for (uint8_t addr = 0x03; addr < 0x78; addr++) {
        if (i2c_master_probe(bus, addr, 50) == ESP_OK) {
            ESP_LOGI(TAG, "Dispositivo I2C encontrado no endereco 0x%02X", addr);
            found++;
        }
    }
    if (found == 0) {
        ESP_LOGW(TAG, "Nenhum dispositivo I2C respondeu. Confira a fiacao (SDA=GPIO%d, SCL=GPIO%d, VCC, GND).",
                 LCD_SDA_GPIO, LCD_SCL_GPIO);
    }

    lcd1602_init(&s_lcd, bus, LCD_I2C_ADDR);
    lcd1602_clear(&s_lcd);
}

void app_main(void)
{
    gpio_reset_pin(PWM_GPIO);
    gpio_set_direction(PWM_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(PWM_GPIO, 0);

    lcd_setup();
    lcd_update_duty(s_duty_percent);

    const esp_timer_create_args_t timer_args = {
        .callback = &pwm_timer_callback,
        .name = "pwm_lcd_timer",
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &s_pwm_timer));
    ESP_ERROR_CHECK(esp_timer_start_once(s_pwm_timer, PWM_PERIOD_US));

    xTaskCreate(serial_input_task, "serial_input", 4096, NULL, 5, NULL);
}
