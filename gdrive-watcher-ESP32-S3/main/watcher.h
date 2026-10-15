#ifndef WATCHER_H
#define WATCHER_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    WATCH_REQ_CHECK = 0,
    WATCH_REQ_AUTH_DEVICE,
    WATCH_REQ_AUTH_PKCE_BEGIN,
    WATCH_REQ_AUTH_PKCE_FINISH,
    WATCH_REQ_LOGOUT,
} watch_req_kind_t;

typedef struct
{
    bool authenticated;
    uint32_t poll_interval_s;
    int link_count;
} watcher_status_t;

/* Cria a task worker e o timer periodico. Chamar depois de store_init(),
 * google_auth_init() e da conexao Wi-Fi. */
esp_err_t watcher_init(void);

/* Poe um pedido na fila do worker (nao bloqueia). "arg" so e usado por
 * WATCH_REQ_AUTH_PKCE_FINISH (o "code" colado pelo usuario) -- pode ser
 * NULL nos outros casos. */
esp_err_t watcher_request(watch_req_kind_t kind, const char *arg);

esp_err_t watcher_set_interval(uint32_t seconds);
void      watcher_status(watcher_status_t *out);

#ifdef __cplusplus
}
#endif

#endif /* WATCHER_H */
