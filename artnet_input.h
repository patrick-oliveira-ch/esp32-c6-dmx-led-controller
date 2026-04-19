#pragma once
#include <Arduino.h>

void          artnetInputBegin(uint8_t net, uint8_t subnet, uint8_t universe);
void          artnetInputSetFilter(uint8_t net, uint8_t subnet, uint8_t universe);
bool          artnetInputHasRecentData();
unsigned long artnetInputLastPacketMs();
uint32_t      artnetInputPacketCount();
uint8_t       artnetInputGet(uint16_t channel);  // 1..512

// Debug
uint16_t      artnetInputLastDataLen();
uint16_t      artnetInputLastPacketRawLen();
uint8_t       artnetInputLastNet();
uint8_t       artnetInputLastSub();
uint8_t       artnetInputLastUni();
uint32_t      artnetInputTotalSeen();      // tous paquets Art-Net vus (avant filtrage)
