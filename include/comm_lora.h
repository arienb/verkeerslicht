#pragma once
#include <Arduino.h>

void loraInit();

void loraSend(const String& msg);

void sendCommandToPeer(char stateChar);

void handleLoraMessage(const String& msg, uint32_t nowMs);

extern uint32_t lastHeartbeatSentMs;

extern uint32_t lastPeerSeenMs;

extern bool peerAlive;