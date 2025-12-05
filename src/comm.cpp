#include "comm.h"
#include <SPI.h>
#include <LoRa.h>
#include "config.h"

#if IS_MASTER
  #include <WiFi.h>
  #include <WiFiClient.h>
  #include <PubSubClient.h>
  #include <cstdio>
#endif

// ========= Gemeenschappelijke state (zowel master als slave) =========

static uint32_t lastHeartbeatSentMs = 0;
static uint32_t lastPeerSeenMs = 0;
static bool     peerAlive = false;

bool commPeerAlive()
{
    return peerAlive;
}

// helper: TrafficState -> korte string
static const char* stateToString(TrafficState st)
{
    switch (st)
    {
    case STATE_RED:    return "RED";
    case STATE_GREEN:  return "GREEN";
    case STATE_YELLOW: return "YELLOW";
    case STATE_ERROR:  return "ERROR";
    default:           return "UNKNOWN";
    }
}

// LoRa bericht versturen (string)
static void loraSend(const String& msg)
{
    LoRa.beginPacket();
    LoRa.print(msg);
    LoRa.endPacket();
}

// CMD naar peer sturen (master gebruikt dit)
static void sendCommandToPeer(char stateChar)
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
static void handleLoraMessage(const String& msg, uint32_t nowMs)
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
                // Serial.printf("[LoRa] HB from %c\n", from);
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

// ========= Master-specifieke zaken (A) =========
#if IS_MASTER

// globale richting state machine
enum GlobalState : uint8_t {
    GS_INIT = 0,
    GS_A_GREEN,
    GS_B_GREEN,
    GS_ALL_RED,
    GS_ERROR
};

static GlobalState gState = GS_INIT;
static uint32_t    gStateStartMs = 0;
static char        lastGreenDir = 'B'; // zodat A eerst groen wordt

// huidige tijden in seconden (config via MQTT)
static uint16_t tGreenA = T_GREEN_A_DEFAULT;
static uint16_t tGreenB = T_GREEN_B_DEFAULT;
static uint16_t tClear  = T_CLEAR_DEFAULT;

// WiFi / MQTT
static WiFiClient espClient;
static PubSubClient mqtt(espClient);

static void mqttPublishStatus();
static void mqttPublishGlobal(const char* text);

static void connectWiFi()
{
    Serial.print("Connecting to WiFi: ");
    Serial.println(WIFI_SSID);

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
}

static void mqttCallback(char* topic, byte* payload, unsigned int length)
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
        // Expect "TgreenA,TgreenB,Tclear", e.g. "20,15,5"
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

            mqttPublishGlobal("CONFIG_UPDATED");
        }
        else
        {
            Serial.println("[CFG] Invalid config payload, expected: \"20,15,5\"");
        }
    }
}

static void mqttEnsureConnected()
{
    while (!mqtt.connected())
    {
        Serial.print("[MQTT] Connecting...");
        if (mqtt.connect(MQTT_CLIENT_ID))
        {
            Serial.println("connected");
            mqtt.subscribe(MQTT_TOPIC_CONFIG);
        }
        else
        {
            Serial.print("failed, rc=");
            Serial.print(mqtt.state());
            Serial.println(" retry in 2 seconds");
            delay(2000);
        }
    }
}

static void mqttPublishStatus()
{
    if (!mqtt.connected()) return;

    TrafficState st = trafficLightGetState();

    String payload;
    payload.reserve(32);
    payload += NODE_ID;
    payload += ",";
    payload += stateToString(st);
    payload += ",";
    payload += (peerAlive ? "peer_ok" : "peer_lost");

    mqtt.publish(MQTT_TOPIC_STATUS, payload.c_str(), true);
}

static void mqttPublishGlobal(const char* text)
{
    if (!mqtt.connected()) return;
    mqtt.publish(MQTT_TOPIC_GLOBAL, text, true);
}

