# KartUp — Telemetria de Kart

## Visão Geral
Projeto de telemetria para kart rodando em um **ESP32-2432S028R (CYD — Cheap Yellow Display)** com display ILI9341 320x240 pixels. Desenvolvido no PlatformIO com framework Arduino.

## Hardware
- **Board:** ESP32-2432S028R (CYD)
- **Display:** ILI9341 — 320x240 pixels, landscape (`setRotation(1)`)
- **Botão:** GPIO 0 (BOOT button físico na placa)
- **GPS:** NEO-M8N via UART2 — RX=22, TX=27 (conector CN1), 115200 baud, 10 Hz
- **Upload port:** COM3, 921600 baud
- **Monitor:** COM3, 115200 baud

## Restrição Crítica de RAM
O ESP32 **não tem RAM suficiente para um sprite full-screen** (320×240×2 = 153.600 bytes). Tentativas anteriores falharam silenciosamente — o sprite não era alocado e o display ficava travado na tela de boot.

**Solução adotada:** desenho direto no TFT com **cache anti-flickering** — cada elemento só é redesenhado quando seu valor muda.

## Estrutura do Projeto
```
cyd-clock/
├── src/
│   ├── main.cpp          ← código principal
│   ├── teste_gps.cpp     ← diagnóstico GPS (compilado só com env:teste_gps)
│   ├── logo.h            ← logo KartUp (120×120 px, RGB565 byte-swapped, PROGMEM)
│   ├── f1_speed_font.h   ← fonte Bahnschrift grande (VLW, PROGMEM)
│   └── f1_time_font.h    ← fonte Bahnschrift tempos (VLW, PROGMEM)
├── platformio.ini
├── CLAUDE.md
└── (raiz do repo)
    └── convert_logo.py   ← script Python: JPEG → RGB565 logo.h (reexecutar ao trocar logo)
```

## Configuração TFT (platformio.ini)
Pinos e flags passados via build_flags (sem User_Setup.h separado):
- MISO=12, MOSI=13, SCLK=14, CS=15, DC=2, RST=-1, BL=21
- Fontes habilitadas: GLCD, Font2, Font4, Font6, Font7, Font8, GFXFF, SMOOTH_FONT
- Lib: `bodmer/TFT_eSPI@^2.5.33`, `mikalhart/TinyGPSPlus@^1.0.3`

## Interface Atual: Dark Cockpit F1
Uma única interface no estilo Fórmula 1:
- **Barra superior:** status GPS (círculo colorido), título "kartUp", velocidade km/h
- **Área central superior:** número de volta (esq), delta em segundos (dir)
- **Tempo atual:** Font 7 (7 segmentos 48px) para M:SS, Font 6 para centésimos em vermelho F1
- **Barra de delta:** fundo vermelho escuro (esq = perda) / verde escuro (dir = ganho), barra colorida
- **Linha divisória:** gradiente cinza → vermelho F1 → cinza
- **Área inferior:** melhor volta (âmbar/roxo) e última volta (cinza) com f1_time_font
- **Animação de recorde:** borda roxa pulsante ao bater melhor volta

## Paleta de Cores (RGB565)
```cpp
C_BG      = 0x0841  // #0a0a0a fundo
C_PANEL   = 0x1082  // #111111 painéis
C_RED_F1  = 0xE020  // vermelho Ferrari
C_AMBER   = 0xFD00  // amarelo-âmbar (melhor volta)
C_PURPLE  = 0x781F  // roxo (recorde)
C_BORDER  = 0x18C3  // bordas suaves
```

## Modos de Operação
- **`#define USE_MOCK_DATA`** (comentado por padrão) — dados simulados, sem GPS. Velocidade oscila 0–95 km/h, delta ±2s, volta automática a cada 65s. Demo inicia imediatamente (lapActive=true no setup). Para ativar: descomentar a linha no topo de main.cpp.
- **GPS real** (padrão) — NEO-M8N via UART2. Velocidade real com filtro de média móvel (5 amostras). Linha de chegada definida por coordenadas FINISH_LAT/LON com raio de 8m.
- **WiFi AP** — ativado por toque em "WiFi / Baixar CSV" na tela pronta. Cria rede `KartUp-Telemetria` (aberta). Acesse `http://192.168.4.1` para listar e baixar arquivos CSV do SD. Sair pelo botão na tela. Logging não ocorre durante WiFi ativo.

