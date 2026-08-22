# Workspace ESP32

- **Autor:** Ronen Rodrigues Silva Filho
- **Instituição:** Universidade de Brasília (UnB) — Pós-graduação, PPMEC
- **Disciplina:** PPMEC0166 - Tópicos Avançados em Sistemas Mecatrônicos 3 (Princípios de Internet das Coisas)
- **Professor:** Jones Yudi Mori Alves da Silva

## Objetivo

Workspace para os projetos ESP32 da disciplina, usando o ambiente oficial
ESP-IDF, com suporte tanto ao Espressif-IDE quanto ao VS Code.

## Ambiente

- Placa: ESP32-WROOM-32 DevKitC (30 pinos, USB-serial CP2102), porta
  `/dev/cu.usbserial-0001`
- Target: `esp32`
- ESP-IDF v5.5.1, duas instalações ativas no sistema:
  - CLI/manual: `~/esp/esp-idf`, ativado via `~/.zshrc` (`IDF_PATH`)
  - Gerenciada pela Espressif-IDE: `~/.espressif/release-v5.5/esp-idf`

  Cada projeto documenta no próprio README qual instalação usa, para
  evitar conflito de ambiente.
- Toolchain, Python (3.12.14) e dependências (`cmake`, `ninja`,
  `dfu-util`, `ccache`, `pkg-config`) instalados via Homebrew
- VS Code: extensão oficial Espressif IDF instalada

## Projetos implementados

| Projeto | Descrição | Link |
| --- | --- | --- |
| `start` | Hello World — validação inicial do ambiente | [GitHub](https://github.com/ronenfilho/ppmec0166-esp32/tree/main/start) |
| `blink` | LED onboard (GPIO2) via `driver/gpio` | [GitHub](https://github.com/ronenfilho/ppmec0166-esp32/tree/main/blink) |
| `pwm_software` | Trabalho 01 — PWM em software, duty-cycle via terminal serial | [GitHub](https://github.com/ronenfilho/ppmec0166-esp32/tree/main/pwm_software) |

## Comandos úteis

Em terminal novo, se `idf.py` não for reconhecido, ative o ambiente
primeiro (normalmente já ativado via `~/.zshrc`):

```
source ~/esp/esp-idf/export.sh
```

Depois, dentro da pasta do projeto:

```
idf.py set-target esp32
idf.py build
idf.py -p /dev/cu.usbserial-0001 flash monitor
```

Sair do monitor: `Ctrl + ]`

## Versionamento

Repositório Git, publicado em
[github.com/ronenfilho/ppmec0166-esp32](https://github.com/ronenfilho/ppmec0166-esp32).

`build/`, `managed_components/`, metadados do Eclipse (`.metadata/` e
`.settings/`) local não são versionados (ver
`.gitignore`).
