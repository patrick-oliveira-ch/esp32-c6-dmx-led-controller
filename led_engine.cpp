#include "led_engine.h"
#include "lua_engine.h"
#include "effects_builtin.h"
#include "logger.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

static CRGB* _s1 = nullptr; static int _n1 = 0; static int _max1 = 0;
static CRGB* _s2 = nullptr; static int _n2 = 0; static int _max2 = 0;
static bool  _linked = false;
static bool  _useTest[2] = {false, false};

static DmxChannels  _ch[2]   = {};
static DmxChannels  _test[2] = {};
static StripConfig  _cfg[2]  = {{1,255,255,255},{8,255,255,255}};

static int   _luaCurrentEffect = -1;  // effet actuellement chargé dans Lua (un seul état)
static unsigned long _lastFrame = 0;
static float _t = 0.0f;

static bool strobeGate(uint8_t strobe) {
  if (strobe == 0) return true;
  float freq = strobe / 255.0f * 25.0f + 1.0f;
  unsigned long period = (unsigned long)(1000.0f / freq);
  return (millis() % period) < (period / 2);
}

static void loadEffect(int strip, uint8_t effect) {
  // Note: un seul Lua state pour les 2 strips. Si les 2 strips ont des effets différents,
  // le state est rechargé à chaque frame pour chaque strip — correct mais coûteux (~20ms/reload).
  if (effect == _luaCurrentEffect) return;  // déjà dans Lua

  CRGB* leds = (strip == 0) ? _s1 : _s2;
  int n = (strip == 0) ? _n1 : _n2;
  luaEngineUnload();
  luaEngineInit();
  luaEngineSetStrip(leds, n);
  _luaCurrentEffect = -1;  // reset avant chargement

  if (effect >= 1 && effect <= 22) {
    LuaStatus s = luaEngineLoadScript(BUILTIN_EFFECTS[effect - 1]);
    if (s == LUA_OK_EXEC) {
      _luaCurrentEffect = effect;
    } else {
      logMsgf("[LED] Built-in effect %d load failed", effect);
    }
  } else if (effect >= 23) {
    int presetIdx = effect - 22;
    String path = "/presets/" + String(presetIdx) + ".json";
    if (!LittleFS.exists(path)) return;
    File f = LittleFS.open(path, "r");
    JsonDocument doc;
    deserializeJson(doc, f);
    f.close();
    _cfg[strip].r2 = doc["r"] | 255;
    _cfg[strip].g2 = doc["g"] | 255;
    _cfg[strip].b2 = doc["b"] | 255;
    int builtinRef = doc["effect"] | 1;
    if (builtinRef >= 1 && builtinRef <= 22) {
      LuaStatus s = luaEngineLoadScript(BUILTIN_EFFECTS[builtinRef - 1]);
      if (s == LUA_OK_EXEC) {
        _luaCurrentEffect = effect;
      }
    }
    // Si builtinRef hors 1-22, _luaCurrentEffect reste -1 → strip noire
  }
}

static void renderStrip(int strip) {
  const DmxChannels& ch = _useTest[strip] ? _test[strip] : _ch[strip];
  const StripConfig& cfg = _cfg[strip];
  CRGB* leds = (strip == 0) ? _s1 : _s2;
  int n = (strip == 0) ? _n1 : _n2;

  if (ch.effect == 0) {
    // Manual mode: direct RGB
    for (int i = 0; i < n; i++) {
      leds[i] = CRGB(ch.r, ch.g, ch.b);
    }
  } else {
    loadEffect(strip, ch.effect);
    if (_luaCurrentEffect == ch.effect) {
      luaEngineSetStrip(leds, n);
      LuaParams p;
      p.r = ch.r; p.g = ch.g; p.b = ch.b;
      p.r2 = cfg.r2; p.g2 = cfg.g2; p.b2 = cfg.b2;
      p.speed = ch.speed;
      p.bpm = ch.speed;  // valeur directe en BPM (1-255)
      p.numLeds = n;
      LuaStatus s = luaEngineRunFrame(_t, p);
      if (s != LUA_OK_EXEC) {
        fill_solid(leds, n, CRGB::Black);
      }
    } else {
      fill_solid(leds, n, CRGB::Black);
    }
  }

  // Apply dimmer (nscale8 = optimized 8-bit scaling with correct rounding)
  if (ch.dimmer < 255) {
    nscale8(leds, n, ch.dimmer);
  }

  // Apply strobe gate
  if (!strobeGate(ch.strobe)) {
    fill_solid(leds, n, CRGB::Black);
  }
}

