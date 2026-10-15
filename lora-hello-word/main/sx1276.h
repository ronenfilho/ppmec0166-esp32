#ifndef SX1276_H
#define SX1276_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Driver mínimo do rádio SX1276/77/78 em modo LoRa, por SPI (sem biblioteca externa).
   Leitura do estado do rádio por consulta aos registradores (sem interrupção no DIO0). */

typedef struct {
    int freq_khz;    /* ex: 915000 */
    int sf;          /* spreading factor 7..12 */
    int tx_power;    /* dBm no PA_BOOST, 2..17 */
} sx1276_config_t;

typedef struct {
    size_t len;
    int rssi_dbm;
    float snr_db;
    bool crc_ok;
} sx1276_rx_info_t;

/* Inicializa o barramento SPI e lê o registrador de versão (0x12 = SX1276). */
esp_err_t sx1276_spi_init(int sck, int miso, int mosi, int cs, uint8_t *versao);

/* Lê o registrador de versão (0x42). */
uint8_t sx1276_versao(void);

/* Pulsa o pino de reset do rádio (ativo em nível baixo). rst < 0: não usa reset. */
void sx1276_reset(int rst);

/* Configura modo LoRa: frequência, SF, banda de 125 kHz, CR 4/5, CRC, preâmbulo 8, sync word 0x12. */
esp_err_t sx1276_configurar(const sx1276_config_t *cfg);

/* Transmite e espera o TxDone. Devolve o tempo no ar medido (ms) em *airtime_ms. */
esp_err_t sx1276_enviar(const uint8_t *dados, size_t len, int timeout_ms, float *airtime_ms);

/* Entra em recepção contínua. */
void sx1276_receber(void);

/* Se chegou pacote (RxDone), copia para buf e preenche info. Devolve true se havia pacote. */
bool sx1276_ler_pacote(uint8_t *buf, size_t max, sx1276_rx_info_t *info);

#ifdef __cplusplus
}
#endif

#endif /* SX1276_H */
