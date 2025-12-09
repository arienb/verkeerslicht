#include "traffic_light.h"

// interne state
static TrafficState currentState = STATE_RED;
static uint32_t yellowStartMs = 0;
static uint32_t errorBlinkStartMs = 0;
static bool errorYellowOn = false;

static void applyLedOutputs()
{
    bool red = false;
    bool yellow = false;
    bool green = false;

    switch (currentState)
    {
    case STATE_RED:
        red = true;
        break;
    case STATE_GREEN:
        green = true;
        break;
    case STATE_YELLOW:
        yellow = true;
        break;
    case STATE_ERROR:
        red = true;
        yellow = errorYellowOn;
        break;
    }

    digitalWrite(PIN_LED_RED,    red    ? HIGH : LOW);
    digitalWrite(PIN_LED_YELLOW, yellow ? HIGH : LOW);
    digitalWrite(PIN_LED_GREEN,  green  ? HIGH : LOW);
}

void trafficLightInit()
{
    pinMode(PIN_LED_RED, OUTPUT);
    pinMode(PIN_LED_YELLOW, OUTPUT);
    pinMode(PIN_LED_GREEN, OUTPUT);

    currentState = STATE_RED;
    errorYellowOn = true;
    yellowStartMs = millis();
    errorBlinkStartMs = millis();
    applyLedOutputs();
}

void trafficLightSetCommand(TrafficState cmdState)
{
    // ERROR heeft hoogste prioriteit, meteen omschakelen
    if (cmdState == STATE_ERROR)
    {
        // Alleen initialiseren als we NIET al in ERROR zitten
        if (currentState != STATE_ERROR)
        {
            currentState = STATE_ERROR;
            errorYellowOn = true;
            errorBlinkStartMs = millis();   // <-- TERUG NAAR millis()
            applyLedOutputs();
        }
        return;
    }

    if (cmdState == STATE_GREEN)
    {
        // altijd direct naar GREEN (vanuit rood)
        currentState = STATE_GREEN;
        applyLedOutputs();
    }
    else if (cmdState == STATE_RED)
    {
        // GREEN -> YELLOW -> RED
        if (currentState == STATE_GREEN)
        {
            currentState = STATE_YELLOW;
            yellowStartMs = millis();
            applyLedOutputs(); // toon geel
        }
        else
        {
            // vanuit alles anders: gewoon rood
            currentState = STATE_RED;
            applyLedOutputs();
        }
    }
}

void trafficLightUpdate(uint32_t nowMs)
{
    switch (currentState)
    {
    case STATE_YELLOW:
        if (nowMs - yellowStartMs >= (uint32_t)T_YELLOW_SECONDS * 1000UL)
        {
            currentState = STATE_RED;
            applyLedOutputs();
        }
        break;

    case STATE_ERROR:
        if (nowMs - errorBlinkStartMs >= ERROR_BLINK_INTERVAL_MS)
        {
            errorBlinkStartMs = nowMs;
            errorYellowOn = !errorYellowOn;
            applyLedOutputs();
        }
        break;

    default:
        // RED / GREEN: niets tijdsafhankelijk
        break;
    }
}

TrafficState trafficLightGetState()
{
    return currentState;
}
