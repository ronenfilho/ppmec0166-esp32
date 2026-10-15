#ifndef SOURCE_PROVIDER_H
#define SOURCE_PROVIDER_H

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Vocabulario neutro de provider, usado pelo console e pelo link_store.
 * Nem um nem outro deve conhecer detalhes de "Drive" -- so falam
 * source_kind_t/item_t. Hoje so existe o provider do Google Drive
 * (gdrive_url.c), mas para adicionar outro tipo de link no futuro basta
 * implementar match_url()/list_items() e registrar aqui.
 */

#define ITEM_ID_LEN   40
#define ITEM_NAME_LEN 64
#define ITEM_MIME_LEN 64
#define ITEM_TIME_LEN 32

typedef enum
{
    SOURCE_KIND_UNKNOWN = 0,
    SOURCE_KIND_GDRIVE_FILE,
    SOURCE_KIND_GDRIVE_FOLDER,
} source_kind_t;

typedef struct
{
    char id[ITEM_ID_LEN];
    char name[ITEM_NAME_LEN];
    char mime[ITEM_MIME_LEN];
    char modified[ITEM_TIME_LEN]; /* RFC-3339, como a API devolve */
    bool removed;                 /* so usado pela Changes API */
} item_t;

typedef struct
{
    const char *scheme_name; /* ex: "gdrive" */
    bool (*match_url)(const char *url, source_kind_t *kind, char *id_out, size_t id_len);
    esp_err_t (*list_items)(source_kind_t kind, const char *id, item_t **out, size_t *n);
} source_provider_t;

/* Percorre a tabela de providers registrados e devolve o primeiro que
 * reconhecer a URL, ou NULL se nenhum reconhecer. */
const source_provider_t *source_provider_resolve(const char *url,
                                                   source_kind_t *kind_out,
                                                   char *id_out, size_t id_len);

#ifdef __cplusplus
}
#endif

#endif /* SOURCE_PROVIDER_H */
