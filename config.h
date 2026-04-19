#pragma once

// ── LEDs ─────────────────────────────────────────────────
// NUM_LEDS_* = taille d'allocation (max possible, compile-time)
// DEFAULT_NUM_LEDS_* = valeur runtime par défaut (modifiable via UI web, persistée)
#define NUM_LEDS_1              300
#define NUM_LEDS_2              300
#define DEFAULT_NUM_LEDS_1      70
#define DEFAULT_NUM_LEDS_2      70
#define PIN_1                   2      // GPIO2 — bande principale (active)
#define PIN_2                   4      // GPIO4 — réservé future 2e bande
#define STRIP_2_ENABLED         0      // 0 = une seule bande (GPIO2), 1 = 2e bande active
#define LED_BRIGHTNESS          80
#define LED_SPEED               5
#define LED_FRAME_DELAY         20

// ── Hardware ─────────────────────────────────────────────
#define RELAY_PIN               3
#define BUTTON_PIN              14

// ── WiFi credentials (laisser vide "" pour utiliser config.json) ─
#define DEFAULT_WIFI_SSID       "VST-2"
#define DEFAULT_WIFI_PASS       "tonnom21"

// ── WiFi ─────────────────────────────────────────────────
#define DEFAULT_AP_SSID         "ESP32-C6-DMX1"
#define DEFAULT_AP_PASS         "dmx12345"
#define DEFAULT_HOSTNAME        "esp32-dmx"
#define WIFI_CONNECT_TIMEOUT_MS 10000
#define WIFI_RETRY_INTERVAL_MS  30000

// ── Watchdog ─────────────────────────────────────────────
#define WDT_TIMEOUT_S           60

// ── DMX ──────────────────────────────────────────────────
#define DEFAULT_DMX_ADDR_1  1
#define DEFAULT_DMX_ADDR_2  8

// ── DMX physique (RS-485) ─────────────────────────────────
// UART1 remappé sur GPIO libres (pour ne pas conflicter avec Serial USB debug).
// Pour inverser RX/TX : permute les deux #define ci-dessous, aucun recâblage.
#define DMX_RX_PIN           22   // GPIO22 = RX DMX (entrée depuis TXD du module)
#define DMX_TX_PIN           20   // GPIO20 = TX DMX (non utilisé en réception, passé à -1)
#define DMX_PACKET_TIMEOUT_MS 5    // silence entre paquets DMX pour considérer fin
#define DMX_SOURCE_TIMEOUT_MS 1000 // pas de paquet pendant 1s → bascule sur Art-Net

// ── Art-Net ──────────────────────────────────────────────
#define ARTNET_PORT                6454
#define DEFAULT_ARTNET_UNIVERSE    0
#define DEFAULT_ARTNET_SUBNET      0
#define DEFAULT_ARTNET_NET         0
