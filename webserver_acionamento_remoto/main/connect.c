#include <stdio.h>
#include <string.h>

#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "connect.h"

static const char *TAG = "WIFI";

static esp_netif_t *esp_netif = NULL;
static EventGroupHandle_t wifi_events = NULL;

static const EventBits_t CONNECTED_GOT_IP = BIT0;
static const EventBits_t DISCONNECTED     = BIT1;


/*
 * Converte o código de motivo de desconexão Wi-Fi em texto.
 * Lista compatível com ESP-IDF 6.0.x.
 */
static const char *get_error(uint8_t code)
{
    switch (code)
    {
        case WIFI_REASON_UNSPECIFIED:
            return "WIFI_REASON_UNSPECIFIED";

        case WIFI_REASON_AUTH_EXPIRE:
            return "WIFI_REASON_AUTH_EXPIRE";

        case WIFI_REASON_AUTH_LEAVE:
            return "WIFI_REASON_AUTH_LEAVE";

        case WIFI_REASON_DISASSOC_DUE_TO_INACTIVITY:
            return "WIFI_REASON_DISASSOC_DUE_TO_INACTIVITY";

        case WIFI_REASON_ASSOC_TOOMANY:
            return "WIFI_REASON_ASSOC_TOOMANY";

        case WIFI_REASON_CLASS2_FRAME_FROM_NONAUTH_STA:
            return "WIFI_REASON_CLASS2_FRAME_FROM_NONAUTH_STA";

        case WIFI_REASON_CLASS3_FRAME_FROM_NONASSOC_STA:
            return "WIFI_REASON_CLASS3_FRAME_FROM_NONASSOC_STA";

        case WIFI_REASON_ASSOC_LEAVE:
            return "WIFI_REASON_ASSOC_LEAVE";

        case WIFI_REASON_ASSOC_NOT_AUTHED:
            return "WIFI_REASON_ASSOC_NOT_AUTHED";

        case WIFI_REASON_DISASSOC_PWRCAP_BAD:
            return "WIFI_REASON_DISASSOC_PWRCAP_BAD";

        case WIFI_REASON_DISASSOC_SUPCHAN_BAD:
            return "WIFI_REASON_DISASSOC_SUPCHAN_BAD";

        case WIFI_REASON_IE_INVALID:
            return "WIFI_REASON_IE_INVALID";

        case WIFI_REASON_MIC_FAILURE:
            return "WIFI_REASON_MIC_FAILURE";

        case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
            return "WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT";

        case WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT:
            return "WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT";

        case WIFI_REASON_IE_IN_4WAY_DIFFERS:
            return "WIFI_REASON_IE_IN_4WAY_DIFFERS";

        case WIFI_REASON_GROUP_CIPHER_INVALID:
            return "WIFI_REASON_GROUP_CIPHER_INVALID";

        case WIFI_REASON_PAIRWISE_CIPHER_INVALID:
            return "WIFI_REASON_PAIRWISE_CIPHER_INVALID";

        case WIFI_REASON_AKMP_INVALID:
            return "WIFI_REASON_AKMP_INVALID";

        case WIFI_REASON_UNSUPP_RSN_IE_VERSION:
            return "WIFI_REASON_UNSUPP_RSN_IE_VERSION";

        case WIFI_REASON_INVALID_RSN_IE_CAP:
            return "WIFI_REASON_INVALID_RSN_IE_CAP";

        case WIFI_REASON_802_1X_AUTH_FAILED:
            return "WIFI_REASON_802_1X_AUTH_FAILED";

        case WIFI_REASON_CIPHER_SUITE_REJECTED:
            return "WIFI_REASON_CIPHER_SUITE_REJECTED";

        case WIFI_REASON_INVALID_PMKID:
            return "WIFI_REASON_INVALID_PMKID";

        case WIFI_REASON_BEACON_TIMEOUT:
            return "WIFI_REASON_BEACON_TIMEOUT";

        case WIFI_REASON_NO_AP_FOUND:
            return "WIFI_REASON_NO_AP_FOUND";

        case WIFI_REASON_AUTH_FAIL:
            return "WIFI_REASON_AUTH_FAIL";

        case WIFI_REASON_ASSOC_FAIL:
            return "WIFI_REASON_ASSOC_FAIL";

        case WIFI_REASON_HANDSHAKE_TIMEOUT:
            return "WIFI_REASON_HANDSHAKE_TIMEOUT";

        case WIFI_REASON_CONNECTION_FAIL:
            return "WIFI_REASON_CONNECTION_FAIL";

        case WIFI_REASON_AP_TSF_RESET:
            return "WIFI_REASON_AP_TSF_RESET";

        case WIFI_REASON_ROAMING:
            return "WIFI_REASON_ROAMING";

        default:
            return "WIFI_REASON_UNKNOWN";
    }
}