void ledEngineInit(CRGB* s1, int max1, CRGB* s2, int max2) {
  _s1 = s1; _max1 = max1; _n1 = 0;
  _s2 = s2; _max2 = max2; _n2 = 0;
  memset(&_ch[0], 0, sizeof(DmxChannels));
  memset(&_ch[1], 0, sizeof(DmxChannels));
  // Valeurs par défaut : blanc à 30%, dimmer max
  _ch[0].r = _ch[0].g = _ch[0].b = 80;  _ch[0].dimmer = 255;
  _ch[1].r = _ch[1].g = _ch[1].b = 80;  _ch[1].dimmer = 255;
  luaEngineInit();
  logMsg("[LED] Engine initialized");
}

void ledEngineSetNumLeds(int strip, int n) {
  if (strip < 0 || strip > 1) return;
  CRGB* leds = (strip == 0) ? _s1 : _s2;
  int& nRef  = (strip == 0) ? _n1 : _n2;
  int  maxN  = (strip == 0) ? _max1 : _max2;
  if (!leds) return;
  if (n < 0) n = 0;
  if (n > maxN) n = maxN;
  // Si on réduit, éteindre les LEDs qui disparaissent pour qu'elles ne restent pas allumées
  if (n < nRef) fill_solid(leds + n, nRef - n, CRGB::Black);
  nRef = n;
  logMsgf("[LED] Strip %d : numLeds = %d (max %d)", strip, n, maxN);
}

int ledEngineGetNumLeds(int strip) {
  if (strip == 0) return _n1;
  if (strip == 1) return _n2;
  return 0;
}

int ledEngineGetMaxLeds(int strip) {
  if (strip == 0) return _max1;
  if (strip == 1) return _max2;
  return 0;
}

void ledEngineSetChannels(int strip, const DmxChannels& ch) {
  if (strip < 0 || strip > 1) return;
  _ch[strip] = ch;
  _useTest[strip] = false;
}

void ledEngineSetConfig(int strip, const StripConfig& cfg) {
  if (strip < 0 || strip > 1) return;
  _cfg[strip] = cfg;
}

void ledEngineSetLinked(bool linked) { _linked = linked; }

void ledEngineSetTestChannels(int strip, const DmxChannels& ch) {
  if (strip < 0 || strip > 1) return;
  _test[strip] = ch;
  _useTest[strip] = true;
}

void ledEngineRestartEffect() {
  // Force un reload du script Lua à la prochaine frame → réinitialise
  // toutes les vars locales des effets stateful (startT, lastBeat, heat, etc.)
  _luaCurrentEffect = -1;
}

void ledEngineTick() {
  if (!_s1 || _n1 == 0) return;

  unsigned long now = millis();
  float dt = (now - _lastFrame) / 1000.0f;
  if (dt < 0.016f) return;  // ~60fps max
  _lastFrame = now;
  _t += dt;
  if (_t > 1e6f) _t = fmodf(_t, 1000.0f);  // éviter perte de précision float

  renderStrip(0);

  static bool _dbgTick = false;
  if (!_dbgTick) {
    _dbgTick = true;
    const DmxChannels& ch0 = _useTest[0] ? _test[0] : _ch[0];
    logMsgf("[DBG] tick! useTest=%d dim=%d r=%d g=%d b=%d fx=%d", _useTest[0], ch0.dimmer, ch0.r, ch0.g, ch0.b, ch0.effect);
  }

  if (_s2 && _n2 > 0) {
    if (_linked) {
      int copyLen = min(_n1, _n2);
      for (int i = 0; i < copyLen; i++) _s2[i] = _s1[i];
      if (_n2 > _n1) fill_solid(_s2 + _n1, _n2 - _n1, CRGB::Black);
    } else {
      renderStrip(1);
    }
  }

  FastLED.show();
}
