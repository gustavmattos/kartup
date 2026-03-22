// =============================================================
//  KARTUP — Telemetria de Kart  |  ESP32 CYD (ILI9341 320x240)
//  Interface: Dark Cockpit F1
//  Desenho direto no TFT com cache anti-flickering
// =============================================================

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <TinyGPS++.h>
#include "logo.h"
#include "f1_speed_font.h"
#include "f1_time_font.h"

// ─── MODO DEMO ───────────────────────────────────────────────
#define DEMO_MODE true

// ─── PINOS ───────────────────────────────────────────────────
#define GPS_RX_PIN  16
#define GPS_TX_PIN  17
#define BOOT_BTN     0

// ─── LINHA DE CHEGADA (GPS) ──────────────────────────────────
#define FINISH_LAT       -27.5954
#define FINISH_LON       -48.5480
#define FINISH_RADIUS_M   8.0

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
#define C_BG        0x0841   // #0a0a0a fundo
#define C_PANEL     0x1082   // #111111 painéis
#define C_RED_F1    0xE020   // vermelho Ferrari
#define C_RED       0xF800
#define C_GREEN     0x07E0
#define C_ORANGE    0xFC60
#define C_AMBER     0xFD00   // amarelo-âmbar melhor volta
#define C_PURPLE    0x781F   // roxo record
#define C_WHITE     TFT_WHITE
#define C_GRAY      0x4208
#define C_GRAY_DK   0x2104
#define C_BORDER    0x18C3

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

// ─── GPS / DEMO ──────────────────────────────────────────────
bool     wasNearFinish = false;
uint32_t demoLastMs   = 0;
float    demoSpeed    = 0.0f;
bool     demoUp       = true;
uint32_t demoLapStart = 0;
float    mockDelta    = 0.0f;
bool     mockDeltaDir = true;
unsigned long lastMockTick = 0;

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

    // Barra superior
    tft.fillRect(0, TOP_Y, W, TOP_H, C_PANEL);
    tft.drawFastHLine(0, TOP_H,   W, C_RED_F1);
    tft.drawFastHLine(0, TOP_H+1, W, C_RED_F1);

    // Labels fixos
    tft.setTextFont(1);
    tft.setTextSize(1);
    tft.setTextColor(C_GRAY_DK, C_BG);

    tft.setTextDatum(ML_DATUM);
    tft.drawString("VOLTA", 10, MID_Y + 10);

    tft.setTextDatum(MR_DATUM);
    tft.drawString("DELTA", W - 8, MID_Y + 20);

    tft.setTextDatum(MC_DATUM);
    tft.drawString("TEMPO ATUAL", W/2, MID_Y + 26);

    // Labels barra de delta
    tft.setTextColor(C_RED, C_BG);
    tft.setTextDatum(ML_DATUM);
    tft.drawString("PERDA", 8, BAR_Y - 8);

    tft.setTextColor(C_GREEN, C_BG);
    tft.setTextDatum(MR_DATUM);
    tft.drawString("GANHO", W - 8, BAR_Y - 8);

    // Marcadores da barra
    tft.setTextColor(C_GRAY_DK, C_BG);
    tft.setTextDatum(ML_DATUM);
    tft.drawString("-2.0", 8, BAR_Y + BAR_H + 6);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("0", W/2, BAR_Y + BAR_H + 6);
    tft.setTextDatum(MR_DATUM);
    tft.drawString("+2.0", W - 8, BAR_Y + BAR_H + 6);

    // Linha divisória gradiente (cinza → vermelho → cinza)
    for (int i = 0; i < W; i++) {
        float    t  = (float)i / W;
        uint16_t lc = (t < 0.2f || t > 0.8f) ? C_BORDER : C_RED_F1;
        tft.drawPixel(i, BOT_Y - 3, lc);
        tft.drawPixel(i, BOT_Y - 2, lc);
    }

    // Labels inferiores
    tft.setTextColor(C_GRAY_DK, C_BG);
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

    uint16_t col = gpsOk ? C_GREEN : C_RED;
    tft.fillCircle(12, TOP_H/2, 4, col);
    tft.setTextFont(1);
    tft.setTextSize(1);
    tft.setTextColor(col, C_PANEL);
    tft.setTextDatum(ML_DATUM);
    tft.drawString("GPS " + String(satellites), 20, TOP_H/2);
}

