# Hello World — ESP32-C3 SuperMini

- **Autor:** Ronen Rodrigues Silva Filho
- **Instituição:** Universidade de Brasília (UnB) — Pós-graduação, PPMEC
- **Disciplina:** PPMEC0166 - Tópicos Avançados em Sistemas Mecatrônicos 3 (Princípios de Internet das Coisas)
- **Professor:** Jones Yudi Mori Alves da Silva

Bring-up de uma placa nova no workspace: [ESP32-C3 SuperMini](https://pt.aliexpress.com/item/1005009925646929.html)
(RISC-V, USB nativo). Diferente do ESP32-WROOM-32 usado nos outros
projetos, este chip exige um toolchain próprio (`riscv32-esp-elf`) e um
target diferente no build (`esp32c3`).

## Hardware

- Placa: ESP32-C3 SuperMini
- Chip: ESP32-C3 (RISC-V single-core), USB-CDC nativo — aparece direto
  como `/dev/cu.usbmodem101`, sem chip USB-serial externo

## Configuração do ambiente

Primeira vez usando um chip RISC-V neste workspace — o toolchain não
vem junto da instalação padrão (que só tinha o Xtensa, do ESP32
clássico). Precisa instalar uma vez:

```
~/esp/esp-idf/install.sh esp32c3
```

## Implementação

[main.c](main/main.c) — hello world padrão do ESP-IDF, mais
`esp_chip_info()` pra imprimir modelo/núcleos/revisão do chip lido em
tempo de execução (confirma que o binário está rodando na placa
certa).

## Resultado

Log de boot via monitor serial:

```
I (154) app_init: Project name:     hello-word-ESP32-C3
I (173) app_init: ESP-IDF:          v5.5.1
Hello world do ESP32-C3 SuperMini!
Chip: modelo 5, 1 nucleo(s), revisao v0.4
ESP-IDF: v5.5.1
Hello from app_main!
```

(`modelo 5` = `CHIP_ESP32C3` em `esp_chip_info_t`.)

## Troubleshooting

**`esptool` reclama de "This chip is ESP32-C3, not ESP32. Wrong --chip
argument?"** — o projeto foi reconfigurado de volta pro target `esp32`
(conflito conhecido entre as duas instalações do ESP-IDF neste
workspace, ver [Ambiente](../README.md#ambiente); geralmente acontece
ao abrir/buildar pelo Espressif-IDE depois de já ter rodado
`set-target` pela CLI, ou vice-versa). Corrige com:

```
idf.py set-target esp32c3
idf.py build
```

## Build, flash e monitor

Em terminal novo, se `idf.py` não for reconhecido:

```
source ~/esp/esp-idf/export.sh
```

Depois:

```
idf.py set-target esp32c3
idf.py build
idf.py -p /dev/cu.usbmodem101 flash monitor
```

Sair do monitor: `Ctrl + ]`
