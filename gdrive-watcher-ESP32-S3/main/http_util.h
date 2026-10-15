#ifndef HTTP_UTIL_H
#define HTTP_UTIL_H

#include <stddef.h>
#include "esp_err.h"
#include "esp_http_client.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    const char *key;
    const char *value;
} http_header_t;

/*
 * Faz uma requisicao HTTP(S) sincrona e devolve o corpo da resposta.
 * *out_body e alocado com malloc (o chamador deve dar free) e vem
 * terminado em '\0'. Usa o certificate bundle do ESP-IDF para validar
 * TLS (esp_crt_bundle_attach) -- funciona para qualquer host com CA
 * publica reconhecida (googleapis.com, oauth2.googleapis.com, etc).
 */
esp_err_t http_request(esp_http_client_method_t method,
                        const char *url,
                        const http_header_t *headers, size_t n_headers,
                        const char *body, size_t body_len,
                        char **out_body, size_t *out_len,
                        int *out_status);

/* Atalho para POST de formulario (application/x-www-form-urlencoded),
 * usado por todas as chamadas OAuth. */
esp_err_t http_post_form(const char *url, const char *form_body,
                          char **out_body, size_t *out_len, int *out_status);

/* Atalho para GET simples, com um header opcional (ex: Authorization). */
esp_err_t http_get(const char *url, const http_header_t *auth_header,
                    char **out_body, size_t *out_len, int *out_status);

/* Codifica um valor pra uso em querystring/form-urlencoded (RFC 3986). */
size_t http_url_encode(const char *src, char *dst, size_t dst_len);

#ifdef __cplusplus
}
#endif

#endif /* HTTP_UTIL_H */
