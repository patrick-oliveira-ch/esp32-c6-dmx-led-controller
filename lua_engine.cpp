#include "lua_engine.h"
#include "logger.h"

extern "C" {
#include "lua/lua.h"
#include "lua/lauxlib.h"
#include "lua/lualib.h"
}

static lua_State* L = nullptr;
static CRGB*      _leds = nullptr;
static int        _numLeds = 0;

static unsigned long _deadlineUs = 0;

static void luaHookTimeout(lua_State* ls, lua_Debug*) {
  if (_deadlineUs > 0 && micros() > _deadlineUs) {
    _deadlineUs = 0;
    luaL_error(ls, "script timeout");
  }
}

static int l_setPixel(lua_State* ls) {
  int idx = (int)lua_tointeger(ls, 1);
  int r   = (int)lua_tointeger(ls, 2);
  int g   = (int)lua_tointeger(ls, 3);
  int b   = (int)lua_tointeger(ls, 4);
  if (_leds && idx >= 0 && idx < _numLeds) {
    _leds[idx] = CRGB(
      (uint8_t)constrain(r, 0, 255),
      (uint8_t)constrain(g, 0, 255),
      (uint8_t)constrain(b, 0, 255)
    );
  }
  return 0;
}

static int l_getNumLeds(lua_State* ls) {
  lua_pushinteger(ls, _numLeds);
  return 1;
}

void luaEngineInit() {
  if (L) lua_close(L);
  logMsgf("[Lua] Heap avant init: %u bytes libres", ESP.getFreeHeap());
  L = luaL_newstate();
  if (!L) { logMsg("[Lua] luaL_newstate FAILED"); return; }
  logMsgf("[Lua] Heap apres luaL_newstate: %u bytes libres", ESP.getFreeHeap());
  luaL_openlibs(L);
  lua_register(L, "setPixel",   l_setPixel);
  lua_register(L, "getNumLeds", l_getNumLeds);
  // Rediriger print vers logMsg
  lua_register(L, "print", [](lua_State* ls) -> int {
    int n = lua_gettop(ls);
    String msg = "";
    for (int i = 1; i <= n; i++) {
      if (i > 1) msg += "\t";
      msg += lua_tostring(ls, i) ? lua_tostring(ls, i) : "nil";
    }
    logMsg("[Lua] " + msg);
    return 0;
  });
  logMsgf("[Lua] Heap apres openlibs: %u bytes libres", ESP.getFreeHeap());
  lua_pushnil(L); lua_setglobal(L, "dofile");
  lua_pushnil(L); lua_setglobal(L, "load");
  lua_pushnil(L); lua_setglobal(L, "loadfile");
  lua_pushnil(L); lua_setglobal(L, "require");

  // Helpers BPM : beat(t,bpm) = phase 0-1 dans le battement courant
  //               beats(t,bpm) = index entier du battement depuis t=0
  //               pulse(t,bpm) = enveloppe décroissante 1→0 sur un battement
  //               pulseExp(t,bpm,exp) = idem avec courbe exponentielle (exp>1 = plus piqué)
  static const char* BPM_HELPERS =
    "function beat(t, bpm) return (t * bpm / 60.0) % 1.0 end\n"
    "function beats(t, bpm) return math.floor(t * bpm / 60.0) end\n"
    "function pulse(t, bpm) return 1.0 - ((t * bpm / 60.0) % 1.0) end\n"
    "function pulseExp(t, bpm, e) return (1.0 - ((t * bpm / 60.0) % 1.0)) ^ (e or 3.0) end\n";
  luaL_dostring(L, BPM_HELPERS);
}

void luaEngineSetStrip(CRGB* leds, int numLeds) {
  _leds = leds;
  _numLeds = numLeds;
}

LuaStatus luaEngineLoadScript(const char* source) {
  if (!L) luaEngineInit();
  int rc = luaL_dostring(L, source);
  if (rc != LUA_OK) {
    size_t len;
    const char* err = luaL_tolstring(L, -1, &len);
    logMsgf("[Lua] Load error: %s", err);
    lua_pop(L, 2);  // tolstring + original error
    return LUA_ERR_LOAD;
  }
  lua_getglobal(L, "onFrame");
  bool ok = lua_isfunction(L, -1);
  lua_pop(L, 1);
  if (!ok) { logMsg("[Lua] onFrame() not found"); return LUA_ERR_LOAD; }
  return LUA_OK_EXEC;
}

LuaStatus luaEngineRunFrame(float t, const LuaParams& p) {
  if (!L) return LUA_ERR_RUNTIME;

  lua_getglobal(L, "onFrame");
  if (!lua_isfunction(L, -1)) { lua_pop(L, 1); return LUA_ERR_RUNTIME; }

  lua_pushnumber(L, t);

  lua_newtable(L);
  lua_pushinteger(L, p.r);       lua_setfield(L, -2, "r");
  lua_pushinteger(L, p.g);       lua_setfield(L, -2, "g");
  lua_pushinteger(L, p.b);       lua_setfield(L, -2, "b");
  lua_pushinteger(L, p.r2);      lua_setfield(L, -2, "r2");
  lua_pushinteger(L, p.g2);      lua_setfield(L, -2, "g2");
  lua_pushinteger(L, p.b2);      lua_setfield(L, -2, "b2");
  lua_pushinteger(L, p.speed);   lua_setfield(L, -2, "speed");
  lua_pushinteger(L, p.bpm);     lua_setfield(L, -2, "bpm");
  lua_pushinteger(L, p.numLeds); lua_setfield(L, -2, "numLeds");

  _deadlineUs = micros() + 25000;  // 25ms deadline
  lua_sethook(L, luaHookTimeout, LUA_MASKCOUNT, 500);
  int rc = lua_pcall(L, 2, 0, 0);
  lua_sethook(L, nullptr, 0, 0);  // désarmer
  bool timedOut = (_deadlineUs == 0);  // mis à 0 par le hook si timeout
  _deadlineUs = 0;

  if (timedOut) {
    logMsg("[Lua] TIMEOUT — script trop lent");
    return LUA_ERR_TIMEOUT;
  }
  if (rc != LUA_OK) {
    size_t len;
    const char* err = luaL_tolstring(L, -1, &len);
    logMsgf("[Lua] Runtime error: %s", err);
    lua_pop(L, 2);  // tolstring + original error
    return LUA_ERR_RUNTIME;
  }
  return LUA_OK_EXEC;
}

void luaEngineUnload() {
  if (L) { lua_close(L); L = nullptr; }
}
