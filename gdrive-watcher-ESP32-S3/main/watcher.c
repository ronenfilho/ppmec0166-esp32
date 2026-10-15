#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "watcher.h"
#include "link_store.h"
#include "google_auth.h"
#include "gdrive_api.h"
#include "secrets.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_timer.h"
#include "esp_log.h"

static const char *TAG = "WATCHER";

typedef struct
{
    watch_req_kind_t kind;
    bool verbose;
    char arg[300];
} watch_req_t;

static QueueHandle_t s_q = NULL;
static esp_timer_handle_t s_timer = NULL;
static uint32_t s_interval_s = 0;

static const item_t *find_by_id(const item_t *arr, size_t n, const char *id)
{
    for (size_t i = 0; i < n; i++)
    {
        if (strcmp(arr[i].id, id) == 0)
        {
            return &arr[i];
        }
    }
    return NULL;
}

static void enqueue(watch_req_kind_t kind, bool verbose, const char *arg)
{
    watch_req_t req = {.kind = kind, .verbose = verbose};
    if (arg != NULL)
    {
        strncpy(req.arg, arg, sizeof(req.arg) - 1);
    }
    else
    {
        req.arg[0] = '\0';
    }
    xQueueSend(s_q, &req, 0);
}

static void timer_cb(void *arg)
{
    (void)arg;
    enqueue(WATCH_REQ_CHECK, false, NULL);
}

/* ---------- o motor unico de verificacao ---------- */

