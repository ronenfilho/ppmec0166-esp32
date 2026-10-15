#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "link_store.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "STORE";
static const char *NS = "gdrive";

static nvs_handle_t s_h;
static bool s_open = false;
static SemaphoreHandle_t s_mutex = NULL;

void store_lock(void)
{
    if (s_mutex != NULL)
    {
        xSemaphoreTakeRecursive(s_mutex, portMAX_DELAY);
    }
}

void store_unlock(void)
{
    if (s_mutex != NULL)
    {
        xSemaphoreGiveRecursive(s_mutex);
    }
}

esp_err_t store_init(void)
{
    s_mutex = xSemaphoreCreateRecursiveMutex();
    if (s_mutex == NULL)
    {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = nvs_open(NS, NVS_READWRITE, &s_h);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "nvs_open falhou: %s", esp_err_to_name(err));
        return err;
    }
    s_open = true;
    return ESP_OK;
}

/* ---------- helpers genericos ---------- */

static esp_err_t get_u8(const char *key, uint8_t *out, uint8_t def)
{
    esp_err_t err = nvs_get_u8(s_h, key, out);
    if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        *out = def;
        return ESP_OK;
    }
    return err;
}

static esp_err_t move_str_key(const char *from, const char *to)
{
    size_t len = 0;
    if (nvs_get_str(s_h, from, NULL, &len) != ESP_OK)
    {
        return nvs_erase_key(s_h, from); /* nada pra mover */
    }
    char *buf = malloc(len);
    if (buf == NULL)
    {
        return ESP_ERR_NO_MEM;
    }
    nvs_get_str(s_h, from, buf, &len);
    esp_err_t err = nvs_set_str(s_h, to, buf);
    free(buf);
    nvs_erase_key(s_h, from);
    return err;
}

static esp_err_t move_blob_key(const char *from, const char *to)
{
    size_t len = 0;
    if (nvs_get_blob(s_h, from, NULL, &len) != ESP_OK)
    {
        nvs_erase_key(s_h, from);
        return ESP_OK;
    }
    void *buf = malloc(len);
    if (buf == NULL)
    {
        return ESP_ERR_NO_MEM;
    }
    nvs_get_blob(s_h, from, buf, &len);
    esp_err_t err = nvs_set_blob(s_h, to, buf, len);
    free(buf);
    nvs_erase_key(s_h, from);
    return err;
}

static esp_err_t move_u32_key(const char *from, const char *to)
{
    uint32_t v;
    if (nvs_get_u32(s_h, from, &v) != ESP_OK)
    {
        nvs_erase_key(s_h, from);
        return ESP_OK;
    }
    esp_err_t err = nvs_set_u32(s_h, to, v);
    nvs_erase_key(s_h, from);
    return err;
}

/* ---------- links registrados ---------- */

int store_link_count(void)
{
    uint8_t n = 0;
    get_u8("n_links", &n, 0);
    return n;
}

esp_err_t store_link_add(const char *url, source_kind_t kind, const char *id, int *index_out)
{
    int n = store_link_count();
    if (n >= STORE_MAX_LINKS)
    {
        return ESP_ERR_NO_MEM;
    }

    char key[16];
    char rec[STORE_URL_LEN + 64];
    snprintf(key, sizeof(key), "link%d", n);
    snprintf(rec, sizeof(rec), "%d|%s|%s", (int)kind, id, url);

    esp_err_t err = nvs_set_str(s_h, key, rec);
    if (err != ESP_OK)
    {
        return err;
    }
    err = nvs_set_u8(s_h, "n_links", (uint8_t)(n + 1));
    if (err != ESP_OK)
    {
        return err;
    }
    nvs_commit(s_h);

    if (index_out != NULL)
    {
        *index_out = n;
    }
    return ESP_OK;
}

