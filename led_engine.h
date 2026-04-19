#pragma once
#include <Arduino.h>
#include <FastLED.h>

struct DmxChannels {
  uint8_t dimmer;   // CH1 0-255
  uint8_t r;        // CH2 0-255
  uint8_t g;        // CH3 0-255
  uint8_t b;        // CH4 0-255
  uint8_t strobe;   // CH5 0=off, 1-255=speed
  uint8_t effect;   // CH6: 0=manual, 1-16=built-in, 17+=custom preset
  uint8_t speed;    // CH7 0-255
};

struct StripConfig {
  uint16_t dmxAddress;  // 1-512
  uint8_t  r2, g2, b2;  // secondary color for presets
};

// max1/max2 = taille d'allocation des tableaux (compile-time)
// numLeds runtime configuré via ledEngineSetNumLeds (0 = strip désactivée)
void ledEngineInit(CRGB* s1, int max1, CRGB* s2, int max2);
void ledEngineSetChannels(int strip, const DmxChannels& ch);
void ledEngineSetConfig(int strip, const StripConfig& cfg);
void ledEngineSetLinked(bool linked);
void ledEngineSetTestChannels(int strip, const DmxChannels& ch);
void ledEngineRestartEffect();
void ledEngineSetNumLeds(int strip, int n);
int  ledEngineGetNumLeds(int strip);
int  ledEngineGetMaxLeds(int strip);
void ledEngineTick();
