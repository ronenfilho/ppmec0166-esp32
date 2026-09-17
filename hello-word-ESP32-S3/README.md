# Hello World — ESP32-S3

- **Autor:** Ronen Rodrigues Silva Filho
- **Instituição:** Universidade de Brasília (UnB) — Pós-graduação, PPMEC
- **Disciplina:** PPMEC0166 - Tópicos Avançados em Sistemas Mecatrônicos 3 (Princípios de Internet das Coisas)
- **Professor:** Jones Yudi Mori Alves da Silva

Bring-up de mais uma placa nova no workspace: [ESP32-S3 com kit adaptador de expansão](https://pt.aliexpress.com/item/1005007319706057.html)
(módulo N16R8, 44 pinos, USB-C). Xtensa dual-core, mesma família de
toolchain do ESP32-WROOM-32, só muda o target (`esp32s3`).

## Hardware

- Placa: ESP32-S3-1 N16R8 (16MB flash, 8MB PSRAM), kit adaptador 44 pinos
- USB-CDC nativo — aparece como `/dev/cu.usbmodemXXXX` (o número muda a
  cada reset/entrada em bootloader, sem CH340/CP2102)
- **Sem circuito de auto-reset/auto-download:** essa placa não entra em
  modo bootloader sozinha via DTR/RTS. Pra flashar, precisa colocar
  manualmente: segurar **BOOT** (`IO0`), apertar e soltar **RESET**
  (`EN`), soltar **BOOT**. Depois de flashar, apertar **RESET** de novo
  (sozinho, sem BOOT) pra rodar o firmware normalmente.

## Implementação

[main.c](main/main.c) — hello world padrão do ESP-IDF, mais
`esp_chip_info()` pra confirmar em runtime modelo/núcleos/revisão do
chip.

## Resultado

Log de boot via monitor serial:

```
I (163) app_init: Project name:     hello-word-ESP32-S3
I (182) app_init: ESP-IDF:          v5.5.1
Hello world do ESP32-S3!
Chip: modelo 9, 2 nucleo(s), revisao v0.2
ESP-IDF: v5.5.1
Hello from app_main!
```

(`modelo 9` = `CHIP_ESP32S3` em `esp_chip_info_t`.)

## Build, flash e monitor

Em terminal novo, se `idf.py` não for reconhecido:

```
source ~/esp/esp-idf/export.sh
```

Depois:

```
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/cu.usbmodemXXXX flash monitor
```

Sair do monitor: `Ctrl + ]`
