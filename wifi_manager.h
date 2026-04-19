#pragma once
#include <Arduino.h>
#include <functional>
#include "led_status.h"

enum WifiManagerState { WM_SEARCHING, WM_STA, WM_AP };

void wifiManagerBegin(std::function<void(SystemState)> onStateChange);
void wifiManagerLoop();
WifiManagerState wifiManagerGetState();
String wifiManagerGetIP();
int    wifiManagerGetRSSI();
String wifiManagerGetSSID();
String wifiManagerGetHostname();
String wifiManagerGetApSSID();
uint16_t wifiManagerGetDmxAddr1();
uint16_t wifiManagerGetDmxAddr2();
bool     wifiManagerGetDmxLinked();
uint16_t wifiManagerGetNumLeds1();
uint16_t wifiManagerGetNumLeds2();
uint8_t  wifiManagerGetArtnetNet();
uint8_t  wifiManagerGetArtnetSubnet();
uint8_t  wifiManagerGetArtnetUniverse();
int      wifiManagerGetDmxRxPin();
void     wifiManagerSetDmxRxPin(int pin);
