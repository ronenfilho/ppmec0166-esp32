# Workspace ESP32

Data: 2026-08-20

## Objetivo
Centralizar projetos ESP32 usando o ambiente oficial ESP-IDF no macOS.

## O que ja foi realizado

### 1. Homebrew instalado
- Homebrew instalado com sucesso
- Comando `brew` validado

### 2. Dependencias principais instaladas via Homebrew
- cmake
- ninja
- dfu-util
- ccache
- pkg-config

### 3. ESP-IDF instalado e configurado
- ESP-IDF v5.5.1 instalado em `~/esp/esp-idf`
- Toolchains e ferramentas do alvo `esp32` instaladas
- `idf.py` validado com versao correta

### 4. Ambiente de shell configurado
- Ativacao automatica do ESP-IDF adicionada ao `~/.zshrc`
- `IDF_PATH` ativo em `/Users/ronen/esp/esp-idf`

### 5. Verificacao de pre-requisitos
- Java: OpenJDK 26.0.2 (Temurin)
- Python: 3.9.6 (CLI IDF local) e Python 3.12.14 (compatibilidade IDE)
- Git: OK
- ESP-IDF tools: OK

### 6. VS Code
- Extensao oficial Espressif IDF instalada

### 7. Espressif-IDE configurada e funcional
- Espressif-IDE instalada e `ESP-IDF Manager` configurado
- Instalacao ativa na IDE: `release-v5.5`
- Local ativo na IDE: `/Users/ronen/.espressif/release-v5.5/esp-idf`
- Ajuste de Python para requisito da IDE (`>=3.10 e <3.14`)
- `python3` global ajustado para `3.12.14`

### 8. Correcao de dependencia da IDE
- Erro resolvido no monitor da IDE:
   - `RuntimeError: Please install the websocket_client package for IDE integration!`
- Pacote instalado no venv da IDE:
   - `/Users/ronen/.espressif/tools/python/release-v5.5/venv`
   - `websocket-client==1.9.0`

### 9. Validacao ponta a ponta (placa real)
- Porta serial detectada: `/dev/cu.usbserial-0001`
- Build executado com sucesso
- Flash executado com sucesso
- Monitor serial executado com sucesso
- Log recebido em loop: `Hello from app_main!`

## Comandos uteis
1. Definir target:
   - `idf.py set-target esp32`
2. Compilar:
   - `idf.py build`
3. Gravar e monitorar:
   - `idf.py -p /dev/cu.usbserial-0001 flash monitor`
4. Sair do monitor:
   - `Ctrl + ]`

## Proximos passos sugeridos
1. Criar base de projeto em `workspace-esp32` com estrutura modular (`main`, `components`, `config`)
2. Trocar exemplo `hello_world` por caso real (GPIO, UART, Wi-Fi ou BLE)
3. Versionar projeto no Git e registrar instrucoes de build/flash no README do projeto

## Estrutura recomendada desta pasta
- `workspace-esp32/hello_world`
- `workspace-esp32/blink`
- `workspace-esp32/projetos-experimentos`

## Observacao
Este workspace pode ser usado tanto no VS Code quanto na Espressif-IDE.

Atencao: hoje existem duas instalacoes funcionais de ESP-IDF no sistema:
- CLI/manual: `~/esp/esp-idf` (v5.5.1)
- IDE gerenciada: `~/.espressif/release-v5.5/esp-idf`

Para evitar confusao de ambiente no dia a dia, escolha uma como principal por projeto e documente no README de cada projeto qual foi usada.
