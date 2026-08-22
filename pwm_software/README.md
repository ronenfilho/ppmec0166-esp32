# Trabalho 01 - PWM em Software

- **Autor:** Ronen Rodrigues Silva Filho
- **Instituição:** Universidade de Brasília (UnB) — Pós-graduação, PPMEC
- **Disciplina:** PPMEC0166 - Tópicos Avançados em Sistemas Mecatrônicos 3 (Princípios de Internet das Coisas)
- **Professor:** Jones Yudi Mori Alves da Silva
- **Data:** 22/08/2026
- **Entrega:** 27/08/2026

## Instruções

Desenvolver um gerador de PWM (Pulse-Width Modulation) em software para
acionar um LED.

A entrada do valor de duty-cycle deve ser feita via terminal serial a
partir de um computador conectado ao microcontrolador.

A temporização deve ser pensada para que a frequência de acionamento do
LED seja imperceptível ao olho humano.

## Hardware

- Placa: ESP32-WROOM-32 DevKitC (30 pinos, USB-serial CP2102)
- Target: `esp32`
- LED: onboard, GPIO2
- ESP-IDF: `release-v5.5` (gerenciado pela Espressif-IDE)

## Implementação

- Duty-cycle de 0 a 100%, com o PWM gerado inteiramente em software — sem
  uso do periférico de hardware LEDC do ESP32.
- Período fixo de 1000 µs (1 kHz), acima do limiar de percepção de
  cintilação do olho humano (aproximadamente 60–100 Hz).
- Temporização implementada com `esp_timer` (timer de interrupção de alta
  resolução, em microssegundos), reagendado a cada transição ON/OFF de
  acordo com o duty-cycle vigente. Essa abordagem evita bloquear a CPU em
  busy-wait e não depende da resolução de tick do FreeRTOS (1 ms), que é
  insuficiente para frações de um período de 1 ms.
- Leitura do duty-cycle via terminal serial: caractere a caractere, com
  eco, até a tecla Enter; o valor é limitado ao intervalo [0, 100].
- Código-fonte: [main/pwm_software_main.c](main/pwm_software_main.c).

## Build, flash e monitor

```
idf.py set-target esp32
idf.py build
idf.py -p /dev/cu.usbserial-0001 flash monitor
```

Sair do monitor: `Ctrl + ]`

## Uso

```
Digite o duty-cycle (0-100) e pressione Enter.
Duty atual: 50%
75
Duty-cycle ajustado para 75%
```

Testado na placa: o brilho do LED responde de forma imediata e
proporcional ao valor de duty-cycle informado.
