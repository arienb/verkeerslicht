#pragma once
#include <Arduino.h>

// ========= NODE ROLE =========
// On node A (master controlling direction):
//   NODE_ID  'A'
//   IS_MASTER 1
// On node B (slave):
//   NODE_ID  'B'
//   IS_MASTER 0
#define NODE_ID   'A'    // change to 'B' on the second board
#define IS_MASTER 1      // 1 = master (A), 0 = slave (B)

// ========= LED PINS (adjust to YOUR wiring) =========
static const uint8_t PIN_LED_RED    = 0;
static const uint8_t PIN_LED_YELLOW = 4;
static const uint8_t PIN_LED_GREEN  = 2;

// ========= LoRa (SX1276/78) PINS + RADIO SETTINGS =========
// Use same pins & init style as your working example
static const uint8_t LORA_NSS_PIN  = 18;      // NSS (SS)
static const uint8_t LORA_RST_PIN  = 23;      // RST
static const uint8_t LORA_DIO0_PIN = 26;      // DIO0
static const long    LORA_BAND     = 868E6;  

// ========= TIMING (seconds) =========
// Master gebruikt deze als default,
// je kan ze via MQTT overschrijven.
static const uint16_t T_GREEN_A_DEFAULT = 20;   // A -> B
static const uint16_t T_GREEN_B_DEFAULT = 20;   // B -> A
static const uint16_t T_CLEAR_DEFAULT   = 5;    // beide rood tussen richtingen

// Vast volgens opgave
static const uint16_t T_YELLOW_SECONDS        = 3;     // geel tussen groen -> rood
static const uint16_t COMM_TIMEOUT_SECONDS    = 10;    // max 10 s zonder communicatie
static const uint16_t ERROR_BLINK_INTERVAL_MS = 1000;  // geel knippert elke 1 s

// LoRa heartbeat (tussen de verkeerslichten)
static const uint16_t HEARTBEAT_INTERVAL_MS = 1000;    // stuur elke 1 s HB

// ========= WiFi + MQTT (alleen gebruikt op master / node A) =========
#if IS_MASTER
static const char* const WIFI_SSID = "TP-Link_42C4";
static const char* const WIFI_PASS = "66243359";

// You can use a hostname here (DNS must be able to resolve it)
static const char* const MQTT_HOST = "192.168.0.105";
static const uint16_t    MQTT_PORT = 1883;

// topics
static const char* const MQTT_CLIENT_ID    = "TrafficMaster";
static const char* const MQTT_TOPIC_CONFIG = "traffic/config";  // "20,15,5"
static const char* const MQTT_TOPIC_STATUS = "traffic/status";  // status van node A
static const char* const MQTT_TOPIC_GLOBAL = "traffic/global";  // globale richting
#endif
