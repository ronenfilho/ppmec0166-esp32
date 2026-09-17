#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "mqtt_test.h"
#include "secrets.h"
#include "mqtt_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "MQTT_TEST";

/* Topicos do teste rapido, namespaced pelo aluno. */
#define TOPIC_ECHO      "esp32/ronen/teste/echo"      /* ESP32 assina: publique aqui pra testar o "ouvir" */
#define TOPIC_HEARTBEAT "esp32/ronen/teste/heartbeat" /* ESP32 publica: assine aqui pra testar o "publicar" */

static esp_mqtt_client_handle_t s_client = NULL;
static volatile bool s_connected = false;

static void heartbeat_task(void *arg)
{
    (void)arg;
    uint32_t count = 0;

    while (1)
    {
        if (s_connected)
        {
            char payload[48];
            snprintf(payload, sizeof(payload), "heartbeat #%lu", (unsigned long)count++);
            esp_mqtt_client_publish(s_client, TOPIC_HEARTBEAT, payload, 0, 0, false);
            ESP_LOGI(TAG, "Publicado em %s: %s", TOPIC_HEARTBEAT, payload);
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                                int32_t event_id, void *event_data)
{
    (void)handler_args;
    (void)base;

    esp_mqtt_event_handle_t event = event_data;

    switch (event_id)
    {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Conectado ao broker");
            esp_mqtt_client_subscribe(event->client, TOPIC_ECHO, 0);
            s_connected = true;
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "Desconectado do broker");
            s_connected = false;
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "Inscrito em %s", TOPIC_ECHO);
            break;

        case MQTT_EVENT_DATA:
        {
            char topic[128] = {0};
            char payload[128] = {0};
            int topic_len = event->topic_len < (int)sizeof(topic) - 1
                                 ? event->topic_len
                                 : (int)sizeof(topic) - 1;
            int data_len = event->data_len < (int)sizeof(payload) - 1
                                ? event->data_len
                                : (int)sizeof(payload) - 1;
            memcpy(topic, event->topic, topic_len);
            memcpy(payload, event->data, data_len);

            ESP_LOGI(TAG, "Recebido em '%s': '%s'", topic, payload);
            break;
        }

        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "Erro no cliente MQTT");
            break;

        default:
            break;
    }
}

void mqtt_test_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
        .credentials.username = MQTT_USERNAME,
        .credentials.authentication.password = MQTT_PASSWORD,
    };

    s_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(s_client);

    xTaskCreate(heartbeat_task, "mqtt_heartbeat", 4096, NULL, 5, NULL);
}
