#ifdef UCENTER_BRIDGE
#include <Arduino.h>

// Bridge serial ESP32 <-> NEO-M8N para uso com u-center
// Pinos KartUp: GPS_RX=22 (ESP recebe), GPS_TX=27 (ESP transmite)
// u-center: COM3, 115200 baud
// Após configurar via u-center, reflashar env:cyd para restaurar firmware

void setup() {
    Serial.begin(115200);
    Serial2.begin(115200, SERIAL_8N1, 22, 27);
}

void loop() {
    if (Serial.available())  Serial2.write(Serial.read());
    if (Serial2.available()) Serial.write(Serial2.read());
}

#endif // UCENTER_BRIDGE
