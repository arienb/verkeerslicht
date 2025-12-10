#pragma once
#include "comm_lora.h"
#include <comm.h>
#include <LoRa.h>
#include "config.h"

extern uint32_t lastHeartbeatSentMs = 0;
extern uint32_t lastPeerSeenMs = 0;
extern bool peerAlive = false;

bool commPeerAlive()
{
    return peerAlive;
}

// LoRa bericht versturen (string)
void loraSend(const String& msg)
{
    LoRa.beginPacket();
    LoRa.print(msg);
    LoRa.endPacket();
}

// CMD naar peer sturen (master gebruikt dit)
void sendCommandToPeer(char stateChar)
{
    // CMD:<TARGET>:<STATE>
    // TARGET is peer: als wij A zijn -> B, omgekeerd.
    char target = (NODE_ID == 'A') ? 'B' : 'A';

    String msg = "CMD:";
    msg += target;
    msg += ':';
    msg += stateChar; // 'R','G','E'
    loraSend(msg);

    Serial.print("[LoRa] Sent command to ");
    Serial.print(target);
    Serial.print(" -> ");
    Serial.println(msg);
}

// LoRa message verwerken
void handleLoraMessage(const String& msg, uint32_t nowMs)
{
    if (msg.startsWith("HB:"))
    {
        // Heartbeat: "HB:A" / "HB:B"
        if (msg.length() >= 4)
        {
            char from = msg.charAt(3);
            if (from != NODE_ID)
            {
                lastPeerSeenMs = nowMs;
                peerAlive = true;
            }
        }
    }
    else if (msg.startsWith("CMD:"))
    {
        // Command: "CMD:A:R" of "CMD:B:G"
        // indices: 0 1 2 3 4 5 6
        // chars :  C M D : T : S
        if (msg.length() >= 7)
        {
            char target = msg.charAt(4);
            char stateChar = msg.charAt(6);

            if (target == NODE_ID)
            {
                TrafficState st = STATE_RED;
                if      (stateChar == 'G') st = STATE_GREEN;
                else if (stateChar == 'R') st = STATE_RED;
                else if (stateChar == 'E') st = STATE_ERROR;

                Serial.print("[LoRa] CMD for me: ");
                Serial.println(msg);

                trafficLightSetCommand(st);
                lastPeerSeenMs = nowMs;
                peerAlive = true;
            }
        }
    }
}