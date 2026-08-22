/* Driver minimo para LCD 16x2 (HD44780) via backpack I2C PCF8574.

   Mapeamento de pinos do PCF8574 (padrao da maioria dos backpacks
   "LCM1602 IIC"): bit0=RS, bit1=RW, bit2=E, bit3=Backlight, bit4-7=D4-D7.
*/
#pragma once

#include <stdint.h>
#include "driver/i2c_master.h"

typedef struct {
    i2c_master_dev_handle_t dev;
} lcd1602_t;

/* Inicializa o LCD no endereco I2C indicado (tipicamente 0x27 ou 0x3F). */
void lcd1602_init(lcd1602_t *lcd, i2c_master_bus_handle_t bus, uint8_t addr);

void lcd1602_clear(lcd1602_t *lcd);
void lcd1602_set_cursor(lcd1602_t *lcd, uint8_t col, uint8_t row);
void lcd1602_print(lcd1602_t *lcd, const char *str);
