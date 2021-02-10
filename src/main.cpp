#include <Arduino.h>
#include <BleRadio.h>

void setup() {
    Serial.begin(115200);
    if(!g_ble.begin()) {
        Serial.println(F("BLE Error starting radio"));
        while(true); // halt
    }
    g_ble.on("Test BLE");
    
}
void loop() {
    g_ble.update();
}