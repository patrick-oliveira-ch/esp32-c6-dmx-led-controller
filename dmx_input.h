#pragma once
#include <Arduino.h>

void          dmxInputBegin();
void          dmxInputSetPins(int rxPin, int txPin);  // reconfigure UART à chaud
void          dmxInputLoop();
bool          dmxInputHasRecentData();          // true si paquet reçu < DMX_SOURCE_TIMEOUT_MS
unsigned long dmxInputLastPacketMs();           // millis() du dernier paquet valide
uint32_t      dmxInputPacketCount();            // total paquets reçus depuis boot
uint8_t       dmxInputGet(uint16_t channel);    // 1..512
