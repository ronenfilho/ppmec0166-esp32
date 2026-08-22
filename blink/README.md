# Blink

Pisca um LED simples via GPIO. Baseado no exemplo oficial `examples/get-started/blink`
do ESP-IDF, simplificado para o caso de uso deste projeto (sem suporte a LED
endereçável / WS2812, que não se aplica a esta placa).

## Hardware

- Placa: ESP32-WROOM-32 DevKitC (30 pinos, USB-serial CP2102)
- LED: onboard, ligado ao **GPIO2** (confirmado por varredura manual — essa
  placa não tem LED de usuário separado, só o LED de power fixo)

## Configuração

- Target: `esp32`
- ESP-IDF usado: instalação gerenciada pela Espressif-IDE (`release-v5.5`,
  `~/.espressif/release-v5.5/esp-idf`) — ver `README.md` do workspace
- `Example Configuration` (`idf.py menuconfig` ou SDK Configuration Editor):
  - `Blink GPIO number`: `2`
  - `Blink period in ms`: `1000`

## Build, flash e monitor

Via IDE (Espressif-IDE): Build → Flash → Monitor, porta `/dev/cu.usbserial-0001`.

Via CLI (equivalente, também funciona no VS Code com a extensão ESP-IDF):

```
idf.py set-target esp32
idf.py build
idf.py -p /dev/cu.usbserial-0001 flash monitor
```

Sair do monitor: `Ctrl + ]`

## Saída esperada

```text
I (315) example: Configurado para piscar LED no GPIO2
I (325) example: LED ligado
I (1325) example: LED desligado
I (2325) example: LED ligado
```

## Troubleshooting

- LED não pisca: confirme `Blink GPIO number` no menuconfig e se o pino
  corresponde ao LED físico da sua placa (varia por fabricante/modelo).
