#ifndef GDRIVE_API_H
#define GDRIVE_API_H

#include <stddef.h>
#include "esp_err.h"
#include "source_provider.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Metadados de um unico arquivo publico (via API key, sem OAuth). */
esp_err_t gdrive_public_get_file(const char *file_id, item_t *out);

/* Lista o nivel raiz de uma pasta publica (via API key, sem OAuth).
 * *out e alocado (o chamador deve dar free); paginado internamente,
 * limitado a STORE_MAX_ITEMS_PER_LINK itens. */
esp_err_t gdrive_public_list_folder(const char *folder_id, item_t **out, size_t *n);

/* Dispatcher generico usado pela tabela de providers em gdrive_url.c --
 * decide entre get_file/list_folder de acordo com o kind. */
esp_err_t gdrive_list_items(source_kind_t kind, const char *id, item_t **out, size_t *n);

/* --- Modo autenticado: Changes API --- */

/* Pega o watermark inicial da Changes API (chamar uma vez ao autenticar,
 * ou quando o token guardado expirar/for invalido). */
esp_err_t gdrive_auth_get_start_page_token(char *out, size_t len);

/* Le as mudancas desde page_token; devolve os itens (incluindo removidos,
 * com item->removed=true) e o novo token em new_start_token. */
esp_err_t gdrive_auth_list_changes(const char *page_token,
                                    item_t **out, size_t *n,
                                    char *new_start_token, size_t tok_len);

/* Lista os arquivos mais recentes do Drive inteiro, so para exibicao
 * (comando "list" em modo autenticado) -- nao usado pro diff. */
esp_err_t gdrive_auth_list_recent(item_t **out, size_t *n);

#ifdef __cplusplus
}
#endif

#endif /* GDRIVE_API_H */
