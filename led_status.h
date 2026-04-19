#pragma once
#include <FastLED.h>

enum SystemState {
  STATE_WIFI_SEARCHING,
  STATE_WIFI_CONNECTED,
  STATE_AP_MODE,
  STATE_OTA_UPDATING,
  STATE_READY
};

void ledStatusBegin(CRGB* leds1, int num1, CRGB* leds2, int num2);
void ledStatusLoop(SystemState state);
