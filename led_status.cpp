#include "led_status.h"
#include "config.h"

static CRGB* _leds1;
static CRGB* _leds2;
static int   _num1, _num2;

void ledStatusBegin(CRGB* leds1, int num1, CRGB* leds2, int num2) {
  _leds1 = leds1; _num1 = num1;
  _leds2 = leds2; _num2 = num2;
}

static void applyToAll(CRGB color) {
  fill_solid(_leds1, _num1, color);
  fill_solid(_leds2, _num2, color);
}

// Bleu — pixels qui courent (scanner)
static void animScanner() {
  static unsigned long last = 0;
  static uint16_t pos = 0;
  unsigned long now = millis();
  if (now - last < 30) return;
  last = now;
  applyToAll(CRGB::Black);
  if (_num1 > 0) _leds1[pos % _num1] = CRGB::Blue;
  if (_num2 > 0) _leds2[pos % _num2] = CRGB::Blue;
  pos++;
  FastLED.show();
}

// Vert — breathing 3s
static void animBreathing() {
  static unsigned long last = 0;
  unsigned long now = millis();
  if (now - last < 20) return;
  last = now;
  float t = (float)(now % 3000) / 3000.0f;
  uint8_t b = (uint8_t)(sinf(t * 2.0f * PI) * 112.0f + 143.0f);
  CRGB c = CRGB::Green;
  c.nscale8(b);
  applyToAll(c);
  FastLED.show();
}

// Orange — pulse 2s
static void animPulse() {
  static unsigned long last = 0;
  unsigned long now = millis();
  if (now - last < 20) return;
  last = now;
  float t = (float)(now % 2000) / 2000.0f;
  uint8_t b = (uint8_t)(sinf(t * 2.0f * PI) * 112.0f + 143.0f);
  CRGB c(255, 102, 0);
  c.nscale8(b);
  applyToAll(c);
  FastLED.show();
}

// Violet — barre de progression OTA
static void animProgress() {
  static unsigned long last = 0;
  static uint16_t pos = 0;
  unsigned long now = millis();
  if (now - last < 50) return;
  last = now;
  applyToAll(CRGB::Black);
  uint16_t p1 = pos % (_num1 + 1);
  uint16_t p2 = pos % (_num2 + 1);
  for (int i = 0; i < p1 && i < _num1; i++) _leds1[i] = CRGB(136, 0, 255);
  for (int i = 0; i < p2 && i < _num2; i++) _leds2[i] = CRGB(136, 0, 255);
  pos++;
  FastLED.show();
}

// Blanc breathing doux — device prêt (10s puis extinction)
static void animReady() {
  static unsigned long last = 0;
  unsigned long now = millis();
  if (now - last < 20) return;
  last = now;
  float t = (float)(now % 2000) / 2000.0f;
  uint8_t b = (uint8_t)(sinf(t * 2.0f * PI) * 60.0f + 80.0f);  // range [20, 140] — discret
  CRGB c(b, b, b);
  applyToAll(c);
  FastLED.show();
}

void ledStatusLoop(SystemState state) {
  if (!_leds1 || _num1 == 0) return;
  switch (state) {
    case STATE_WIFI_SEARCHING: animScanner();  break;
    case STATE_WIFI_CONNECTED: animBreathing(); break;
    case STATE_AP_MODE:        animPulse();    break;
    case STATE_OTA_UPDATING:   animProgress(); break;
    case STATE_READY:          animReady();    break;
  }
}