esp_err_t store_link_get(int index, link_rec_t *out)
{
    if (index < 0 || index >= store_link_count() || out == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    char key[16];
    snprintf(key, sizeof(key), "link%d", index);

    char rec[STORE_URL_LEN + 64];
    size_t len = sizeof(rec);
    esp_err_t err = nvs_get_str(s_h, key, rec, &len);
    if (err != ESP_OK)
    {
        return err;
    }

    int kind = 0;
    char id[ITEM_ID_LEN] = {0};
    char url[STORE_URL_LEN] = {0};

    char *bar1 = strchr(rec, '|');
    if (bar1 == NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }
    *bar1 = '\0';
    kind = atoi(rec);

    char *bar2 = strchr(bar1 + 1, '|');
    if (bar2 == NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }
    *bar2 = '\0';
    strncpy(id, bar1 + 1, sizeof(id) - 1);
    strncpy(url, bar2 + 1, sizeof(url) - 1);

    memset(out, 0, sizeof(*out));
    out->kind = (source_kind_t)kind;
    strncpy(out->id, id, sizeof(out->id) - 1);
    strncpy(out->url, url, sizeof(out->url) - 1);

    char mark_key[16];
    snprintf(mark_key, sizeof(mark_key), "mark%d", index);
    uint32_t mark = 0;
    nvs_get_u32(s_h, mark_key, &mark);
    out->last_check = mark;

    return ESP_OK;
}

esp_err_t store_link_touch(int index, uint32_t when)
{
    char key[16];
    snprintf(key, sizeof(key), "mark%d", index);
    esp_err_t err = nvs_set_u32(s_h, key, when);
    nvs_commit(s_h);
    return err;
}

esp_err_t store_link_remove(int index)
{
    int n = store_link_count();
    if (index < 0 || index >= n)
    {
        return ESP_ERR_INVALID_ARG;
    }

    /* compacta: desloca index+1..n-1 para index..n-2 */
    for (int i = index; i < n - 1; i++)
    {
        char from_link[16], to_link[16];
        char from_snap[16], to_snap[16];
        char from_mark[16], to_mark[16];

        snprintf(from_link, sizeof(from_link), "link%d", i + 1);
        snprintf(to_link, sizeof(to_link), "link%d", i);
        snprintf(from_snap, sizeof(from_snap), "snap%d", i + 1);
        snprintf(to_snap, sizeof(to_snap), "snap%d", i);
        snprintf(from_mark, sizeof(from_mark), "mark%d", i + 1);
        snprintf(to_mark, sizeof(to_mark), "mark%d", i);

        move_str_key(from_link, to_link);
        move_blob_key(from_snap, to_snap);
        move_u32_key(from_mark, to_mark);
    }

    /* apaga as chaves finais (agora sobrando) */
    char last_link[16], last_snap[16], last_mark[16];
    snprintf(last_link, sizeof(last_link), "link%d", n - 1);
    snprintf(last_snap, sizeof(last_snap), "snap%d", n - 1);
    snprintf(last_mark, sizeof(last_mark), "mark%d", n - 1);
    nvs_erase_key(s_h, last_link);
    nvs_erase_key(s_h, last_snap);
    nvs_erase_key(s_h, last_mark);

    esp_err_t err = nvs_set_u8(s_h, "n_links", (uint8_t)(n - 1));
    nvs_commit(s_h);
    return err;
}

/* ---------- snapshots (JSON: [{"i":..,"n":..,"m":..,"t":..}, ...]) ---------- */

static char *items_to_json(const item_t *items, size_t n)
{
    cJSON *arr = cJSON_CreateArray();
    for (size_t k = 0; k < n; k++)
    {
        cJSON *o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "i", items[k].id);
        cJSON_AddStringToObject(o, "n", items[k].name);
        cJSON_AddStringToObject(o, "m", items[k].mime);
        cJSON_AddStringToObject(o, "t", items[k].modified);
        cJSON_AddItemToArray(arr, o);
    }
    char *out = cJSON_PrintUnformatted(arr);
    cJSON_Delete(arr);
    return out; /* chamador deve free() */
}

static esp_err_t json_to_items(const char *json, item_t **out, size_t *n)
{
    *out = NULL;
    *n = 0;

    cJSON *arr = cJSON_Parse(json);
    if (arr == NULL || !cJSON_IsArray(arr))
    {
        if (arr != NULL)
        {
            cJSON_Delete(arr);
        }
        return ESP_OK; /* snapshot vazio/invalido -> trata como vazio */
    }

    int count = cJSON_GetArraySize(arr);
    if (count == 0)
    {
        cJSON_Delete(arr);
        return ESP_OK;
    }

    item_t *items = calloc((size_t)count, sizeof(item_t));
    if (items == NULL)
    {
        cJSON_Delete(arr);
        return ESP_ERR_NO_MEM;
    }

    for (int k = 0; k < count; k++)
    {
        cJSON *o = cJSON_GetArrayItem(arr, k);
        cJSON *fi = cJSON_GetObjectItem(o, "i");
        cJSON *fn = cJSON_GetObjectItem(o, "n");
        cJSON *fm = cJSON_GetObjectItem(o, "m");
        cJSON *ft = cJSON_GetObjectItem(o, "t");
        if (cJSON_IsString(fi))
        {
            strncpy(items[k].id, fi->valuestring, sizeof(items[k].id) - 1);
        }
        if (cJSON_IsString(fn))
        {
            strncpy(items[k].name, fn->valuestring, sizeof(items[k].name) - 1);
        }
        if (cJSON_IsString(fm))
        {
            strncpy(items[k].mime, fm->valuestring, sizeof(items[k].mime) - 1);
        }
        if (cJSON_IsString(ft))
        {
            strncpy(items[k].modified, ft->valuestring, sizeof(items[k].modified) - 1);
        }
    }

    cJSON_Delete(arr);
    *out = items;
    *n = (size_t)count;
    return ESP_OK;
}

