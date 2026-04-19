#include <FastLED.h>
#include <LittleFS.h>
#include <esp_task_wdt.h>
#include "config.h"
#include "led_status.h"
#include "wifi_manager.h"
#include "ota_manager.h"
#include "web_server.h"
#include "logger.h"
#include "led_engine.h"
#include "dmx_input.h"
#include "artnet_input.h"
#include "input_manager.h"

// ── LEDs ────────────────────────────────────────────────
CRGB leds1[NUM_LEDS_1];
CRGB leds2[NUM_LEDS_2];

// ── État global ──────────────────────────────────────────
SystemState   systemState  = STATE_WIFI_SEARCHING;
bool          ledsOn       = true;
unsigned long readyAt      = 0;   // timestamp quand STATE_READY atteint

// ── Setup ────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(200);
  logMsg("=== ESP32-C6 DMX Controller ===");

  // Watchdog 60s — désactivé pour diagnostic
  // esp_task_wdt_config_t wdt_config = {
  //   .timeout_ms    = WDT_TIMEOUT_S * 1000,
  //   .idle_core_mask = 0,
  //   .trigger_panic  = true
  // };
  // esp_task_wdt_reconfigure(&wdt_config);
  // esp_task_wdt_add(NULL);

  // Hardware
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // LEDs
  FastLED.addLeds<WS2812B, PIN_1, GRB>(leds1, NUM_LEDS_1);
#if STRIP_2_ENABLED
  FastLED.addLeds<WS2812B, PIN_2, GRB>(leds2, NUM_LEDS_2);
#endif
  FastLED.setBrightness(LED_BRIGHTNESS);
  // setMaxPower retiré — LEDs 24V, calcul incompatible
  // Test hardware direct
  fill_solid(leds1, NUM_LEDS_1, CRGB::Red);
#if STRIP_2_ENABLED
  fill_solid(leds2, NUM_LEDS_2, CRGB::Red);
#endif
  FastLED.show();
  logMsg("[BOOT] Flash rouge 2s");
  delay(2000);

  // ledEngineInit reçoit la taille MAX d'allocation (compile-time)
  // ledStatusBegin reçoit la taille runtime par défaut (pour anims boot)
  // Les vraies valeurs runtime sont appliquées dans le callback WiFi après chargement config
  const int maxN2 = STRIP_2_ENABLED ? NUM_LEDS_2 : 0;
  const int defN2 = STRIP_2_ENABLED ? DEFAULT_NUM_LEDS_2 : 0;
  ledStatusBegin(leds1, DEFAULT_NUM_LEDS_1, leds2, defN2);
  ledEngineInit(leds1, NUM_LEDS_1, leds2, maxN2);

  // Réception DMX physique (démarre immédiatement, indépendante du WiFi)
  dmxInputBegin();
  inputManagerBegin();

  // WiFi (non-bloquant) — OTA + web server démarrés dans le callback
  wifiManagerBegin([](SystemState s) {
    systemState = s;  // transitoire — sera écrasé par STATE_READY ci-dessous si OTA+web démarrent
    static bool started = false;
    if (!started && (s == STATE_WIFI_CONNECTED || s == STATE_AP_MODE)) {
      started = true;
      otaManagerBegin(
        wifiManagerGetHostname().c_str(),
        []() { systemState = STATE_OTA_UPDATING; },
        []() { systemState = STATE_READY; }
      );
      webServerBegin();
      logTelnetBegin();
      // Art-Net nécessite le réseau — démarrage après connexion WiFi
      artnetInputBegin(
        wifiManagerGetArtnetNet(),
        wifiManagerGetArtnetSubnet(),
        wifiManagerGetArtnetUniverse()
      );
      // Applique la pin RX DMX configurée (peut être différente du défaut compile-time)
      dmxInputSetPins(wifiManagerGetDmxRxPin(), -1);
      ledEngineSetLinked(wifiManagerGetDmxLinked());
      StripConfig cfg1 = {wifiManagerGetDmxAddr1(), 255, 255, 255};
      StripConfig cfg2 = {wifiManagerGetDmxAddr2(), 255, 255, 255};
      ledEngineSetConfig(0, cfg1);
      ledEngineSetConfig(1, cfg2);
      // Tailles runtime (persistées dans config.json, éditables via UI)
      const int runtimeN1 = wifiManagerGetNumLeds1();
      const int runtimeN2 = STRIP_2_ENABLED ? wifiManagerGetNumLeds2() : 0;
      ledEngineSetNumLeds(0, runtimeN1);
      ledEngineSetNumLeds(1, runtimeN2);
      // led_status utilise aussi la vraie taille (pour une reconnexion WiFi future)
      ledStatusBegin(leds1, runtimeN1, leds2, runtimeN2);
      systemState = STATE_READY;
      readyAt = millis();
    }
  });

  // Créer dossiers LittleFS (monté dans wifiManagerBegin)
  if (!LittleFS.exists("/presets")) LittleFS.mkdir("/presets");
  if (!LittleFS.exists("/effects")) LittleFS.mkdir("/effects");
}

// ── Loop ─────────────────────────────────────────────────
void loop() {
  // esp_task_wdt_reset();

  // Reboot différé (web server)
  if (webServerRebootPending()) {
    ESP.restart();
  }

  otaManagerLoop();
  wifiManagerLoop();
  webServerLoop();
  logTelnetLoop();
  dmxInputLoop();
  inputManagerLoop();

  // Bouton toggle LEDs
  static bool          lastRaw      = HIGH;
  static bool          btnState     = HIGH;
  static unsigned long lastDebounce = 0;
  bool reading = digitalRead(BUTTON_PIN);
  if (reading != lastRaw) lastDebounce = millis();
  if (millis() - lastDebounce > 50) {
    if (reading != btnState) {
      btnState = reading;
      if (btnState == LOW) {
        ledsOn = !ledsOn;
        logMsgf("[BTN] LEDs %s", ledsOn ? "ON" : "OFF");
      }
    }
  }
  lastRaw = reading;

  // Extinction automatique désactivée pour le moment
  // if (ledsOn && systemState == STATE_READY && readyAt > 0 && millis() - readyAt > 10000) {
  //   ledsOn = false;
  //   logMsg("[LED] Extinction automatique");
  // }

  // Animations LED — debug one-shot
  static bool _dbg = false;
  if (!_dbg) {
    _dbg = true;
    logMsgf("[DBG] ledsOn=%d state=%d", ledsOn, systemState);
  }
  if (ledsOn) {
    if (systemState == STATE_READY) {
      ledEngineTick();
    } else {
      ledStatusLoop(systemState);
    }
  } else {
    fill_solid(leds1, NUM_LEDS_1, CRGB::Black);
#if STRIP_2_ENABLED
    fill_solid(leds2, NUM_LEDS_2, CRGB::Black);
#endif
    FastLED.show();
  }
}