// globale A/B richting FSM
static void updateGlobalFSM(uint32_t nowMs)
{
    // als peer weg is => ERROR
    if (!peerAlive)
    {
        if (gState != GS_ERROR)
        {
            gState = GS_ERROR;
            trafficLightSetCommand(STATE_ERROR);
            sendCommandToPeer('E');
            mqttPublishGlobal("ERROR_NO_COMM");
            Serial.println("[FSM] -> ERROR (no communication)");
        }
        return;
    }

    switch (gState)
    {
    case GS_INIT:
        // eerste keer dat alles leeft -> alles rood, dan A groen
        gState = GS_ALL_RED;
        gStateStartMs = nowMs;
        lastGreenDir = 'B'; // zodat A eerst krijgt
        trafficLightSetCommand(STATE_RED);
        sendCommandToPeer('R');
        mqttPublishGlobal("ALL_RED_INIT");
        Serial.println("[FSM] INIT -> ALL_RED");
        break;

    case GS_ERROR:
        // communicatie terug: eerst veilig alles rood
        gState = GS_ALL_RED;
        gStateStartMs = nowMs;
        lastGreenDir = 'B'; // start terug bij A
        trafficLightSetCommand(STATE_RED);
        sendCommandToPeer('R');
        mqttPublishGlobal("RECOVER_ALL_RED");
        Serial.println("[FSM] ERROR -> ALL_RED");
        break;

    case GS_ALL_RED:
        if (nowMs - gStateStartMs >= (uint32_t)tClear * 1000UL)
        {
            if (lastGreenDir == 'B')
            {
                // nu A groen, B rood
                gState = GS_A_GREEN;
                gStateStartMs = nowMs;
                lastGreenDir = 'A';

                trafficLightSetCommand(STATE_GREEN); // A groen
                sendCommandToPeer('R');             // B rood

                mqttPublishGlobal("A_GREEN");
                Serial.println("[FSM] ALL_RED -> A_GREEN");
            }
            else
            {
                // nu B groen, A rood
                gState = GS_B_GREEN;
                gStateStartMs = nowMs;
                lastGreenDir = 'B';

                trafficLightSetCommand(STATE_RED);  // A rood
                sendCommandToPeer('G');             // B groen

                mqttPublishGlobal("B_GREEN");
                Serial.println("[FSM] ALL_RED -> B_GREEN");
            }
        }
        break;

    case GS_A_GREEN:
        if (nowMs - gStateStartMs >= (uint32_t)tGreenA * 1000UL)
        {
            // richting wisselen: eerst alles rood
            gState = GS_ALL_RED;
            gStateStartMs = nowMs;

            trafficLightSetCommand(STATE_RED);  // A: green->yellow->red
            sendCommandToPeer('R');             // B rood

            mqttPublishGlobal("ALL_RED_AFTER_A");
            Serial.println("[FSM] A_GREEN -> ALL_RED");
        }
        break;

    case GS_B_GREEN:
        if (nowMs - gStateStartMs >= (uint32_t)tGreenB * 1000UL)
        {
            gState = GS_ALL_RED;
            gStateStartMs = nowMs;

            trafficLightSetCommand(STATE_RED);  // A rood
            sendCommandToPeer('R');             // B: green->yellow->red

            mqttPublishGlobal("ALL_RED_AFTER_B");
            Serial.println("[FSM] B_GREEN -> ALL_RED");
        }
        break;
    }
}

#endif // IS_MASTER

// ========= Gemeenschappelijke init / loop =========

void commInit()
{
    Serial.println("Initialising LoRa...");

    // Use same pins as defined in config.h
    LoRa.setPins(LORA_NSS_PIN, LORA_RST_PIN, LORA_DIO0_PIN);

    if (!LoRa.begin(LORA_BAND))
    {
        Serial.println("LoRa init failed. Check wiring.");
        while (true)
            delay(1000);
    }
    Serial.println("LoRa initialised.");

    // basic radio settings
    LoRa.setSignalBandwidth(125E3);
    LoRa.setSpreadingFactor(8);
    LoRa.setCodingRate4(6);
    LoRa.enableCrc();

#if IS_MASTER
    connectWiFi();
    mqtt.setServer(MQTT_HOST, MQTT_PORT);
    mqtt.setCallback(mqttCallback);
#endif

    lastHeartbeatSentMs = millis();
    lastPeerSeenMs = 0;
    peerAlive = false;

    Serial.print("Node ID: ");
    Serial.println((char)NODE_ID);
}


void commLoop(uint32_t nowMs)
{
    // ===== LoRa RX =====
    int packetSize = LoRa.parsePacket();
    if (packetSize > 0)
    {
        String msg;
        while (LoRa.available())
        {
            char c = (char)LoRa.read();
            if (c != '\r' && c != '\n')
                msg += c;
        }
        if (msg.length() > 0)
        {
            Serial.print("[LoRa RX] ");
            Serial.println(msg);
            handleLoraMessage(msg, nowMs);
        }
    }

    // ===== Heartbeats verzenden =====
    if (nowMs - lastHeartbeatSentMs >= HEARTBEAT_INTERVAL_MS)
    {
        lastHeartbeatSentMs = nowMs;
        String hb = "HB:";
        hb += (char)NODE_ID;
        loraSend(hb);
        // Serial.print("[LoRa] Sent HB: ");
        // Serial.println(hb);
    }

    // ===== Communicatie timeout check =====
    if (lastPeerSeenMs == 0)
    {
        peerAlive = false;
    }
    else if (nowMs - lastPeerSeenMs > (uint32_t)COMM_TIMEOUT_SECONDS * 1000UL)
    {
        peerAlive = false;
    }

#if IS_MASTER
    // Master: global verkeerslicht-FSM + MQTT
    mqttEnsureConnected();
    mqtt.loop();

    updateGlobalFSM(nowMs);

    static uint32_t lastStatusPublishMs = 0;
    if (nowMs - lastStatusPublishMs >= 1000UL)
    {
        lastStatusPublishMs = nowMs;
        mqttPublishStatus();
    }
#else
    // Slave: als peer dood is -> zelf in ERROR
    if (!peerAlive)
    {
        trafficLightSetCommand(STATE_ERROR);
    }
    // Autorecovery gebeurt wanneer master via CMD weer RED/GREEN stuurt
#endif
}
