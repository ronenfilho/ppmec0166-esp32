#include "sx1276.h"

#include <string.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "SX1276";

/* Registradores (datasheet SX1276/77/78/79, Semtech, seção 6.4) */
#define REG_FIFO               0x00
#define REG_OP_MODE            0x01
#define REG_FRF_MSB            0x06
#define REG_FRF_MID            0x07
#define REG_FRF_LSB            0x08
#define REG_PA_CONFIG          0x09
#define REG_OCP                0x0B
#define REG_LNA                0x0C
#define REG_FIFO_ADDR_PTR      0x0D
#define REG_FIFO_TX_BASE       0x0E
#define REG_FIFO_RX_BASE       0x0F
#define REG_FIFO_RX_CURRENT    0x10
#define REG_IRQ_FLAGS          0x12
#define REG_RX_NB_BYTES        0x13
#define REG_PKT_SNR            0x19
#define REG_PKT_RSSI           0x1A
#define REG_MODEM_CONFIG_1     0x1D
#define REG_MODEM_CONFIG_2     0x1E
#define REG_PREAMBLE_MSB       0x20
#define REG_PREAMBLE_LSB       0x21
#define REG_PAYLOAD_LENGTH     0x22
#define REG_MODEM_CONFIG_3     0x26
#define REG_SYNC_WORD          0x39
#define REG_VERSION            0x42
#define REG_PA_DAC             0x4D

#define MODO_LORA              0x80
#define MODO_SLEEP             0x00
#define MODO_STDBY             0x01
#define MODO_TX                0x03
#define MODO_RX_CONTINUO       0x05

#define IRQ_TX_DONE            0x08
#define IRQ_CRC_ERRO           0x20
#define IRQ_RX_DONE            0x40

static spi_device_handle_t s_spi;

static uint8_t ler_reg(uint8_t reg)
{
    spi_transaction_t t = {
        .flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA,
        .length = 16,
        .tx_data = {reg & 0x7F, 0x00},
    };
    spi_device_polling_transmit(s_spi, &t);
    return t.rx_data[1];
}

static void escrever_reg(uint8_t reg, uint8_t valor)
{
    spi_transaction_t t = {
        .flags = SPI_TRANS_USE_TXDATA,
        .length = 16,
        .tx_data = {reg | 0x80, valor},
    };
    spi_device_polling_transmit(s_spi, &t);
}

static void modo(uint8_t m)
{
    escrever_reg(REG_OP_MODE, MODO_LORA | m);
}

esp_err_t sx1276_spi_init(int sck, int miso, int mosi, int cs, uint8_t *versao)
{
    spi_bus_config_t bus = {
        .mosi_io_num = mosi,
        .miso_io_num = miso,
        .sclk_io_num = sck,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_DISABLED));

    spi_device_interface_config_t dev = {
        .clock_speed_hz = 4 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = cs,
        .queue_size = 1,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &dev, &s_spi));

    *versao = ler_reg(REG_VERSION);
    return (*versao == 0x12) ? ESP_OK : ESP_ERR_NOT_FOUND;
}

uint8_t sx1276_versao(void)
{
    return ler_reg(REG_VERSION);
}

