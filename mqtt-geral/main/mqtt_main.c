#include <stdbool.h>
#include <string.h>

#include "mqtt_main.h"
#include "toogleLed.h"
#include "secrets.h"
#include "mqtt_client.h"
#include "esp_log.h"

static const char *TAG = "MQTT";

/* Topicos namespaced pelo aluno, para nao colidir com outros alunos
   no mesmo broker compartilhado. */
#define TOPIC_SET    "esp32/ronen/led/set"
#define TOPIC_STATUS "esp32/ronen/led/status"

static void publish_status(esp_mqtt_client_handle_t client)
{
    bool on = toogle_led_get_state();
    /* QoS 1, retain=true: quem assinar depois ja recebe o ultimo estado. */
    esp_mqtt_client_publish(client, TOPIC_STATUS, on ? "on" : "off", 0, 1, true);
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                                int32_t event_id, void *event_data)
{
    (void)handler_args;
    (void)base;

    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;

    switch (event_id)
    {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Conectado ao broker");
            esp_mqtt_client_subscribe(client, TOPIC_SET, 1);
            publish_status(client);
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "Desconectado do broker");
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "Inscrito em %s", TOPIC_SET);
            break;

        case MQTT_EVENT_DATA:
        {
            char topic[128] = {0};
            char payload[32] = {0};
            int topic_len = event->topic_len < (int)sizeof(topic) - 1
                                 ? event->topic_len
                                 : (int)sizeof(topic) - 1;
            int data_len = event->data_len < (int)sizeof(payload) - 1
                                ? event->data_len
                                : (int)sizeof(payload) - 1;
            memcpy(topic, event->topic, topic_len);
            memcpy(payload, event->data, data_len);

            ESP_LOGI(TAG, "Mensagem em '%s': '%s'", topic, payload);

            if (strncmp(topic, TOPIC_SET, sizeof(TOPIC_SET) - 1) == 0)
            {
                if (strcmp(payload, "on") == 0)
                {
                    toogle_led_set(true);
                    publish_status(client);
                }
                else if (strcmp(payload, "off") == 0)
                {
                    toogle_led_set(false);
                    publish_status(client);
                }
                else
                {
                    ESP_LOGW(TAG, "Payload invalido (esperado \"on\" ou \"off\")");
                }
            }
            break;
        }

        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "Erro no cliente MQTT");
            break;

        default:
            break;
    }
}

void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
        .credentials.username = MQTT_USERNAME,
        .credentials.authentication.password = MQTT_PASSWORD,
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}
