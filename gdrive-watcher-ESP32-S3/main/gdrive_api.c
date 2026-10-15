#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gdrive_api.h"
#include "http_util.h"
#include "link_store.h"
#include "google_auth.h"
#include "secrets.h"
#include "esp_log.h"
#include "cJSON.h"

static const char *TAG = "GDRIVE";

#define URL_BUF_LEN  768
#define ENC_BUF_LEN  256

static void fill_item_from_json(cJSON *o, item_t *item)
{
    memset(item, 0, sizeof(*item));
    cJSON *fid = cJSON_GetObjectItem(o, "id");
    cJSON *fname = cJSON_GetObjectItem(o, "name");
    cJSON *fmime = cJSON_GetObjectItem(o, "mimeType");
    cJSON *ftime = cJSON_GetObjectItem(o, "modifiedTime");
    if (cJSON_IsString(fid))
    {
        strncpy(item->id, fid->valuestring, sizeof(item->id) - 1);
    }
    if (cJSON_IsString(fname))
    {
        strncpy(item->name, fname->valuestring, sizeof(item->name) - 1);
    }
    if (cJSON_IsString(fmime))
    {
        strncpy(item->mime, fmime->valuestring, sizeof(item->mime) - 1);
    }
    if (cJSON_IsString(ftime))
    {
        strncpy(item->modified, ftime->valuestring, sizeof(item->modified) - 1);
    }
}

static esp_err_t check_status(int status, const char *body)
{
    if (status >= 200 && status < 300)
    {
        return ESP_OK;
    }
    ESP_LOGE(TAG, "HTTP %d: %s", status, body != NULL ? body : "(sem corpo)");
    if (status == 404)
    {
        return ESP_ERR_NOT_FOUND;
    }
    if (status == 401)
    {
        return ESP_ERR_INVALID_STATE; /* chamador decide se tenta refresh */
    }
    return ESP_FAIL;
}

esp_err_t gdrive_public_get_file(const char *file_id, item_t *out)
{
    char url[URL_BUF_LEN];
    snprintf(url, sizeof(url),
             "https://www.googleapis.com/drive/v3/files/%s"
             "?fields=id,name,mimeType,modifiedTime,size"
             "&supportsAllDrives=true&key=%s",
             file_id, GDRIVE_API_KEY);

    char *body = NULL;
    size_t len = 0;
    int status = 0;
    esp_err_t err = http_get(url, NULL, &body, &len, &status);
    if (err != ESP_OK)
    {
        return err;
    }

    err = check_status(status, body);
    if (err != ESP_OK)
    {
        free(body);
        return err;
    }

    cJSON *json = cJSON_Parse(body);
    free(body);
    if (json == NULL)
    {
        return ESP_FAIL;
    }
    fill_item_from_json(json, out);
    cJSON_Delete(json);
    return ESP_OK;
}

esp_err_t gdrive_public_list_folder(const char *folder_id, item_t **out, size_t *n)
{
    *out = NULL;
    *n = 0;

    item_t *items = calloc(STORE_MAX_ITEMS_PER_LINK, sizeof(item_t));
    if (items == NULL)
    {
        return ESP_ERR_NO_MEM;
    }
    size_t count = 0;

    char q_raw[128];
    snprintf(q_raw, sizeof(q_raw), "'%s' in parents and trashed=false", folder_id);
    char q_enc[ENC_BUF_LEN];
    http_url_encode(q_raw, q_enc, sizeof(q_enc));

    char page_token[256] = {0};
    esp_err_t final_err = ESP_OK;

    do
    {
        char url[URL_BUF_LEN];
        if (page_token[0] != '\0')
        {
            char tok_enc[256];
            http_url_encode(page_token, tok_enc, sizeof(tok_enc));
            snprintf(url, sizeof(url),
                     "https://www.googleapis.com/drive/v3/files?q=%s"
                     "&fields=nextPageToken,files(id,name,mimeType,modifiedTime)"
                     "&pageSize=100&supportsAllDrives=true&includeItemsFromAllDrives=true"
                     "&pageToken=%s&key=%s",
                     q_enc, tok_enc, GDRIVE_API_KEY);
        }
        else
        {
            snprintf(url, sizeof(url),
                     "https://www.googleapis.com/drive/v3/files?q=%s"
                     "&fields=nextPageToken,files(id,name,mimeType,modifiedTime)"
                     "&pageSize=100&supportsAllDrives=true&includeItemsFromAllDrives=true"
                     "&key=%s",
                     q_enc, GDRIVE_API_KEY);
        }

        char *body = NULL;
        size_t len = 0;
        int status = 0;
        esp_err_t err = http_get(url, NULL, &body, &len, &status);
        if (err != ESP_OK)
        {
            final_err = err;
            break;
        }

        err = check_status(status, body);
        if (err != ESP_OK)
        {
            free(body);
            final_err = err;
            break;
        }

        cJSON *json = cJSON_Parse(body);
        free(body);
        if (json == NULL)
        {
            final_err = ESP_FAIL;
            break;
        }

        cJSON *files = cJSON_GetObjectItem(json, "files");
        if (cJSON_IsArray(files))
        {
            int fn = cJSON_GetArraySize(files);
            for (int i = 0; i < fn && count < STORE_MAX_ITEMS_PER_LINK; i++)
            {
                fill_item_from_json(cJSON_GetArrayItem(files, i), &items[count]);
                count++;
            }
        }

        cJSON *next = cJSON_GetObjectItem(json, "nextPageToken");
        if (cJSON_IsString(next) && count < STORE_MAX_ITEMS_PER_LINK)
        {
            strncpy(page_token, next->valuestring, sizeof(page_token) - 1);
        }
        else
        {
            page_token[0] = '\0';
        }

        cJSON_Delete(json);
    } while (page_token[0] != '\0');

    if (final_err != ESP_OK && count == 0)
    {
        free(items);
        return final_err;
    }

    *out = items;
    *n = count;
    return ESP_OK;
}