// ─────────────────────────────────────────────────────────────
// VELOCIDADE (barra superior direita)
// ─────────────────────────────────────────────────────────────
void drawSpeed() {
    if (fabsf(curSpeed - cacheSpeed) < 1.0f) return;
    cacheSpeed = curSpeed;

    tft.fillRect(SPD_X, 4, SPD_W, TOP_H - 8, C_PANEL);
    tft.setTextFont(1);
    tft.setTextSize(1);
    tft.setTextColor(C_WHITE, C_PANEL);
    tft.setTextDatum(MR_DATUM);
    tft.drawString(String((int)curSpeed) + " km/h", W - 6, TOP_H/2);
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

    tft.fillRect(DELTA_NUM_X, MID_Y + 2, DELTA_NUM_W, 16, C_BG);

    uint16_t col = (fabsf(deltaS) < 0.1f) ? C_GRAY
                 : (deltaS >= 0)           ? C_RED
                                           : C_GREEN;
    tft.setTextFont(2);
    tft.setTextSize(1);
    tft.setTextColor(col, C_BG);
    tft.setTextDatum(MR_DATUM);
    tft.drawString(formatDelta(deltaS), W - 8, MID_Y + 10);
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

    tft.setTextFont(1);
    tft.setTextSize(1);
}

// ─────────────────────────────────────────────────────────────
// BARRA DE DELTA
// ─────────────────────────────────────────────────────────────
void drawDeltaBar() {
    float maxDelt = 2.0f;

    // Fundos
    tft.fillRect(BAR_X,          BAR_Y, BAR_W/2, BAR_H, tft.color565(80, 10, 10));
    tft.fillRect(BAR_X + BAR_W/2, BAR_Y, BAR_W/2, BAR_H, tft.color565(10, 40, 10));
    tft.drawRect(BAR_X, BAR_Y, BAR_W, BAR_H, C_BORDER);

    int cx = BAR_X + BAR_W/2;
    tft.drawFastVLine(cx,              BAR_Y,   BAR_H,   C_GRAY);
    tft.drawFastVLine(BAR_X + BAR_W/4, BAR_Y+2, BAR_H-4, C_BORDER);
    tft.drawFastVLine(BAR_X+3*BAR_W/4, BAR_Y+2, BAR_H-4, C_BORDER);

    float    ratio  = constrain(fabsf(deltaS) / maxDelt, 0.0f, 1.0f);
    int      barW   = (int)(ratio * (BAR_W/2 - 2));
    uint16_t barCol = (deltaS >= 0) ? ((ratio > 0.5f) ? C_RED : C_ORANGE) : C_GREEN;

    if (deltaS >= 0 && barW > 0)
        tft.fillRect(cx - barW, BAR_Y+2, barW, BAR_H-4, barCol);
    else if (deltaS < 0 && barW > 0)
        tft.fillRect(cx + 1,   BAR_Y+2, barW, BAR_H-4, barCol);

    // Centro branco
    tft.fillRect(cx - 1, BAR_Y, 3, BAR_H, C_WHITE);
}

// ─────────────────────────────────────────────────────────────
// MELHOR VOLTA
// ─────────────────────────────────────────────────────────────
void drawBestLap() {
    if (bestLapMs == cacheBestLap && cacheNewBest == newBest) return;
    cacheBestLap = bestLapMs;
    cacheNewBest = newBest;

    tft.fillRect(BEST_X, BOT_TEXT_Y, BEST_W, BOT_H - 22, C_BG);
    tft.loadFont(f1_time_font);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(bestColor(), C_BG);
    tft.drawString(formatTime(bestLapMs), BEST_X, BOT_TEXT_Y + 8);
    tft.unloadFont();
}