static void check_public_links(bool verbose)
{
    int n = store_link_count();
    for (int i = 0; i < n; i++)
    {
        link_rec_t rec;
        if (store_link_get(i, &rec) != ESP_OK)
        {
            continue;
        }

        item_t *fresh = NULL;
        size_t fresh_n = 0;
        esp_err_t err = gdrive_list_items(rec.kind, rec.id, &fresh, &fresh_n);
        if (err != ESP_OK)
        {
            ESP_LOGW(TAG, "link #%d: falha ao consultar (%s)", i, esp_err_to_name(err));
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        item_t *old = NULL;
        size_t old_n = 0;
        store_snapshot_load(i, &old, &old_n);

        int new_count = 0;
        for (size_t k = 0; k < fresh_n; k++)
        {
            const item_t *prev = find_by_id(old, old_n, fresh[k].id);
            if (prev == NULL || strcmp(prev->modified, fresh[k].modified) != 0)
            {
                ESP_LOGI(TAG, "[NOVO] link #%d: %s (%s)", i, fresh[k].name, fresh[k].mime);
                new_count++;
            }
        }
        if (new_count == 0 && verbose)
        {
            ESP_LOGI(TAG, "link #%d: nenhuma novidade", i);
        }

        store_snapshot_save(i, fresh, fresh_n);
        store_link_touch(i, (uint32_t)time(NULL));

        free(old);
        free(fresh);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

static void check_drive_changes(bool verbose)
{
    if (!google_auth_is_authenticated())
    {
        return;
    }

    char token[256] = {0};
    store_get_page_token(token, sizeof(token));

    if (token[0] == '\0')
    {
        char fresh_token[256] = {0};
        esp_err_t err = gdrive_auth_get_start_page_token(fresh_token, sizeof(fresh_token));
        if (err == ESP_OK && fresh_token[0] != '\0')
        {
            store_set_page_token(fresh_token);
            ESP_LOGI(TAG, "Drive autenticado: watermark inicial obtido, novidades a partir de agora");
        }
        else
        {
            ESP_LOGW(TAG, "Falha ao obter o watermark inicial da Changes API");
        }
        return;
    }

    item_t *changes = NULL;
    size_t cn = 0;
    char new_token[256] = {0};
    esp_err_t err = gdrive_auth_list_changes(token, &changes, &cn, new_token, sizeof(new_token));

    if (err == ESP_ERR_NOT_FOUND)
    {
        ESP_LOGW(TAG, "Watermark do Drive expirou -- reiniciando");
        store_clear_drive_state();
        return;
    }
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "Falha ao consultar mudancas do Drive: %s", esp_err_to_name(err));
        return;
    }

    if (cn == 0 && verbose)
    {
        ESP_LOGI(TAG, "Drive: nenhuma novidade");
    }

    item_t *old = NULL;
    size_t old_n = 0;
    store_drive_snapshot_load(&old, &old_n);

    item_t *merged = calloc(old_n + cn, sizeof(item_t));
    size_t merged_n = 0;
    if (merged != NULL)
    {
        /* mantem os antigos que nao foram tocados nessa rodada de mudancas */
        for (size_t i = 0; i < old_n; i++)
        {
            if (find_by_id(changes, cn, old[i].id) == NULL)
            {
                merged[merged_n++] = old[i];
            }
        }
        /* aplica as mudancas: adiciona as nao-removidas, loga cada uma */
        for (size_t k = 0; k < cn; k++)
        {
            if (changes[k].removed)
            {
                ESP_LOGI(TAG, "[REMOVIDO] Drive: %s", changes[k].id);
                continue;
            }
            ESP_LOGI(TAG, "[NOVO] Drive: %s (%s)", changes[k].name, changes[k].mime);
            merged[merged_n++] = changes[k];
        }
        /* limite simples: se passou do cap, descarta os mais antigos (inicio) */
        if (merged_n > STORE_MAX_DRIVE_ITEMS)
        {
            size_t drop = merged_n - STORE_MAX_DRIVE_ITEMS;
            memmove(merged, merged + drop, (merged_n - drop) * sizeof(item_t));
            merged_n -= drop;
        }
        store_drive_snapshot_save(merged, merged_n);
        free(merged);
    }
    free(old);
    free(changes);

    if (new_token[0] != '\0')
    {
        store_set_page_token(new_token);
    }
}

static void watcher_check_all(bool verbose)
{
    store_lock();
    check_public_links(verbose);
    check_drive_changes(verbose);
    store_unlock();
}

/* ---------- pedidos ---------- */

static void handle_request(const watch_req_t *req)
{
    switch (req->kind)
    {
        case WATCH_REQ_CHECK:
            watcher_check_all(req->verbose);
            break;

        case WATCH_REQ_AUTH_DEVICE:
        {
            char user_code[32] = {0};
            char verif_url[128] = {0};
            esp_err_t err = google_auth_device_begin(user_code, sizeof(user_code),
                                                      verif_url, sizeof(verif_url));
            if (err == ESP_OK)
            {
                printf("\r\nAbra %s e digite o codigo: %s\r\n", verif_url, user_code);
            }
            else
            {
                ESP_LOGE(TAG, "Nao foi possivel iniciar o device flow: %s", esp_err_to_name(err));
            }
            break;
        }

        case WATCH_REQ_AUTH_PKCE_BEGIN:
        {
            char url[512] = {0};
            esp_err_t err = google_auth_pkce_begin(url, sizeof(url));
            if (err == ESP_OK)
            {
                printf("\r\nAbra esta URL, aprove, e copie o valor de \"code=\" "
                       "da URL de retorno (a pagina vai dar erro, é esperado):\r\n%s\r\n"
                       "Depois rode: auth code <codigo>\r\n",
                       url);
            }
            else
            {
                ESP_LOGE(TAG, "Nao foi possivel iniciar o fluxo PKCE: %s", esp_err_to_name(err));
            }
            break;
        }

        case WATCH_REQ_AUTH_PKCE_FINISH:
        {
            esp_err_t err = google_auth_pkce_finish(req->arg);
            if (err == ESP_OK)
            {
                printf("Autenticado! escopo: %s\r\n", google_auth_scope());
                watcher_check_all(false); /* busca o watermark inicial ja */
            }
            else
            {
                ESP_LOGE(TAG, "Falha ao trocar o codigo por token: %s", esp_err_to_name(err));
            }
            break;
        }

        case WATCH_REQ_LOGOUT:
            google_auth_logout();
            printf("Logout feito -- links publicos continuam registrados.\r\n");
            break;
    }
}

static void watcher_task(void *arg)
{
    (void)arg;
    watch_req_t req;

    for (;;)
    {
        TickType_t wait = (google_auth_state() == AUTH_DEVICE_PENDING)
                               ? pdMS_TO_TICKS(1000)
                               : portMAX_DELAY;

        if (xQueueReceive(s_q, &req, wait) == pdTRUE)
        {
            handle_request(&req);
        }

        if (google_auth_state() == AUTH_DEVICE_PENDING)
        {
            esp_err_t r = google_auth_device_poll_once();
            if (r == ESP_OK)
            {
                printf("Autenticado! escopo: %s\r\n", google_auth_scope());
            }
            else if (r == ESP_ERR_TIMEOUT)
            {
                ESP_LOGW(TAG, "Codigo de dispositivo expirou -- rode \"auth\" de novo");
            }
            else if (r == ESP_FAIL)
            {
                ESP_LOGE(TAG, "Autenticacao negada ou erro definitivo");
            }
            /* ESP_ERR_NOT_FINISHED: continua esperando, silencioso */
        }
    }
}

esp_err_t watcher_init(void)
{
    s_q = xQueueCreate(8, sizeof(watch_req_t));
    if (s_q == NULL)
    {
        return ESP_ERR_NO_MEM;
    }

    store_get_interval(&s_interval_s);
    if (s_interval_s == 0)
    {
        s_interval_s = POLL_INTERVAL_SECONDS_DEFAULT;
    }

    esp_timer_create_args_t targs = {
        .callback = timer_cb,
        .name = "gdrive_poll",
    };
    esp_err_t err = esp_timer_create(&targs, &s_timer);
    if (err != ESP_OK)
    {
        return err;
    }
    esp_timer_start_periodic(s_timer, (uint64_t)s_interval_s * 1000000ULL);

    BaseType_t ok = xTaskCreate(watcher_task, "watcher", 10240, NULL, 5, NULL);
    if (ok != pdPASS)
    {
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t watcher_request(watch_req_kind_t kind, const char *arg)
{
    if (s_q == NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }
    enqueue(kind, true, arg);
    return ESP_OK;
}

esp_err_t watcher_set_interval(uint32_t seconds)
{
    if (seconds < 30)
    {
        seconds = 30;
    }
    if (seconds > 86400)
    {
        seconds = 86400;
    }
    s_interval_s = seconds;
    store_set_interval(seconds);
    esp_timer_stop(s_timer);
    esp_timer_start_periodic(s_timer, (uint64_t)seconds * 1000000ULL);
    return ESP_OK;
}

void watcher_status(watcher_status_t *out)
{
    out->authenticated = google_auth_is_authenticated();
    out->poll_interval_s = s_interval_s;
    out->link_count = store_link_count();
}
