#pragma once
#include <Arduino.h>

// ========= ROLES =========
#define NODE_ID   'A'    // 1 = master (A), 0 = slave (B)
#define IS_MASTER 1      // 1 = master (A), 0 = slave (B)

// ========= LED PINS =========
static const uint8_t PIN_LED_RED    = 0;
static const uint8_t PIN_LED_YELLOW = 4;
static const uint8_t PIN_LED_GREEN  = 2;

// ========= LoRa PINS =========
static const uint8_t LORA_NSS_PIN  = 18;      // NSS (SS)
static const uint8_t LORA_RST_PIN  = 23;      // RST
static const uint8_t LORA_DIO0_PIN = 26;      // DIO0
static const long    LORA_BAND     = 868E6;  

// ========= TIMING (seconds) =========
// default waarden die je kan overschrijven met MQTT webpagina
static const uint16_t T_GREEN_A_DEFAULT = 20;   
static const uint16_t T_GREEN_B_DEFAULT = 20;  
static const uint16_t T_CLEAR_DEFAULT   = 5;   

// ========= Error Timings =========
static const uint16_t T_YELLOW_SECONDS        = 3;
static const uint16_t COMM_TIMEOUT_SECONDS    = 2;
static const uint16_t ERROR_BLINK_INTERVAL_MS = 1000;

// ========= LoRa heartbeat (voor error tussen de verkeerslichten) =========
static const uint16_t HEARTBEAT_INTERVAL_MS = 1000;

// ========= WiFi + MQTT (alleen gebruikt op master / node A) =========
#if IS_MASTER
static const char* const WIFI_SSID = "TP-Link_42C4";
static const char* const WIFI_PASS = "66243359";

static const char* const MQTT_HOST = "192.168.0.105";
static const uint16_t    MQTT_PORT = 1883;

// ========= topics =========
static const char* const MQTT_CLIENT_ID    = "TrafficMaster";
static const char* const MQTT_TOPIC_CONFIG = "traffic/config";
static const char* const MQTT_TOPIC_STATUS = "traffic/status";
static const char* const MQTT_TOPIC_GLOBAL = "traffic/global";
#endif