/*
 * Handler único para eventos Wi-Fi e IP.
 *
 * No ESP-IDF atual, event_id deve ser interpretado juntamente com event_base.
 */
static void event_handler(void *event_handler_arg,
                          esp_event_base_t event_base,
                          int32_t event_id,
                          void *event_data)
{
    (void)event_handler_arg;

    if (event_base == WIFI_EVENT)
    {
        switch (event_id)
        {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "connecting...");
                ESP_ERROR_CHECK(esp_wifi_connect());
                break;

            case WIFI_EVENT_STA_CONNECTED:
                ESP_LOGI(TAG, "connected");
                break;

            case WIFI_EVENT_STA_DISCONNECTED:
            {
                wifi_event_sta_disconnected_t *event =
                    (wifi_event_sta_disconnected_t *)event_data;

                if (wifi_events != NULL)
                {
                    xEventGroupClearBits(wifi_events, CONNECTED_GOT_IP);
                }

                /*
                 * Mantém o comportamento do código original:
                 * WIFI_REASON_ASSOC_LEAVE é tratado como desconexão solicitada.
                 */
                if (event->reason == WIFI_REASON_ASSOC_LEAVE)
                {
                    ESP_LOGI(TAG, "disconnected");
                    if (wifi_events != NULL)
                    {
                        xEventGroupSetBits(wifi_events, DISCONNECTED);
                    }
                    break;
                }

                ESP_LOGE(TAG,
                         "disconnected: reason=%u (%s)",
                         (unsigned int)event->reason,
                         get_error(event->reason));

                /*
                 * Tenta reconectar automaticamente quando a queda não foi
                 * classificada como WIFI_REASON_ASSOC_LEAVE.
                 */
                esp_err_t err = esp_wifi_connect();
                if (err != ESP_OK)
                {
                    ESP_LOGE(TAG,
                             "esp_wifi_connect() failed: %s",
                             esp_err_to_name(err));
                }
                break;
            }

            case WIFI_EVENT_AP_START:
                ESP_LOGI(TAG, "AP started");
                break;

            case WIFI_EVENT_AP_STOP:
                ESP_LOGI(TAG, "AP stopped");
                break;

            default:
                break;
        }
    }
    else if ((event_base == IP_EVENT) && (event_id == IP_EVENT_STA_GOT_IP))
    {
        ESP_LOGI(TAG, "GOT IP");

        if (wifi_events != NULL)
        {
            xEventGroupClearBits(wifi_events, DISCONNECTED);
            xEventGroupSetBits(wifi_events, CONNECTED_GOT_IP);
        }
    }
}


void wifi_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config));

    ESP_ERROR_CHECK(
        esp_event_handler_register(
            WIFI_EVENT,
            ESP_EVENT_ANY_ID,
            &event_handler,
            NULL));

    ESP_ERROR_CHECK(
        esp_event_handler_register(
            IP_EVENT,
            IP_EVENT_STA_GOT_IP,
            &event_handler,
            NULL));

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
}


