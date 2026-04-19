#include "web_server.h"
#include "wifi_manager.h"
#include "config.h"
#include "logger.h"
#include "led_engine.h"
#include "effects_builtin.h"
#include "input_manager.h"
#include "dmx_input.h"
#include "artnet_input.h"
#include <WebServer.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

extern bool ledsOn;
static WebServer server(80);
static bool      _rebootPending = false;
static uint32_t  _rebootAt      = 0;

bool webServerRebootPending() {
  if (_rebootPending && millis() >= _rebootAt) {
    _rebootPending = false;
    return true;
  }
  return false;
}

static const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="fr">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>ESP32-C6 DMX</title>
  <style>
    *{box-sizing:border-box;margin:0;padding:0}
    body{font-family:sans-serif;max-width:600px;margin:40px auto;padding:0 16px;background:#111;color:#eee}
    h1{color:#88f;margin-bottom:24px}
    h2{color:#aaf;border-bottom:1px solid #444;padding-bottom:8px;margin-bottom:12px}
    .card{background:#1e1e1e;border-radius:8px;padding:20px;margin:16px 0}
    label{display:block;margin:10px 0 4px;color:#aaa;font-size:.9em}
    input{width:100%;padding:9px;background:#2a2a2a;color:#eee;border:1px solid #555;border-radius:4px}
    button{background:#446;color:#eee;border:none;padding:10px 22px;border-radius:4px;cursor:pointer;margin-top:10px}
    button:hover{background:#558}
    .badge{padding:3px 12px;border-radius:12px;font-size:.85em;font-weight:bold}
    .ap{background:#f60;color:#000}.sta{background:#080;color:#fff}
    p{margin:6px 0;font-size:.95em}
    .val{font-weight:bold;color:#fff}
  </style>
</head>
<body>
  <h1>ESP32-C6 DMX Controller</h1>
  <div class="card">
    <h2>Statut</h2>
    <p>Mode : <span id="mode">...</span></p>
    <p>IP : <span class="val" id="ip">...</span></p>
    <p>SSID : <span class="val" id="ssid">...</span></p>
    <p>Signal : <span class="val" id="rssi">...</span> dBm</p>
    <p>OTA hostname : <span class="val" id="host">...</span>.local</p>
  </div>
  <div class="card">
    <h2>Configuration WiFi</h2>
    <label>SSID réseau</label>
    <input type="text" id="wifi-ssid" placeholder="Nom du réseau WiFi">
    <label>Mot de passe réseau</label>
    <input type="password" id="wifi-pass" placeholder="Mot de passe">
    <label>SSID point d'accès</label>
    <input type="text" id="ap-ssid" placeholder="ESP32-C6-DMX">
    <label>Mot de passe point d'accès</label>
    <input type="password" id="ap-pass" placeholder="dmx12345">
    <label>Hostname mDNS — accès via hostname.local dans le navigateur et Arduino IDE OTA</label>
    <input type="text" id="hostname" placeholder="esp32-dmx">
    <button onclick="saveWifi()">Sauvegarder &amp; Reboot</button>
  </div>
  <div class="card">
    <h2>OTA — Mise à jour Arduino IDE</h2>
    <p>Dans Arduino IDE &#8594; Tools &#8594; Port &#8594; sélectionner <span class="val" id="ota-host">...</span>.local</p>
    <p>Puis uploader normalement.</p>
  </div>
  <div class="card">
    <h2>Sources DMX / Art-Net</h2>
    <p>Source active : <span id="src-badge" class="badge" style="background:#444;color:#eee">—</span></p>
    <p style="font-size:.85em;color:#aaa">
      Priorité : DMX physique &gt; Art-Net &gt; aucune (reprend valeurs précédentes). Basculement automatique après 1s sans paquet.
    </p>
    <p>DMX paquets : <span class="val" id="src-dmx-count">0</span> — dernier : <span id="src-dmx-age">—</span></p>
    <p>Art-Net paquets : <span class="val" id="src-art-count">0</span> — dernier : <span id="src-art-age">—</span></p>
    <h3 style="color:#aaf;margin:12px 0 8px;font-size:.9em">Canaux reçus en direct (strip 1)</h3>
    <div id="src-ch-live" style="font-family:monospace;font-size:.82em;color:#8cf;line-height:1.6"></div>
    <label style="margin-top:14px">Pin RX DMX (là où TXD du module est branché)</label>
    <select id="dmx-rx-sel" style="width:100%;padding:9px;background:#2a2a2a;color:#eee;border:1px solid #555;border-radius:4px;margin-top:4px">
      <option value="20">GPIO 20</option>
      <option value="22">GPIO 22</option>
    </select>
    <p style="font-size:.82em;color:#8cf;margin:6px 0">
      Pin RX active : <span class="val" id="dmx-pin-rx">—</span>
    </p>
    <button onclick="saveDmxPins()" style="margin-top:6px">Appliquer</button>
    <h3 style="color:#aaf;margin:14px 0 8px;font-size:.9em">Filtre Art-Net</h3>
    <label>Net (0-127)</label>
    <input type="number" id="art-net" min="0" max="127" value="0">
    <label>Sub-net (0-15)</label>
    <input type="number" id="art-subnet" min="0" max="15" value="0">
    <label>Univers (0-15)</label>
    <input type="number" id="art-uni" min="0" max="15" value="0">
    <button onclick="saveArtnet()">Sauvegarder Art-Net</button>
  </div>
  <div class="card">
    <h2>Configuration LED</h2>
    <label>Nombre de LEDs Bande 1 (max <span id="lc-max1">?</span>)</label>
    <input type="number" id="lc-n1" min="0" value="60">
    <label>Nombre de LEDs Bande 2 (max <span id="lc-max2">?</span>)</label>
    <input type="number" id="lc-n2" min="0" value="60">
    <button onclick="saveLedConfig()">Sauvegarder (appliqué en direct)</button>
  </div>
  <div class="card">
    <h2>Configuration DMX</h2>
    <label>Adresse DMX Bande 1 (1-512)</label>
    <input type="number" id="dmx-addr1" min="1" max="512" value="1">
    <label>Adresse DMX Bande 2 (1-512)</label>
    <input type="number" id="dmx-addr2" min="1" max="512" value="8">
    <label style="display:flex;align-items:center;gap:8px;margin-top:12px">
      <input type="checkbox" id="dmx-linked">
      Lier les bandes (bande 2 miroir bande 1)
    </label>
    <button onclick="saveDmxConfig()">Sauvegarder DMX</button>
  </div>
  <div class="card">
    <h2>Test en direct</h2>
    <label>Strip :
      <select id="test-strip" style="background:#2a2a2a;color:#eee;border:1px solid #555;padding:4px;border-radius:4px;margin-left:8px">
        <option value="0">Bande 1</option>
        <option value="1">Bande 2</option>
        <option value="2">Les deux</option>
      </select>
    </label>
    <label>Dimmer <span id="lbl-dim">255</span></label>
    <input type="range" min="0" max="255" value="255" oninput="document.getElementById('lbl-dim').textContent=this.value" id="sl-dim">
    <label>Rouge <span id="lbl-r">255</span></label>
    <input type="range" min="0" max="255" value="255" oninput="document.getElementById('lbl-r').textContent=this.value" id="sl-r">
    <label>Vert <span id="lbl-g">0</span></label>
    <input type="range" min="0" max="255" value="0" oninput="document.getElementById('lbl-g').textContent=this.value" id="sl-g">
    <label>Bleu <span id="lbl-b">0</span></label>
    <input type="range" min="0" max="255" value="0" oninput="document.getElementById('lbl-b').textContent=this.value" id="sl-b">
    <label>Strobe <span id="lbl-strobe">0</span></label>
    <input type="range" min="0" max="255" value="0" oninput="document.getElementById('lbl-strobe').textContent=this.value" id="sl-strobe">
    <label>Effet (0=manuel, 1-22=built-in) <span id="lbl-fx">0</span></label>
    <input type="range" min="0" max="22" value="0" oninput="document.getElementById('lbl-fx').textContent=this.value" id="sl-fx">
    <label>BPM <span id="lbl-spd">128</span></label>
    <input type="range" min="0" max="255" value="128" oninput="document.getElementById('lbl-spd').textContent=this.value" id="sl-spd">
    <button onclick="sendTest()">Envoyer</button>
    <button onclick="stopTest()" style="background:#622;margin-left:8px">Stop</button>
  </div>
  <div class="card">
    <h2>Effets &amp; Presets</h2>
    <div id="effects-list" style="font-size:.82em;color:#aaa;margin-bottom:12px;line-height:1.8"></div>
    <h3 style="color:#aaf;margin:12px 0 8px;font-size:.9em">Nouveau preset</h3>
    <label>Nom</label><input type="text" id="p-name" placeholder="Mon preset">
    <label>Effet de base</label>
    <select id="p-effect" style="width:100%;padding:9px;background:#2a2a2a;color:#eee;border:1px solid #555;border-radius:4px;margin-top:4px"></select>
    <label>Couleur principale</label>
    <input type="color" id="p-color1" value="#ff6600" style="width:100%;height:40px;margin-top:4px;border:none;border-radius:4px">
    <label>Couleur secondaire</label>
    <input type="color" id="p-color2" value="#ff0000" style="width:100%;height:40px;margin-top:4px;border:none;border-radius:4px">
    <label>BPM (0-255)</label>
    <input type="number" id="p-speed" min="0" max="255" value="128">
    <button onclick="createPreset()" style="margin-top:10px">Créer preset</button>
    <div id="presets-list" style="margin-top:12px"></div>
  </div>
  <div class="card">
    <h2>Système</h2>
    <button onclick="doReboot()">Reboot</button>
  </div>
  <div class="card">
    <h2>Logs <button style="float:right;padding:4px 10px;font-size:.8em" onclick="clearView()">Effacer vue</button></h2>
    <div id="log-box" style="background:#0a0a0a;border-radius:4px;padding:10px;height:220px;overflow-y:auto;font-family:monospace;font-size:.78em;color:#8f8"></div>
  </div>
  <script>
    async function loadStatus() {
      try {
        const d = await (await fetch('/api/status')).json();
        const modeEl = document.getElementById('mode');
        const badge = document.createElement('span');
        badge.className = 'badge ' + (d.mode==='AP' ? 'ap' : 'sta');
        badge.textContent = d.mode==='AP' ? "Point d'accès" : 'Connecté';
        modeEl.textContent = '';
        modeEl.appendChild(badge);
        document.getElementById('ip').textContent   = d.ip;
        document.getElementById('ssid').textContent = d.ssid || '—';
        document.getElementById('rssi').textContent = d.rssi || '—';
        document.getElementById('host').textContent = d.hostname;
        document.getElementById('ota-host').textContent = d.hostname;
        document.getElementById('ap-ssid').placeholder = d.ap_ssid || 'ESP32-C6-DMX';
        document.getElementById('hostname').placeholder = d.hostname || 'esp32-dmx';
      } catch(e) {}
    }
    async function saveWifi() {
      const body = {
        ssid:     document.getElementById('wifi-ssid').value,
        password: document.getElementById('wifi-pass').value,
        ap_ssid:  document.getElementById('ap-ssid').value,
        ap_pass:  document.getElementById('ap-pass').value,
        hostname: document.getElementById('hostname').value
      };
      await fetch('/api/wifi', {
        method:'POST',
        headers:{'Content-Type':'application/json'},
        body: JSON.stringify(body)
      });
      alert('Sauvegardé — reboot en cours...');
    }
    async function doReboot() {
      if (!confirm('Rebooter le device ?')) return;
      await fetch('/api/reboot', {method:'POST'});
    }
    let _lastLogCount = 0;
    let _hiddenLogs = [];

    async function loadLogs() {
      try {
        const lines = await (await fetch('/api/logs')).json();
        if (lines.length === _lastLogCount) return;
        _lastLogCount = lines.length;
        const box = document.getElementById('log-box');
        const atBottom = box.scrollHeight - box.scrollTop <= box.clientHeight + 10;
        box.innerHTML = lines.filter(l => !_hiddenLogs.includes(l))
          .map(l => '<div>' + l.replace(/</g,'&lt;') + '</div>').join('');
        if (atBottom) box.scrollTop = box.scrollHeight;
      } catch(e) {}
    }

    function clearView() {
      fetch('/api/logs').then(r=>r.json()).then(lines => {
        _hiddenLogs = [...lines];
        _lastLogCount = 0;
        document.getElementById('log-box').innerHTML = '';
      });
    }

    loadStatus();
    loadLogs();
    setInterval(loadStatus, 5000);
    setInterval(loadLogs, 1500);
    function fmtAge(ms) {
      if (!ms) return '—';
      if (ms < 1000) return ms + ' ms';
      return (ms/1000).toFixed(1) + ' s';
    }
    async function loadInputStatus() {
      try {
        const d = await (await fetch('/api/input-status')).json();
        const badge = document.getElementById('src-badge');
        badge.textContent = d.source;
        badge.style.background = d.source === 'DMX' ? '#080'
                               : d.source === 'ArtNet' ? '#058'
                               : '#444';
        badge.style.color = '#fff';
        document.getElementById('src-dmx-count').textContent = d.dmxCount;
        document.getElementById('src-art-count').textContent = d.artCount;
        document.getElementById('src-dmx-age').textContent = fmtAge(d.dmxAge);
        document.getElementById('src-art-age').textContent = fmtAge(d.artAge);
        const names = ['Dimmer', 'R', 'G', 'B', 'Strobe', 'Effect', 'Speed(BPM)'];
        const vals  = d.ch;
        document.getElementById('src-ch-live').innerHTML =
          names.map((n,i) => `<div>${n.padEnd(12,' ').replace(/ /g,'&nbsp;')}: <span style="color:#fff">${vals[i]}</span></div>`).join('');
      } catch(e) {}
    }
    async function loadArtnetConfig() {
      try {
        const d = await (await fetch('/api/artnet-config')).json();
        document.getElementById('art-net').value    = d.net;
        document.getElementById('art-subnet').value = d.subnet;
        document.getElementById('art-uni').value    = d.universe;
      } catch(e) {}
    }
    async function loadDmxPins() {
      try {
        const d = await (await fetch('/api/dmx-pins')).json();
        document.getElementById('dmx-rx-sel').value = String(d.rxPin);
        document.getElementById('dmx-pin-rx').textContent = 'GPIO' + d.rxPin;
      } catch(e) {}
    }
    async function saveDmxPins() {
      const body = { rxPin: parseInt(document.getElementById('dmx-rx-sel').value) };
      await fetch('/api/dmx-pins', {method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
      await loadDmxPins();
      alert('Pin DMX appliquée en direct');
    }
    async function saveArtnet() {
      const body = {
        net:      parseInt(document.getElementById('art-net').value),
        subnet:   parseInt(document.getElementById('art-subnet').value),
        universe: parseInt(document.getElementById('art-uni').value)
      };
      await fetch('/api/artnet-config', {method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
      alert('Art-Net sauvegardé (appliqué en direct)');
    }
    async function loadLedConfig() {
      try {
        const d = await (await fetch('/api/led-config')).json();
        document.getElementById('lc-n1').value = d.numLeds1;
        document.getElementById('lc-n2').value = d.numLeds2;
        document.getElementById('lc-n1').max = d.maxLeds1;
        document.getElementById('lc-n2').max = d.maxLeds2;
        document.getElementById('lc-max1').textContent = d.maxLeds1;
        document.getElementById('lc-max2').textContent = d.maxLeds2;
      } catch(e) {}
    }
    async function saveLedConfig() {
      const body = {
        numLeds1: parseInt(document.getElementById('lc-n1').value),
        numLeds2: parseInt(document.getElementById('lc-n2').value)
      };
      await fetch('/api/led-config', {method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
      alert('Config LED sauvegardée');
    }
    async function loadDmxConfig() {
      try {
        const d = await (await fetch('/api/dmx-config')).json();
        document.getElementById('dmx-addr1').value = d.addr1;
        document.getElementById('dmx-addr2').value = d.addr2;
        document.getElementById('dmx-linked').checked = d.linked;
      } catch(e) {}
    }
    async function saveDmxConfig() {
      const body = {
        addr1:  parseInt(document.getElementById('dmx-addr1').value),
        addr2:  parseInt(document.getElementById('dmx-addr2').value),
        linked: document.getElementById('dmx-linked').checked
      };
      await fetch('/api/dmx-config', {method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
      alert('Config DMX sauvegardée');
    }
    function hexToRgb(hex) {
      return {r:parseInt(hex.slice(1,3),16),g:parseInt(hex.slice(3,5),16),b:parseInt(hex.slice(5,7),16)};
    }
    async function sendTest() {
      const body = {
        strip:  parseInt(document.getElementById('test-strip').value),
        dimmer: parseInt(document.getElementById('sl-dim').value),
        r:      parseInt(document.getElementById('sl-r').value),
        g:      parseInt(document.getElementById('sl-g').value),
        b:      parseInt(document.getElementById('sl-b').value),
        strobe: parseInt(document.getElementById('sl-strobe').value),
        effect: parseInt(document.getElementById('sl-fx').value),
        speed:  parseInt(document.getElementById('sl-spd').value)
      };
      await fetch('/api/test-dmx', {method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
    }
    async function stopTest() {
      await fetch('/api/test-dmx', {method:'POST',headers:{'Content-Type':'application/json'},
        body:JSON.stringify({strip:2,dimmer:0,r:0,g:0,b:0,strobe:0,effect:0,speed:0})});
    }
    async function loadEffects() {
      try {
        const d = await (await fetch('/api/effects')).json();
        const sel = document.getElementById('p-effect');
        if (sel) sel.innerHTML = d.builtin.map(e=>`<option value="${e.id}">${e.id} — ${e.name}</option>`).join('');
        const el = document.getElementById('effects-list');
        if (el) el.innerHTML = d.builtin.map(e=>`<span style="margin-right:10px">[${e.id}] ${e.name}</span>`).join('');
        const pl = document.getElementById('presets-list');
        if (pl) pl.innerHTML = (d.presets||[]).map(p=>`
          <div style="display:flex;justify-content:space-between;align-items:center;margin:4px 0;background:#2a2a2a;padding:6px;border-radius:4px">
            <span>${p.name||'?'} (effet ${p.effect||'?'})</span>
            <button onclick="deletePreset(${p.id})" style="padding:2px 8px;background:#622;border:none;color:#eee;border-radius:3px;cursor:pointer">Suppr.</button>
          </div>`).join('');
      } catch(e) {}
    }
    async function createPreset() {
      const c1 = hexToRgb(document.getElementById('p-color1').value);
      const c2 = hexToRgb(document.getElementById('p-color2').value);
      const body = {
        name:   document.getElementById('p-name').value||'Preset',
        effect: parseInt(document.getElementById('p-effect').value),
        r:c1.r,g:c1.g,b:c1.b,r2:c2.r,g2:c2.g,b2:c2.b,
        speed:  parseInt(document.getElementById('p-speed').value)
      };
      await fetch('/api/presets',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
      loadEffects();
    }
    async function deletePreset(id) {
      await fetch('/api/presets?id='+id,{method:'DELETE'});
      loadEffects();
    }
    loadLedConfig();
    loadDmxConfig();
    loadArtnetConfig();
    loadDmxPins();
    loadInputStatus();
    loadEffects();
    setInterval(loadEffects, 15000);
    setInterval(loadInputStatus, 500);
  </script>
</body>
</html>
)rawliteral";

void webServerBegin() {
  server.on("/", HTTP_GET, []() {
    server.send_P(200, "text/html", INDEX_HTML);
  });

  server.on("/api/status", HTTP_GET, []() {
    JsonDocument doc;
    doc["mode"]     = (wifiManagerGetState() == WM_AP) ? "AP" : "STA";
    doc["ip"]       = wifiManagerGetIP();
    doc["ssid"]     = wifiManagerGetSSID();
    doc["rssi"]     = wifiManagerGetRSSI();
    doc["hostname"] = wifiManagerGetHostname();
    doc["ap_ssid"]  = wifiManagerGetApSSID();
    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
  });

  server.on("/api/wifi", HTTP_POST, []() {
    if (!server.hasArg("plain")) {
      server.send(400, "application/json", "{\"error\":\"no body\"}");
      return;
    }
    JsonDocument body;
    if (deserializeJson(body, server.arg("plain"))) {
      server.send(400, "application/json", "{\"error\":\"invalid json\"}");
      return;
    }
    const char* ssid = body["ssid"] | "";
    if (strlen(ssid) == 0) {
      server.send(400, "application/json", "{\"error\":\"ssid required\"}");
      return;
    }
    JsonDocument cfg;
    if (LittleFS.exists("/config.json")) {
      File fr = LittleFS.open("/config.json", "r");
      if (fr) { deserializeJson(cfg, fr); fr.close(); }
    }
    cfg["wifi_ssid"]     = body["ssid"]     | "";
    cfg["wifi_password"] = body["password"] | "";
    if (strlen(body["ap_ssid"]  | "") > 0) cfg["ap_ssid"]    = body["ap_ssid"];
    if (strlen(body["ap_pass"]  | "") > 0) cfg["ap_password"] = body["ap_pass"];
    if (strlen(body["hostname"] | "") > 0) cfg["hostname"]    = body["hostname"];
    File fw = LittleFS.open("/config.json", "w");
    if (!fw) {
      server.send(500, "application/json", "{\"error\":\"filesystem error\"}");
      return;
    }
    serializeJson(cfg, fw);
    fw.close();
    server.send(200, "application/json", "{\"ok\":true}");
    _rebootPending = true;
    _rebootAt = millis() + 500;
  });

  server.on("/api/logs", HTTP_GET, []() {
    server.send(200, "application/json", logGetJson());
  });

  server.on("/api/reboot", HTTP_POST, []() {
    server.send(200, "application/json", "{\"ok\":true}");
    _rebootPending = true;
    _rebootAt = millis() + 500;
  });

  // ── GET /api/effects ──────────────────────────────────────
  server.on("/api/effects", HTTP_GET, []() {
    String out = "{\"builtin\":[";
    for (int i = 0; i < 22; i++) {
      if (i > 0) out += ",";
      out += "{\"id\":" + String(i+1) + ",\"name\":\"" + BUILTIN_NAMES[i] + "\"}";
    }
    out += "],\"presets\":[";
    bool first = true;
    if (LittleFS.exists("/presets")) {
      File root = LittleFS.open("/presets");
      if (root) {
        File f = root.openNextFile();
        while (f) {
          if (!first) out += ",";
          first = false;
          out += f.readString();
          f.close();
          f = root.openNextFile();
        }
        root.close();
      }
    }
    out += "]}";
    server.send(200, "application/json", out);
  });

  server.on("/api/presets", HTTP_POST, []() {
    if (!server.hasArg("plain")) { server.send(400, "application/json", "{\"error\":\"no body\"}"); return; }
    JsonDocument body;
    if (deserializeJson(body, server.arg("plain"))) { server.send(400, "application/json", "{\"error\":\"invalid json\"}"); return; }
    int nextId = 1;
    while (LittleFS.exists("/presets/" + String(nextId) + ".json")) nextId++;
    body["id"] = nextId;
    File fw = LittleFS.open("/presets/" + String(nextId) + ".json", "w");
    if (!fw) { server.send(500, "application/json", "{\"error\":\"fs error\"}"); return; }
    serializeJson(body, fw);
    fw.close();
    server.send(200, "application/json", "{\"ok\":true,\"id\":" + String(nextId) + "}");
  });

  server.on("/api/presets", HTTP_DELETE, []() {
    String id = server.arg("id");
    if (id.isEmpty()) { server.send(400, "application/json", "{\"error\":\"id required\"}"); return; }
    String path = "/presets/" + id + ".json";
    if (LittleFS.exists(path)) {
      LittleFS.remove(path);
      server.send(200, "application/json", "{\"ok\":true}");
    } else {
      server.send(404, "application/json", "{\"error\":\"not found\"}");
    }
  });

  server.on("/api/input-status", HTTP_GET, []() {
    JsonDocument doc;
    doc["source"] = inputManagerSourceName();
    uint32_t dmxLast = dmxInputLastPacketMs();
    uint32_t artLast = artnetInputLastPacketMs();
    doc["dmxCount"] = dmxInputPacketCount();
    doc["artCount"] = artnetInputPacketCount();
    doc["dmxAge"]   = dmxLast ? (millis() - dmxLast) : 0;
    doc["artAge"]   = artLast ? (millis() - artLast) : 0;
    JsonArray ch = doc["ch"].to<JsonArray>();
    ch.add(inputManagerLastDimmer());
    ch.add(inputManagerLastR());
    ch.add(inputManagerLastG());
    ch.add(inputManagerLastB());
    ch.add(inputManagerLastStrobe());
    ch.add(inputManagerLastEffect());
    ch.add(inputManagerLastSpeed());
    // Debug Art-Net : dataLen du dernier paquet + canaux bruts + infos filtre
    doc["artDataLen"]   = artnetInputLastDataLen();
    doc["artRawLen"]    = artnetInputLastPacketRawLen();
    doc["artTotalSeen"] = artnetInputTotalSeen();
    doc["artLastNet"]   = artnetInputLastNet();
    doc["artLastSub"]   = artnetInputLastSub();
    doc["artLastUni"]   = artnetInputLastUni();
    JsonArray raw = doc["artRaw"].to<JsonArray>();
    for (int i = 1; i <= 32; i++) raw.add(artnetInputGet(i));
    JsonArray dmxRaw = doc["dmxRaw"].to<JsonArray>();
    for (int i = 1; i <= 32; i++) dmxRaw.add(dmxInputGet(i));
    // Liste des canaux non-zéro dans 1..512 (max 20)
    JsonArray nz = doc["artNonZero"].to<JsonArray>();
    for (int i = 1; i <= 512; i++) {
      uint8_t v = artnetInputGet(i);
      if (v != 0) {
        JsonObject o = nz.add<JsonObject>();
        o["ch"] = i;
        o["val"] = v;
        if (nz.size() >= 20) break;
      }
    }
    String out; serializeJson(doc, out);
    server.send(200, "application/json", out);
  });

  server.on("/api/dmx-pins", HTTP_GET, []() {
    JsonDocument doc;
    doc["rxPin"] = wifiManagerGetDmxRxPin();
    String out; serializeJson(doc, out);
    server.send(200, "application/json", out);
  });

  server.on("/api/dmx-pins", HTTP_POST, []() {
    if (!server.hasArg("plain")) { server.send(400, "application/json", "{\"error\":\"no body\"}"); return; }
    JsonDocument body;
    if (deserializeJson(body, server.arg("plain"))) { server.send(400, "application/json", "{\"error\":\"invalid json\"}"); return; }
    int rxPin = body["rxPin"] | (int)DMX_RX_PIN;
    if (rxPin != 20 && rxPin != 22) {
      server.send(400, "application/json", "{\"error\":\"rxPin must be 20 or 22\"}");
      return;
    }
    JsonDocument cfg;
    if (LittleFS.exists("/config.json")) {
      File fr = LittleFS.open("/config.json", "r");
      if (fr) { deserializeJson(cfg, fr); fr.close(); }
    }
    cfg["dmx_rx_pin"] = rxPin;
    File fw = LittleFS.open("/config.json", "w");
    if (!fw) { server.send(500, "application/json", "{\"error\":\"fs error\"}"); return; }
    serializeJson(cfg, fw);
    fw.close();
    wifiManagerSetDmxRxPin(rxPin);
    dmxInputSetPins(rxPin, -1);
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/artnet-config", HTTP_GET, []() {
    JsonDocument doc;
    doc["net"]      = wifiManagerGetArtnetNet();
    doc["subnet"]   = wifiManagerGetArtnetSubnet();
    doc["universe"] = wifiManagerGetArtnetUniverse();
    String out; serializeJson(doc, out);
    server.send(200, "application/json", out);
  });

  server.on("/api/artnet-config", HTTP_POST, []() {
    if (!server.hasArg("plain")) { server.send(400, "application/json", "{\"error\":\"no body\"}"); return; }
    JsonDocument body;
    if (deserializeJson(body, server.arg("plain"))) { server.send(400, "application/json", "{\"error\":\"invalid json\"}"); return; }
    JsonDocument cfg;
    if (LittleFS.exists("/config.json")) {
      File fr = LittleFS.open("/config.json", "r");
      if (fr) { deserializeJson(cfg, fr); fr.close(); }
    }
    uint8_t net = body["net"]      | 0;
    uint8_t sub = body["subnet"]   | 0;
    uint8_t uni = body["universe"] | 0;
    if (net > 127) net = 127;
    if (sub > 15)  sub = 15;
    if (uni > 15)  uni = 15;
    cfg["artnet_net"]    = net;
    cfg["artnet_subnet"] = sub;
    cfg["artnet_uni"]    = uni;
    File fw = LittleFS.open("/config.json", "w");
    if (!fw) { server.send(500, "application/json", "{\"error\":\"fs error\"}"); return; }
    serializeJson(cfg, fw);
    fw.close();
    artnetInputSetFilter(net, sub, uni);
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/led-config", HTTP_GET, []() {
    JsonDocument doc;
    doc["numLeds1"] = ledEngineGetNumLeds(0);
    doc["numLeds2"] = ledEngineGetNumLeds(1);
    doc["maxLeds1"] = ledEngineGetMaxLeds(0);
    doc["maxLeds2"] = ledEngineGetMaxLeds(1);
    String out; serializeJson(doc, out);
    server.send(200, "application/json", out);
  });

  server.on("/api/led-config", HTTP_POST, []() {
    if (!server.hasArg("plain")) { server.send(400, "application/json", "{\"error\":\"no body\"}"); return; }
    JsonDocument body;
    if (deserializeJson(body, server.arg("plain"))) { server.send(400, "application/json", "{\"error\":\"invalid json\"}"); return; }
    JsonDocument cfg;
    if (LittleFS.exists("/config.json")) {
      File fr = LittleFS.open("/config.json", "r");
      if (fr) { deserializeJson(cfg, fr); fr.close(); }
    }
    if (!body["numLeds1"].isNull()) {
      int n = (int)body["numLeds1"];
      int max = ledEngineGetMaxLeds(0);
      if (n < 0) n = 0; if (n > max) n = max;
      cfg["num_leds_1"] = n;
      ledEngineSetNumLeds(0, n);
    }
    if (!body["numLeds2"].isNull()) {
      int n = (int)body["numLeds2"];
      int max = ledEngineGetMaxLeds(1);
      if (n < 0) n = 0; if (n > max) n = max;
      cfg["num_leds_2"] = n;
      ledEngineSetNumLeds(1, n);
    }
    File fw = LittleFS.open("/config.json", "w");
    if (!fw) { server.send(500, "application/json", "{\"error\":\"fs error\"}"); return; }
    serializeJson(cfg, fw);
    fw.close();
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/dmx-config", HTTP_GET, []() {
    JsonDocument doc;
    doc["addr1"]  = wifiManagerGetDmxAddr1();
    doc["addr2"]  = wifiManagerGetDmxAddr2();
    doc["linked"] = wifiManagerGetDmxLinked();
    String out; serializeJson(doc, out);
    server.send(200, "application/json", out);
  });

  server.on("/api/dmx-config", HTTP_POST, []() {
    if (!server.hasArg("plain")) { server.send(400, "application/json", "{\"error\":\"no body\"}"); return; }
    JsonDocument body;
    if (deserializeJson(body, server.arg("plain"))) { server.send(400, "application/json", "{\"error\":\"invalid json\"}"); return; }
    JsonDocument cfg;
    if (LittleFS.exists("/config.json")) {
      File fr = LittleFS.open("/config.json", "r");
      if (fr) { deserializeJson(cfg, fr); fr.close(); }
    }
    if (!body["addr1"].isNull()) cfg["dmx_addr_1"] = (int)body["addr1"];
    if (!body["addr2"].isNull()) cfg["dmx_addr_2"] = (int)body["addr2"];
    if (!body["linked"].isNull()) cfg["dmx_linked"] = (bool)body["linked"];
    File fw = LittleFS.open("/config.json", "w");
    if (!fw) { server.send(500, "application/json", "{\"error\":\"fs error\"}"); return; }
    serializeJson(cfg, fw);
    fw.close();
    if (!body["linked"].isNull()) ledEngineSetLinked((bool)body["linked"]);
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/test-dmx", HTTP_POST, []() {
    if (!server.hasArg("plain")) { server.send(400, "application/json", "{\"error\":\"no body\"}"); return; }
    JsonDocument body;
    if (deserializeJson(body, server.arg("plain"))) { server.send(400, "application/json", "{\"error\":\"invalid json\"}"); return; }
    DmxChannels ch;
    ch.dimmer = body["dimmer"] | 255;
    ch.r      = body["r"]      | 255;
    ch.g      = body["g"]      | 255;
    ch.b      = body["b"]      | 255;
    ch.strobe = body["strobe"] | 0;
    ch.effect = body["effect"] | 0;
    ch.speed  = body["speed"]  | 128;
    int strip = body["strip"] | 0;
    ledsOn = true;
    if (strip == 2) {
      ledEngineSetTestChannels(0, ch);
      ledEngineSetTestChannels(1, ch);
    } else if (strip == 0 || strip == 1) {
      ledEngineSetTestChannels(strip, ch);
    }
    // Chaque envoi redémarre l'effet (retrigger) → réinitialise state des effets one-shot
    ledEngineRestartEffect();
    logMsgf("[TEST] strip=%d dim=%d r=%d g=%d b=%d fx=%d", strip, ch.dimmer, ch.r, ch.g, ch.b, ch.effect);
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/effects/push-lua", HTTP_POST, []() {
    if (!server.hasArg("plain")) { server.send(400, "application/json", "{\"error\":\"no body\"}"); return; }
    JsonDocument body;
    if (deserializeJson(body, server.arg("plain"))) { server.send(400, "application/json", "{\"error\":\"invalid json\"}"); return; }
    const char* script = body["script"] | "";
    const char* name   = body["name"]   | "custom";
    if (strlen(script) == 0) { server.send(400, "application/json", "{\"error\":\"empty script\"}"); return; }
    int nextId = 1;
    while (LittleFS.exists("/effects/custom_" + String(nextId) + ".lua")) nextId++;
    File fw = LittleFS.open("/effects/custom_" + String(nextId) + ".lua", "w");
    if (!fw) { server.send(500, "application/json", "{\"error\":\"fs error\"}"); return; }
    fw.print(script);
    fw.close();
    logMsgf("[LED] Lua script '%s' uploaded as custom_%d", name, nextId);
    server.send(200, "application/json", "{\"ok\":true,\"id\":" + String(nextId) + "}");
  });

  server.begin();
  Serial.println("Web server started on port 80");
}

void webServerLoop() {
  server.handleClient();
}
