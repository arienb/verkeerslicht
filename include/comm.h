#pragma once
#include <Arduino.h>
#include "config.h"
#include "traffic_light.h"
#include "comm_lora.h"
#include "comm_mqtt.h"

// Initialiseer LoRa (+ WiFi/MQTT op master)
void commInit();

// Call elke loop met millis()
// - afhandelen LoRa RX/TX (heartbeats, commando's)
// - communicatietimeout detecteren
// - op master: globale A/B-cyclus + MQTT
void commLoop(uint32_t nowMs);

// true als peer de laatste 10 s gezien is
bool commPeerAlive();
