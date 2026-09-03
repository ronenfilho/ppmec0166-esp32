#ifndef TOOGLE_LED_H
#define TOOGLE_LED_H

#include <stdbool.h>

/* Configura o GPIO do LED como saida (nivel inicial: desligado). */
void toogle_led_init(void);

/* Liga (true) ou desliga (false) o LED. */
void toogle_led_set(bool on);

/* Estado atual do LED (true = ligado). */
bool toogle_led_get_state(void);

#endif /* TOOGLE_LED_H */
