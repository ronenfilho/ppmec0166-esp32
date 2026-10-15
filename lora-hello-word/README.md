# LoRa Hello World — ESP32 DevKit + shield LilyGO T-Higrow LoRa (SX1276, 915 MHz)

- **Autor:** Ronen Rodrigues Silva Filho
- **Instituição:** Universidade de Brasília (UnB) — Pós-graduação, PPMEC
- **Disciplina:** PPMEC0166 - Tópicos Avançados em Sistemas Mecatrônicos 3 (Princípios de Internet das Coisas)
- **Professor:** Jones Yudi Mori Alves da Silva

Primeiro contato com LoRa: a placa inicializa o rádio SX1276 por SPI, envia `hello #N de <id>` periodicamente e escuta entre os envios, mostrando RSSI e SNR do que receber.

## Hardware

- **ESP32 DevKit V1** (DOIT, módulo ESP-WROOM-32, ESP32 clássico).
- **Shield LilyGO T-Higrow LoRa** (placa marcada `20191223`) com o **módulo TTGO LoRa32 868/915 MHz** (rádio SX1276). Não tem display.
- **Antena sempre conectada antes de ligar**: transmitir sem antena pode danificar o estágio de potência do rádio.

| Sinal do rádio | GPIO do ESP32 |
|---|---|
| SCK | 14 |
| MISO | 13 |
| MOSI | 12 |
| CS (NSS) | 15 |
| RST | 32 |

Atenção: o shield usa os pinos do HSPI, mas com **MISO e MOSI trocados** em relação ao padrão do ESP32 (MISO 12 / MOSI 13). Com a pinagem das placas LoRa32 integradas (5/19/27/18) o rádio não responde. Os pinos são configuráveis no `menuconfig`.

## Uma placa só ou duas?

O SX1276 é **half-duplex**: transmite **ou** recebe, nunca os dois ao mesmo tempo, então **não recebe a própria mensagem**.

- **Com uma placa**, o teste confirma:
  - o rádio respondendo no SPI (registrador de versão = `0x12`);
  - a transmissão concluída pelo rádio (flag *TxDone*), com o tempo no ar medido;
  - a recepção ativa entre envios (aparece qualquer pacote LoRa de outro transmissor com a mesma configuração).
- **Com duas placas** gravadas com o mesmo firmware, cada uma mostra as mensagens da outra, com RSSI e SNR. Um sorteio de até 1 s no intervalo evita que transmitam sempre juntas.

## Configuração LoRa (menuconfig → LoRa hello world)

- 915 MHz (faixa ISM no Brasil), SF7, BW 125 kHz, CR 4/5, CRC ligado, preâmbulo de 8 símbolos, sync word `0x12` (rede privada).
- Potência de 14 dBm no PA_BOOST, intervalo de 5 s.

## Implementação

- [main/sx1276.c](main/sx1276.c)/[sx1276.h](main/sx1276.h): driver mínimo do SX1276 em modo LoRa, sem biblioteca externa.
  - Faz acesso a registrador por SPI e configura frequência (`Frf = f·2¹⁹/32 MHz`), modem e potência.
  - Transmite consultando o *TxDone* e recebe em modo contínuo, lendo o FIFO, o RSSI (`−157 + RegPktRssiValue`, porta HF) e o SNR (`RegPktSnrValue/4`).
- [main/main.c](main/main.c): reset do rádio, verificação da versão, configuração e laço de envio e escuta.

## Compilar e gravar

Descobrir a porta da placa (no macOS; a LoRa32 aparece como `/dev/cu.usbserial-*`):

```
ls /dev/cu.*
```

Para ter certeza de qual porta é a placa, rode o comando com ela desconectada e depois conectada: a porta que aparecer é a dela.

```
idf.py set-target esp32
idf.py build
idf.py -p /dev/cu.usbserial-XXXX flash monitor
```

## Monitorar o log

Só abrir a serial, sem gravar de novo (a placa reinicia e o log aparece desde o boot):

```
idf.py -p /dev/cu.usbserial-XXXX monitor
```

Para sair do monitor, use `Ctrl+]`.

## Resultado (15/out/2026)

```
I (297) LORA: SX1276 encontrado (versao 0x12)
I (307) SX1276: LoRa: 915.000 MHz, SF7, BW 125 kHz, CR 4/5, 14 dBm
I (307) LORA: no 5BDC pronto: envia a cada ~5000 ms e escuta no intervalo
I (367) LORA: enviado  "hello #0 de 5BDC" (16 B, TxDone em 57.6 ms)
I (717) LORA: recebido "Hello" (5 B, RSSI -74 dBm, SNR 9.8 dB)
I (2387) LORA: recebido "ola" (3 B, RSSI -59 dBm, SNR 9.5 dB)
I (6207) LORA: enviado  "hello #1 de 5BDC" (16 B, TxDone em 59.5 ms)
```

- Rádio identificado e transmissão confirmada pelo *TxDone* a cada envio.
- A placa recebeu pacotes de **outros transmissores** próximos com a mesma configuração (`"Hello"` e `"ola"`, RSSI de −59 a −74 dBm, SNR de 9,5 a 11,8 dB), o que valida também a recepção.
- O *TxDone* medido (~58 ms) é maior que o teórico (~46 ms, abaixo) porque o estado do rádio é consultado a cada tick do FreeRTOS (10 ms a 100 Hz). A medida tem resolução de até 10 ms.

O tempo no ar teórico de 16 B em SF7/125 kHz/CR 4/5, com preâmbulo de 8 símbolos, cabeçalho explícito e CRC, é de ~46 ms: 45,25 símbolos de 1,024 ms (Semtech AN1200.13). O valor medido aparece na serial.
