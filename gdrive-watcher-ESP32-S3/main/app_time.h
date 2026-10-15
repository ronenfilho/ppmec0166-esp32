#ifndef APP_TIME_H
#define APP_TIME_H

#include <stddef.h>
#include <time.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Sincroniza o relogio via NTP (SNTP) e configura o fuso horario de
 * Brasilia (UTC-3). Bloqueia ate a sincronizacao (ou timeout). Chamar
 * depois que o Wi-Fi ja estiver conectado.
 */
esp_err_t app_time_sync(void);

/* Formata um time_t em "dd/mm/aaaa HH:MM:SS" (hora local) em buf. */
void app_time_format(time_t t, char *buf, size_t len);

/* time_t do instante atual (atalho pra time(NULL)). */
time_t app_time_now(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_TIME_H */
