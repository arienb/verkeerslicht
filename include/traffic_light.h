#pragma once
#include <Arduino.h>
#include "config.h"

enum TrafficState : uint8_t {
    STATE_RED = 0,
    STATE_GREEN,
    STATE_YELLOW,
    STATE_ERROR
};

// Initialise LEDs + start in RED
void trafficLightInit();

// Call elke loop met millis()
void trafficLightUpdate(uint32_t nowMs);

// Verkeerslicht opdracht geven (GREEN/RED/ERROR)
// - RED vanuit GREEN => eerst 3 s geel, daarna rood
// - ERROR => rood + geel knipperend
void trafficLightSetCommand(TrafficState cmdState);

// Huidige toestand uitlezen
TrafficState trafficLightGetState();
