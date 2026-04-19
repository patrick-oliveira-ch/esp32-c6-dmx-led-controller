# ESP32-C6 DMX/Art-Net LED Controller

Contrôleur de bandes LED adressables WS2812B pilotable en **DMX512 physique** (RS-485) ou **Art-Net** (UDP), avec interface web de configuration et moteur d'effets Lua synchronisés au BPM.

## Fonctionnalités

- Réception **DMX512** via UART hardware (détection BREAK matérielle, parsing 100% fiable)
- Réception **Art-Net** sur UDP port 6454 (Net / Subnet / Universe configurables)
- Priorité automatique **DMX > Art-Net** (bascule sur Art-Net après 1 s sans paquet DMX)
- Pilotage 7 canaux par bande (dimmer, R, G, B, strobe, effet, vitesse)
- Interface web responsive : configuration WiFi, adresses DMX, univers Art-Net, nombre de LEDs, GPIO RX, monitoring temps réel
- Moteur **Lua** embarqué pour effets personnalisés (presets utilisateur via web UI)
- 22 effets built-in synchronisés au BPM (Thunder, Fire, Rainbow, Strobe, Blink 1/2 1/4 2/4 3/4, Meteor, Liquid, Aurora, etc.)
- Persistance config via **LittleFS**
- **OTA** update
- Mode **AP** fallback si WiFi STA indisponible

## Hardware

| Composant | Référence |
|-----------|-----------|
| MCU | ESP32-C6 (DevKit) |
| LEDs | WS2812B (testé sur bande Govee H66413D1, 70 LEDs) |
| RS-485 | Module 3.3 V (MAX485 ou équivalent) |

### Câblage

| Signal | GPIO ESP32-C6 | Vers |
|--------|---------------|------|
| Data LED | GPIO 2 | DIN bande WS2812B |
| DMX RX | GPIO 22 (configurable) | TXD module RS-485 |
| DMX TX | GPIO 20 | RXD module RS-485 (non utilisé en RX seule) |
| RE/DE | GND | Module en mode réception permanent |
| GND | GND | GND module + GND alim LED + masse XLR (pin 1) |
| 3V3 | VCC | VCC module RS-485 |
| 5 V ext | — | VCC bande LED (alim séparée recommandée) |

**XLR DMX** : pin 2 → A, pin 3 → B, pin 1 → masse commune avec GND ESP.

## Build / Flash

Projet **Arduino IDE** (ESP32 core 3.x, IDF 5.x).

### Bibliothèques requises

- FastLED
- ArduinoJson (v7+)
- AsyncUDP (inclus core ESP32)
- ESPAsyncWebServer
- LittleFS (inclus core ESP32)

### Étapes

1. Ouvrir `esp32_c6_test_leds.ino` dans Arduino IDE
2. Sélectionner la carte **ESP32C6 Dev Module**
3. Partition Scheme : **Default 4MB with spiffs** (ou variante avec LittleFS)
4. Compiler et flasher

## Configuration

Au premier boot (sans credentials WiFi) : l'ESP démarre en mode AP `ESP32-C6-DMX1` (mot de passe `dmx12345`). Connectez-vous, puis ouvrez `http://192.168.4.1` pour configurer le WiFi STA.

Une fois connecté en STA, le device est accessible via son IP locale (visible dans les logs série) ou via mDNS si supporté par votre réseau.

### Paramètres web

- **WiFi** : SSID, mot de passe, hostname
- **DMX** : adresse de base bande 1 (1-512), bande 2 (si activée), liaison
- **Art-Net** : Net (0-127), Subnet (0-15), Universe (0-15)
- **LEDs** : nombre de LEDs par bande (≤ allocation compile-time)
- **DMX RX Pin** : GPIO 20 ou 22 (reboot requis pour appliquer)

## Mapping DMX (par bande)

7 canaux consécutifs à partir de l'adresse de base configurée :

| Offset | Canal | Plage | Rôle |
|--------|-------|-------|------|
| +0 | Dimmer | 0-255 | Luminosité globale |
| +1 | Rouge | 0-255 | Composante R |
| +2 | Vert | 0-255 | Composante G |
| +3 | Bleu | 0-255 | Composante B |
| +4 | Strobe | 0-255 | 0 = off, sinon vitesse strobe |
| +5 | Effet | 0-255 | 0 = couleur manuelle, 1-22 = built-in, 23+ = preset Lua |
| +6 | Speed | 0-255 | BPM direct (1 = 1 BPM, 134 = 134 BPM) |

## Effets built-in (canal 6)

| Index | Nom |
|-------|-----|
| 1 | Thunder |
| 2 | Fire |
| 3 | Rainbow |
| 4 | Ocean |
| 5 | Forest |
| 6 | Police |
| 7 | Disco |
| 8 | Meteor |
| 9 | Breathing |
| 10 | Twinkle |
| 11 | Lava |
| 12 | Aurora |
| 13 | Sunrise |
| 14 | Strobe |
| 15 | Matrix |
| 16 | Liquid |
| 17 | RGB Strobe |
| 18 | Blink 1/2 |
| 19 | Blink 1/4 |
| 20 | Blink 2/4 |
| 21 | Blink 3/4 |
| 22 | Meteor Bounce |

Tous les effets sont synchronisés au BPM via le canal 7. Helpers Lua disponibles : `beat(t,bpm)`, `beats(t,bpm)`, `pulse(t,bpm)`, `pulseExp(t,bpm,e)`.

## Presets Lua

Créez vos propres effets via l'interface web (canal 6 ≥ 23). Exemple minimal :

```lua
function frame(t, n, p)
  for i = 0, n - 1 do
    local v = pulseExp(t, p.bpm, 3.0)
    setRGB(i, p.r * v, p.g * v, p.b * v)
  end
end
```

Variables disponibles dans `p` : `bpm`, `r`, `g`, `b`, `r2`, `g2`, `b2`, `dimmer`.

## Architecture

```
esp32_c6_test_leds.ino   — sketch principal (setup/loop)
config.h                  — pins, defaults, timeouts
wifi_manager.*            — STA/AP fallback, persistence config
web_server.*              — UI HTTP + REST API
dmx_input.*               — UART IDF + détection BREAK matérielle
artnet_input.*            — AsyncUDP, parsing OpDmx
input_manager.*           — routing prioritaire DMX > Art-Net
led_engine.*              — moteur effets, FastLED
lua_engine.*              — runtime Lua (timeout via lua_sethook)
effects_builtin.h         — 22 effets BPM-synced
ota_manager.*             — OTA update HTTP
logger.*                  — logs ring buffer (UI + serial)
lua/                      — Lua 5.4 vendored
```

## Licence

Tous droits réservés Patapps.
