#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "http_util.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"

static const char *TAG = "HTTP";

#define HTTP_MAX_RESPONSE (16 * 1024)
#define HTTP_READ_CHUNK   1024

esp_err_t http_request(esp_http_client_method_t method,
                        const char *url,
                        const http_header_t *headers, size_t n_headers,
                        const char *body, size_t body_len,
                        char **out_body, size_t *out_len,
                        int *out_status)
{
    if (url == NULL || out_body == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    *out_body = NULL;
    if (out_len != NULL)
    {
        *out_len = 0;
    }

    esp_http_client_config_t config = {
        .url = url,
        .method = method,
        .timeout_ms = 15000,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .buffer_size = 2048,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL)
    {
        return ESP_FAIL;
    }

    for (size_t i = 0; i < n_headers; i++)
    {
        esp_http_client_set_header(client, headers[i].key, headers[i].value);
    }

    esp_err_t err = esp_http_client_open(client, (int)body_len);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "open() falhou: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return err;
    }

    if (body != NULL && body_len > 0)
    {
        int written = esp_http_client_write(client, body, (int)body_len);
        if (written < 0 || (size_t)written != body_len)
        {
            ESP_LOGE(TAG, "write() incompleto (%d/%d)", written, (int)body_len);
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            return ESP_FAIL;
        }
    }

    int content_len = esp_http_client_fetch_headers(client);
    (void)content_len; /* pode vir -1 (chunked); confiamos so no laco de leitura */

    size_t cap = HTTP_READ_CHUNK;
    size_t len = 0;
    char *buf = malloc(cap + 1);
    if (buf == NULL)
    {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_ERR_NO_MEM;
    }

    while (len < HTTP_MAX_RESPONSE)
    {
        if (len + HTTP_READ_CHUNK + 1 > cap)
        {
            size_t new_cap = cap * 2;
            char *grown = realloc(buf, new_cap + 1);
            if (grown == NULL)
            {
                free(buf);
                esp_http_client_close(client);
                esp_http_client_cleanup(client);
                return ESP_ERR_NO_MEM;
            }
            buf = grown;
            cap = new_cap;
        }

        int r = esp_http_client_read(client, buf + len, HTTP_READ_CHUNK);
        if (r < 0)
        {
            free(buf);
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            return ESP_FAIL;
        }
        if (r == 0)
        {
            break;
        }
        len += (size_t)r;
    }
    buf[len] = '\0';

    if (out_status != NULL)
    {
        *out_status = esp_http_client_get_status_code(client);
    }

    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    *out_body = buf;
    if (out_len != NULL)
    {
        *out_len = len;
    }
    return ESP_OK;
}

esp_err_t http_post_form(const char *url, const char *form_body,
                          char **out_body, size_t *out_len, int *out_status)
{
    http_header_t headers[] = {
        {"Content-Type", "application/x-www-form-urlencoded"},
    };
    return http_request(HTTP_METHOD_POST, url, headers, 1,
                         form_body, form_body ? strlen(form_body) : 0,
                         out_body, out_len, out_status);
}

esp_err_t http_get(const char *url, const http_header_t *auth_header,
                    char **out_body, size_t *out_len, int *out_status)
{
    return http_request(HTTP_METHOD_GET, url,
                         auth_header, auth_header ? 1 : 0,
                         NULL, 0, out_body, out_len, out_status);
}

static bool is_unreserved(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~';
}

size_t http_url_encode(const char *src, char *dst, size_t dst_len)
{
    static const char hex[] = "0123456789ABCDEF";
    size_t written = 0;

    for (const unsigned char *p = (const unsigned char *)src; *p != '\0'; p++)
    {
        if (is_unreserved((char)*p))
        {
            if (written + 1 >= dst_len)
            {
                break;
            }
            dst[written++] = (char)*p;
        }
        else
        {
            if (written + 3 >= dst_len)
            {
                break;
            }
            dst[written++] = '%';
            dst[written++] = hex[(*p >> 4) & 0xF];
            dst[written++] = hex[*p & 0xF];
        }
    }
    dst[written] = '\0';
    return written;
}
