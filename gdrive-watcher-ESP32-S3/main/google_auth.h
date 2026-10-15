#ifndef GOOGLE_AUTH_H
#define GOOGLE_AUTH_H

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    AUTH_NONE = 0,
    AUTH_DEVICE_PENDING,
    AUTH_PKCE_PENDING,
    AUTH_OK,
} auth_state_t;

/* Carrega refresh_token/scope salvos na NVS (via link_store). Chamar
 * uma vez no boot, depois de store_init(). */
esp_err_t google_auth_init(void);

auth_state_t google_auth_state(void);
bool         google_auth_is_authenticated(void);
const char  *google_auth_scope(void); /* escopo concedido, ou "" */

/* --- Fluxo "device code" (codigo no terminal, escopo drive.file) --- */

/* Inicia o fluxo: pede um device_code ao Google e devolve o codigo e
 * a URL que o usuario deve abrir. Buffers: user_code>=16, verif_url>=128. */
esp_err_t google_auth_device_begin(char *user_code, size_t user_code_len,
                                    char *verification_url, size_t url_len);

/* Uma tentativa de poll (o proprio modulo se autopaça internamente
 * respeitando o "interval"/"slow_down" do Google -- pode ser chamado
 * com frequencia pelo watcher sem medo de floodar a API).
 * ESP_OK = autenticado agora; ESP_ERR_NOT_FINISHED = continua esperando;
 * ESP_ERR_TIMEOUT = expirou; ESP_FAIL = negado/erro definitivo. */
esp_err_t google_auth_device_poll_once(void);

/* --- Fluxo PKCE / "app desktop" (Drive inteiro, escopo drive.readonly) --- */

/* Gera code_verifier/challenge e devolve a URL de autorizacao completa
 * (auth_url_out >= 512). */
esp_err_t google_auth_pkce_begin(char *auth_url_out, size_t len);

/* Troca o "code" (colado pelo usuario) por um token. */
esp_err_t google_auth_pkce_finish(const char *code);

/* --- Comuns --- */

/* Bearer valido (renova sozinho se estiver perto de expirar). NULL se
 * nao autenticado ou se o refresh falhar. */
const char *google_auth_get_bearer(void);

/* Forca um refresh agora. */
esp_err_t google_auth_refresh(void);

/* Segundos até o access token expirar (0 se nao autenticado). */
int google_auth_expires_in(void);

/* Limpa tokens (memoria + NVS). Mantem os links publicos registrados. */
void google_auth_logout(void);

#ifdef __cplusplus
}
#endif

#endif /* GOOGLE_AUTH_H */