## Configuração GPS
- `configureGPS()` roda no `setup()` a cada boot — não precisa de intervenção manual
- Detecta automaticamente o baud atual: tenta 115200 primeiro (boot quente), cai para 9600 se necessário (cold power-on)
- Envia dois comandos UBX: `CFG-PRT` (115200 baud) + `CFG-RATE` (100ms = 10 Hz)
- Cold start (primeira ligação ou após longo tempo sem energia): aguardar 1–3 min ao ar livre para fix
- `gpsOk` = `gps.location.isValid()` — círculo verde na barra superior quando com fix
- Velocidade zerada automaticamente se perder fix

## Lógica de Volta
- `triggerNewLap()` — registra volta, incrementa contador, verifica novo recorde
- `lapStartMs` — timestamp de início da volta atual
- `curLapMs = millis() - lapStartMs` — tempo corrente
- `deltaS` — diferença em segundos vs melhor volta (demo usa valor simulado)

## Fontes
- `tft.setTextFont(7)` — 7 segmentos 48px, só suporta `0-9 : . -` (perfeito para tempos)
- `tft.setTextFont(6)` — 48px mais caracteres
- `tft.setTextFont(4)` — ~26px (usado nas voltas inferior)
- `tft.loadFont(f1_time_font)` — fonte VLW custom (Bahnschrift), usada só na área inferior; sempre chamar `tft.unloadFont()` após usar. **Não usar durante boot screen** — pode corromper estado do display.
- `tft.loadFont(f1_speed_font)` — fonte VLW maior, disponível mas não usada na interface atual

## WiFi AP — Detalhes de Implementação
- `WebServer httpServer(80)` — global, handlers registrados no `setup()` uma única vez
- `enterWiFiMode()` — re-inicializa SD (sdOk=false após stopLogging), sobe AP, inicia servidor
- `exitWiFiMode()` — para servidor, desliga WiFi, sdOk=false (SD re-init em próximo startLogging)
- `handleLogo()` — converte `logo_kartup[]` RGB565 → BMP 24-bit on-the-fly via `malloc(10854)` + `send_P`
- `handleRoot()` — HTML dark F1 com logo, card de sessão (lapCount/bestLapMs/lastLapMs/deltaS), lista de arquivos
- `handleDownload()` — `setContentLength` + `send(200,"text/csv","")` + `httpServer.client().write()` para stream do SD
- Diretório: `File32 dir = sd.open("/")` — obrigatório usar `sd.open()` (File32 default-constructed não tem volume)
- Loop WiFi: `httpServer.handleClient()` + detecção de toque no botão SAIR, early-return (GPS/display pausados)

## Logo
- Fonte: `3.LOGO/LOGO_MAIN.jpeg` (1254×1254px)
- Script de conversão: `convert_logo.py` na raiz do repo
- Formato gerado: RGB565 **com byte swap** para TFT_eSPI no ESP32 (`((px & 0xFF) << 8) | (px >> 8)`)
- Fundo navy (#0D1B2A) substituído por preto puro (#000000) antes da conversão
- UnsharpMask aplicado pré e pós-resize para manter nitidez
- Ao trocar a imagem: reexecutar `convert_logo.py`, recompilar e flashar

## Histórico de Problemas Resolvidos
1. **Texto sobreposto no boot** — causado por `tft.loadFont()` durante a tela inicial. Solução: boot screen usa apenas fontes built-in (Font1, Font2, Font4).
2. **Tela travada no boot** — sprite full-screen falhava silenciosamente por falta de RAM. Solução: removido sprite, desenho direto com cache.
3. **Demo sem dados iniciais** — `lapActive` iniciava false e levava 65s para primeira volta. Solução: `lapActive=true` + `lapStartMs=millis()` no setup.
4. **GPS sem fix após mudança de baud** — `configureGPS()` mudava para 115200 mas se o comando UBX não era recebido o módulo ficava em 9600 e o ESP32 em 115200, sem comunicação. Solução: `configureGPS()` detecta baud automaticamente antes de reconfigurar.
5. **GPS oscilando entre 0 e 4 satélites em movimento** — antena cerâmica patch embutida tem ganho baixo para uso veicular. Solução paliativa: filtro de média móvel na velocidade. Solução definitiva: antena ativa externa com conector U.FL.
