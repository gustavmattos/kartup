// =============================================================
//  KARTUP — Telemetria de Kart  |  ESP32 CYD (ILI9341 320x240)
//  Interface: Dark Cockpit F1
//  Desenho direto no TFT com cache anti-flickering
// =============================================================
#ifndef TESTE_GPS

#include <Arduino.h>
#include <SPI.h>
#include <SdFat.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <TinyGPS++.h>
#include "logo.h"
#include "settings_icon.h"
#include "wifi_icon.h"
#include "f1_speed_font.h"
#include "f1_time_font.h"
#include <WiFi.h>
#include <WebServer.h>

// ─── MOCK DATA (descomente a linha abaixo para simular sem GPS) ───
// #define USE_MOCK_DATA true

// ─── WiFi AP ─────────────────────────────────────────────────
#define WIFI_SSID      "KartUp-Telemetria"
#define WIFI_PASS      ""     // rede aberta; coloque senha se quiser

// Botão WiFi na tela pronta (canto inferior)
#define WIFI_BTN_X    100
#define WIFI_BTN_Y    205
#define WIFI_BTN_W    120
#define WIFI_BTN_H     24

// Botão SAIR na tela WiFi
#define WIFI_SAIR_X    90
#define WIFI_SAIR_Y   185
#define WIFI_SAIR_W   140
#define WIFI_SAIR_H    40

// Botão CONFIG / brilho na tela inicial
#define SETTINGS_BTN_X   266
#define SETTINGS_BTN_Y    18
#define SETTINGS_BTN_W    38
#define SETTINGS_BTN_H    38
#define SETTINGS_TOUCH_PAD 8

// Botão BRILHO na tela Settings
#define BRIGHTNESS_BTN_X   14
#define BRIGHTNESS_BTN_Y   44
#define BRIGHTNESS_BTN_W  292
#define BRIGHTNESS_BTN_H   42

// Botão WiFi AP na tela Settings
#define WIFI_AP_BTN_X      BRIGHTNESS_BTN_X
#define WIFI_AP_BTN_Y     (BRIGHTNESS_BTN_Y + BRIGHTNESS_BTN_H + 10)
#define WIFI_AP_BTN_W      BRIGHTNESS_BTN_W
#define WIFI_AP_BTN_H      BRIGHTNESS_BTN_H

// Botão VOLTAR no topo esquerdo
#define BACK_BTN_X      6
#define BACK_BTN_Y      2
#define BACK_BTN_W     32
#define BACK_BTN_H     20
#define BACK_TOUCH_PAD 12

// ─── PINOS ───────────────────────────────────────────────────
#define GPS_RX_PIN   22
#define GPS_TX_PIN   27
#define BOOT_BTN      0
#define GPS_BAUD_INIT 9600
#define GPS_BAUD      115200

// SD — pinos próprios (SoftSPI, não conflita com display nem touch)
#define SD_CS_PIN  5
#define SD_SCK    18
#define SD_MISO   19
#define SD_MOSI   23

// ─── PISTAS ──────────────────────────────────────────────────
struct Track {
    const char* name;
    const char* location;
    double      finishLat;
    double      finishLon;
    float       finishRadius;
};

const Track TRACKS[] = {
    { "Pista dos Ingleses", "Florianopolis, SC", -27.442399, -48.425761, 5.0 },
};
const int TRACK_COUNT = 1;

int selectedTrack = 0;  // índice da pista selecionada

// ─── DIMENSÕES ───────────────────────────────────────────────
#define W  320
#define H  240

// ─── LAYOUT ──────────────────────────────────────────────────
#define TOP_Y     0
#define TOP_H    24    // barra superior
#define MID_Y    26    // início área central
#define BOT_Y   185    // início barra inferior
#define BOT_H   (H - BOT_Y)

// Áreas dinâmicas — usadas para limpar antes de redesenhar
#define SPD_X   200   // velocidade (top right)
#define SPD_W   116
#define LAP_X    44   // número de volta
#define LAP_W    40
#define DELTA_NUM_X  220  // delta numérico (top right da área central)
#define DELTA_NUM_W   96
#define TIME_X    10   // tempo atual grande
#define TIME_Y    60
#define TIME_W   295
#define TIME_H    62
#define BAR_X      8   // barra de delta
#define BAR_Y    158
#define BAR_W   (W - 16)
#define BAR_H     14
#define BEST_X    10   // melhor volta
#define BEST_W   148
#define LAST_X   162   // última volta
#define LAST_W  (W - LAST_X - 6)
#define BOT_TEXT_Y (BOT_Y + 20)

// ─── PALETA F1 ───────────────────────────────────────────────
#define C_BG        0x0842   // #080810  azul-preto profundo
#define C_PANEL     0x0863   // #0f0f1a  surface/paineis
#define C_PANEL2    0x10A4   // #151525  surface hover/destaque
#define C_RED_F1    0xE803   // #e8001e  vermelho F1 (accent)
#define C_RED_BRT   0xF98A   // #ff3355  vermelho bright (highlights)
#define C_RED       0xF8A8   // #ff1744  vermelho perda
#define C_GREEN     0x072E   // #00e676  verde ganho
#define C_SPEED     0x06BF   // #00d4ff  ciano velocidade
#define C_AMBER     0xF524   // #f5a623  amber melhor volta
#define C_ORANGE    0xF524   // alias p/ compat (mesmo que AMBER)
#define C_PURPLE    0x9ADE   // #9b59f5  recorde pessoal
#define C_WHITE     TFT_WHITE
#define C_GRAY      0x8C55   // #8888aa  cinza medio (labels visiveis)
#define C_GRAY_DK   0x2948   // #2a2a40  cinza escuro (labels fracos)
#define C_BORDER    0x2948   // #2a2a40  bordas (igual gray_dk)
// Tintas dim p/ badges (usar tft.color565 em runtime):
//   RED_DIM   = tft.color565(60,  6, 14)
//   GREEN_DIM = tft.color565( 0, 50, 22)
//   AMBER_DIM = tft.color565(60, 38,  6)
//   SPEED_DIM = tft.color565( 0, 36, 60)

// ─── TOUCH (VSPI separado do display) ────────────────────────
#define TOUCH_CS   33
#define TOUCH_MOSI 32
#define TOUCH_MISO 39
#define TOUCH_CLK  25
SPIClass touchSPI(HSPI);  // VSPI já ocupado pelo TFT_eSPI
XPT2046_Touchscreen ts(TOUCH_CS);

// ─── SD CARD (SoftSPI — sem conflito com display ou touch) ───
typedef SoftSpiDriver<SD_MISO, SD_MOSI, SD_SCK> SdSoftSpi;
SdSoftSpi softSpi;
SdFat32   sd;
File32    logFile;
bool      sdOk      = false;
char      logPath[32];

enum UiScreen {
    SCREEN_BOOT,
    SCREEN_SETTINGS,
    SCREEN_TRACK_MENU,
    SCREEN_READY,
    SCREEN_WIFI,
    SCREEN_LOGGING,
    SCREEN_SHUTDOWN_CONFIRM
};

// ─── ESTADO DA APLICAÇÃO ─────────────────────────────────────
bool isLogging = false;
bool wifiMode  = false;
WebServer httpServer(80);
UiScreen currentScreen = SCREEN_BOOT;
UiScreen shutdownReturnScreen = SCREEN_BOOT;
UiScreen wifiReturnScreen = SCREEN_READY;
int      trackMenuHighlight = 0;
uint8_t  brightnessIndex = 3;

// Botão INICIAR LOG (tela pronta)
#define ILOG_X  60
#define ILOG_Y  148
#define ILOG_W  200
#define ILOG_H  48

// Zona ENCERRAR: centro do top bar (entre GPS e velocidade)
#define STOP_X1  90
#define STOP_X2  210

// ─── INSTÂNCIAS ──────────────────────────────────────────────
TFT_eSPI       tft;
TinyGPSPlus    gps;
HardwareSerial gpsSerial(2);

// ─── ESTADO ──────────────────────────────────────────────────
float    curSpeed    = 0.0f;
uint32_t curLapMs   = 0;
uint32_t lastLapMs  = 0;
uint32_t bestLapMs  = 0;
int      lapCount   = 0;
bool     lapActive  = false;
uint32_t lapStartMs = 0;
float    deltaS     = 0.0f;
bool     newBest    = false;
int      satellites = 0;
bool     gpsOk      = false;

// ─── CACHE anti-flickering ────────────────────────────────────
float    cacheSpeed   = -1;
uint32_t cacheCurLap  = UINT32_MAX;
uint32_t cacheBestLap = UINT32_MAX;
uint32_t cacheLastLap = UINT32_MAX;
int      cacheLapCnt  = -1;
float    cacheDelta   = -999;
bool     cacheGpsOk   = false;
int      cacheSats    = -1;
bool     cacheNewBest = false;

// ─── ANIMAÇÃO DE RECORDE ─────────────────────────────────────
bool          recordAnim    = false;
int           recordFlashes = 0;
unsigned long recordTimer   = 0;
#define RECORD_FLASH_MS  250
#define RECORD_FLASHES   6

// ─── FILTRO DE VELOCIDADE (média móvel) ──────────────────────
#define SPEED_SAMPLES 5
float   speedBuf[SPEED_SAMPLES] = {};
uint8_t speedBufIdx = 0;

// ─── GPS / DEMO ──────────────────────────────────────────────
bool     wasNearFinish = false;
uint32_t demoLastMs   = 0;
float    demoSpeed    = 0.0f;
bool     demoUp       = true;
uint32_t demoLapStart = 0;
float    mockDelta    = 0.0f;
bool     mockDeltaDir = true;
unsigned long lastMockTick = 0;

