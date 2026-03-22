#ifdef TESTE_GPS

#include <Arduino.h>
#include <TinyGPSPlus.h>

// Serial2 nos pinos do conector CN1
static const int GPS_RX = 22;
static const int GPS_TX = 27;
static const uint32_t GPS_BAUD = 9600;

TinyGPSPlus gps;

void setup() {
    Serial.begin(115200);
    Serial2.begin(GPS_BAUD, SERIAL_8N1, GPS_RX, GPS_TX);
    Serial.println("\n=== Teste GPS NEO-M8N ===");
    Serial.println("Aguardando sentenças NMEA...\n");
}

void loop() {
    // Alimenta o parser com todos os bytes disponíveis
    while (Serial2.available()) {
        gps.encode(Serial2.read());
    }

    // Relatório a cada 1 segundo
    static uint32_t lastReport = 0;
    if (millis() - lastReport >= 1000) {
        lastReport = millis();

        Serial.println("--- GPS ---");
        Serial.printf("Sentenças processadas : %lu\n", gps.passedChecksum());
        Serial.printf("Satélites visíveis    : %d\n",
                      gps.satellites.isValid() ? (int)gps.satellites.value() : -1);
        Serial.printf("Fix válido            : %s\n",
                      gps.location.isValid() ? "SIM" : "não");

        if (gps.location.isValid()) {
            Serial.printf("Latitude              : %.6f\n", gps.location.lat());
            Serial.printf("Longitude             : %.6f\n", gps.location.lng());
        }

        Serial.printf("Velocidade (km/h)     : %.1f\n",
                      gps.speed.isValid() ? gps.speed.kmph() : 0.0);
        Serial.printf("HDOP                  : %.2f\n",
                      gps.hdop.isValid() ? gps.hdop.hdop() : 99.99);
        Serial.println();
    }
}

#endif // TESTE_GPS
