# Connect with Input

- **Autor:** Ronen Rodrigues Silva Filho
- **Instituição:** Universidade de Brasília (UnB) — Pós-graduação, PPMEC
- **Disciplina:** PPMEC0166 - Tópicos Avançados em Sistemas Mecatrônicos 3 (Princípios de Internet das Coisas)
- **Professor:** Jones Yudi Mori Alves da Silva

Parte 5 do material [Internet Connections](../connect/conectividadeDeRede.pdf)
("Comandos Remotos"): ligar/desligar o LED do ESP32 remotamente, via
requisição HTTP na rede local.

## Hardware

- Placa: ESP32-WROOM-32 DevKitC
- LED: onboard, GPIO2
- Rede Wi-Fi: mesma configurada no projeto [connect](../connect) (Parte 4)

## Implementação

- **[connect.c](main/connect.c)/[connect.h](main/connect.h)** — copiados
  sem alteração do projeto `connect` (Parte 4): conexão Wi-Fi em modo
  estação (`wifi_connect_sta`), com reconexão automática em queda de
  sinal.
- **[toogleLed.c](main/toogleLed.c)/[toogleLed.h](main/toogleLed.h)** —
  novos: configuram o LED como saída e implementam o handler HTTP que
  liga/desliga o LED a partir do corpo da requisição.
- **[main.c](main/main.c)** — conecta ao Wi-Fi e sobe um servidor HTTP
  com uma rota `POST /` que aceita um corpo JSON `{"is_on": true|false}`
  e responde confirmando o novo estado do LED.

## Testando

1. Build, flash e monitor (comandos abaixo); anote o IP mostrado no log
   de conexão.
2. Instale a extensão **Thunder Client** no VS Code.
3. Nova requisição: método `POST`, URL = IP do dispositivo (ex.
   `192.168.1.100`).
4. Aba **Headers**: `Content-Type: application/json`.
5. Aba **Body** → JSON:
   ```json
   { "is_on": true }
   ```
6. **Send** — o LED deve ligar, e a resposta confirma
   `{"is_on":true}`. Trocar para `false` e enviar de novo desliga o LED.

### Via curl (alternativa ao Thunder Client)

Troque `192.168.1.100` pelo IP real do dispositivo:

```
curl -X POST http://192.168.1.100/ \
  -H "Content-Type: application/json" \
  -d '{"is_on": true}'
```

```
curl -X POST http://192.168.1.100/ \
  -H "Content-Type: application/json" \
  -d '{"is_on": false}'
```

## Build, flash e monitor

Em terminal novo, se `idf.py` não for reconhecido:

```
source ~/esp/esp-idf/export.sh
```

Depois:

```
idf.py set-target esp32
idf.py build
idf.py -p /dev/cu.usbserial-0001 flash monitor
```

Sair do monitor: `Ctrl + ]`
