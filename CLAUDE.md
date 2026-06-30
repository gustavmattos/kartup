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
- **WiFi AP** — ativado por toque em "WiFi / Baixar CSV" na tela pronta ou pela tela `Settings`. Cria rede `KartUp-Telemetria` (aberta). Acesse `http://192.168.4.1` para listar e baixar arquivos CSV do SD. Sair pelo botão na tela. Logging não ocorre durante WiFi ativo.
- **Settings** — ícone de engrenagem na tela inicial abre uma tela `Settings` em formato HUD 3×2. Tiles: `01//BRT` (brilho), `02//WLAN` (WiFi AP), `03//STG` (histórico de sessões), `04//NAV` (GPS info), `05//SYS` (versão), `06//PURGE` (apagar logs).
- **Brilho** — tile `01//BRT` abre modal com níveis `25%`, `50%`, `75%` e `100%`. Controle via LEDC PWM no `TFT_BL` (GPIO 21), 5 kHz, canal 0.
- **Histórico de Sessões** — tile `03//STG` abre lista rolável de até 200 sessões salvas no SD (`HIST_MAX_SESSIONS=200`, `HIST_VISIBLE_ROWS=6`, `HIST_ROW_H=32`). Carregamento em duas fases: fase 1 exibe nomes imediatamente (`loadHistoryBasenames()`), fase 2 carrega `.meta` progressivamente (`loadHistoryMeta(idx)`). **Rolagem por arraste:** `handleHistoryTouch` rastreia o dedo com scroll ao vivo (desloca 1 linha a cada `HIST_ROW_H` px) + inércia (fricção 0.88) pós-release. Debounce de release (4 leituras de `z<60`) tolera oscilação de pressão do touch resistivo; decisão tap-vs-drag por **pico** de deslocamento (`peakDy<12` = tap), evitando que arraste vire clique. Scrollbar lateral indica posição. Linhas estilo HUD neon: brackets cyan (`C_SPEED`) nos cantos, barra accent à esquerda, título `GPS095` (Font2) + data, badge de voltas `NV` em accent com glow. Resumo: total de voltas, hora de início, melhor volta (nº da volta), velocidade máxima, arquivo CSV. Sessões antigas sem `.meta` reparseiam do CSV. Voltar retorna para Settings.
- **Voltar** — ícone `<` no canto superior esquerdo em todas as telas, exceto inicial, logging e WiFi. Na tela pronta volta para seleção de pista; na seleção de pista volta para a tela inicial; em modais fecha o modal e retorna para a tela anterior.

## Configuração GPS
- `configureGPS()` roda no `setup()` a cada boot — não precisa de intervenção manual
- Detecta automaticamente o baud atual: tenta 115200 primeiro (boot quente), cai para 9600 se necessário (cold power-on)
- Envia dois comandos UBX: `CFG-PRT` (115200 baud) + `CFG-RATE` (100ms = 10 Hz)
- Cold start (primeira ligação ou após longo tempo sem energia): aguardar 1–3 min ao ar livre para fix
- `gpsOk` = `gps.location.isValid()` — círculo verde na barra superior quando com fix
- Velocidade zerada automaticamente se perder fix

## Lógica de Volta
- `triggerNewLap()` — registra volta, incrementa contador, verifica novo recorde; atualiza `sessionBestLapNum` quando bate recorde
- `lapStartMs` — timestamp de início da volta atual; atualizado imediatamente em `triggerNewLap()`, antes de qualquer display
- `curLapMs = millis() - lapStartMs` — tempo corrente; atualizado no loop independente do display
- `deltaS` — diferença em segundos vs melhor volta (demo usa valor simulado)
- **Out-lap (GPS real):** `startLogging()` inicia com `lapActive=false`. Primeira passagem na linha de chegada apenas ativa o timer (`lapActive=true`, `lapStartMs=now`) sem gravar volta — é a saída do box. Segunda passagem grava Volta 1. `lapCount` conta voltas completadas; display e CSV usam `lapCount + 1` = número da volta atual. **Out-lap não é gravado no CSV:** enquanto `lapActive=false`, `sdLog()` é pulado no loop (o caminho box→linha não entra no log); `checkFinishCrossing()` continua rodando para detectar o 1º cruzamento. CSV começa na Volta 1. Demo mode mantém `lapActive=true` (protegido por `#ifdef USE_MOCK_DATA`).

