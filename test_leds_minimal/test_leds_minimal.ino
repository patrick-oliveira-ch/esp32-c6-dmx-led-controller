#include <FastLED.h>
#include <WiFi.h>
#include <ArduinoOTA.h>

// ── Config ───────────────────────────────────────────────
#define NUM_LEDS_1   100
#define NUM_LEDS_2   100
#define PIN_1        1
#define PIN_2        2
#define BRIGHTNESS   80

#define WIFI_SSID    "VST-2"
#define WIFI_PASS    "tonnom21"

// ── LEDs ─────────────────────────────────────────────────
CRGB leds1[NUM_LEDS_1];
CRGB leds2[NUM_LEDS_2];

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("=== TEST LEDS MINIMAL ===");

  // MOSFET 24V
  pinMode(3, OUTPUT);
  digitalWrite(3, HIGH);
  delay(100);

  // Init FastLED
  FastLED.addLeds<WS2812B, PIN_1, GRB>(leds1, NUM_LEDS_1);
  FastLED.addLeds<WS2812B, PIN_2, GRB>(leds2, NUM_LEDS_2);
  FastLED.setBrightness(BRIGHTNESS);

  // Test flash : rouge 1s, vert 1s, bleu 1s
  Serial.println("Test Rouge...");
  fill_solid(leds1, NUM_LEDS_1, CRGB::Red);
  fill_solid(leds2, NUM_LEDS_2, CRGB::Red);
  FastLED.show();
  delay(1000);

  Serial.println("Test Vert...");
  fill_solid(leds1, NUM_LEDS_1, CRGB::Green);
  fill_solid(leds2, NUM_LEDS_2, CRGB::Green);
  FastLED.show();
  delay(1000);

  Serial.println("Test Bleu...");
  fill_solid(leds1, NUM_LEDS_1, CRGB::Blue);
  fill_solid(leds2, NUM_LEDS_2, CRGB::Blue);
  FastLED.show();
  delay(1000);

  // WiFi pour OTA
  Serial.print("WiFi connexion...");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 20) {
    delay(500);
    Serial.print(".");
    tries++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi OK : " + WiFi.localIP().toString());
  } else {
    Serial.println("\nWiFi echoue — OTA indisponible");
  }

  ArduinoOTA.setHostname("esp32-test");
  ArduinoOTA.begin();
  Serial.println("OTA pret");
}

void loop() {
  ArduinoOTA.handle();

  // Scanner bleu en boucle
  static unsigned long last = 0;
  static uint16_t pos = 0;
  unsigned long now = millis();
  if (now - last < 30) return;
  last = now;

  fill_solid(leds1, NUM_LEDS_1, CRGB::Black);
  fill_solid(leds2, NUM_LEDS_2, CRGB::Black);
  leds1[pos % NUM_LEDS_1] = CRGB::Blue;
  leds2[pos % NUM_LEDS_2] = CRGB::Blue;
  FastLED.show();
  pos++;
}
