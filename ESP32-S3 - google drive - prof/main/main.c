#include <stdio.h>
#include "connect.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_http_server.h"

#include "esp_http_client.h"
#include "esp_crt_bundle.h"
static const char *TAG1 = "GOOGLE_DRIVE";

#define DRIVE_URL "https://drive.usercontent.google.com/download?id=1lKI0hCynH8Wl-JiwmeW4k7ggptxUehTp&export=download&confirm=t"


static const char *TAG = "SERVER";

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id)
    {
        case HTTP_EVENT_ON_DATA:
            if (evt->data_len > 0)
            {
                fwrite(evt->data, 1, evt->data_len, stdout);
                fflush(stdout);
            }
            break;

        default:
            break;
    }

    return ESP_OK;
}

void download_google_drive_file(void)
{
    esp_http_client_config_t config = {
        .url = DRIVE_URL,
        .event_handler = http_event_handler,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 15000,
        .disable_auto_redirect = false,
        .max_redirection_count = 5
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    if (client == NULL)
    {
        ESP_LOGE(TAG1, "Erro criando cliente HTTP");
        return;
    }

    printf("\n\n");
    printf("=====================================\n");
    printf("BAIXANDO ARQUIVO DO GOOGLE DRIVE\n");
    printf("=====================================\n\n");

    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK)
    {
        int status = esp_http_client_get_status_code(client);
        int64_t tamanho = esp_http_client_get_content_length(client);

        printf("\n\n");
        printf("=====================================\n");
        printf("DOWNLOAD FINALIZADO\n");
        printf("HTTP Status: %d\n", status);
        printf("Content-Length: %lld\n", (long long)tamanho);
        printf("=====================================\n");
    }
    else
    {
        ESP_LOGE(TAG1, "Erro HTTP: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
}


static esp_err_t on_default_url(httpd_req_t *req)
{
    ESP_LOGI(TAG,"URL: %s",req->uri);
    httpd_resp_sendstr(req,"Ola, eu sou um ESP32!");
    return ESP_OK;
}

static void init_server()
{
  httpd_handle_t server = NULL;
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();

  ESP_ERROR_CHECK(httpd_start(&server, &config));

  httpd_uri_t default_url = {
    .uri ="/",
    .method = HTTP_GET,
    .handler = on_default_url
  };
  httpd_register_uri_handler(server,&default_url);

}

void app_main(void)
{
  ESP_ERROR_CHECK(nvs_flash_init());
  wifi_init();
  ESP_ERROR_CHECK(wifi_connect_sta("labSHP", "labSHP2025", 10000));

  init_server();
  
  download_google_drive_file();
  
}