## Log CSV e Meta de Sessão
- **Header CSV (v2.1+):** `date_utc,time_utc,millis,lat,lon,speed_kmh,lap,lap_ms,delta_s`
- `date_utc` — capturado de `gps.date` na primeira linha com fix; `??/??/????` se sem fix
- `time_utc` — formato `HH:MM:SS.cc` (centésimos via `gps.time.centisecond()`). 10 linhas por segundo terão timestamps distintos. Usar coluna `millis` como referência primária de tempo (1ms resolução).
- **Flush SD:** `sdLog()` não faz flush manual — SdFat auto-flush a cada setor cheio (~7 linhas / ~700ms). `triggerNewLap()` chama `logFile.flush()` a cada volta para garantir commit ao cruzar a linha. Perda máxima em crash: ~700ms de dados.
- **Diagnóstico GPS no serial:** `[LOG] ms=... dt=... age=...` a cada linha gravada; `[LOG_SKIP] fix=... age=...` quando `fixFresh=false` bloqueia o log. Útil para diagnosticar lacunas no CSV.
- **Arquivo `.meta`** — gravado por `stopLogging()` ao encerrar sessão (apenas se `lapCount > 0`). Formato key=value: `date`, `start` (hora UTC `HH:MM:SS` de início), `laps`, `best_ms`, `best_lap`, `max_kmh`. Usado pela tela de histórico; zero parsing de CSV em runtime. `start` capturado em `startLogging()` via `gps.time` (fallback: 1ª linha gravada no `sdLog`); vazio→`--:--:--` se sem fix. Sessões antigas sem `start` no `.meta` reparseiam do CSV (coluna `time_utc`). Exibido na linha HORA INICIO do resumo.
- Variáveis de sessão: `sessionMaxSpeed` (float), `sessionBestLapNum` (int), `sessionDate[12]` (char) — inicializadas em `startLogging()`, populadas durante logging, gravadas em `stopLogging()`.

### Overlay de última volta (3s)
Ao cruzar a linha de chegada, `drawCurrentLap()` exibe o tempo da volta encerrada por 8 segundos antes de voltar ao cronômetro atual. A próxima volta já está contando em background via `lapStartMs`.
- `showLapOverlay` (bool) — ativa overlay
- `lapOverlayStartMs` (uint32_t) — timestamp de início do overlay
- `overlayLapMs` (uint32_t) — tempo da volta a exibir
- `overlayIsNewBest` (bool) — determina cor: `C_GREEN` se recorde, `C_RED_F1` se pior
- `cacheOverlayActive` (bool) — anti-flickering: redraw único durante os 8s

## Fontes
- `tft.setTextFont(7)` — 7 segmentos 48px, só suporta `0-9 : . -` (perfeito para tempos)
- `tft.setTextFont(6)` — 48px mais caracteres
- `tft.setTextFont(4)` — ~26px (usado nas voltas inferior)
- `tft.loadFont(f1_time_font)` — fonte VLW custom (Bahnschrift), usada só na área inferior; sempre chamar `tft.unloadFont()` após usar. **Não usar durante boot screen** — pode corromper estado do display.
- `tft.loadFont(f1_speed_font)` — fonte VLW maior, disponível mas não usada na interface atual

## WiFi AP — Detalhes de Implementação
- `WebServer httpServer(80)` — global, handlers registrados no `setup()` uma única vez
- `enterWiFiMode()` — re-inicializa SD (sdOk=false após stopLogging), sobe AP, inicia servidor; guarda a tela de retorno (`Ready` ou `Settings`)
- `exitWiFiMode()` — para servidor, desliga WiFi, sdOk=false (SD re-init em próximo startLogging) e retorna para a tela que abriu o AP
- `handleLogo()` — converte `logo_kartup[]` RGB565 → BMP 24-bit on-the-fly via `malloc(10854)` + `send_P`
- `handleRoot()` — HTML dark F1 com logo, card de sessão (lapCount/bestLapMs/lastLapMs/deltaS), lista de arquivos
- `handleDownload()` — `setContentLength` + `send(200,"text/csv","")` + `httpServer.client().write()` para stream do SD
- Diretório: `File32 dir = sd.open("/")` — obrigatório usar `sd.open()` (File32 default-constructed não tem volume)
- Loop WiFi: `httpServer.handleClient()` + detecção de toque no botão SAIR, early-return (GPS/display pausados)

## Assets Gerados
- `src/settings_icon.h` — ícone de engrenagem convertido para RGB565/PROGMEM, usado na tela `Settings`
- `src/wifi_icon.h` — ícone WiFi convertido para RGB565/PROGMEM, usado na lista da tela `Settings`
- `3.LOGO/seeting_icon1.png` — fonte do ícone de engrenagem
- `3.LOGO/wifi_icon.png` — fonte do ícone WiFi

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
6. **Volta zero / out-lap contado como Volta 1** — `startLogging()` iniciava `lapActive=true`, então a primeira passagem na linha gravava o tempo desde o início da sessão (saída do box) como Volta 1. Solução: GPS real inicia com `lapActive=false`; primeira passagem só ativa o timer. Display e CSV usam `lapCount + 1` para mostrar número da volta atual a partir de 1.
7. **`time_utc` sem sub-segundo no CSV** — formato `HH:MM:SS` fazia 10 linhas a 10Hz aparecerem com timestamp igual, parecendo que o log não estava a 10Hz. Solução: formato alterado para `HH:MM:SS.cc` usando `gps.time.centisecond()`. Usar `millis` como referência primária.

## What's Next
**Gatilho:** só implementar item desta lista quando o usuário disser exatamente "quais os proximos passos claude". Não executar proativamente.

- Padronizar arquivo CSV conforme padrão Alfano.