void shutdownSystem();
void drawShutdownConfirm();
void drawBackButton();
bool isBackPressed(int sx, int sy);
int waitForTrackSelection();
void runStartupFlow();
void applyBrightness();
void drawSettingsButton();
void drawSettingsScreen();
void drawBrightnessModal();
bool isSettingsPressed(int sx, int sy);
void enterWiFiMode();
void exitWiFiMode();

// ─────────────────────────────────────────────────────────────
// FILTRO DE VELOCIDADE
// ─────────────────────────────────────────────────────────────
float smoothSpeed(float raw) {
    speedBuf[speedBufIdx] = raw;
    speedBufIdx = (speedBufIdx + 1) % SPEED_SAMPLES;
    float sum = 0;
    for (uint8_t i = 0; i < SPEED_SAMPLES; i++) sum += speedBuf[i];
    return sum / SPEED_SAMPLES;
}

void resetSpeedFilter() {
    memset(speedBuf, 0, sizeof(speedBuf));
    speedBufIdx = 0;
}

// ─────────────────────────────────────────────────────────────
// CONFIGURAÇÃO GPS (10 Hz, 115200 baud)
// ─────────────────────────────────────────────────────────────
void configureGPS() {
    gpsSerial.end();
    gpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
    delay(200);

    // Reset frio — limpa estado ruim de sessões anteriores
    // UBX CFG-RST: clearMask=0xFFFF (tudo), resetMode=0x01 (controlled software reset)
    static const uint8_t coldReset[] = {
        0xB5, 0x62, 0x06, 0x04, 0x04, 0x00,
        0xFF, 0xFF, 0x01, 0x00,
        0x0E, 0x61
    };
    gpsSerial.write(coldReset, sizeof(coldReset));
    gpsSerial.flush();
    delay(1000); // aguarda módulo reiniciar

    Serial.println("[GPS] Reset frio enviado, aguardando...");
    delay(500);
}

// ─────────────────────────────────────────────────────────────
// FORMATAÇÃO
// ─────────────────────────────────────────────────────────────
String formatTime(uint32_t ms) {
    if (ms == 0) return "--:--.--";
    char buf[12];
    snprintf(buf, sizeof(buf), "%u:%02u.%02u",
        ms / 60000, (ms % 60000) / 1000, (ms % 1000) / 10);
    return String(buf);
}

String formatDelta(float d) {
    char buf[10];
    snprintf(buf, sizeof(buf), "%s%.2f", d >= 0 ? "+" : "", d);
    return String(buf);
}

String fmtLap(uint32_t ms) {
    if (ms == 0) return "--:--.---";
    char buf[16];
    snprintf(buf, sizeof(buf), "%d:%02d.%03d",
        (int)(ms / 60000), (int)((ms % 60000) / 1000), (int)(ms % 1000));
    return String(buf);
}

// ─────────────────────────────────────────────────────────────
// ANIMAÇÃO DE RECORDE
// ─────────────────────────────────────────────────────────────
void handleRecordAnimation() {
    if (!recordAnim) return;
    unsigned long now = millis();
    if (now - recordTimer >= RECORD_FLASH_MS) {
        recordTimer = now;
        recordFlashes++;
        if (recordFlashes >= RECORD_FLASHES * 2) {
            recordAnim = false;
            newBest    = false;
        }
    }
}

uint16_t bestColor() {
    if (!recordAnim) return C_AMBER;
    return (recordFlashes % 2 == 0) ? C_PURPLE : C_AMBER;
}

// ─────────────────────────────────────────────────────────────
// LAYOUT ESTÁTICO — desenhado uma vez no setup
// ─────────────────────────────────────────────────────────────
void drawStaticLayout() {
    tft.fillScreen(C_BG);

    // Top bar
    tft.fillRect(0, TOP_Y, W, TOP_H, C_PANEL);
    tft.drawFastHLine(0, TOP_H,   W, C_RED_F1);
    tft.drawFastHLine(0, TOP_H+1, W, C_RED_F1);

    // Footer com bg solido (separa zona claramente)
    tft.fillRect(0, BOT_Y, W, BOT_H, C_PANEL);

    // Divider gradiente acima do footer: border -> red -> border
    for (int i = 0; i < W; i++) {
        float t = (float)i / (W - 1);
        uint16_t lc;
        if (t < 0.20f || t > 0.80f) {
            lc = C_BORDER;
        } else if (t < 0.40f) {
            float k = (t - 0.20f) / 0.20f;
            uint8_t r = 42 + (uint8_t)((232 - 42) * k);
            uint8_t g = 42 + (uint8_t)((  0 - 42) * k);
            uint8_t b = 64 + (uint8_t)(( 30 - 64) * k);
            lc = tft.color565(r, g, b);
        } else if (t > 0.60f) {
            float k = (t - 0.60f) / 0.20f;
            uint8_t r = 232 + (uint8_t)((42 - 232) * k);
            uint8_t g =   0 + (uint8_t)((42 -   0) * k);
            uint8_t b =  30 + (uint8_t)((64 -  30) * k);
            lc = tft.color565(r, g, b);
        } else {
            lc = C_RED_F1;
        }
        tft.drawPixel(i, BOT_Y - 3, lc);
        tft.drawPixel(i, BOT_Y - 2, lc);
    }

    // Divisor vertical entre MELHOR e ULTIMA no footer
    tft.drawFastVLine(W/2, BOT_Y + 8, BOT_H - 16, C_BORDER);

    // Labels area central
    tft.setTextFont(1);
    tft.setTextSize(1);

    // VOLTA - label esquerdo (DELTA agora e badge, sem label fixo)
    tft.setTextColor(C_GRAY, C_BG);
    tft.setTextDatum(ML_DATUM);
    tft.drawString("VOLTA", 10, MID_Y + 10);

    // Labels barra de delta
    tft.setTextColor(C_RED, C_BG);
    tft.setTextDatum(ML_DATUM);
    tft.drawString("PERDA", 8, BAR_Y - 8);
    tft.setTextColor(C_GREEN, C_BG);
    tft.setTextDatum(MR_DATUM);
    tft.drawString("GANHO", W - 8, BAR_Y - 8);

    // Ticks da barra
    tft.setTextColor(C_GRAY_DK, C_BG);
    tft.setTextDatum(ML_DATUM);
    tft.drawString("-2.0", 8, BAR_Y + BAR_H + 6);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("0", W/2, BAR_Y + BAR_H + 6);
    tft.setTextDatum(MR_DATUM);
    tft.drawString("+2.0", W - 8, BAR_Y + BAR_H + 6);

    // Labels footer (sobre C_PANEL agora)
    tft.setTextColor(C_GRAY, C_PANEL);
    tft.setTextDatum(ML_DATUM);
    tft.drawString("MELHOR", BEST_X, BOT_TEXT_Y - 10);
    tft.setTextDatum(MR_DATUM);
    tft.drawString("ULTIMA", W - 6, BOT_TEXT_Y - 10);
}

// ─────────────────────────────────────────────────────────────
// GPS STATUS (barra superior esquerda)
// ─────────────────────────────────────────────────────────────
void drawGPS() {
    if (gpsOk == cacheGpsOk && satellites == cacheSats) return;
    cacheGpsOk = gpsOk;
    cacheSats  = satellites;

    tft.fillRect(6, 4, 90, TOP_H - 8, C_PANEL);

    uint16_t dotCol = gpsOk ? C_GREEN : C_RED;
    tft.fillCircle(12, TOP_H/2, 4, dotCol);
    if (gpsOk) tft.drawCircle(12, TOP_H/2, 5, tft.color565(0, 60, 30));

    tft.setTextFont(1);
    tft.setTextSize(1);
    tft.setTextColor(gpsOk ? C_WHITE : C_RED, C_PANEL);
    tft.setTextDatum(ML_DATUM);
    tft.drawString("GPS " + String(satellites), 22, TOP_H/2);
}

// ─────────────────────────────────────────────────────────────
// VELOCIDADE (barra superior direita)
// ─────────────────────────────────────────────────────────────
void drawSpeed() {
    if (fabsf(curSpeed - cacheSpeed) < 1.0f) return;
    cacheSpeed = curSpeed;

    tft.fillRect(SPD_X, 4, SPD_W, TOP_H - 8, C_PANEL);

    char buf[8];
    snprintf(buf, sizeof(buf), "%03d", (int)curSpeed);

    // Numero em ciano (Font 2)
    tft.setTextFont(2);
    tft.setTextSize(1);
    tft.setTextColor(C_SPEED, C_PANEL);
    tft.setTextDatum(MR_DATUM);
    tft.drawString(buf, W - 36, TOP_H/2);

    // Unidade em cinza (Font 1)
    tft.setTextFont(1);
    tft.setTextColor(C_GRAY, C_PANEL);
    tft.setTextDatum(MR_DATUM);
    tft.drawString("km/h", W - 6, TOP_H/2 + 1);
}

// ─────────────────────────────────────────────────────────────
// NÚMERO DE VOLTA
// ─────────────────────────────────────────────────────────────
void drawLapCount() {
    if (lapCount == cacheLapCnt) return;
    cacheLapCnt = lapCount;

    tft.fillRect(LAP_X, MID_Y + 2, LAP_W, 16, C_BG);
    tft.setTextFont(2);
    tft.setTextSize(1);
    tft.setTextColor(C_WHITE, C_BG);
    tft.setTextDatum(ML_DATUM);
    tft.drawString(String(lapCount), LAP_X, MID_Y + 10);
}

