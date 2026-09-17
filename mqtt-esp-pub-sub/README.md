# MQTT ESP Pub/Sub

- **Autor:** Ronen Rodrigues Silva Filho
- **Instituição:** Universidade de Brasília (UnB) — Pós-graduação, PPMEC
- **Disciplina:** PPMEC0166 - Tópicos Avançados em Sistemas Mecatrônicos 3 (Princípios de Internet das Coisas)
- **Professor:** Jones Yudi Mori Alves da Silva

Teste rápido de MQTT no ESP32: um lado publica (heartbeat periódico),
o outro assina (eco de qualquer mensagem recebida). Serve pra validar
a conexão com um broker (local, do professor, ou público) antes de
partir pra um projeto MQTT mais completo.

## Hardware

- Placa: ESP32-WROOM-32 DevKitC

## Configuração

Credenciais de Wi-Fi e do broker ficam num header separado que não é
versionado — equivalente a um `.env`. Antes de compilar:

```
cp main/secrets.h.example main/secrets.h
```

E edite `main/secrets.h` com os valores reais:

```c
#define WIFI_SSID     "SEU_SSID_AQUI"
#define WIFI_PASSWORD "SUA_SENHA_AQUI"
#define MQTT_BROKER_URI "mqtt://SEU_IP_DO_BROKER:1883"
#define MQTT_USERNAME ""
#define MQTT_PASSWORD ""
```

O broker e o ESP32 precisam estar na mesma rede local (a não ser que
seja um broker público na internet).

## Tópicos

| Tópico | Direção | Payload |
| --- | --- | --- |
| `esp32/ronen/teste/heartbeat` | ESP32 publica, a cada 5s | `heartbeat #N` |
| `esp32/ronen/teste/echo` | ESP32 assina (eco no monitor serial) | qualquer texto |

## Implementação

- **[connect.c](main/connect.c)/[connect.h](main/connect.h)** — conexão
  Wi-Fi em modo estação (`wifi_connect_sta`), com reconexão automática
  em queda de sinal.
- **[mqtt_test.c](main/mqtt_test.c)/[mqtt_test.h](main/mqtt_test.h)** —
  cliente MQTT (`esp-mqtt`): ao conectar, assina `esp32/ronen/teste/echo`
  e sobe uma task que publica um heartbeat em
  `esp32/ronen/teste/heartbeat` a cada 5 segundos. Qualquer mensagem
  recebida no tópico de eco é logada no monitor serial.

## Testando

Com o ESP32 conectado, usando `mosquitto_pub`/`mosquitto_sub` da linha
de comando, apontando pro mesmo broker do `secrets.h`:

**Ouvir o heartbeat que o ESP32 publica:**

```
mosquitto_sub -h <IP-do-broker> -t esp32/ronen/teste/heartbeat
```

**Publicar algo pro ESP32 ecoar (acompanhe pelo monitor serial):**

```
mosquitto_pub -h <IP-do-broker> -t esp32/ronen/teste/echo -m "ola esp32"
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
