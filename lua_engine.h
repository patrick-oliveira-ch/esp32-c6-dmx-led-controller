#pragma once
#include <Arduino.h>
#include <FastLED.h>

enum LuaStatus { LUA_OK_EXEC, LUA_ERR_LOAD, LUA_ERR_RUNTIME, LUA_ERR_TIMEOUT };

void luaEngineInit();
void luaEngineSetStrip(CRGB* leds, int numLeds);

LuaStatus luaEngineLoadScript(const char* source);

struct LuaParams {
  uint8_t  r, g, b;
  uint8_t  r2, g2, b2;
  uint8_t  speed;   // raw DMX 0-255 (legacy)
  uint16_t bpm;     // battements/minute (30-240, mappé depuis speed)
  int      numLeds;
};
LuaStatus luaEngineRunFrame(float t, const LuaParams& params);

void luaEngineUnload();