esp_err_t wifi_connect_sta(const char *ssid, const char *pass, int timeout)
{
    if ((ssid == NULL) || (pass == NULL) || (timeout <= 0))
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (wifi_events == NULL)
    {
        wifi_events = xEventGroupCreate();

        if (wifi_events == NULL)
        {
            return ESP_ERR_NO_MEM;
        }
    }
    else
    {
        xEventGroupClearBits(wifi_events, CONNECTED_GOT_IP | DISCONNECTED);
    }

    if (esp_netif == NULL)
    {
        esp_netif = esp_netif_create_default_wifi_sta();

        if (esp_netif == NULL)
        {
            return ESP_ERR_NO_MEM;
        }
    }

    wifi_config_t wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_config));

    strncpy((char *)wifi_config.sta.ssid,
            ssid,
            sizeof(wifi_config.sta.ssid) - 1);

    strncpy((char *)wifi_config.sta.password,
            pass,
            sizeof(wifi_config.sta.password) - 1);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    /*
     * ESP-IDF 6:
     * ESP_IF_WIFI_STA foi removido.
     * O identificador atual é WIFI_IF_STA.
     */
    ESP_ERROR_CHECK(
        esp_wifi_set_config(
            WIFI_IF_STA,
            &wifi_config));

    ESP_ERROR_CHECK(esp_wifi_start());

    /*
     * Desliga o modo de economia de energia do Wi-Fi. Com power-save
     * ligado (padrao), o ESP32 pode perder pacotes do handshake WPA2
     * especificamente com o Hotspot Pessoal do iPhone, travando em
     * "assoc -> run" sem nunca completar a conexao.
     */
    esp_wifi_set_ps(WIFI_PS_NONE);

    EventBits_t result =
        xEventGroupWaitBits(
            wifi_events,
            CONNECTED_GOT_IP | DISCONNECTED,
            pdTRUE,
            pdFALSE,
            pdMS_TO_TICKS(timeout));

    if ((result & CONNECTED_GOT_IP) != 0)
    {
        return ESP_OK;
    }

    return ESP_FAIL;
}


void wifi_connect_ap(const char *ssid, const char *pass)
{
    if ((ssid == NULL) || (pass == NULL))
    {
        ESP_LOGE(TAG, "Invalid AP SSID/password");
        return;
    }

    if (esp_netif == NULL)
    {
        esp_netif = esp_netif_create_default_wifi_ap();

        if (esp_netif == NULL)
        {
            ESP_LOGE(TAG, "Could not create default Wi-Fi AP netif");
            return;
        }
    }

    wifi_config_t wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_config));

    strncpy((char *)wifi_config.ap.ssid,
            ssid,
            sizeof(wifi_config.ap.ssid) - 1);

    strncpy((char *)wifi_config.ap.password,
            pass,
            sizeof(wifi_config.ap.password) - 1);

    wifi_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    wifi_config.ap.max_connection = 4;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));

    /*
     * ESP-IDF 6:
     * ESP_IF_WIFI_AP foi removido.
     * O identificador atual é WIFI_IF_AP.
     */
    ESP_ERROR_CHECK(
        esp_wifi_set_config(
            WIFI_IF_AP,
            &wifi_config));

    ESP_ERROR_CHECK(esp_wifi_start());
}


void wifi_disconnect(void)
{
    esp_err_t err;

    err = esp_wifi_disconnect();
    if ((err != ESP_OK) &&
        (err != ESP_ERR_WIFI_NOT_STARTED) &&
        (err != ESP_ERR_WIFI_NOT_INIT))
    {
        ESP_LOGW(TAG,
                 "esp_wifi_disconnect(): %s",
                 esp_err_to_name(err));
    }

    err = esp_wifi_stop();
    if ((err != ESP_OK) &&
        (err != ESP_ERR_WIFI_NOT_INIT))
    {
        ESP_LOGW(TAG,
                 "esp_wifi_stop(): %s",
                 esp_err_to_name(err));
    }
}
