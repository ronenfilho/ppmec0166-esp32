#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_mac.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sx1276.h"

static const char *TAG = "LORA";

void app_main(void)
{
    /* Reset antes de qualquer acesso: com o pino de reset sem nível definido o rádio pode não responder. */
    sx1276_reset(CONFIG_LORA_PIN_RST);

    uint8_t versao = 0;
    if (sx1276_spi_init(CONFIG_LORA_PIN_SCK, CONFIG_LORA_PIN_MISO, CONFIG_LORA_PIN_MOSI, CONFIG_LORA_PIN_CS,
                        &versao) != ESP_OK) {
        ESP_LOGE(TAG, "radio nao respondeu (versao=0x%02x, esperado 0x12). Confira os pinos no menuconfig.", versao);
        return;
    }
    ESP_LOGI(TAG, "SX1276 encontrado (versao 0x%02x)", versao);

    sx1276_config_t cfg = {
        .freq_khz = CONFIG_LORA_FREQ_KHZ,
        .sf = CONFIG_LORA_SF,
        .tx_power = CONFIG_LORA_TX_POWER_DBM,
    };
    ESP_ERROR_CHECK(sx1276_configurar(&cfg));

    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char id[8];
    snprintf(id, sizeof(id), "%02X%02X", mac[4], mac[5]);
    ESP_LOGI(TAG, "no %s pronto: envia a cada ~%d ms e escuta no intervalo", id, CONFIG_LORA_INTERVALO_MS);

    uint32_t contador = 0;
    while (1) {
        char msg[48];
        int n = snprintf(msg, sizeof(msg), "hello #%lu de %s", (unsigned long)contador++, id);
        float airtime = 0;
        esp_err_t err = sx1276_enviar((const uint8_t *)msg, n, 3000, &airtime);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "enviado  \"%s\" (%d B, TxDone em %.1f ms)", msg, n, airtime);
        } else {
            ESP_LOGE(TAG, "falha no envio: %s", esp_err_to_name(err));
        }

        /* Escuta até o próximo envio (intervalo + até 1 s aleatório, para duas placas não colidirem sempre) */
        sx1276_receber();
        int64_t fim = esp_timer_get_time() + (CONFIG_LORA_INTERVALO_MS + esp_random() % 1000) * 1000LL;
        while (esp_timer_get_time() < fim) {
            uint8_t buf[256];
            sx1276_rx_info_t info;
            if (sx1276_ler_pacote(buf, sizeof(buf) - 1, &info)) {
                buf[info.len] = '\0';
                if (info.crc_ok) {
                    ESP_LOGI(TAG, "recebido \"%s\" (%u B, RSSI %d dBm, SNR %.1f dB)", (char *)buf,
                             (unsigned)info.len, info.rssi_dbm, info.snr_db);
                } else {
                    ESP_LOGW(TAG, "pacote com CRC invalido (%u B, RSSI %d dBm)", (unsigned)info.len, info.rssi_dbm);
                }
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}
