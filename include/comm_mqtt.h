#pragma once
#include <Arduino.h>
#include <PubSubClient.h>

void mqttEnsureConnected(uint32_t nowMs);

void mqttCallback(char* topic, byte* payload, unsigned int length);

extern uint16_t tGreenA, tGreenB, tClear;

extern PubSubClient mqtt;