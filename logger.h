#pragma once
#include <Arduino.h>

void logMsg(const String& msg);
void logMsgf(const char* fmt, ...);
String logGetJson();

// Telnet logger (port 23)
void logTelnetBegin();
void logTelnetLoop();