// ─────────────────────────────────────────────────────────────
// DELTA NUMÉRICO
// ─────────────────────────────────────────────────────────────
void drawDeltaNum() {
    if (fabsf(deltaS - cacheDelta) < 0.01f) return;
    cacheDelta = deltaS;

    const int bx = DELTA_NUM_X + 4;
    const int by = MID_Y + 2;
    const int bw = DELTA_NUM_W - 8;
    const int bh = 18;
    tft.fillRect(DELTA_NUM_X, MID_Y, DELTA_NUM_W, bh + 4, C_BG);

    char buf[12];
    snprintf(buf, sizeof(buf), "%+.2fs", deltaS);

    // Estado neutro: sem badge, so texto cinza
    if (fabsf(deltaS) < 0.05f) {
        tft.setTextFont(2);
        tft.setTextSize(1);
        tft.setTextColor(C_GRAY, C_BG);
        tft.setTextDatum(MR_DATUM);
        tft.drawString("0.00s", W - 8, by + bh/2);
        return;
    }

    bool     gain      = (deltaS < 0);
    uint16_t bgCol     = gain ? tft.color565( 0, 50, 22)   // green-dim
                              : tft.color565(60,  6, 14);  // red-dim
    uint16_t borderCol = gain ? C_GREEN : C_RED;
    uint16_t textCol   = gain ? C_GREEN : C_RED;

    tft.fillRoundRect(bx, by, bw, bh, 4, bgCol);
    tft.drawRoundRect(bx, by, bw, bh, 4, borderCol);

    tft.setTextFont(2);
    tft.setTextSize(1);
    tft.setTextColor(textCol, bgCol);
    tft.setTextDatum(MR_DATUM);
    tft.drawString(buf, bx + bw - 6, by + bh/2 + 1);
}

// ─────────────────────────────────────────────────────────────
// TEMPO ATUAL — elemento dominante (Font 7 = 7 segmentos)
// ─────────────────────────────────────────────────────────────
void drawCurrentLap() {
    // Atualiza a cada ~20ms (centésimos visíveis)
    if (curLapMs != 0 && (int32_t)(curLapMs - cacheCurLap) < 20
        && cacheCurLap != UINT32_MAX && !newBest) return;
    cacheCurLap = curLapMs;

    tft.fillRect(TIME_X, TIME_Y, TIME_W, TIME_H, C_BG);

    // Cor: roxo se bateu recorde, verde se melhor que o best, branco senão
    uint16_t col = C_WHITE;
    if (newBest)                                          col = C_PURPLE;
    else if (bestLapMs > 0 && lapActive && deltaS < 0)   col = C_GREEN;

    // M:SS em Font 7 (48px, 7 segmentos)
    String msStr = "";
    String csStr = "--";
    if (curLapMs > 0 || lapActive) {
        uint32_t ms = curLapMs;
        int m = ms / 60000;
        int s = (ms % 60000) / 1000;
        int c = (ms % 1000) / 10;
        char buf[12];
        snprintf(buf, sizeof(buf), "%d:%02d.", m, s);
        msStr = String(buf);
        snprintf(buf, sizeof(buf), "%02d", c);
        csStr = String(buf);
    } else {
        msStr = "--:--.";
    }

    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(col, C_BG);
    tft.setTextFont(7);
    tft.setTextSize(1);
    tft.drawString(msStr, 236, TIME_Y);

    // Centésimos em vermelho F1, Font 6
    tft.setTextFont(6);
    tft.setTextColor(C_RED_F1, C_BG);
    tft.setTextDatum(TL_DATUM);
    tft.drawString(csStr, 240, TIME_Y + 10);

    // Label flutuante minusculo no canto inferior direito
    tft.setTextFont(1);
    tft.setTextSize(1);
    tft.setTextColor(C_GRAY_DK, C_BG);
    tft.setTextDatum(BR_DATUM);
    tft.drawString("TEMPO ATUAL", W - 8, TIME_Y + TIME_H - 2);
}

// ─────────────────────────────────────────────────────────────
// BARRA DE DELTA
// ─────────────────────────────────────────────────────────────
void drawDeltaBar() {
    static float cacheBarDelta = -999.0f;
    if (fabsf(deltaS - cacheBarDelta) < 0.02f) return;
    cacheBarDelta = deltaS;

    const float maxDelt = 2.0f;
    const int   radius  = BAR_H / 2;
    const int   cx      = BAR_X + BAR_W/2;

    // Trilho com borda arredondada
    tft.fillRoundRect(BAR_X, BAR_Y, BAR_W, BAR_H, radius, C_PANEL2);
    tft.drawRoundRect(BAR_X, BAR_Y, BAR_W, BAR_H, radius, C_BORDER);

    // Marcas internas em -1.0 e +1.0
    tft.drawFastVLine(BAR_X +     BAR_W/4, BAR_Y + 3, BAR_H - 6, C_BORDER);
    tft.drawFastVLine(BAR_X + 3 * BAR_W/4, BAR_Y + 3, BAR_H - 6, C_BORDER);

    float ratio = constrain(fabsf(deltaS) / maxDelt, 0.0f, 1.0f);
    int   fillW = (int)(ratio * (BAR_W/2 - 2));

    // Gradiente em strips: intenso na extremidade, esmaece p/ centro
    if (deltaS > 0.05f && fillW > 0) {
        int x0 = cx - fillW;
        for (int i = 0; i < fillW; i++) {
            float k = 1.0f - (float)i / (float)fillW;
            uint8_t r = (uint8_t)(255 * (0.25f + 0.75f * k));
            uint8_t g = (uint8_t)( 23 * k);
            uint8_t b = (uint8_t)( 68 * (0.30f + 0.70f * k));
            tft.drawFastVLine(x0 + i, BAR_Y + 2, BAR_H - 4, tft.color565(r, g, b));
        }
    } else if (deltaS < -0.05f && fillW > 0) {
        int x0 = cx + 1;
        for (int i = 0; i < fillW; i++) {
            float k = 1.0f - (float)i / (float)fillW;
            uint8_t r = (uint8_t)(  0 * k);
            uint8_t g = (uint8_t)(230 * (0.25f + 0.75f * k));
            uint8_t b = (uint8_t)(118 * (0.30f + 0.70f * k));
            tft.drawFastVLine(x0 + i, BAR_Y + 2, BAR_H - 4, tft.color565(r, g, b));
        }
    }

    // Centro branco
    tft.fillRect(cx - 1, BAR_Y - 1, 3, BAR_H + 2, C_WHITE);
}

// ─────────────────────────────────────────────────────────────
// MELHOR VOLTA
// ─────────────────────────────────────────────────────────────
void drawBestLap() {
    if (bestLapMs == cacheBestLap && cacheNewBest == newBest) return;
    cacheBestLap = bestLapMs;
    cacheNewBest = newBest;

    tft.fillRect(BEST_X, BOT_TEXT_Y, BEST_W, BOT_H - 22, C_PANEL);
    tft.loadFont(f1_time_font);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(bestColor(), C_PANEL);
    tft.drawString(formatTime(bestLapMs), BEST_X, BOT_TEXT_Y + 8);
    tft.unloadFont();
}

// ─────────────────────────────────────────────────────────────
// ÚLTIMA VOLTA
// ─────────────────────────────────────────────────────────────
void drawLastLap() {
    if (lastLapMs == cacheLastLap) return;
    cacheLastLap = lastLapMs;

    tft.fillRect(LAST_X, BOT_TEXT_Y, LAST_W, BOT_H - 22, C_PANEL);
    tft.loadFont(f1_time_font);
    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(C_GRAY, C_PANEL);
    tft.drawString(formatTime(lastLapMs), W - 6, BOT_TEXT_Y + 8);
    tft.unloadFont();
}

// ─────────────────────────────────────────────────────────────
// FLASH DE RECORDE (borda roxa pulsante)
// ─────────────────────────────────────────────────────────────
void drawRecordBorder() {
    if (!recordAnim) return;
    if (recordFlashes % 2 == 0) {
        tft.drawRect(0, 0, W, H, C_PURPLE);
        tft.drawRect(1, 1, W-2, H-2, C_PURPLE);
        tft.drawRect(2, 2, W-4, H-4, C_PURPLE);
    } else {
        tft.drawRect(0, 0, W, H, C_BG);
        tft.drawRect(1, 1, W-2, H-2, C_BG);
        tft.drawRect(2, 2, W-4, H-4, C_BG);
    }
}

// ─────────────────────────────────────────────────────────────
// NOVA VOLTA
// ─────────────────────────────────────────────────────────────
void triggerNewLap() {
    uint32_t now = millis();
    newBest = false;

    if (lapActive) {
        uint32_t lap = now - lapStartMs;
        lastLapMs = lap;
        lapCount++;
        if (bestLapMs == 0 || lap < bestLapMs) {
            bestLapMs   = lap;
            newBest     = true;
            recordAnim  = true;
            recordFlashes = 0;
            recordTimer = now;
        }
    }

    lapStartMs = now;
    lapActive  = true;
    curLapMs   = 0;
    cacheCurLap = UINT32_MAX;
}

// ─────────────────────────────────────────────────────────────
// GPS — LINHA DE CHEGADA
// ─────────────────────────────────────────────────────────────
void checkGPSFinishLine() {
    if (!gps.location.isValid()) return;
    const Track &t = TRACKS[selectedTrack];
    double dist = TinyGPSPlus::distanceBetween(
        gps.location.lat(), gps.location.lng(),
        t.finishLat, t.finishLon);
    bool near = dist < t.finishRadius;
    if (near && !wasNearFinish) triggerNewLap();
    wasNearFinish = near;
}