static esp_err_t blob_save_items(const char *key, const item_t *items, size_t n)
{
    char *json = items_to_json(items, n);
    if (json == NULL)
    {
        return ESP_ERR_NO_MEM;
    }
    esp_err_t err = nvs_set_blob(s_h, key, json, strlen(json) + 1);
    free(json);
    if (err == ESP_OK)
    {
        nvs_commit(s_h);
    }
    return err;
}

static esp_err_t blob_load_items(const char *key, item_t **out, size_t *n)
{
    *out = NULL;
    *n = 0;

    size_t len = 0;
    esp_err_t err = nvs_get_blob(s_h, key, NULL, &len);
    if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        return ESP_OK; /* nunca salvo ainda -> vazio */
    }
    if (err != ESP_OK)
    {
        return err;
    }

    char *buf = malloc(len);
    if (buf == NULL)
    {
        return ESP_ERR_NO_MEM;
    }
    nvs_get_blob(s_h, key, buf, &len);
    err = json_to_items(buf, out, n);
    free(buf);
    return err;
}

esp_err_t store_snapshot_load(int index, item_t **out, size_t *n)
{
    char key[16];
    snprintf(key, sizeof(key), "snap%d", index);
    return blob_load_items(key, out, n);
}

esp_err_t store_snapshot_save(int index, const item_t *items, size_t n)
{
    char key[16];
    snprintf(key, sizeof(key), "snap%d", index);
    return blob_save_items(key, items, n);
}

/* ---------- Drive inteiro (autenticado) ---------- */

esp_err_t store_drive_snapshot_load(item_t **out, size_t *n)
{
    return blob_load_items("drv_snap", out, n);
}

esp_err_t store_drive_snapshot_save(const item_t *items, size_t n)
{
    return blob_save_items("drv_snap", items, n);
}

esp_err_t store_get_page_token(char *out, size_t len)
{
    esp_err_t err = nvs_get_str(s_h, "drv_token", out, &len);
    if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        out[0] = '\0';
        return ESP_OK;
    }
    return err;
}

esp_err_t store_set_page_token(const char *tok)
{
    esp_err_t err = nvs_set_str(s_h, "drv_token", tok);
    if (err == ESP_OK)
    {
        nvs_commit(s_h);
    }
    return err;
}

esp_err_t store_clear_drive_state(void)
{
    nvs_erase_key(s_h, "drv_snap");
    nvs_erase_key(s_h, "drv_token");
    nvs_commit(s_h);
    return ESP_OK;
}

/* ---------- OAuth ---------- */

esp_err_t store_get_refresh_token(char *out, size_t len)
{
    esp_err_t err = nvs_get_str(s_h, "oauth_rt", out, &len);
    if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        out[0] = '\0';
        return ESP_OK;
    }
    return err;
}

esp_err_t store_set_refresh_token(const char *token)
{
    esp_err_t err = nvs_set_str(s_h, "oauth_rt", token);
    if (err == ESP_OK)
    {
        nvs_commit(s_h);
    }
    return err;
}

esp_err_t store_get_scope(char *out, size_t len)
{
    esp_err_t err = nvs_get_str(s_h, "oauth_scope", out, &len);
    if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        out[0] = '\0';
        return ESP_OK;
    }
    return err;
}

esp_err_t store_set_scope(const char *scope)
{
    esp_err_t err = nvs_set_str(s_h, "oauth_scope", scope);
    if (err == ESP_OK)
    {
        nvs_commit(s_h);
    }
    return err;
}

esp_err_t store_clear_auth(void)
{
    nvs_erase_key(s_h, "oauth_rt");
    nvs_erase_key(s_h, "oauth_scope");
    store_clear_drive_state();
    nvs_commit(s_h);
    return ESP_OK;
}

/* ---------- configuracao ---------- */

esp_err_t store_get_interval(uint32_t *seconds)
{
    esp_err_t err = nvs_get_u32(s_h, "poll_sec", seconds);
    if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        *seconds = 0; /* caller aplica o default do secrets.h */
        return ESP_OK;
    }
    return err;
}

esp_err_t store_set_interval(uint32_t seconds)
{
    esp_err_t err = nvs_set_u32(s_h, "poll_sec", seconds);
    if (err == ESP_OK)
    {
        nvs_commit(s_h);
    }
    return err;
}

esp_err_t store_factory_reset(void)
{
    nvs_erase_all(s_h);
    nvs_commit(s_h);
    return ESP_OK;
}
