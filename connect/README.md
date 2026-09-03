# Connect

- **Autor:** Ronen Rodrigues Silva Filho
- **Instituição:** Universidade de Brasília (UnB) — Pós-graduação, PPMEC
- **Disciplina:** PPMEC0166 - Tópicos Avançados em Sistemas Mecatrônicos 3 (Princípios de Internet das Coisas)
- **Professor:** Jones Yudi Mori Alves da Silva

Parte 4 do material [Internet Connections](conectividadeDeRede.pdf)
("Micro Webserver"): conectar o ESP32 à rede Wi-Fi e hospedar uma mini
página web nele.

## Hardware

- Placa: ESP32-WROOM-32 DevKitC
- Rede Wi-Fi: SSID/senha configurados diretamente em
  [main/main.c](main/main.c) (`wifi_connect_sta`), sem usar menuconfig

## Implementação

- **[connect.c](main/connect.c)/[connect.h](main/connect.h)** — biblioteca
  de conexão Wi-Fi em modo estação: inicializa o driver Wi-Fi, conecta
  com SSID/senha e timeout configuráveis, reconecta automaticamente em
  queda de sinal (exceto quando a desconexão foi solicitada).
- **[main.c](main/main.c)** — conecta ao Wi-Fi e sobe um servidor HTTP
  (`esp_http_server`) com uma rota `GET /` que responde com uma mensagem
  de texto simples.

## Testando

1. Build, flash e monitor; anote o IP mostrado no log de conexão.
2. Acesse esse IP num navegador (mesma rede) e veja a mensagem.

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

## Próximos passos

Continuação em [webserver_acionamento_remoto](../webserver_acionamento_remoto) (Parte 5:
Comandos Remotos), que reaproveita `connect.c`/`connect.h` para
ligar/desligar um LED via requisição HTTP.
