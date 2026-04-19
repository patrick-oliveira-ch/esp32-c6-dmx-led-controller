#include "logger.h"
#include <stdarg.h>
#include <WiFiServer.h>
#include <WiFiClient.h>

#define LOG_MAX_LINES 60

static String     _lines[LOG_MAX_LINES];
static int        _head  = 0;
static int        _count = 0;

static WiFiServer _telnetServer(23);
static WiFiClient _telnetClient;

static void _store(const String& msg) {
  Serial.println(msg);
  if (_telnetClient && _telnetClient.connected()) {
    _telnetClient.println(msg);
  }
  _lines[_head] = msg;
  _head = (_head + 1) % LOG_MAX_LINES;
  if (_count < LOG_MAX_LINES) _count++;
}

void logMsg(const String& msg) {
  _store(msg);
}

void logMsgf(const char* fmt, ...) {
  char buf[192];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  _store(String(buf));
}

void logTelnetBegin() {
  _telnetServer.begin();
  _telnetServer.setNoDelay(true);
  Serial.println("[Telnet] Serveur démarré port 23");
}

void logTelnetLoop() {
  if (_telnetServer.hasClient()) {
    if (_telnetClient && _telnetClient.connected()) {
      _telnetClient.stop();  // un seul client à la fois
    }
    _telnetClient = _telnetServer.accept();
    _telnetClient.println("=== ESP32-C6 Logger ===");
    // Envoyer l'historique des logs
    int start = (_count < LOG_MAX_LINES) ? 0 : _head;
    for (int i = 0; i < _count; i++) {
      _telnetClient.println(_lines[(start + i) % LOG_MAX_LINES]);
    }
  }
}

String logGetJson() {
  String out = "[";
  int start = (_count < LOG_MAX_LINES) ? 0 : _head;
  for (int i = 0; i < _count; i++) {
    int idx = (start + i) % LOG_MAX_LINES;
    if (i > 0) out += ",";
    String line = _lines[idx];
    line.replace("\\", "\\\\");
    line.replace("\"", "\\\"");
    out += "\"" + line + "\"";
  }
  out += "]";
  return out;
}
