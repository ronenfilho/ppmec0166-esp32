#ifndef LINK_STORE_H
#define LINK_STORE_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "source_provider.h"

#ifdef __cplusplus
extern "C" {
#endif

#define STORE_MAX_LINKS           8
#define STORE_MAX_ITEMS_PER_LINK  64
#define STORE_MAX_DRIVE_ITEMS     25
#define STORE_URL_LEN             256

typedef struct
{
    source_kind_t kind;
    char id[ITEM_ID_LEN];
    char url[STORE_URL_LEN];
    uint32_t last_check; /* unix time, 0 = nunca */
} link_rec_t;

/* Inicializa o namespace NVS "gdrive". Chamar uma vez no boot. */
esp_err_t store_init(void);

/* Mutex recursivo -- quem for fazer mais de uma chamada store_* em
 * sequencia que precise ser atomica (ex: ler+comparar+salvar um
 * snapshot) deve envolver com store_lock()/store_unlock(). */
void store_lock(void);
void store_unlock(void);

/* --- Links registrados (modo publico) --- */
int       store_link_count(void);
esp_err_t store_link_add(const char *url, source_kind_t kind, const char *id, int *index_out);
esp_err_t store_link_remove(int index);
esp_err_t store_link_get(int index, link_rec_t *out);
esp_err_t store_link_touch(int index, uint32_t when); /* atualiza last_check */

esp_err_t store_snapshot_load(int index, item_t **out, size_t *n);   /* *out alocado, chamador libera */
esp_err_t store_snapshot_save(int index, const item_t *items, size_t n);

/* --- Snapshot do Drive inteiro (modo autenticado) --- */
esp_err_t store_drive_snapshot_load(item_t **out, size_t *n);
esp_err_t store_drive_snapshot_save(const item_t *items, size_t n);
esp_err_t store_get_page_token(char *out, size_t len);
esp_err_t store_set_page_token(const char *tok);
esp_err_t store_clear_drive_state(void); /* limpa drv_snap + drv_token */

/* --- OAuth --- */
esp_err_t store_get_refresh_token(char *out, size_t len);
esp_err_t store_set_refresh_token(const char *token);
esp_err_t store_get_scope(char *out, size_t len);
esp_err_t store_set_scope(const char *scope);
esp_err_t store_clear_auth(void); /* oauth_rt + oauth_scope + drv_snap + drv_token */

/* --- Configuracao --- */
esp_err_t store_get_interval(uint32_t *seconds);
esp_err_t store_set_interval(uint32_t seconds);

/* Zera tudo (links, snapshots, auth, config). */
esp_err_t store_factory_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* LINK_STORE_H */