void sx1276_reset(int rst)
{
    if (rst < 0) {
        return;
    }
    gpio_reset_pin(rst);
    gpio_set_direction(rst, GPIO_MODE_OUTPUT);
    gpio_set_level(rst, 0);
    vTaskDelay(pdMS_TO_TICKS(2));
    gpio_set_level(rst, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
}

esp_err_t sx1276_configurar(const sx1276_config_t *cfg)
{
    /* O bit de modo LoRa só pode ser alterado em SLEEP */
    escrever_reg(REG_OP_MODE, MODO_SLEEP);
    modo(MODO_SLEEP);
    vTaskDelay(pdMS_TO_TICKS(10));
    if (ler_reg(REG_OP_MODE) != (MODO_LORA | MODO_SLEEP)) {
        ESP_LOGE(TAG, "nao entrou em modo LoRa (RegOpMode=0x%02x)", ler_reg(REG_OP_MODE));
        return ESP_FAIL;
    }

    /* Frf = f * 2^19 / 32 MHz */
    uint64_t frf = ((uint64_t)cfg->freq_khz * 1000ULL << 19) / 32000000ULL;
    escrever_reg(REG_FRF_MSB, (uint8_t)(frf >> 16));
    escrever_reg(REG_FRF_MID, (uint8_t)(frf >> 8));
    escrever_reg(REG_FRF_LSB, (uint8_t)frf);

    escrever_reg(REG_FIFO_TX_BASE, 0x00);
    escrever_reg(REG_FIFO_RX_BASE, 0x00);
    escrever_reg(REG_LNA, ler_reg(REG_LNA) | 0x03);             /* boost do LNA em HF */

    /* LoRa32: antena ligada ao PA_BOOST. Pout = 2 + OutputPower (dBm), até 17 dBm. */
    int p = cfg->tx_power < 2 ? 2 : (cfg->tx_power > 17 ? 17 : cfg->tx_power);
    escrever_reg(REG_PA_DAC, 0x84);                              /* sem o modo de +20 dBm */
    escrever_reg(REG_OCP, 0x20 | 0x0B);                          /* limite de corrente: 100 mA */
    escrever_reg(REG_PA_CONFIG, 0x80 | (p - 2));

    escrever_reg(REG_MODEM_CONFIG_1, 0x70 | 0x02);               /* BW 125 kHz, CR 4/5, cabeçalho explícito */
    escrever_reg(REG_MODEM_CONFIG_2, (uint8_t)((cfg->sf << 4) | 0x04)); /* SF, CRC ligado */
    /* AGC automático; LowDataRateOptimize obrigatório quando o símbolo passa de 16 ms (SF11/12 a 125 kHz) */
    escrever_reg(REG_MODEM_CONFIG_3, 0x04 | (cfg->sf >= 11 ? 0x08 : 0x00));
    escrever_reg(REG_PREAMBLE_MSB, 0x00);
    escrever_reg(REG_PREAMBLE_LSB, 0x08);
    escrever_reg(REG_SYNC_WORD, 0x12);                           /* rede privada (0x34 = LoRaWAN público) */

    modo(MODO_STDBY);
    ESP_LOGI(TAG, "LoRa: %d.%03d MHz, SF%d, BW 125 kHz, CR 4/5, %d dBm",
             cfg->freq_khz / 1000, cfg->freq_khz % 1000, cfg->sf, p);
    return ESP_OK;
}

esp_err_t sx1276_enviar(const uint8_t *dados, size_t len, int timeout_ms, float *airtime_ms)
{
    if (len == 0 || len > 255) {
        return ESP_ERR_INVALID_ARG;
    }
    modo(MODO_STDBY);
    escrever_reg(REG_FIFO_ADDR_PTR, 0x00);
    for (size_t i = 0; i < len; i++) {
        escrever_reg(REG_FIFO, dados[i]);
    }
    escrever_reg(REG_PAYLOAD_LENGTH, (uint8_t)len);
    escrever_reg(REG_IRQ_FLAGS, 0xFF);                           /* limpa as flags */

    int64_t t0 = esp_timer_get_time();
    modo(MODO_TX);
    while (!(ler_reg(REG_IRQ_FLAGS) & IRQ_TX_DONE)) {
        if ((esp_timer_get_time() - t0) / 1000 > timeout_ms) {
            modo(MODO_STDBY);
            return ESP_ERR_TIMEOUT;
        }
        vTaskDelay(1);
    }
    *airtime_ms = (esp_timer_get_time() - t0) / 1000.0f;
    escrever_reg(REG_IRQ_FLAGS, IRQ_TX_DONE);
    return ESP_OK;                                               /* o rádio volta sozinho a STANDBY */
}

void sx1276_receber(void)
{
    escrever_reg(REG_IRQ_FLAGS, 0xFF);
    escrever_reg(REG_FIFO_ADDR_PTR, 0x00);
    modo(MODO_RX_CONTINUO);
}

bool sx1276_ler_pacote(uint8_t *buf, size_t max, sx1276_rx_info_t *info)
{
    uint8_t irq = ler_reg(REG_IRQ_FLAGS);
    if (!(irq & IRQ_RX_DONE)) {
        return false;
    }
    escrever_reg(REG_IRQ_FLAGS, 0xFF);

    size_t n = ler_reg(REG_RX_NB_BYTES);
    escrever_reg(REG_FIFO_ADDR_PTR, ler_reg(REG_FIFO_RX_CURRENT));
    info->len = n < max ? n : max;
    for (size_t i = 0; i < n; i++) {
        uint8_t b = ler_reg(REG_FIFO);
        if (i < max) {
            buf[i] = b;
        }
    }
    info->crc_ok = !(irq & IRQ_CRC_ERRO);
    info->snr_db = ((int8_t)ler_reg(REG_PKT_SNR)) / 4.0f;
    info->rssi_dbm = -157 + ler_reg(REG_PKT_RSSI);               /* porta de alta frequência (> 862 MHz) */
    return true;
}
