#include "comm.h"
#include <SPI.h>
#include <LoRa.h>
#include "config.h"
#include <WiFiClient.h>

static WiFiClient wifiClient;
extern PubSubClient mqtt(wifiClient);

extern uint32_t lastMqttAttemptMs = 0;

// huidige tijden in seconden
static uint16_t tGreenA = T_GREEN_A_DEFAULT;
static uint16_t tGreenB = T_GREEN_B_DEFAULT;
static uint16_t tClear  = T_CLEAR_DEFAULT;


void mqttEnsureConnected(uint32_t nowMs)
{
    if (mqtt.connected()) return;

    if (nowMs - lastMqttAttemptMs < 2000)
        return;

    lastMqttAttemptMs = nowMs;

    Serial.print("[MQTT] Connecting...");
    if (mqtt.connect(MQTT_CLIENT_ID))
    {
        Serial.println("connected");
        mqtt.subscribe(MQTT_TOPIC_CONFIG);
    }
    else
    {
        Serial.print("failed, rc=");
        Serial.println(mqtt.state());
    }
}

void mqttCallback(char* topic, byte* payload, unsigned int length)
{
    String t(topic);
    String msg;
    for (unsigned int i = 0; i < length; ++i)
        msg += (char)payload[i];

    Serial.print("[MQTT] Message on ");
    Serial.print(t);
    Serial.print(" : ");
    Serial.println(msg);

    if (t == MQTT_TOPIC_CONFIG)
    {
        // Expect "TgreenA,TgreenB,Tclear", e.g. "10,10,5"
        uint16_t a = 0, b = 0, c = 0;
        int parsed = sscanf(msg.c_str(), "%hu,%hu,%hu", &a, &b, &c);
        if (parsed == 3)
        {
            tGreenA = a;
            tGreenB = b;
            tClear  = c;

            Serial.print("[CFG] New timings A/B/clear = ");
            Serial.print(tGreenA);
            Serial.print("/");
            Serial.print(tGreenB);
            Serial.print("/");
            Serial.println(tClear);
        }
        else
        {
            Serial.println("[CFG] Invalid config payload, expected: \"20,15,5\"");
        }
    }
}