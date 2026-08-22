#include "lcd1602.h"
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_rom_sys.h"
#include "esp_log.h"

static const char *TAG = "lcd1602";

#define LCD_BACKLIGHT 0x08
#define LCD_ENABLE    0x04
#define LCD_RS        0x01

/* Alguns backpacks I2C ligam D4-D7 na ordem invertida (D7,D6,D5,D4 em vez
   de D4,D5,D6,D7). Se o texto sair embaralhado com o valor 0 aqui, troque
   para 1 e regrave. */
#define LCD_REVERSE_NIBBLE 0

static uint8_t reverse_nibble(uint8_t n)
{
    return ((n & 0x1) << 3) | ((n & 0x2) << 1) | ((n & 0x4) >> 1) | ((n & 0x8) >> 3);
}

static void lcd_i2c_write(lcd1602_t *lcd, uint8_t data)
{
    esp_err_t err = i2c_master_transmit(lcd->dev, &data, 1, -1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c_master_transmit falhou (0x%02X): %s", data, esp_err_to_name(err));
    }
}

/* Pulsa o pino Enable pra o HD44780 "ler" o nibble que ja esta nas linhas
   de dados (D4-D7 do PCF8574). */
static void lcd_pulse_enable(lcd1602_t *lcd, uint8_t data)
{
    lcd_i2c_write(lcd, data | LCD_ENABLE);
    esp_rom_delay_us(20);
    lcd_i2c_write(lcd, data & ~LCD_ENABLE);
    esp_rom_delay_us(200);
}

static void lcd_write4bits(lcd1602_t *lcd, uint8_t nibble, bool rs)
{
#if LCD_REVERSE_NIBBLE
    nibble = reverse_nibble(nibble);
#endif
    uint8_t data = (nibble << 4) | LCD_BACKLIGHT | (rs ? LCD_RS : 0);
    lcd_i2c_write(lcd, data);
    esp_rom_delay_us(20); /* tempo de setup antes de pulsar Enable */
    lcd_pulse_enable(lcd, data);
}

/* Envia um byte completo (comando ou dado) em dois nibbles de 4 bits,
   como o HD44780 espera quando operando em modo 4 bits. */
static void lcd_send(lcd1602_t *lcd, uint8_t value, bool rs)
{
    lcd_write4bits(lcd, value >> 4, rs);
    lcd_write4bits(lcd, value & 0x0F, rs);
}

static void lcd_command(lcd1602_t *lcd, uint8_t cmd)
{
    lcd_send(lcd, cmd, false);
}

static void lcd_data(lcd1602_t *lcd, uint8_t data)
{
    lcd_send(lcd, data, true);
}

void lcd1602_init(lcd1602_t *lcd, i2c_master_bus_handle_t bus, uint8_t addr)
{
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = 50000, /* mais devagar para maior robustez em fiacao de protoboard */
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &dev_cfg, &lcd->dev));

    vTaskDelay(pdMS_TO_TICKS(50)); /* aguarda o LCD estabilizar apos ligar */

    /* Sequencia de inicializacao em 4 bits (datasheet HD44780) */
    lcd_write4bits(lcd, 0x03, false);
    vTaskDelay(pdMS_TO_TICKS(5));
    lcd_write4bits(lcd, 0x03, false);
    esp_rom_delay_us(150);
    lcd_write4bits(lcd, 0x03, false);
    esp_rom_delay_us(150);
    lcd_write4bits(lcd, 0x02, false); /* liga modo 4 bits */

    lcd_command(lcd, 0x28); /* function set: 4 bits, 2 linhas, fonte 5x8 */
    lcd_command(lcd, 0x08); /* display off */
    lcd_command(lcd, 0x01); /* clear display */
    vTaskDelay(pdMS_TO_TICKS(2));
    lcd_command(lcd, 0x06); /* entry mode: incrementa cursor, sem shift */
    lcd_command(lcd, 0x0C); /* display on, cursor off, blink off */
}

void lcd1602_clear(lcd1602_t *lcd)
{
    lcd_command(lcd, 0x01);
    vTaskDelay(pdMS_TO_TICKS(2));
}

void lcd1602_set_cursor(lcd1602_t *lcd, uint8_t col, uint8_t row)
{
    static const uint8_t row_offsets[] = {0x00, 0x40};
    lcd_command(lcd, 0x80 | (col + row_offsets[row & 0x01]));
}

void lcd1602_print(lcd1602_t *lcd, const char *str)
{
    while (*str) {
        lcd_data(lcd, (uint8_t)*str++);
    }
}