esp_err_t gdrive_list_items(source_kind_t kind, const char *id, item_t **out, size_t *n)
{
    if (kind == SOURCE_KIND_GDRIVE_FOLDER)
    {
        return gdrive_public_list_folder(id, out, n);
    }

    *out = calloc(1, sizeof(item_t));
    if (*out == NULL)
    {
        return ESP_ERR_NO_MEM;
    }
    esp_err_t err = gdrive_public_get_file(id, *out);
    if (err != ESP_OK)
    {
        free(*out);
        *out = NULL;
        *n = 0;
        return err;
    }
    *n = 1;
    return ESP_OK;
}

/* ---------- modo autenticado ---------- */

static esp_err_t authed_get(const char *url, char **out_body, int *out_status)
{
    const char *bearer = google_auth_get_bearer();
    if (bearer == NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }

    char auth_val[2100];
    snprintf(auth_val, sizeof(auth_val), "Bearer %s", bearer);
    http_header_t hdr = {"Authorization", auth_val};

    size_t len = 0;
    esp_err_t err = http_get(url, &hdr, out_body, &len, out_status);
    if (err != ESP_OK)
    {
        return err;
    }

    if (*out_status == 401)
    {
        free(*out_body);
        *out_body = NULL;
        if (google_auth_refresh() != ESP_OK)
        {
            return ESP_ERR_INVALID_STATE;
        }
        bearer = google_auth_get_bearer();
        if (bearer == NULL)
        {
            return ESP_ERR_INVALID_STATE;
        }
        snprintf(auth_val, sizeof(auth_val), "Bearer %s", bearer);
        err = http_get(url, &hdr, out_body, &len, out_status);
    }

    return err;
}

esp_err_t gdrive_auth_get_start_page_token(char *out, size_t len)
{
    char *body = NULL;
    int status = 0;
    esp_err_t err = authed_get(
        "https://www.googleapis.com/drive/v3/changes/startPageToken?fields=startPageToken",
        &body, &status);
    if (err != ESP_OK)
    {
        return err;
    }
    err = check_status(status, body);
    if (err != ESP_OK)
    {
        free(body);
        return err;
    }

    cJSON *json = cJSON_Parse(body);
    free(body);
    if (json == NULL)
    {
        return ESP_FAIL;
    }
    cJSON *tok = cJSON_GetObjectItem(json, "startPageToken");
    if (cJSON_IsString(tok))
    {
        strncpy(out, tok->valuestring, len - 1);
        out[len - 1] = '\0';
    }
    else
    {
        out[0] = '\0';
    }
    cJSON_Delete(json);
    return ESP_OK;
}