// ─────────────────────────────────────────────────────────────
// DEMO
// ─────────────────────────────────────────────────────────────
void updateDemo() {
    uint32_t now = millis();

    if (now - demoLastMs >= 40) {
        demoLastMs = now;
        if (demoUp) { demoSpeed += 0.5f; if (demoSpeed >= 95) demoUp = false; }
        else        { demoSpeed -= 0.3f; if (demoSpeed <= 0)  { demoSpeed = 0; demoUp = true; } }
        curSpeed   = demoSpeed;
        gpsOk      = true;
        satellites = 12;
    }

    if (now - lastMockTick >= 100) {
        lastMockTick = now;
        if (mockDeltaDir) { mockDelta += 0.05f; if (mockDelta >= 2.0f) mockDeltaDir = false; }
        else              { mockDelta -= 0.05f; if (mockDelta <= -2.0f) mockDeltaDir = true; }
        deltaS = mockDelta;
    }

    if (demoLapStart == 0) demoLapStart = now;
    if (now - demoLapStart > 65000) {
        demoLapStart = now;
        triggerNewLap();
    }
}

// ─────────────────────────────────────────────────────────────
// SPLASH SCREEN
// ─────────────────────────────────────────────────────────────
#define BTN_X  80
#define BTN_Y  144
#define BTN_W  160
#define BTN_H   42

const uint8_t BRIGHTNESS_LEVELS[] = {24, 72, 144, 255};

void drawBootScreen() {
    currentScreen = SCREEN_BOOT;
    tft.fillScreen(C_BG);

    // Cantos informativos
    tft.setTextFont(1);
    tft.setTextSize(1);
    tft.setTextColor(C_GRAY_DK, C_BG);
    tft.setTextDatum(TL_DATUM);
    tft.drawString("ESP32 CYD", 8, 8);
    tft.setTextDatum(TR_DATUM);
    tft.drawString("v2.0", W - 8, 8);

    // Logo centralizado
    const int logoX = (W - LOGO_W) / 2;
    const int logoY = 16;
    tft.pushImage(logoX, logoY, LOGO_W, LOGO_H, logo_kartup);
    tft.fillRect(logoX, logoY,                  LOGO_W, 4, C_BG);
    tft.fillRect(logoX, logoY + LOGO_H - 4,     LOGO_W, 4, C_BG);

    // Divider gradiente curto sob o logo (pico vermelho no centro)
    const int dvY = logoY + LOGO_H + 6;
    const int dvW = 140;
    const int dvX = (W - dvW) / 2;
    for (int i = 0; i < dvW; i++) {
        float t = (float)i / (dvW - 1);
        float k = 1.0f - fabsf(t - 0.5f) * 2.0f;
        uint8_t r = (uint8_t)(232 * k);
        uint8_t g = 0;
        uint8_t b = (uint8_t)(30 * k);
        tft.drawPixel(dvX + i, dvY, tft.color565(r, g, b));
    }

    // Tagline
    tft.setTextFont(1);
    tft.setTextColor(C_GRAY, C_BG);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("TELEMETRIA  DE  KART", W/2, dvY + 10);

    drawSettingsButton();

    // Botao START (mantem touch zone)
    tft.fillRoundRect(BTN_X, BTN_Y, BTN_W, BTN_H, 8, C_RED_F1);
    tft.drawRoundRect(BTN_X - 1, BTN_Y - 1, BTN_W + 2, BTN_H + 2, 9, C_RED_BRT);
    tft.setTextFont(4);
    tft.setTextColor(C_WHITE, C_RED_F1);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("START", W/2, BTN_Y + BTN_H/2);

    // Progress bar fininha abaixo do botao (decoracao)
    const int pbX = (W - 160) / 2;
    const int pbY = BTN_Y + BTN_H + 10;
    tft.fillRect(pbX, pbY, 160, 2, C_GRAY_DK);
    tft.fillRect(pbX, pbY,  96, 2, C_RED_F1);

    tft.setTextFont(1);
    tft.setTextColor(C_GRAY_DK, C_BG);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("kartUp.pro", W/2, H - 12);
}

// Helper: desenha um item de settings com icon box tintada + badge
static void drawSettingsItem(int x, int y, int w, int h,
                             uint16_t iconBgTint,
                             const uint16_t* iconBmp, int iconW, int iconH,
                             const char* title, const char* desc,
                             const char* badgeText, uint16_t badgeColor)
{
    // Card
    tft.fillRoundRect(x, y, w, h, 8, C_PANEL);
    tft.drawRoundRect(x, y, w, h, 8, C_BORDER);

    // Icon box tintada a esquerda
    const int iboxW = 28, iboxH = 28;
    const int iboxX = x + 8;
    const int iboxY = y + (h - iboxH) / 2;
    tft.fillRoundRect(iboxX, iboxY, iboxW, iboxH, 6, iconBgTint);
    tft.pushImage(iboxX + (iboxW - iconW) / 2,
                  iboxY + (iboxH - iconH) / 2,
                  iconW, iconH, iconBmp, iconBgTint);

    // Titulo + descricao
    tft.setTextDatum(ML_DATUM);
    tft.setTextFont(2);
    tft.setTextColor(C_WHITE, C_PANEL);
    tft.drawString(title, iboxX + iboxW + 10, y + 15);

    tft.setTextFont(1);
    tft.setTextColor(C_GRAY, C_PANEL);
    tft.drawString(desc, iboxX + iboxW + 10, y + 31);

    // Badge de status (canto direito)
    if (badgeText && badgeText[0]) {
        tft.setTextFont(1);
        int bw = tft.textWidth(badgeText) + 12;
        int bh = 14;
        int bx = x + w - bw - 22;
        int by = y + (h - bh) / 2;
        tft.fillRoundRect(bx, by, bw, bh, 4, C_BG);
        tft.drawRoundRect(bx, by, bw, bh, 4, C_BORDER);
        tft.setTextColor(badgeColor, C_BG);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(badgeText, bx + bw/2, by + bh/2 + 1);
    }

    // Chevron
    tft.setTextDatum(MR_DATUM);
    tft.setTextFont(2);
    tft.setTextColor(C_GRAY, C_PANEL);
    tft.drawString(">", x + w - 8, y + h/2);
}

void drawSettingsScreen() {
    currentScreen = SCREEN_SETTINGS;
    tft.fillScreen(C_BG);

    // Top bar
    tft.fillRect(0, 0, W, TOP_H, C_PANEL);
    tft.drawFastHLine(0, TOP_H,   W, C_RED_F1);
    tft.drawFastHLine(0, TOP_H+1, W, C_RED_F1);
    drawBackButton();

    tft.setTextFont(2);
    tft.setTextColor(C_WHITE, C_PANEL);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Settings", W/2, TOP_H/2);

    tft.setTextFont(1);
    tft.setTextColor(C_GRAY, C_BG);
    tft.setTextDatum(ML_DATUM);
    tft.drawString("AJUSTES  DO  SISTEMA", 14, 36);

    // Item BRILHO - icon box amber, badge com % atual
    const char* pct[4] = {"25%", "50%", "75%", "100%"};
    drawSettingsItem(BRIGHTNESS_BTN_X, BRIGHTNESS_BTN_Y,
                     BRIGHTNESS_BTN_W, BRIGHTNESS_BTN_H,
                     tft.color565(60, 38, 6),
                     settings_icon, SETTINGS_ICON_W, SETTINGS_ICON_H,
                     "Brilho", "Ajustar iluminacao da tela",
                     pct[brightnessIndex], C_AMBER);

    // Item WiFi - icon box ciano, badge ON/OFF
    drawSettingsItem(WIFI_AP_BTN_X, WIFI_AP_BTN_Y,
                     WIFI_AP_BTN_W, WIFI_AP_BTN_H,
                     tft.color565(0, 36, 60),
                     wifi_icon, WIFI_ICON_W, WIFI_ICON_H,
                     "WiFi AP", "Arquivos pelo navegador",
                     wifiMode ? "ON" : "OFF",
                     wifiMode ? C_GREEN : C_GRAY);

    tft.setTextFont(1);
    tft.setTextColor(C_GRAY_DK, C_BG);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("kartUp.pro", W/2, H - 14);
}

// Calibração aproximada CYD ESP32-2432S028R (rotation 1)
// Ajuste X/Y_MIN/MAX se o mapeamento estiver errado
#define T_X_MIN  300
#define T_X_MAX  3900
#define T_Y_MIN  300
#define T_Y_MAX  3900

void touchToScreen(TS_Point p, int &sx, int &sy) {
    sx = constrain(map(p.x,        T_X_MIN, T_X_MAX, 0, W - 1), 0, W - 1);
    sy = constrain(map(p.y,        T_Y_MIN, T_Y_MAX, 0, H - 1), 0, H - 1);
}

bool inRect(int sx, int sy, int x, int y, int w, int h, int pad = 0) {
    return sx >= x - pad && sx <= x + w + pad &&
           sy >= y - pad && sy <= y + h + pad;
}

void drawSettingsButton() {
    tft.fillRoundRect(SETTINGS_BTN_X, SETTINGS_BTN_Y, SETTINGS_BTN_W, SETTINGS_BTN_H, 8, C_PANEL);
    tft.drawRoundRect(SETTINGS_BTN_X, SETTINGS_BTN_Y, SETTINGS_BTN_W, SETTINGS_BTN_H, 8, C_BORDER);
    tft.pushImage(SETTINGS_BTN_X + (SETTINGS_BTN_W - SETTINGS_ICON_W) / 2,
                  SETTINGS_BTN_Y + (SETTINGS_BTN_H - SETTINGS_ICON_H) / 2,
                  SETTINGS_ICON_W, SETTINGS_ICON_H, settings_icon, TFT_BLACK);
}

bool isSettingsPressed(int sx, int sy) {
    return inRect(sx, sy, SETTINGS_BTN_X, SETTINGS_BTN_Y, SETTINGS_BTN_W, SETTINGS_BTN_H, SETTINGS_TOUCH_PAD);
}

