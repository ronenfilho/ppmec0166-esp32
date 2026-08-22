# Trabalho 01 - PWM em Software

**Aluno:** Ronen Rodrigues Silva Filho
**Instituição:** Universidade de Brasília (UnB) — Pós-graduação, PPMEC
**Disciplina:** PPMEC0166 - Tópicos Avançados em Sistemas Mecatrônicos 3
**Professor:** Alysson Martins Almeida Silva

Prazo de conclusão: **27/08/2026 às 23:59** (múltiplos envios permitidos)

## Enunciado

Desenvolver um gerador de PWM (Pulse-Width Modulation) em software para
acionar um LED.

- A entrada do valor de duty-cycle deve ser feita via terminal serial a
  partir de um computador conectado ao microcontrolador.
- A temporização deve ser pensada para que a frequência de acionamento do
  LED seja imperceptível ao olho humano.

## Hardware

- Placa: ESP32-WROOM-32 DevKitC (mesma do projeto [blink](../blink))
- LED: onboard, GPIO2 (ver [blink/README.md](../blink/README.md))
- ESP-IDF: instalação gerenciada pela Espressif-IDE (`release-v5.5`)

## Como funciona

"Em software" aqui significa que o duty-cycle **não** usa o periférico de
hardware PWM/LEDC do ESP32 — o LED é ligado e desligado manualmente pelo
código, com temporização própria:

- Período fixo de PWM: **1000 µs (1 kHz)**. O olho humano deixa de perceber
  cintilação (flicker) a partir de ~60–100 Hz, então 1 kHz dá boa margem.
- O duty-cycle (0–100%) é convertido em tempo ligado/desligado dentro de
  cada período.
- A temporização usa `esp_timer` (timer de interrupção de alta resolução,
  em microssegundos) reagendado a cada transição ON/OFF — em vez de um
  loop com `vTaskDelay`/busy-wait, que não teria resolução suficiente
  (tick do FreeRTOS = 1 ms) e prenderia a CPU sem ceder tempo pra tarefa
  idle (o que dispara o watchdog).
- O duty-cycle é lido do terminal serial: digite um número de **0 a 100**
  e pressione **Enter**. A tarefa de leitura roda em paralelo, então o LED
  continua piscando enquanto você digita.

## Configuração

- Target: `esp32`
- `PWM_GPIO` = 2, `PWM_PERIOD_US` = 1000 (definidos em
  [main/pwm_software_main.c](main/pwm_software_main.c))

## Build, flash e monitor

Via IDE (Espressif-IDE): Build → Flash → Monitor, porta `/dev/cu.usbserial-0001`.

Via CLI (equivalente, também funciona no VS Code com a extensão ESP-IDF):

```
idf.py set-target esp32
idf.py build
idf.py -p /dev/cu.usbserial-0001 flash monitor
```

Sair do monitor: `Ctrl + ]`

## Uso

No monitor serial, digite um valor e pressione Enter:

```
Digite o duty-cycle (0-100) e pressione Enter.
Duty atual: 50%
75
Duty-cycle ajustado para 75%
```

Valores fora do intervalo são ajustados automaticamente para 0 ou 100;
entradas não numéricas são rejeitadas com uma mensagem de erro.

## Troubleshooting

- LED não pisca / fica sempre aceso ou apagado: confira `PWM_GPIO` e se o
  valor digitado está sendo aceito no monitor (deve ecoar o número
  digitado).
- Nada acontece ao digitar: confirme que o monitor serial está enviando
  `\n` ou `\r` ao pressionar Enter (padrão do `idf.py monitor`).

## Status

- [x] Projeto criado no Espressif-IDE
- [x] Loop de PWM em software implementado
- [x] Leitura de duty-cycle via serial implementada
- [ ] Testado na placa (frequência sem cintilação visível)
