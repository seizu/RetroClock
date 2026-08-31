#ifndef GLOBALS_H
#define GLOBALS_H

#define APP_VERSION "1.9.2"
#define APP_NAME "RETRO CLOCK"

#define TIMENOW millis()

// Build-variant tag appended to the version line (flags set per env in
// platformio.ini): debug -> " (D)", test -> " (T)", prod -> "" (nothing).
#if defined(DEBUG)
  #define BUILD_TAG " (D)"
#elif defined(TEST)
  #define BUILD_TAG " (T)"
#else
  #define BUILD_TAG ""
#endif

#include <Arduino.h>
#if __has_include("secrets.h")
  #include "secrets.h"
#else
  #include "secrets_template.h"
#endif
#include "config/hardware.h"
#include <EEPROM.h>
#include <ESPAsyncWebServer.h>

#if defined(ESP8266)
  #include <ESPAsyncTCP.h>
  #include <ESP8266HTTPClient.h>
  #include <WiFiClientSecure.h>
#elif defined(ESP32)
  #include <AsyncTCP.h>
  #include <HTTPClient.h>
  #include <WiFiClientSecure.h>
#endif

#include <ArduinoJson.h>
#include <RetroGfx.h>
#include <SimpleWifi.h>
#include <WebPrefs.h>
#include <config/webPrefsConfig.h>
#include <TimeManager.h>
#include <MelodyPlayer.h>

// === global instances ===
extern DeviceConfig dc;
extern TimeManager tm;
extern SimpleWifi sw;
extern WebPrefs wp;
extern MelodyPlayer buzzer;          // shared passive-buzzer melody player (LEDC ch 0)
extern ulong ntp_update_event;
extern bool ntp_updated;
extern volatile bool g_fetchActive;  // true while any SSL fetch is in progress

// === shared helpers ===
constexpr uint32_t ALARM_CUTOFF_MS = 5UL * 60UL * 1000UL;  // buzzer safety auto-stop (5 min)

// Parse "HH:MM" into minutes-of-day (0..1439), or -1 if invalid.
int parseHHMM(const char* s);

#endif // GLOBALS_H
