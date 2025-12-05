#include <Arduino.h>
#include "config.h"
#include "traffic_light.h"
#include "comm.h"

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("=== Wireless Traffic Light ===");
    Serial.print("Node: ");
    Serial.println((char)NODE_ID);
#if IS_MASTER
    Serial.println("Role: MASTER (A)");
#else
    Serial.println("Role: SLAVE (B)");
#endif

    trafficLightInit();
    commInit();
}

void loop()
{
    uint32_t now = millis();

    trafficLightUpdate(now);  // lokale LED state machine (geel / error blink)
    commLoop(now);            // LoRa + (op master) MQTT + globale richting
}
