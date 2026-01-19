#include "comm.h"
#include <SPI.h>
#include <LoRa.h>
#include "config.h"
#include "comm_mqtt.h"

#if IS_MASTER
  #include <WiFi.h>
  #include <WiFiClient.h>
  #include <PubSubClient.h>
  #include <cstdio>
#endif

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

// ========= Master-specifiek (A) =========
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
static char        lastGreenDir = 'B'; // master wordt eerst groen



// WiFi / MQTT
static WiFiClient wifiClient;

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

static void setAllRed(uint32_t nowMs, const char* reason)
{
    gState = GS_ALL_RED;
    gStateStartMs = nowMs;
    lastGreenDir = 'B'; // start terug bij A
    trafficLightSetCommand(STATE_RED);
    sendCommandToPeer('R');
    Serial.print("[FSM] -> ALL_RED (");
    Serial.print(reason);
    Serial.println(")");
}

static void switchToAGreen(uint32_t nowMs)
{
    gState = GS_A_GREEN;
    gStateStartMs = nowMs;
    lastGreenDir = 'A';
    trafficLightSetCommand(STATE_GREEN);
    sendCommandToPeer('R');
    Serial.println("[FSM] ALL_RED -> A_GREEN");
}

static void switchToBGreen(uint32_t nowMs)
{
    gState = GS_B_GREEN;
    gStateStartMs = nowMs;
    lastGreenDir = 'B';
    trafficLightSetCommand(STATE_RED);
    sendCommandToPeer('G');
    Serial.println("[FSM] ALL_RED -> B_GREEN");
}


// globale A/B richting FSM
static void updateGlobalFSM(uint32_t nowMs)
{
    // als peer weg is -> ERROR
    if (!peerAlive)
    {
        if (gState != GS_ERROR)
        {
            gState = GS_ERROR;
            trafficLightSetCommand(STATE_ERROR);
            sendCommandToPeer('E');
            Serial.println("[FSM] -> ERROR (no communication)");
        }
        return;
    }

    switch (gState)
    {
    case GS_INIT:
        // eerste keer dat alles leeft -> alles rood, dan A groen
        setAllRed(nowMs, "INIT");
        break;


    case GS_ERROR:
        // communicatie terug: eerst veilig alles rood
        setAllRed(nowMs, "RECOVER");
        break;

    case GS_ALL_RED:
        if (nowMs - gStateStartMs >= (uint32_t)tClear * 1000UL)
        {
            if (lastGreenDir == 'B') switchToAGreen(nowMs);
            else switchToBGreen(nowMs);
        }
        break;

    case GS_A_GREEN:
        if (nowMs - gStateStartMs >= (uint32_t)tGreenA * 1000UL)
            setAllRed(nowMs, "AFTER_A");
        break;

    case GS_B_GREEN:
        if (nowMs - gStateStartMs >= (uint32_t)tGreenB * 1000UL)
            setAllRed(nowMs, "AFTER_B");
        break;
    }
}

#endif // IS_MASTER

// ========= Gemeenschappelijke init / loop =========

void commInit()
{
    Serial.println("Initialising LoRa...");

    LoRa.setPins(LORA_NSS_PIN, LORA_RST_PIN, LORA_DIO0_PIN);

    if (!LoRa.begin(LORA_BAND))
    {
        Serial.println("LoRa init failed. Check wiring.");
        while (true)
            delay(1000);
    }
    Serial.println("LoRa initialised.");

    // basic radio settings (LoRa)
    LoRa.setSignalBandwidth(125E3);
    LoRa.setSpreadingFactor(8);
    LoRa.setCodingRate4(5);
    LoRa.enableCrc();

#if IS_MASTER
    // connecten met wifi + mqtt
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
    mqttEnsureConnected(nowMs);
    mqtt.loop();
    updateGlobalFSM(nowMs);

#else
    // Slave: als peer dood is -> zelf in ERROR
    if (!peerAlive)
    {
        trafficLightSetCommand(STATE_ERROR);
    }
    // Autorecovery gebeurt wanneer master via CMD weer RED/GREEN stuurt
#endif
}