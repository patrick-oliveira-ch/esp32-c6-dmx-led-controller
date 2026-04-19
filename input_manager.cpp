#include "input_manager.h"
#include "config.h"
#include "logger.h"
#include "dmx_input.h"
#include "artnet_input.h"
#include "led_engine.h"
#include "wifi_manager.h"

// Priorités :
//   1. DMX physique (si paquet < DMX_SOURCE_TIMEOUT_MS)
//   2. Art-Net     (si paquet < DMX_SOURCE_TIMEOUT_MS)
//   3. Aucune      (moteur garde sa dernière valeur / mode TEST UI actif)
//
// À chaque frame, on lit les 7 canaux de la source active depuis l'adresse
// DMX configurée (wifiManagerGetDmxAddr1/2) et on les applique au moteur LED.
// `ledEngineSetChannels` désactive automatiquement le mode TEST UI.

static InputSource _current     = SRC_NONE;
static uint8_t     _lastCh[7]   = {0,0,0,0,0,0,0};

static uint8_t readChannel(InputSource src, uint16_t c) {
  if (src == SRC_DMX)    return dmxInputGet(c);
  if (src == SRC_ARTNET) return artnetInputGet(c);
  return 0;
}

void inputManagerBegin() {
  _current = SRC_NONE;
}

void inputManagerLoop() {
  InputSource src = SRC_NONE;
  if      (dmxInputHasRecentData())    src = SRC_DMX;
  else if (artnetInputHasRecentData()) src = SRC_ARTNET;

  if (src != _current) {
    _current = src;
    logMsgf("[INPUT] Source → %s", inputManagerSourceName());
  }

  if (src == SRC_NONE) return;

  // Strip 0
  uint16_t base1 = wifiManagerGetDmxAddr1();
  DmxChannels ch1;
  ch1.dimmer = readChannel(src, base1 + 0);
  ch1.r      = readChannel(src, base1 + 1);
  ch1.g      = readChannel(src, base1 + 2);
  ch1.b      = readChannel(src, base1 + 3);
  ch1.strobe = readChannel(src, base1 + 4);
  ch1.effect = readChannel(src, base1 + 5);
  ch1.speed  = readChannel(src, base1 + 6);
  ledEngineSetChannels(0, ch1);

  // Mémorise pour l'UI (strip 0 uniquement pour l'instant)
  _lastCh[0] = ch1.dimmer;
  _lastCh[1] = ch1.r;
  _lastCh[2] = ch1.g;
  _lastCh[3] = ch1.b;
  _lastCh[4] = ch1.strobe;
  _lastCh[5] = ch1.effect;
  _lastCh[6] = ch1.speed;

  // Strip 1 si active (STRIP_2_ENABLED via config.h et DMX addr 2)
  if (ledEngineGetNumLeds(1) > 0) {
    uint16_t base2 = wifiManagerGetDmxAddr2();
    DmxChannels ch2;
    ch2.dimmer = readChannel(src, base2 + 0);
    ch2.r      = readChannel(src, base2 + 1);
    ch2.g      = readChannel(src, base2 + 2);
    ch2.b      = readChannel(src, base2 + 3);
    ch2.strobe = readChannel(src, base2 + 4);
    ch2.effect = readChannel(src, base2 + 5);
    ch2.speed  = readChannel(src, base2 + 6);
    ledEngineSetChannels(1, ch2);
  }
}

InputSource inputManagerCurrentSource() { return _current; }

const char* inputManagerSourceName() {
  switch (_current) {
    case SRC_DMX:    return "DMX";
    case SRC_ARTNET: return "ArtNet";
    default:         return "None";
  }
}

uint8_t inputManagerLastDimmer() { return _lastCh[0]; }
uint8_t inputManagerLastR()      { return _lastCh[1]; }
uint8_t inputManagerLastG()      { return _lastCh[2]; }
uint8_t inputManagerLastB()      { return _lastCh[3]; }
uint8_t inputManagerLastStrobe() { return _lastCh[4]; }
uint8_t inputManagerLastEffect() { return _lastCh[5]; }
uint8_t inputManagerLastSpeed()  { return _lastCh[6]; }
