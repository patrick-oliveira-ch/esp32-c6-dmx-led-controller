#pragma once
#include <Arduino.h>

enum InputSource { SRC_NONE, SRC_DMX, SRC_ARTNET };

void         inputManagerBegin();
void         inputManagerLoop();
InputSource  inputManagerCurrentSource();
const char*  inputManagerSourceName();
uint8_t      inputManagerLastDimmer();
uint8_t      inputManagerLastR();
uint8_t      inputManagerLastG();
uint8_t      inputManagerLastB();
uint8_t      inputManagerLastStrobe();
uint8_t      inputManagerLastEffect();
uint8_t      inputManagerLastSpeed();