void drawBackButton() {
    tft.fillRoundRect(BACK_BTN_X, BACK_BTN_Y, BACK_BTN_W, BACK_BTN_H, 5, C_PANEL);
    tft.drawRoundRect(BACK_BTN_X, BACK_BTN_Y, BACK_BTN_W, BACK_BTN_H, 5, C_BORDER);
    tft.setTextFont(2);
    tft.setTextColor(C_WHITE, C_PANEL);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("<", BACK_BTN_X + BACK_BTN_W / 2, BACK_BTN_Y + BACK_BTN_H / 2);
}

bool isBackPressed(int sx, int sy) {
    return sy <= (TOP_H + 6) && sx <= 72;
}

bool inButton(int sx, int sy) {
    return sx >= BTN_X && sx <= BTN_X + BTN_W &&
           sy >= BTN_Y && sy <= BTN_Y + BTN_H;
}

void applyBrightness() {
    uint8_t duty = BRIGHTNESS_LEVELS[brightnessIndex];
    analogWrite(TFT_BL, duty);
    Serial.printf("[BRIGHTNESS] idx=%d duty=%d\n", brightnessIndex, duty);
}

void drawBrightnessModal() {
    const int modalX = 36;
    const int modalY = 74;
    const int modalW = 248;
    const int modalH = 112;
    const int btnY = 126;
    const int btnW = 50;
    const int btnH = 32;
    const int btnGap = 6;
    const int btnStartX = 49;
    const char* labels[4] = {"25", "50", "75", "100"};

    tft.fillRoundRect(modalX, modalY, modalW, modalH, 10, C_PANEL);
    tft.drawRoundRect(modalX, modalY, modalW, modalH, 10, C_BORDER);
    tft.setTextDatum(MC_DATUM);
    tft.setTextFont(2);
    tft.setTextColor(C_WHITE, C_PANEL);
    tft.drawString("Brilho", W / 2, 94);
    tft.setTextFont(1);
    tft.setTextColor(C_GRAY, C_PANEL);
    tft.drawString("Toque para ajustar", W / 2, 110);

    for (int i = 0; i < 4; i++) {
        int x = btnStartX + i * (btnW + btnGap);
        uint16_t fill = (i == brightnessIndex) ? C_RED_F1 : C_BG;
        uint16_t text = (i == brightnessIndex) ? C_WHITE : C_GRAY;
        uint16_t border = (i == brightnessIndex) ? C_WHITE : C_BORDER;
        tft.fillRoundRect(x, btnY, btnW, btnH, 6, fill);
        tft.drawRoundRect(x, btnY, btnW, btnH, 6, border);
        tft.setTextFont(2);
        tft.setTextColor(text, fill);
        tft.drawString(labels[i], x + btnW / 2, btnY + btnH / 2);
    }

    while (ts.getPoint().z > 100) delay(10);

    while (true) {
        delay(20);
        TS_Point p = ts.getPoint();
        if (p.z < 200) continue;
        int sx, sy;
        touchToScreen(p, sx, sy);
        Serial.printf("[BRIGHTNESS_TOUCH] screen(%d,%d)\n", sx, sy);

        if (!inRect(sx, sy, modalX, modalY, modalW, modalH, 8)) {
            while (ts.getPoint().z > 100) delay(10);
            drawSettingsScreen();
            return;
        }

        for (int i = 0; i < 4; i++) {
            int x = btnStartX + i * (btnW + btnGap);
            if (inRect(sx, sy, x, btnY, btnW, btnH, 10)) {
                brightnessIndex = i;
                applyBrightness();
                while (ts.getPoint().z > 100) delay(10);
                drawSettingsScreen();
                return;
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────
// MENU DE PISTAS
// ─────────────────────────────────────────────────────────────
#define CARD_X   10
#define CARD_Y   56
#define CARD_W  300
#define CARD_H   96
#define CARDS_PER_PAGE 2

void drawTrackCard(int idx, int cardY, bool selected) {
    const Track &t = TRACKS[idx];
    uint16_t borderCol = selected ? C_RED_F1 : C_BORDER;

    tft.fillRoundRect(CARD_X, cardY, CARD_W, CARD_H, 6, C_PANEL);
    tft.drawRoundRect(CARD_X, cardY, CARD_W, CARD_H, 6, borderCol);
    if (selected) {
        tft.drawRoundRect(CARD_X + 1, cardY + 1, CARD_W - 2, CARD_H - 2, 5, C_RED_F1);
    }

    // Faixa lateral vermelha
    tft.fillRoundRect(CARD_X, cardY, 6, CARD_H, 6, C_RED_F1);
    tft.fillRect(CARD_X + 3, cardY, 3, CARD_H, C_RED_F1);

    // Nome da pista
    tft.setTextDatum(ML_DATUM);
    tft.setTextFont(4);
    tft.setTextColor(C_WHITE, C_PANEL);
    tft.drawString(t.name, CARD_X + 18, cardY + 28);

    // Localização
    tft.setTextFont(2);
    tft.setTextColor(C_GRAY, C_PANEL);
    tft.drawString(t.location, CARD_X + 18, cardY + 54);

    // Coordenadas
    tft.setTextFont(1);
    tft.setTextColor(C_GRAY_DK, C_PANEL);
    char buf[40];
    snprintf(buf, sizeof(buf), "%.4f, %.4f  r=%.0fm",
        t.finishLat, t.finishLon, t.finishRadius);
    tft.drawString(buf, CARD_X + 18, cardY + 72);
}

void drawTrackMenu(int highlight) {
    currentScreen = SCREEN_TRACK_MENU;
    trackMenuHighlight = highlight;
    tft.fillScreen(C_BG);

    // Header
    tft.fillRect(0, 0, W, 24, C_PANEL);
    tft.drawFastHLine(0, 24, W, C_RED_F1);
    tft.drawFastHLine(0, 25, W, C_RED_F1);
    drawBackButton();
    tft.setTextFont(2);
    tft.setTextColor(C_WHITE, C_PANEL);
    tft.setTextDatum(ML_DATUM);
    tft.drawString("SELECIONAR PISTA", 46, 12);

    // Contador
    char buf[16];
    snprintf(buf, sizeof(buf), "%d pista(s)", TRACK_COUNT);
    tft.setTextColor(C_GRAY_DK, C_PANEL);
    tft.setTextDatum(MR_DATUM);
    tft.drawString(buf, W - 8, 12);

    // Cards
    for (int i = 0; i < TRACK_COUNT && i < CARDS_PER_PAGE; i++) {
        int cy = CARD_Y + i * (CARD_H + 10);
        drawTrackCard(i, cy, i == highlight);
    }

    // Dica inferior
    tft.setTextFont(1);
    tft.setTextColor(C_GRAY_DK, C_BG);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("toque na pista para selecionar", W / 2, H - 8);
}

int waitForTrackSelection() {
    int highlighted = 0;
    drawTrackMenu(highlighted);

    while (ts.getPoint().z > 100) delay(10);  // debounce

    while (true) {
        delay(20);
        TS_Point p = ts.getPoint();
        if (p.z < 200) continue;
        int sx, sy;
        touchToScreen(p, sx, sy);
        Serial.printf("[TRACK_TOUCH] raw(%d,%d) -> screen(%d,%d) back=%d\n",
            p.x, p.y, sx, sy, isBackPressed(sx, sy) ? 1 : 0);
        if (isBackPressed(sx, sy)) {
            while (ts.getPoint().z > 100) delay(10);
            return -1;
        }

        for (int i = 0; i < TRACK_COUNT && i < CARDS_PER_PAGE; i++) {
            int cy = CARD_Y + i * (CARD_H + 10);
            if (sx >= CARD_X && sx <= CARD_X + CARD_W &&
                sy >= cy      && sy <= cy + CARD_H) {
                // flash de confirmação
                drawTrackCard(i, cy, true);
                tft.fillRect(CARD_X + 2, cy + CARD_H - 14, CARD_W - 4, 10, C_RED_F1);
                tft.setTextFont(1);
                tft.setTextColor(C_WHITE, C_RED_F1);
                tft.setTextDatum(MC_DATUM);
                tft.drawString("SELECIONADO", W / 2, cy + CARD_H - 9);
                delay(600);
                return i;
            }
        }
    }
}

void waitForStart() {
    Serial.println("[TOUCH] aguardando START...");
    while (ts.getPoint().z > 100) delay(10);  // debounce
    while (true) {
        delay(20);
        if (wifiMode) httpServer.handleClient();
        TS_Point p = ts.getPoint();
        if (p.z < 200) continue;
        int sx, sy;
        touchToScreen(p, sx, sy);
        Serial.printf("[TOUCH] raw(%d,%d) → screen(%d,%d) btn=%d\n",
            p.x, p.y, sx, sy, inButton(sx, sy) ? 1 : 0);
        if (currentScreen == SCREEN_SETTINGS) {
            Serial.printf("[SETTINGS_TOUCH] screen(%d,%d) back=%d brightness=%d wifi=%d\n",
                sx, sy,
                isBackPressed(sx, sy) ? 1 : 0,
                inRect(sx, sy, BRIGHTNESS_BTN_X, BRIGHTNESS_BTN_Y, BRIGHTNESS_BTN_W, BRIGHTNESS_BTN_H, 10) ? 1 : 0,
                inRect(sx, sy, WIFI_AP_BTN_X, WIFI_AP_BTN_Y, WIFI_AP_BTN_W, WIFI_AP_BTN_H, 10) ? 1 : 0);
            if (isBackPressed(sx, sy)) {
                while (ts.getPoint().z > 100) delay(10);
                drawBootScreen();
                continue;
            }
            if (inRect(sx, sy, BRIGHTNESS_BTN_X, BRIGHTNESS_BTN_Y, BRIGHTNESS_BTN_W, BRIGHTNESS_BTN_H, 10)) {
                while (ts.getPoint().z > 100) delay(10);
                drawBrightnessModal();
                continue;
            }
            if (inRect(sx, sy, WIFI_AP_BTN_X, WIFI_AP_BTN_Y, WIFI_AP_BTN_W, WIFI_AP_BTN_H, 10)) {
                while (ts.getPoint().z > 100) delay(10);
                enterWiFiMode();
                continue;
            }
            continue;
        }
        if (currentScreen == SCREEN_WIFI) {
            Serial.printf("[WIFI_TOUCH] raw(%d,%d) -> screen(%d,%d) sair=%d\n",
                p.x, p.y, sx, sy,
                inRect(sx, sy, WIFI_SAIR_X, WIFI_SAIR_Y, WIFI_SAIR_W, WIFI_SAIR_H, 12) ? 1 : 0);
            if (inRect(sx, sy, WIFI_SAIR_X, WIFI_SAIR_Y, WIFI_SAIR_W, WIFI_SAIR_H, 12)) {
                while (ts.getPoint().z > 100) delay(10);
                exitWiFiMode();
            }
            continue;
        }
        if (isSettingsPressed(sx, sy)) {
            while (ts.getPoint().z > 100) delay(10);
            drawSettingsScreen();
            continue;
        }
        if (!inButton(sx, sy)) continue;
        // toque no botão confirmado
        tft.fillRoundRect(BTN_X, BTN_Y, BTN_W, BTN_H, 8, C_WHITE);
        tft.setTextFont(4);
        tft.setTextColor(C_RED_F1, C_WHITE);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("START", W/2, BTN_Y + BTN_H/2);
        delay(250);
        return;
    }
}

// ─────────────────────────────────────────────────────────────
// SD — INIT E LOG
// ─────────────────────────────────────────────────────────────
void initSD() {
    SdSpiConfig cfg(SD_CS_PIN, DEDICATED_SPI, SD_SCK_MHZ(2), &softSpi);
    if (!sd.begin(cfg)) {
        Serial.println("[SD] Falha ao inicializar — sem cartão ou pino CS errado");
        sd.initErrorPrint(&Serial);
        sdOk = false;
        return;
    }
    sdOk = true;

    // Cria arquivo com número de sessão incremental
    for (int i = 1; i <= 999; i++) {
        snprintf(logPath, sizeof(logPath), "gps_%03d.csv", i);
        if (!sd.exists(logPath)) break;
    }
    logFile = sd.open(logPath, O_WRITE | O_CREAT | O_TRUNC);
    if (!logFile) {
        Serial.printf("[SD] Falha ao criar %s\n", logPath);
        sdOk = false;
        return;
    }
    logFile.println("time_utc,millis,lat,lon,speed_kmh,lap,lap_ms,delta_s");
    logFile.flush();
    Serial.printf("[SD] OK — Log: %s\n", logPath);
}

void sdLog(const char* timeStr, uint32_t ms, double lat, double lon, float spd) {
    if (!sdOk || !logFile) return;
    char buf[112];
    snprintf(buf, sizeof(buf), "%s,%lu,%.7f,%.7f,%.1f,%d,%lu,%.2f",
        timeStr, ms, lat, lon, spd, lapCount, curLapMs, deltaS);
    logFile.println(buf);
    // Flush a cada 10s evita perda de dados sem travar o loop
    static uint32_t lastFlush = 0;
    if (millis() - lastFlush >= 10000) {
        lastFlush = millis();
        logFile.flush();
    }
}

// ─────────────────────────────────────────────────────────────
// TELA PRONTA (aguardando INICIAR LOG)
// ─────────────────────────────────────────────────────────────
void drawReadyScreen() {
    currentScreen = SCREEN_READY;
    tft.fillScreen(C_BG);
    tft.fillRect(0, 0, W, TOP_H, C_PANEL);
    tft.drawFastHLine(0, TOP_H,   W, C_RED_F1);
    tft.drawFastHLine(0, TOP_H+1, W, C_RED_F1);
    drawBackButton();

    tft.setTextFont(2);
    tft.setTextColor(C_WHITE, C_PANEL);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("kartUp", W/2, TOP_H/2);

    const Track &t = TRACKS[selectedTrack];
    tft.setTextFont(4);
    tft.setTextColor(C_WHITE, C_BG);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(t.name, W/2, 78);

    tft.setTextFont(2);
    tft.setTextColor(C_GRAY, C_BG);
    tft.drawString(t.location, W/2, 104);

    // Botão INICIAR LOG
    uint16_t C_GREEN_DK = tft.color565(0, 100, 0);
    tft.fillRoundRect(ILOG_X, ILOG_Y, ILOG_W, ILOG_H, 8, C_GREEN_DK);
    tft.drawRoundRect(ILOG_X-1, ILOG_Y-1, ILOG_W+2, ILOG_H+2, 9, C_GREEN);
    tft.setTextFont(4);
    tft.setTextColor(C_WHITE, C_GREEN_DK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("INICIAR LOG", W/2, ILOG_Y + ILOG_H/2);

    // Botão WiFi — baixar CSV pós-sessão
    tft.fillRoundRect(WIFI_BTN_X, WIFI_BTN_Y, WIFI_BTN_W, WIFI_BTN_H, 5, C_PANEL);
    tft.drawRoundRect(WIFI_BTN_X, WIFI_BTN_Y, WIFI_BTN_W, WIFI_BTN_H, 5, C_BORDER);
    tft.setTextFont(1);
    tft.setTextColor(C_GRAY, C_PANEL);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("WiFi / Baixar CSV", WIFI_BTN_X + WIFI_BTN_W/2, WIFI_BTN_Y + WIFI_BTN_H/2);

    tft.setTextColor(C_GRAY_DK, C_BG);
    tft.drawString("kartUp.pro", W/2, H - 8);
}

void updateReadyGPS() {
    // Atualiza indicador GPS na tela pronta (canto superior esquerdo)
    tft.fillRect(44, 4, 50, TOP_H - 8, C_PANEL);
    uint16_t col = gpsOk ? C_GREEN : C_RED;
    tft.fillCircle(50, TOP_H/2, 4, col);
    tft.setTextFont(1);
    tft.setTextColor(col, C_PANEL);
    tft.setTextDatum(ML_DATUM);
    tft.drawString("GPS " + String(satellites), 58, TOP_H/2);
}

void resetLapState() {
    curLapMs = 0; lastLapMs = 0; bestLapMs = 0;
    lapCount = 0; lapActive = false; lapStartMs = 0;
    deltaS = 0; newBest = false;
    recordAnim = false; recordFlashes = 0;
    wasNearFinish = false;
    cacheSpeed = -1; cacheCurLap = UINT32_MAX;
    cacheBestLap = UINT32_MAX; cacheLastLap = UINT32_MAX;
    cacheLapCnt = -1; cacheDelta = -999;
    cacheGpsOk = false; cacheSats = -1; cacheNewBest = false;
    resetSpeedFilter();
}

void drawStopZone() {
    // "■ STOP" clicável no centro do top bar
    tft.fillRect(STOP_X1, 2, STOP_X2 - STOP_X1, TOP_H - 4, C_PANEL);
    tft.setTextFont(1);
    tft.setTextColor(C_RED_F1, C_PANEL);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("[ ENCERRAR ]", (STOP_X1 + STOP_X2) / 2, TOP_H/2);
}

// ─────────────────────────────────────────────────────────────
// TELA WiFi
// ─────────────────────────────────────────────────────────────
void drawWiFiScreen() {
    currentScreen = SCREEN_WIFI;
    tft.fillScreen(C_BG);

    tft.fillRect(0, 0, W, TOP_H, C_PANEL);
    tft.drawFastHLine(0, TOP_H,   W, C_RED_F1);
    tft.drawFastHLine(0, TOP_H+1, W, C_RED_F1);
    tft.setTextFont(2);
    tft.setTextColor(C_RED_F1, C_PANEL);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("WiFi ATIVO", W/2, TOP_H/2);

    tft.setTextDatum(ML_DATUM);
    tft.setTextFont(2);
    tft.setTextColor(C_GRAY, C_BG);
    tft.drawString("Rede:", 16, 52);
    tft.setTextColor(C_ORANGE, C_BG);
    tft.drawString(WIFI_SSID, 72, 52);

    tft.setTextColor(C_GRAY, C_BG);
    tft.drawString("IP:", 16, 76);
    tft.setTextColor(C_ORANGE, C_BG);
    tft.drawString(WiFi.softAPIP().toString(), 72, 76);

    tft.setTextFont(1);
    tft.setTextColor(C_GRAY_DK, C_BG);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Conecte na rede e abra o IP no navegador", W/2, 112);

    tft.fillRoundRect(WIFI_SAIR_X, WIFI_SAIR_Y, WIFI_SAIR_W, WIFI_SAIR_H, 8, C_RED_F1);
    tft.drawRoundRect(WIFI_SAIR_X-1, WIFI_SAIR_Y-1, WIFI_SAIR_W+2, WIFI_SAIR_H+2, 9, C_WHITE);
    tft.setTextFont(4);
    tft.setTextColor(C_WHITE, C_RED_F1);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("SAIR", WIFI_SAIR_X + WIFI_SAIR_W/2, WIFI_SAIR_Y + WIFI_SAIR_H/2);
}

void startLogging() {
    initSD();
    resetLapState();
    lapActive    = true;
    lapStartMs   = millis();
    demoLapStart = millis();
    isLogging    = true;
    currentScreen = SCREEN_LOGGING;
    drawStaticLayout();
    drawStopZone();
    drawGPS(); drawSpeed(); drawLapCount();
    drawDeltaNum(); drawCurrentLap(); drawDeltaBar();
    drawBestLap(); drawLastLap();
    Serial.printf("[LOG] Iniciado — %s\n", sdOk ? logPath : "sem SD");
}

void stopLogging() {
    if (sdOk && logFile) { logFile.flush(); logFile.close(); }
    sdOk      = false;
    isLogging = false;
    Serial.println("[LOG] Encerrado");
    drawReadyScreen();
}

// ─────────────────────────────────────────────────────────────
// WiFi AP — HTTP HANDLERS
// ─────────────────────────────────────────────────────────────
void handleLogo() {
    const int pixDataSize = LOGO_W * LOGO_H * 3;   // 60*60*3 = 10800
    const int fileSize    = 54 + pixDataSize;       // 10854

    uint8_t* buf = (uint8_t*)malloc(fileSize);
    if (!buf) { httpServer.send(500, "text/plain", "OOM"); return; }

    // Arquivo BMP — file header (14 bytes)
    buf[0] = 'B'; buf[1] = 'M';
    buf[2]=(uint8_t)fileSize; buf[3]=(uint8_t)(fileSize>>8);
    buf[4]=(uint8_t)(fileSize>>16); buf[5]=(uint8_t)(fileSize>>24);
    buf[6]=buf[7]=buf[8]=buf[9]=0;
    buf[10]=54; buf[11]=buf[12]=buf[13]=0;

    // DIB header (40 bytes) — top-down: height negativo
    int32_t ww = LOGO_W, hh = -LOGO_H;
    buf[14]=40; buf[15]=buf[16]=buf[17]=0;
    memcpy(buf+18, &ww, 4);
    memcpy(buf+22, &hh, 4);
    buf[26]=1; buf[27]=0;    // planes = 1
    buf[28]=24; buf[29]=0;   // bpp = 24
    memset(buf+30, 0, 24);   // compression, sizes, colors (zero)

    // Pixels: RGB565 → BGR24
    uint8_t* p = buf + 54;
    for (int i = 0; i < LOGO_W * LOGO_H; i++) {
        uint16_t px = logo_kartup[i];
        *p++ = (px & 0x1F) << 3;           // B
        *p++ = ((px >> 5) & 0x3F) << 2;    // G
        *p++ = ((px >> 11) & 0x1F) << 3;   // R
    }

    httpServer.sendHeader("Cache-Control", "max-age=3600");
    httpServer.send_P(200, "image/bmp", (const char*)buf, fileSize);
    free(buf);
}

void handleRoot() {
    String html;
    html.reserve(4096);

    html = F("<!DOCTYPE html><html><head>"
        "<meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>KartUp Telemetria</title>"
        "<style>"
        "*{box-sizing:border-box;margin:0;padding:0}"
        "body{background:#0a0a0a;color:#fff;font-family:'Courier New',monospace;padding:20px;max-width:480px;margin:0 auto}"
        "header{display:flex;align-items:center;gap:14px;margin-bottom:24px;border-bottom:2px solid #E8001D;padding-bottom:14px}"
        "header img{width:60px;height:60px}"
        "header h1{font-size:1.6em;color:#E8001D;letter-spacing:2px}"
        "header p{font-size:.75em;color:#888;margin-top:2px}"
        ".card{background:#141414;border:1px solid #2a2a2a;border-radius:8px;padding:16px;margin-bottom:20px}"
        ".card h2{font-size:.75em;color:#E8001D;letter-spacing:2px;margin-bottom:12px}"
        ".row{display:flex;justify-content:space-between;padding:5px 0;border-bottom:1px solid #1e1e1e;font-size:.9em}"
        ".row:last-child{border-bottom:none}"
        ".lbl{color:#888}"
        ".val{color:#FFA500;font-weight:bold}"
        ".dp{color:#ff4444}"
        ".dn{color:#44ff44}"
        ".fi{display:flex;justify-content:space-between;align-items:center;padding:10px 0;border-bottom:1px solid #1e1e1e}"
        ".fi:last-child{border-bottom:none}"
        ".fn{color:#fff;font-size:.9em}"
        ".fs{color:#555;font-size:.8em;margin-top:2px}"
        "a.btn{background:#E8001D;color:#fff;text-decoration:none;padding:6px 14px;border-radius:4px;font-size:.8em;letter-spacing:1px}"
        ".empty{color:#555;font-size:.9em;padding:12px 0}"
        "</style></head><body>");

    html += F("<header><img src='/logo.bmp' alt='KartUp'>"
              "<div><h1>KARTUP</h1><p>Telemetria GPS</p></div></header>");

    // Card sessão
    html += F("<div class='card'><h2>SESSAO ATUAL</h2>");
    html += "<div class='row'><span class='lbl'>Arquivo</span><span class='val'>";
    html += logPath; html += "</span></div>";
    html += "<div class='row'><span class='lbl'>Voltas</span><span class='val'>";
    html += lapCount; html += "</span></div>";
    html += "<div class='row'><span class='lbl'>Melhor volta</span><span class='val'>";
    html += fmtLap(bestLapMs); html += "</span></div>";
    html += "<div class='row'><span class='lbl'>Ultima volta</span><span class='val'>";
    html += fmtLap(lastLapMs); html += "</span></div>";
    html += "<div class='row'><span class='lbl'>Delta</span><span class='";
    html += (deltaS <= 0.0f) ? "dn" : "dp";
    html += "'>";
    if (deltaS > 0.0f) html += '+';
    html += String(deltaS, 2); html += "s</span></div>";
    html += F("</div>");

    // Lista arquivos
    html += F("<div class='card'><h2>ARQUIVOS NO SD</h2>");
    if (!sdOk) {
        html += F("<p class='empty'>SD card indisponivel</p>");
    } else {
        File32 dir = sd.open("/");
        File32 entry;
        int count = 0;
        while (entry.openNext(&dir, O_READ)) {
            char name[32];
            entry.getName(name, sizeof(name));
            if (strncmp(name, "gps_", 4) == 0) {
                uint32_t sz = entry.fileSize();
                html += "<div class='fi'><div><div class='fn'>";
                html += name;
                html += "</div><div class='fs'>";
                html += String(sz / 1024.0f, 1);
                html += " KB</div></div><a class='btn' href='/download?file=";
                html += name;
                html += "'>BAIXAR</a></div>";
                count++;
            }
            entry.close();
        }
        dir.close();
        if (count == 0) html += F("<p class='empty'>Nenhum arquivo encontrado</p>");
    }
    html += F("</div></body></html>");

    httpServer.send(200, "text/html; charset=utf-8", html);
}

void handleDownload() {
    if (!httpServer.hasArg("file")) {
        httpServer.send(400, "text/plain", "Parametro 'file' ausente");
        return;
    }
    String filename = httpServer.arg("file");
    if (!filename.startsWith("gps_") || !filename.endsWith(".csv") || filename.length() > 12) {
        httpServer.send(403, "text/plain", "Arquivo nao permitido");
        return;
    }
    if (!sdOk) {
        httpServer.send(503, "text/plain", "SD indisponivel");
        return;
    }
    File32 file;
    if (!file.open(filename.c_str(), O_READ)) {
        httpServer.send(404, "text/plain", "Nao encontrado");
        return;
    }
    httpServer.sendHeader("Content-Disposition",
        "attachment; filename=\"" + filename + "\"");
    httpServer.setContentLength(file.fileSize());
    httpServer.send(200, "text/csv", "");

    WiFiClient client = httpServer.client();
    uint8_t buf[512];
    int n;
    while ((n = file.read(buf, sizeof(buf))) > 0) {
        client.write(buf, n);
    }
    file.close();
}

// ─────────────────────────────────────────────────────────────
// ENTRA / SAI do modo WiFi
// ─────────────────────────────────────────────────────────────
void enterWiFiMode() {
    // Re-inicializa SD em modo leitura (sdOk estava false após stopLogging)
    SdSpiConfig cfg(SD_CS_PIN, DEDICATED_SPI, SD_SCK_MHZ(2), &softSpi);
    sdOk = sd.begin(cfg);

    wifiReturnScreen = currentScreen;
    wifiMode = true;
    WiFi.mode(WIFI_AP);
    WiFi.softAP(WIFI_SSID, strlen(WIFI_PASS) ? WIFI_PASS : nullptr);
    httpServer.begin();

    Serial.printf("[WiFi] AP: %s  IP: %s\n", WIFI_SSID,
        WiFi.softAPIP().toString().c_str());
    drawWiFiScreen();
}

void exitWiFiMode() {
    httpServer.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    sdOk     = false;
    wifiMode = false;
    Serial.println("[WiFi] Desligado");
    if (wifiReturnScreen == SCREEN_SETTINGS) drawSettingsScreen();
    else drawReadyScreen();
}

// ─────────────────────────────────────────────────────────────
// DESLIGAMENTO DO SISTEMA (deep sleep → IP5306 auto-desliga)
// ─────────────────────────────────────────────────────────────
void shutdownSystem() {
    if (isLogging) {
        if (sdOk && logFile) { logFile.flush(); logFile.close(); }
        sdOk      = false;
        isLogging = false;
    }
    tft.fillScreen(C_BG);
    tft.setTextFont(4);
    tft.setTextColor(C_WHITE, C_BG);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Ate logo...", W / 2, 110);
    tft.setTextFont(2);
    tft.setTextColor(C_GRAY, C_BG);
    tft.drawString("Ligue pelo botao do shield", W / 2, 148);
    delay(1500);
    digitalWrite(TFT_BL, LOW);  // apaga backlight
    esp_deep_sleep_start();
}

void runStartupFlow() {
    while (true) {
        currentScreen = SCREEN_BOOT;
        drawBootScreen();
        waitForStart();
        int track = waitForTrackSelection();
        if (track >= 0) {
            selectedTrack = track;
            Serial.printf("[MENU] Pista selecionada: %s\n", TRACKS[selectedTrack].name);
            drawReadyScreen();
            return;
        }
    }
}

void drawShutdownConfirm() {
    shutdownReturnScreen = currentScreen;
    currentScreen = SCREEN_SHUTDOWN_CONFIRM;
    // Modal de confirmação
    tft.fillRoundRect(40, 82, 240, 88, 8, C_PANEL);
    tft.drawRoundRect(40, 82, 240, 88, 8, C_BORDER);
    drawBackButton();
    tft.setTextFont(2);
    tft.setTextColor(C_WHITE, C_PANEL);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Desligar sistema?", W / 2, 108);

    // SIM
    tft.fillRoundRect(60, 133, 86, 27, 6, C_RED_F1);
    tft.setTextFont(2);
    tft.setTextColor(C_WHITE, C_RED_F1);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("SIM", 103, 147);

    // NAO
    tft.fillRoundRect(174, 133, 86, 27, 6, C_GRAY_DK);
    tft.drawRoundRect(174, 133, 86, 27, 6, C_BORDER);
    tft.setTextColor(C_GRAY, C_GRAY_DK);
    tft.drawString("NAO", 217, 147);

    while (ts.getPoint().z > 100) delay(10);

    while (true) {
        delay(20);
        TS_Point p = ts.getPoint();
        if (p.z < 200) continue;
        int sx, sy;
        touchToScreen(p, sx, sy);
        if (isBackPressed(sx, sy)) {
            while (ts.getPoint().z > 100) delay(10);
            if (shutdownReturnScreen == SCREEN_READY) {
                drawReadyScreen();
            } else {
                currentScreen = SCREEN_BOOT;
                drawBootScreen();
            }
            return;
        }
        if (inRect(sx, sy, 60, 133, 86, 27, 8)) {
            while (ts.getPoint().z > 100) delay(10);
            shutdownSystem();
            return;
        }
        if (inRect(sx, sy, 174, 133, 86, 27, 8)) {
            while (ts.getPoint().z > 100) delay(10);
            if (shutdownReturnScreen == SCREEN_READY) {
                drawReadyScreen();
            } else {
                currentScreen = SCREEN_BOOT;
                drawBootScreen();
            }
            return;
        }
    }
}

// ─────────────────────────────────────────────────────────────
// SETUP
// ─────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    pinMode(BOOT_BTN, INPUT_PULLUP);
    pinMode(TFT_BL, OUTPUT);
    analogWriteResolution(8);
    analogWriteFrequency(20000);
    applyBrightness();

#ifndef USE_MOCK_DATA
    gpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
#endif

    touchSPI.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
    ts.begin(touchSPI);
    ts.setRotation(1);

    tft.init();
    tft.setRotation(1);

#ifndef USE_MOCK_DATA
    configureGPS();
#endif

    httpServer.on("/",         HTTP_GET, handleRoot);
    httpServer.on("/logo.bmp", HTTP_GET, handleLogo);
    httpServer.on("/download", HTTP_GET, handleDownload);

    runStartupFlow();
    Serial.println("KartUp pronto.");
}

// ─────────────────────────────────────────────────────────────
// LOOP
// ─────────────────────────────────────────────────────────────
void loop() {
    // ── Modo WiFi: processa requisições e botão SAIR ─────────
    if (wifiMode) {
        httpServer.handleClient();
        TS_Point p = ts.getPoint();
        if (p.z > 200) {
            int sx, sy;
            touchToScreen(p, sx, sy);
            Serial.printf("[WIFI_TOUCH] raw(%d,%d) -> screen(%d,%d) sair=%d\n",
                p.x, p.y, sx, sy,
                inRect(sx, sy, WIFI_SAIR_X, WIFI_SAIR_Y, WIFI_SAIR_W, WIFI_SAIR_H, 12) ? 1 : 0);
            if (inRect(sx, sy, WIFI_SAIR_X, WIFI_SAIR_Y, WIFI_SAIR_W, WIFI_SAIR_H, 12)) {
                while (ts.getPoint().z > 100) delay(10);
                exitWiFiMode();
            }
        }
        return;
    }

    uint32_t now = millis();

    // ── Leitura GPS (sempre ativa, independente de estado) ──
#ifndef USE_MOCK_DATA
    while (gpsSerial.available()) gps.encode(gpsSerial.read());
    gpsOk      = gps.location.isValid();
    satellites = gps.satellites.isValid() ? (int)gps.satellites.value() : 0;
#endif

    // ── TELA PRONTA: aguardando INICIAR LOG ─────────────────
    if (!isLogging) {
        // Atualiza GPS indicator a cada 1s
        static uint32_t lastReadyGps = 0;
        if (now - lastReadyGps >= 1000) {
            lastReadyGps = now;
            updateReadyGPS();
        }
        // Verifica toque
        TS_Point p = ts.getPoint();
        if (p.z > 200) {
            int sx, sy;
            touchToScreen(p, sx, sy);
            Serial.printf("[READY_TOUCH] raw(%d,%d) -> screen(%d,%d) back=%d log=%d wifi=%d\n",
                p.x, p.y, sx, sy,
                isBackPressed(sx, sy) ? 1 : 0,
                (sx >= ILOG_X && sx <= ILOG_X + ILOG_W &&
                 sy >= ILOG_Y && sy <= ILOG_Y + ILOG_H) ? 1 : 0,
                (sx >= WIFI_BTN_X && sx <= WIFI_BTN_X + WIFI_BTN_W &&
                 sy >= WIFI_BTN_Y && sy <= WIFI_BTN_Y + WIFI_BTN_H) ? 1 : 0);
            if (isBackPressed(sx, sy)) {
                while (ts.getPoint().z > 100) delay(10);
                runStartupFlow();
                return;
            }
            // Botão INICIAR LOG
            if (sx >= ILOG_X && sx <= ILOG_X + ILOG_W &&
                sy >= ILOG_Y && sy <= ILOG_Y + ILOG_H) {
                while (ts.getPoint().z > 100) delay(10);
                startLogging();
                return;
            }
            // Botão WiFi
            if (sx >= WIFI_BTN_X && sx <= WIFI_BTN_X + WIFI_BTN_W &&
                sy >= WIFI_BTN_Y && sy <= WIFI_BTN_Y + WIFI_BTN_H) {
                while (ts.getPoint().z > 100) delay(10);
                enterWiFiMode();
                return;
            }
        }
        return;
    }

    // ── ESTADO LOGGING ───────────────────────────────────────

    // Verifica toque em ENCERRAR (centro do top bar)
    {
        TS_Point p = ts.getPoint();
        if (p.z > 200) {
            int sx, sy;
            touchToScreen(p, sx, sy);
            if (sy <= TOP_H && sx >= STOP_X1 && sx <= STOP_X2) {
                while (ts.getPoint().z > 100) delay(10);
                stopLogging();
                return;
            }
        }
    }

#ifdef USE_MOCK_DATA
    updateDemo();
    if (lapActive) curLapMs = now - lapStartMs;
#else
    uint32_t nowMs = now;

#ifndef GPS_CSV_ONLY
    static uint32_t lastGpsLog = 0;
    if (nowMs - lastGpsLog >= 2000) {
        lastGpsLog = nowMs;
        Serial.print("[GPS] sats="); Serial.print(satellites);
        Serial.print(" fix="); Serial.print(gpsOk ? "SIM" : "NAO");
        if (gpsOk) {
            Serial.print(" spd="); Serial.print(gps.speed.kmph(), 1);
        }
        Serial.println();
    }
#endif

    // Log no SD a cada 100ms (10 Hz) — fix fresco
    static uint32_t lastCsvLog = 0;
    bool fixFresh = gps.location.isValid() && gps.location.age() < 200;
    if (fixFresh && nowMs - lastCsvLog >= 100) {
        lastCsvLog = nowMs;
        char timeStr[12];
        snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d",
            gps.time.hour(), gps.time.minute(), gps.time.second());
        sdLog(timeStr, nowMs, gps.location.lat(), gps.location.lng(), gps.speed.kmph());
    }

    if (!gpsOk) {
        resetSpeedFilter();
        curSpeed = 0.0f;
    } else if (gps.speed.isValid()) {
        curSpeed = smoothSpeed(gps.speed.kmph());
    }

    checkGPSFinishLine();

    if (lapActive) {
        curLapMs = nowMs - lapStartMs;
        deltaS   = bestLapMs > 0
            ? (float)((int32_t)curLapMs - (int32_t)bestLapMs) / 1000.0f
            : 0.0f;
    }
#endif

    handleRecordAnimation();

    // Contador de loops/s
    static uint32_t loopCount = 0, lastLoopLog = 0;
    loopCount++;
    if (now - lastLoopLog >= 5000) {
        Serial.printf("[CPU] loops/s=%lu  RAM livre=%u bytes\n",
            loopCount / 5, ESP.getFreeHeap());
        loopCount = 0; lastLoopLog = now;
    }

    // Redesenha apenas o que mudou
    static uint32_t lastDraw = 0;
    if (now - lastDraw >= 50) {
        lastDraw = now;
        drawGPS();
        drawSpeed();
        drawStopZone();  // mantém ENCERRAR visível
        drawLapCount();
        drawDeltaNum();
        drawCurrentLap();
        drawDeltaBar();
        drawBestLap();
        drawLastLap();
        drawRecordBorder();
    }
}

#endif // TESTE_GPS
