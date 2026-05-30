#if defined(TESTE_GPS) && !defined(UCENTER_BRIDGE)

#include <Arduino.h>
#include <TinyGPSPlus.h>

static const int GPS_RX = 22;
static const int GPS_TX = 27;

// Bauds a testar em sequência (NEO-M8N pode estar em qualquer um)
static const uint32_t BAUDS[]   = {115200, 9600, 38400, 57600};
static const int      BAUD_CNT  = sizeof(BAUDS) / sizeof(BAUDS[0]);

TinyGPSPlus    gps;
HardwareSerial gpsSerial(2);

// Estatísticas brutas
static uint32_t totalBytes    = 0;
static uint32_t bytesThisSec  = 0;
static bool     detectedBaud  = false;
static uint32_t activeBaud    = 0;

// Buffer NMEA para printar sentença crua
static char     nmeaBuf[90];
static uint8_t  nmeaPos = 0;

// ─── Detecta qual baud o GPS está respondendo ────────────────────────────────
uint32_t detectBaud() {
    Serial.println("\n[AUTO-DETECT] Testando bauds...");
    for (int i = 0; i < BAUD_CNT; i++) {
        Serial.printf("  Tentando %u baud... ", BAUDS[i]);
        gpsSerial.end();
        gpsSerial.begin(BAUDS[i], SERIAL_8N1, GPS_RX, GPS_TX);
        delay(800);

        uint32_t count = 0;
        uint32_t t = millis();
        while (millis() - t < 600) {
            if (gpsSerial.available()) count++;
            gpsSerial.read();
        }
        if (count > 5) {
            Serial.printf("OK! (%u bytes recebidos)\n", count);
            return BAUDS[i];
        }
        Serial.println("sem resposta.");
    }
    return 0;
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n========================================");
    Serial.println("  KartUp — Diagnóstico GPS NEO-M8N");
    Serial.println("  Pinos: RX=22, TX=27");
    Serial.println("========================================\n");

    activeBaud = detectBaud();

    if (activeBaud == 0) {
        Serial.println("[ERRO] GPS não respondeu em nenhum baud!");
        Serial.println("  Verifique: cabo RX/TX, alimentação, pinos 22/27.");
        detectedBaud = false;
        // Fica em 9600 como fallback
        gpsSerial.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);
    } else {
        Serial.printf("[OK] GPS detectado em %u baud\n", activeBaud);
        detectedBaud = true;
        // gpsSerial já aberto no baud correto após detectBaud()
    }

    Serial.println("\nMonitorando... (relatório a cada 2s)");
    Serial.println("Se 'Bytes recebidos=0' → problema de hardware/cabo.\n");
}

void loop() {
    // Lê e parseia bytes, imprime sentenças NMEA cruas
    while (gpsSerial.available()) {
        char c = gpsSerial.read();
        totalBytes++;
        bytesThisSec++;
        gps.encode(c);

        // Captura linha NMEA completa para debug
        if (c == '$') {
            nmeaPos = 0;
        }
        if (nmeaPos < sizeof(nmeaBuf) - 1) {
            nmeaBuf[nmeaPos++] = c;
        }
        if (c == '\n' && nmeaPos > 0) {
            nmeaBuf[nmeaPos] = '\0';
            // Só imprime GGA e RMC (mais informativos)
            if (strncmp(nmeaBuf, "$GNGGA", 6) == 0 ||
                strncmp(nmeaBuf, "$GPGGA", 6) == 0 ||
                strncmp(nmeaBuf, "$GNRMC", 6) == 0 ||
                strncmp(nmeaBuf, "$GPGSV", 6) == 0 ||
                strncmp(nmeaBuf, "$GNGSV", 6) == 0) {
                Serial.print("[NMEA] ");
                Serial.print(nmeaBuf);
            }
            nmeaPos = 0;
        }
    }

    // Relatório a cada 2 segundos
    static uint32_t lastReport = 0;
    uint32_t now = millis();
    if (now - lastReport >= 2000) {
        uint32_t elapsed = now - lastReport;
        lastReport = now;

        float bps = (bytesThisSec * 1000.0f) / elapsed;
        bytesThisSec = 0;

        Serial.println("────────────────────────────────────────");
        Serial.printf("Uptime              : %lus\n", now / 1000);
        Serial.printf("Baud detectado      : %s (%u)\n",
                      detectedBaud ? "SIM" : "NAO", activeBaud);
        Serial.printf("Bytes totais rx     : %lu\n", totalBytes);
        Serial.printf("Taxa atual          : %.0f bytes/s\n", bps);
        Serial.printf("Sentenças OK        : %lu\n", gps.passedChecksum());
        Serial.printf("Sentenças com erro  : %lu  ← se alto = baud errado\n",
                      gps.failedChecksum());
        Serial.println("---");

        int sats = gps.satellites.isValid() ? (int)gps.satellites.value() : -1;
        Serial.printf("Satélites visíveis  : %d\n", sats);
        Serial.printf("Fix válido          : %s\n",
                      gps.location.isValid() ? ">>> SIM <<<" : "não");

        if (gps.location.isValid()) {
            Serial.printf("Latitude            : %.6f\n", gps.location.lat());
            Serial.printf("Longitude           : %.6f\n", gps.location.lng());
            Serial.printf("Altitude (m)        : %.1f\n",
                          gps.altitude.isValid() ? gps.altitude.meters() : 0.0);
        }

        Serial.printf("Velocidade (km/h)   : %.1f\n",
                      gps.speed.isValid() ? gps.speed.kmph() : 0.0);
        Serial.printf("HDOP                : %.2f  ← <2.0 é bom\n",
                      gps.hdop.isValid() ? gps.hdop.hdop() : 99.99);

        // Diagnóstico de situação
        Serial.println("---");
        if (totalBytes == 0) {
            Serial.println("[!] NENHUM byte recebido — problema de hardware!");
            Serial.println("    Verifique cabos RX/TX e alimentação do GPS.");
        } else if (gps.passedChecksum() == 0 && gps.failedChecksum() > 5) {
            Serial.println("[!] Bytes chegando mas checksum falhando — baud errado?");
        } else if (sats == 0 || sats == -1) {
            Serial.println("[~] GPS comunicando, aguardando satélites.");
            Serial.println("    Leve para área aberta (janela ou externo).");
            Serial.printf("    Cold start pode levar 1-3 min.\n");
        } else if (sats > 0 && !gps.location.isValid()) {
            Serial.printf("[~] %d satélites — fix iminente...\n", sats);
        } else if (gps.location.isValid()) {
            Serial.println("[OK] GPS com fix!");
        }

        Serial.println("────────────────────────────────────────\n");
    }
}

#endif // TESTE_GPS && !UCENTER_BRIDGE