esp_err_t gdrive_auth_list_changes(const char *page_token,
                                    item_t **out, size_t *n,
                                    char *new_start_token, size_t tok_len)
{
    *out = NULL;
    *n = 0;
    new_start_token[0] = '\0';

    item_t *items = calloc(STORE_MAX_DRIVE_ITEMS, sizeof(item_t));
    if (items == NULL)
    {
        return ESP_ERR_NO_MEM;
    }
    size_t count = 0;

    char token[256];
    strncpy(token, page_token, sizeof(token) - 1);
    token[sizeof(token) - 1] = '\0';
    esp_err_t final_err = ESP_OK;

    for (;;)
    {
        char tok_enc[256];
        http_url_encode(token, tok_enc, sizeof(tok_enc));

        char url[URL_BUF_LEN];
        snprintf(url, sizeof(url),
                 "https://www.googleapis.com/drive/v3/changes?pageToken=%s"
                 "&pageSize=100&spaces=drive&includeRemoved=true&restrictToMyDrive=true"
                 "&fields=newStartPageToken,nextPageToken,"
                 "changes(fileId,removed,file(id,name,mimeType,modifiedTime,trashed))",
                 tok_enc);

        char *body = NULL;
        int status = 0;
        esp_err_t err = authed_get(url, &body, &status);
        if (err != ESP_OK)
        {
            final_err = err;
            break;
        }

        err = check_status(status, body);
        if (err == ESP_ERR_NOT_FOUND)
        {
            /* pageTokenInvalid (token velho demais) -- chamador deve
             * reobter o startPageToken e tentar de novo depois. */
            free(body);
            final_err = ESP_ERR_NOT_FOUND;
            break;
        }
        if (err != ESP_OK)
        {
            free(body);
            final_err = err;
            break;
        }

        cJSON *json = cJSON_Parse(body);
        free(body);
        if (json == NULL)
        {
            final_err = ESP_FAIL;
            break;
        }

        cJSON *changes = cJSON_GetObjectItem(json, "changes");
        if (cJSON_IsArray(changes))
        {
            int cn = cJSON_GetArraySize(changes);
            for (int i = 0; i < cn && count < STORE_MAX_DRIVE_ITEMS; i++)
            {
                cJSON *c = cJSON_GetArrayItem(changes, i);
                cJSON *removed = cJSON_GetObjectItem(c, "removed");
                cJSON *file = cJSON_GetObjectItem(c, "file");
                cJSON *fileId = cJSON_GetObjectItem(c, "fileId");

                item_t *it = &items[count];
                memset(it, 0, sizeof(*it));

                bool is_removed = cJSON_IsTrue(removed);
                if (file != NULL)
                {
                    fill_item_from_json(file, it);
                    cJSON *trashed = cJSON_GetObjectItem(file, "trashed");
                    if (cJSON_IsTrue(trashed))
                    {
                        is_removed = true;
                    }
                }
                else if (cJSON_IsString(fileId))
                {
                    strncpy(it->id, fileId->valuestring, sizeof(it->id) - 1);
                }
                it->removed = is_removed;
                count++;
            }
        }

        cJSON *next = cJSON_GetObjectItem(json, "nextPageToken");
        cJSON *newstart = cJSON_GetObjectItem(json, "newStartPageToken");
        if (cJSON_IsString(newstart))
        {
            strncpy(new_start_token, newstart->valuestring, tok_len - 1);
            new_start_token[tok_len - 1] = '\0';
        }

        bool has_next = cJSON_IsString(next);
        if (has_next)
        {
            strncpy(token, next->valuestring, sizeof(token) - 1);
        }
        cJSON_Delete(json);

        if (!has_next || count >= STORE_MAX_DRIVE_ITEMS)
        {
            break;
        }
    }

    if (final_err != ESP_OK && count == 0)
    {
        free(items);
        return final_err;
    }

    *out = items;
    *n = count;
    return ESP_OK;
}

esp_err_t gdrive_auth_list_recent(item_t **out, size_t *n)
{
    *out = NULL;
    *n = 0;

    char *body = NULL;
    int status = 0;
    esp_err_t err = authed_get(
        "https://www.googleapis.com/drive/v3/files"
        "?orderBy=modifiedTime%20desc&pageSize=25"
        "&fields=files(id,name,mimeType,modifiedTime)&q=trashed%3Dfalse",
        &body, &status);
    if (err != ESP_OK)
    {
        return err;
    }
    err = check_status(status, body);
    if (err != ESP_OK)
    {
        free(body);
        return err;
    }

    cJSON *json = cJSON_Parse(body);
    free(body);
    if (json == NULL)
    {
        return ESP_FAIL;
    }

    cJSON *files = cJSON_GetObjectItem(json, "files");
    if (!cJSON_IsArray(files))
    {
        cJSON_Delete(json);
        return ESP_OK;
    }

    int fn = cJSON_GetArraySize(files);
    item_t *items = calloc((size_t)fn, sizeof(item_t));
    if (items == NULL)
    {
        cJSON_Delete(json);
        return ESP_ERR_NO_MEM;
    }
    for (int i = 0; i < fn; i++)
    {
        fill_item_from_json(cJSON_GetArrayItem(files, i), &items[i]);
    }
    cJSON_Delete(json);

    *out = items;
    *n = (size_t)fn;
    return ESP_OK;
}
