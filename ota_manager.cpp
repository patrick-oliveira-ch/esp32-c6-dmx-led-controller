#include "ota_manager.h"
#include "logger.h"
#include <ArduinoOTA.h>
#include <ESPmDNS.h>

void otaManagerBegin(const char* hostname,
                     std::function<void()> onStart,
                     std::function<void()> onEnd) {
  if (!MDNS.begin(hostname)) {
    logMsg("[OTA] MDNS.begin FAILED — hostname not resolvable");
  }
  ArduinoOTA.setHostname(hostname);
  ArduinoOTA.setPassword("dmx12345");
  ArduinoOTA.onStart([onStart]() {
    logMsg("[OTA] Update started");
    if (onStart) onStart();
  });
  ArduinoOTA.onEnd([onEnd]() {
    logMsg("[OTA] Update complete");
    if (onEnd) onEnd();
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    if (total == 0) return;
    logMsgf("[OTA] %u%%", (progress * 100u) / total);
  });
  ArduinoOTA.onError([](ota_error_t error) {
    logMsgf("[OTA] Error[%u]", error);
    ESP.restart();
  });
  ArduinoOTA.begin();
  logMsgf("[OTA] Ready — %s.local", hostname);
}

void otaManagerLoop() {
  ArduinoOTA.handle();
}
