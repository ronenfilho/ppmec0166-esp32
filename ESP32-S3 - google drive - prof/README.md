# Google Drive Download — Exemplo do Professor

- **Autor:** Ronen Rodrigues Silva Filho
- **Instituição:** Universidade de Brasília (UnB) — Pós-graduação, PPMEC
- **Disciplina:** PPMEC0166 - Tópicos Avançados em Sistemas Mecatrônicos 3 (Princípios de Internet das Coisas)
- **Professor:** Jones Yudi Mori Alves da Silva

Projeto de exemplo fornecido pelo professor: o ESP32 conecta ao Wi-Fi,
sobe um webserver simples e baixa um arquivo hospedado no Google Drive
via HTTP, logando status e tamanho do download no monitor serial.

O `main.c` veio pronto do professor, mas o projeto não trazia o
componente de conexão Wi-Fi (`connect.c`/`connect.h`) que ele depende
— foi completado com o mesmo componente já usado nos outros projetos
deste workspace para poder compilar e rodar.

## Hardware

- Placa: ESP32-S3 N16R8 (16MB flash, 8MB PSRAM)
- USB-CDC nativo — aparece como `/dev/cu.usbmodemXXXX`

## Configuração

Diferente dos outros projetos do workspace, aqui as credenciais de
Wi-Fi estão fixas direto no código (como veio do professor), em
[main.c](main/main.c):

```c
ESP_ERROR_CHECK(wifi_connect_sta("labSHP", "labSHP2025", 10000));
```

Para usar em outra rede, edite essa linha antes de compilar.

## Implementação

- **[connect.c](main/connect.c)/[connect.h](main/connect.h)** — conexão
  Wi-Fi em modo estação (`wifi_connect_sta`), com reconexão automática
  em queda de sinal. Adicionado para completar o exemplo (ver acima).
- **[main.c](main/main.c)** — conecta ao Wi-Fi, sobe um webserver HTTP
  que responde `Ola, eu sou um ESP32!` na rota `/`, e baixa o arquivo
  de `DRIVE_URL` (Google Drive) via `esp_http_client`, imprimindo o
  conteúdo recebido e, ao final, status HTTP e `Content-Length`.

## Resultado

Log de boot via monitor serial:

```
I (1327) wifi:connected with labSHP, aid = 14, channel 6, BW20, bssid = 40:ed:00:84:a5:b3
I (2397) WIFI: GOT IP
I (2397) esp_netif_handlers: sta ip: 192.168.0.4, mask: 255.255.255.0, gw: 192.168.0.1

=====================================
BAIXANDO ARQUIVO DO GOOGLE DRIVE
=====================================

I (3037) esp-x509-crt-bundle: Certificate validated
Ola turma de Internet das Coisas!

Esta mensagem veio do Google Drive.


=====================================
DOWNLOAD FINALIZADO
HTTP Status: 200
Content-Length: 74
=====================================
```

## Testando

1. Build, flash e monitor (comandos abaixo); anote o IP mostrado no log
   de conexão (`esp_netif_handlers: sta ip: ...`).
2. Acesse `http://<IP-do-dispositivo>/` num navegador, na mesma rede —
   a página mostra `Ola, eu sou um ESP32!`.
3. No próprio monitor serial, acompanhe o download do arquivo do
   Google Drive logo após o boot.

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
