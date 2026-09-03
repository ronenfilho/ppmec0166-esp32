#ifndef CLOCK_H
#define CLOCK_H

#include "esp_http_server.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Sincroniza o relogio do ESP32 via NTP (SNTP) e configura o fuso
 * horario de Brasilia (UTC-3). Bloqueia ate a sincronizacao (ou timeout).
 * Chamar depois que o Wi-Fi ja estiver conectado.
 */
void clock_init(void);

/*
 * Handler HTTP (GET) que retorna uma pagina HTML com a data/hora atual
 * do dispositivo, com auto-atualizacao a cada 10 segundos.
 */
esp_err_t on_get_clock(httpd_req_t *req);

#ifdef __cplusplus
}
#endif

#endif /* CLOCK_H */