// ─────────────────────────────────────────────────────────────
// ÚLTIMA VOLTA
// ─────────────────────────────────────────────────────────────
void drawLastLap() {
    if (lastLapMs == cacheLastLap) return;
    cacheLastLap = lastLapMs;

    tft.fillRect(LAST_X, BOT_TEXT_Y, LAST_W, BOT_H - 22, C_BG);
    tft.loadFont(f1_time_font);
    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(C_GRAY, C_BG);
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
    double dist = TinyGPSPlus::distanceBetween(
        gps.location.lat(), gps.location.lng(),
        FINISH_LAT, FINISH_LON);
    bool near = dist < FINISH_RADIUS_M;
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
void drawBootScreen() {
    tft.fillScreen(TFT_BLACK);

    tft.fillRect(0, 0, W, 4, C_RED_F1);
    tft.fillRect(0, H - 4, W, 4, C_RED_F1);

    int logoX = (W - LOGO_W) / 2;
    int logoY = 30;
    tft.pushImage(logoX, logoY, LOGO_W, LOGO_H, logo_kartup);

    tft.setTextDatum(MC_DATUM);
    tft.setTextFont(4);
    tft.setTextColor(C_WHITE, TFT_BLACK);
    tft.drawString("kart", W/2 - 22, logoY + LOGO_H + 22);
    tft.setTextColor(C_RED_F1, TFT_BLACK);
    tft.drawString("Up", W/2 + 16, logoY + LOGO_H + 22);

    tft.setTextFont(2);
    tft.setTextColor(C_GRAY, TFT_BLACK);
    tft.drawString("Telemetria para kart", W/2, logoY + LOGO_H + 52);
    tft.drawString(DEMO_MODE ? "[ MODO DEMO ]" : "[ GPS ATIVO ]", W/2, logoY + LOGO_H + 68);

    tft.setTextFont(1);
    tft.setTextColor(C_GRAY_DK, TFT_BLACK);
    tft.drawString("kartUp.pro", W/2, H - 14);

    delay(2000);
}

// ─────────────────────────────────────────────────────────────
// SETUP
// ─────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    pinMode(BOOT_BTN, INPUT_PULLUP);

    if (!DEMO_MODE) {
        gpsSerial.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
    }

    tft.init();
    tft.setRotation(1);

    drawBootScreen();

    drawStaticLayout();
    drawGPS();
    drawSpeed();
    drawLapCount();
    drawDeltaNum();
    drawCurrentLap();
    drawDeltaBar();
    drawBestLap();
    drawLastLap();

    // Inicia demo imediatamente
    lapStartMs   = millis();
    lapActive    = true;
    demoLapStart = millis();

    Serial.println("KartUp iniciado!");
}

// ─────────────────────────────────────────────────────────────
// LOOP
// ─────────────────────────────────────────────────────────────
void loop() {
    if (DEMO_MODE) {
        updateDemo();
        if (lapActive) curLapMs = millis() - lapStartMs;
    } else {
        while (gpsSerial.available()) gps.encode(gpsSerial.read());

        if (gps.speed.isValid()) curSpeed   = gps.speed.kmph();
        gpsOk      = gps.satellites.isValid();
        satellites = gpsOk ? (int)gps.satellites.value() : 0;

        checkGPSFinishLine();

        if (lapActive) {
            curLapMs = millis() - lapStartMs;
            deltaS   = bestLapMs > 0
                ? (float)((int32_t)curLapMs - (int32_t)bestLapMs) / 1000.0f
                : 0.0f;
        }
    }

    handleRecordAnimation();

    // Redesenha apenas o que mudou
    drawGPS();
    drawSpeed();
    drawLapCount();
    drawDeltaNum();
    drawCurrentLap();
    drawDeltaBar();
    drawBestLap();
    drawLastLap();
    drawRecordBorder();

    delay(50);
}
