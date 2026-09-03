#ifndef TOOGLE_LED_H
#define TOOGLE_LED_H

#include <stdbool.h>
#include "esp_http_server.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Configura o GPIO do LED como saida (nivel inicial: desligado). */
void toogle_led_init(void);

/* Estado atual do LED (true = ligado). */
bool toogle_led_get_state(void);

/*
 * Handler HTTP (POST) que le do corpo da requisicao um JSON no formato
 * {"is_on": true|false} e liga/desliga o LED de acordo.
 * Responde com o mesmo JSON confirmando o novo estado.
 */
esp_err_t on_toggle_led(httpd_req_t *req);

#ifdef __cplusplus
}
#endif

#endif /* TOOGLE_LED_H */
