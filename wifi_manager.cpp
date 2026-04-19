#include "wifi_manager.h"
#include "config.h"
#include "logger.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

static WifiManagerState _state = WM_SEARCHING;
static unsigned long    _connectStart = 0;
static unsigned long    _lastRetry    = 0;
static bool             _connecting   = false;
static bool             _fsOk         = false;
static String           _ssid         = "";
static String           _pass         = "";
static String           _apPass       = DEFAULT_AP_PASS;
static String           _apSSID       = DEFAULT_AP_SSID;
static String           _hostname     = DEFAULT_HOSTNAME;
static uint16_t         _dmxAddr1     = DEFAULT_DMX_ADDR_1;
static uint16_t         _dmxAddr2     = DEFAULT_DMX_ADDR_2;
static bool             _dmxLinked    = false;
static uint16_t         _numLeds1     = DEFAULT_NUM_LEDS_1;
static uint16_t         _numLeds2     = DEFAULT_NUM_LEDS_2;
static uint8_t          _artnetNet     = DEFAULT_ARTNET_NET;
static uint8_t          _artnetSubnet  = DEFAULT_ARTNET_SUBNET;
static uint8_t          _artnetUni     = DEFAULT_ARTNET_UNIVERSE;
static int              _dmxRxPin      = DMX_RX_PIN;
static std::function<void(SystemState)> _onStateChange;

static void loadConfig() {
  if (!_fsOk) return;
  if (!LittleFS.exists("/config.json")) return;
  File f = LittleFS.open("/config.json", "r");
  JsonDocument doc;
  deserializeJson(doc, f);
  f.close();
  _ssid      = doc["wifi_ssid"]     | DEFAULT_WIFI_SSID;
  _pass      = doc["wifi_password"] | DEFAULT_WIFI_PASS;
  _apPass    = doc["ap_password"] | DEFAULT_AP_PASS;
  _apSSID    = doc["ap_ssid"]     | DEFAULT_AP_SSID;
  _hostname  = doc["hostname"]    | DEFAULT_HOSTNAME;
  _dmxAddr1  = doc["dmx_addr_1"] | (int)DEFAULT_DMX_ADDR_1;
  _dmxAddr2  = doc["dmx_addr_2"] | (int)DEFAULT_DMX_ADDR_2;
  _dmxLinked = doc["dmx_linked"] | false;
  _numLeds1  = doc["num_leds_1"] | (int)DEFAULT_NUM_LEDS_1;
  _numLeds2  = doc["num_leds_2"] | (int)DEFAULT_NUM_LEDS_2;
  if (_numLeds1 > NUM_LEDS_1) _numLeds1 = NUM_LEDS_1;
  if (_numLeds2 > NUM_LEDS_2) _numLeds2 = NUM_LEDS_2;
  _artnetNet    = doc["artnet_net"]    | (int)DEFAULT_ARTNET_NET;
  _artnetSubnet = doc["artnet_subnet"] | (int)DEFAULT_ARTNET_SUBNET;
  _artnetUni    = doc["artnet_uni"]    | (int)DEFAULT_ARTNET_UNIVERSE;
  _dmxRxPin = doc["dmx_rx_pin"] | (int)DMX_RX_PIN;
}

static void startAP() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(_apSSID.c_str(), _apPass.c_str());
  _state = WM_AP;
  logMsgf("[WiFi] AP mode: %s", WiFi.softAPIP().toString().c_str());
  if (_onStateChange) _onStateChange(STATE_AP_MODE);
}

void wifiManagerBegin(std::function<void(SystemState)> onStateChange) {
  _onStateChange = onStateChange;
  _fsOk = LittleFS.begin(true);
  if (!_fsOk) logMsg("[WiFi] LittleFS mount FAILED — config will not persist");
  loadConfig();

  if (_ssid.isEmpty()) {
    startAP();
    return;
  }

  WiFi.mode(WIFI_STA);
  esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N | WIFI_PROTOCOL_11AX);
  WiFi.begin(_ssid.c_str(), _pass.c_str());
  _connectStart = millis();
  _connecting = true;
  _state = WM_SEARCHING;
  logMsgf("[WiFi] Connecting to %s...", _ssid.c_str());
  if (_onStateChange) _onStateChange(STATE_WIFI_SEARCHING);
}

void wifiManagerLoop() {
  if (_connecting) {
    if (WiFi.status() == WL_CONNECTED) {
      _connecting = false;
      _state = WM_STA;
      logMsgf("[WiFi] STA connected: %s", WiFi.localIP().toString().c_str());
      if (_onStateChange) _onStateChange(STATE_WIFI_CONNECTED);
    } else if (millis() - _connectStart > WIFI_CONNECT_TIMEOUT_MS) {
      _connecting = false;
      logMsg("[WiFi] Timeout — fallback AP");
      startAP();
      _lastRetry = millis();  // prochain retry dans 30s
    }
    return;
  }

  // Retry depuis STA (connexion perdue) ou AP (fallback) si on a des credentials
  bool shouldRetry = false;
  if (_state == WM_STA && WiFi.status() != WL_CONNECTED) shouldRetry = true;
  if (_state == WM_AP && !_ssid.isEmpty()) shouldRetry = true;

  if (shouldRetry && millis() - _lastRetry > WIFI_RETRY_INTERVAL_MS) {
    _lastRetry = millis();
    logMsgf("[WiFi] Retrying connection to %s...", _ssid.c_str());
    if (_onStateChange) _onStateChange(STATE_WIFI_SEARCHING);
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N | WIFI_PROTOCOL_11AX);
    WiFi.begin(_ssid.c_str(), _pass.c_str());
    _connectStart = millis();
    _connecting = true;
    _state = WM_SEARCHING;
  }
}

WifiManagerState wifiManagerGetState() { return _state; }

String wifiManagerGetIP() {
  return _state == WM_STA ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
}

int wifiManagerGetRSSI() {
  return _state == WM_STA ? WiFi.RSSI() : 0;
}

String wifiManagerGetSSID()     { return _ssid; }
String wifiManagerGetHostname() { return _hostname; }
String wifiManagerGetApSSID()   { return _apSSID; }
uint16_t wifiManagerGetDmxAddr1()  { return _dmxAddr1; }
uint16_t wifiManagerGetDmxAddr2()  { return _dmxAddr2; }
bool     wifiManagerGetDmxLinked() { return _dmxLinked; }
uint16_t wifiManagerGetNumLeds1()  { return _numLeds1; }
uint16_t wifiManagerGetNumLeds2()  { return _numLeds2; }
uint8_t  wifiManagerGetArtnetNet()      { return _artnetNet; }
uint8_t  wifiManagerGetArtnetSubnet()   { return _artnetSubnet; }
uint8_t  wifiManagerGetArtnetUniverse() { return _artnetUni; }
int      wifiManagerGetDmxRxPin()       { return _dmxRxPin; }
void     wifiManagerSetDmxRxPin(int p)  { _dmxRxPin = p; }
