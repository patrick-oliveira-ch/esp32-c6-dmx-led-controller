#pragma once

// Tous les effets se synchronisent sur params.bpm (30-240 BPM, mappé depuis DMX speed 0-255).
// Helpers globaux disponibles (injectés par luaEngineInit) :
//   beat(t, bpm)       → phase 0-1 dans le battement courant
//   beats(t, bpm)      → index entier du battement depuis t=0
//   pulse(t, bpm)      → enveloppe décroissante linéaire 1→0 sur un battement
//   pulseExp(t, bpm,e) → idem avec courbe exponentielle (e par défaut 3)

static const char* BUILTIN_EFFECTS[22] = {

// ── 01 Thunder ── éclair aléatoire (~40% des beats), décroissance exp ─
R"LUA(
local lastBeat = -1
local flashStart = 0
local flashActive = false
function onFrame(t, params)
  local n = params.numLeds
  local bi = beats(t, params.bpm)
  if bi ~= lastBeat then
    lastBeat = bi
    flashActive = math.random() < 0.4
    if flashActive then flashStart = t end
  end
  local env = 0
  if flashActive then
    local dt = (t - flashStart) * params.bpm / 60.0
    env = math.max(0, (1 - dt) ^ 3)
  end
  local v = math.floor(env * 255)
  for i=0,n-1 do
    local j = 1.0
    if math.random() < 0.08 then j = 0.2 + math.random() * 0.6 end
    local vi = math.floor(v * j)
    setPixel(i, vi, vi, vi)
  end
end
)LUA",

// ── 02 Fire ── feu continu, boost d'intensité sur chaque beat ─────────
R"LUA(
local heat = nil
function onFrame(t, params)
  local n = params.numLeds
  if heat == nil or #heat ~= n then
    heat = {}
    for i=1,n do heat[i]=0 end
  end
  local cool = math.max(1, math.floor(params.bpm / 120 * 4))
  for i=1,n do heat[i]=math.max(0, heat[i]-math.random(0,cool)) end
  for i=n,3,-1 do heat[i]=math.floor((heat[i-1]+heat[i-2]+heat[i-2])/3) end
  local boost = math.floor(pulseExp(t, params.bpm, 2) * 80)
  if math.random()<0.6 then
    local y=math.random(1,math.min(5,n))
    heat[y]=math.min(255, heat[y]+math.random(120,200)+boost)
  end
  for i=1,n do
    local h=heat[i]
    local r,g,b
    if h<85 then r=h*3;g=0;b=0
    elseif h<170 then r=255;g=(h-85)*3;b=0
    else r=255;g=255;b=(h-170)*3 end
    setPixel(i-1,
      math.floor(r*params.r/255),
      math.floor(g*math.max(params.g,30)/255),
      math.floor(b*params.b/255))
  end
end
)LUA",

// ── 03 Rainbow ── 1 tour complet / 4 beats (1 mesure 4/4) ─────────────
R"LUA(
function onFrame(t, params)
  local n = params.numLeds
  local offset = t * params.bpm / 60.0 / 4.0
  for i=0,n-1 do
    local hue = (i/n + offset) % 1.0
    local h6 = hue * 6.0
    local f = h6 - math.floor(h6)
    local s = math.floor(h6)
    local r,g,b
    if s==0 then r=255;g=math.floor(255*f);b=0
    elseif s==1 then r=math.floor(255*(1-f));g=255;b=0
    elseif s==2 then r=0;g=255;b=math.floor(255*f)
    elseif s==3 then r=0;g=math.floor(255*(1-f));b=255
    elseif s==4 then r=math.floor(255*f);g=0;b=255
    else r=255;g=0;b=math.floor(255*(1-f)) end
    setPixel(i,r,g,b)
  end
end
)LUA",

// ── 04 Ocean ── vagues, 1 cycle / 2 beats ─────────────────────────────
R"LUA(
function onFrame(t, params)
  local n = params.numLeds
  local spd = params.bpm / 60.0 / 2.0 * 2 * math.pi
  for i=0,n-1 do
    local wave1 = math.sin(i/n*6.28 + t*spd) * 0.5 + 0.5
    local wave2 = math.sin(i/n*12.56 - t*spd*0.7) * 0.3 + 0.3
    local v = wave1 * 0.7 + wave2 * 0.3
    local r = math.floor(v * params.r)
    local g = math.floor(v * 0.6 * params.g)
    local b = math.floor((v * 0.8 + 0.2) * params.b)
    setPixel(i, r, g, b)
  end
end
)LUA",

// ── 05 Forest ── ondulation lente, 1 cycle / 4 beats ──────────────────
R"LUA(
function onFrame(t, params)
  local n = params.numLeds
  local spd = params.bpm / 60.0 / 4.0 * 2 * math.pi
  for i=0,n-1 do
    local phase = math.sin(t * spd + i * 0.3) * 0.5 + 0.5
    local rustle = math.sin(t * spd * 5 + i * 1.1) * 0.1 + 0.9
    local v = phase * rustle
    setPixel(i,
      math.floor(v * params.r * 0.3),
      math.floor(v * params.g),
      math.floor(v * params.b * 0.1))
  end
end
)LUA",

// ── 06 Police ── swap rouge/bleu chaque demi-beat (8-notes) ───────────
R"LUA(
function onFrame(t, params)
  local n = params.numLeds
  local bi = beats(t, params.bpm * 2)
  local bt = bi % 2
  local half = math.floor(n / 2)
  for i=0,n-1 do
    if i < half then
      if bt == 0 then setPixel(i, 255, 0, 0) else setPixel(i, 0, 0, 0) end
    else
      if bt == 1 then setPixel(i, 0, 0, 255) else setPixel(i, 0, 0, 0) end
    end
  end
end
)LUA",

// ── 07 Disco ── change quelques LEDs à chaque beat ────────────────────
R"LUA(
local cols = nil
local lastBeat = -1
function onFrame(t, params)
  local n = params.numLeds
  if cols == nil or #cols ~= n then
    cols = {}
    for i=1,n do cols[i]={math.random(0,255),math.random(0,255),math.random(0,255)} end
  end
  local bi = beats(t, params.bpm)
  if bi ~= lastBeat then
    lastBeat = bi
    local k = math.max(1, math.floor(n/8))
    for j=1,k do
      local idx = math.random(1, n)
      cols[idx] = {math.random(0,255), math.random(0,255), math.random(0,255)}
    end
  end
  for i=0,n-1 do
    setPixel(i, cols[i+1][1], cols[i+1][2], cols[i+1][3])
  end
end
)LUA",

// ── 08 Meteor ── traversée en 2 beats ─────────────────────────────────
R"LUA(
function onFrame(t, params)
  local n = params.numLeds
  local tail = 12
  local pos = (t * params.bpm / 60.0 / 2.0 * (n + tail)) % (n + tail)
  for i=0,n-1 do
    local dist = pos - i
    local v = 0
    if dist >= 0 and dist < tail then
      v = math.floor(255 * (1 - dist / tail) ^ 2)
    end
    setPixel(i,
      math.floor(v * params.r / 255),
      math.floor(v * params.g / 255),
      math.floor(v * params.b / 255))
  end
end
)LUA",

// ── 09 Breathing ── 1 respiration / 4 beats ───────────────────────────
R"LUA(
function onFrame(t, params)
  local n = params.numLeds
  local phase = t * params.bpm / 60.0 / 4.0 * 2 * math.pi
  local v = math.sin(phase) * 112.0 + 143.0
  local scale = v / 255.0
  for i=0,n-1 do
    setPixel(i,
      math.floor(params.r * scale),
      math.floor(params.g * scale),
      math.floor(params.b * scale))
  end
end
)LUA",

// ── 10 Twinkle ── burst de sparks à chaque beat ───────────────────────
R"LUA(
local sparks = nil
local lastBeat = -1
function onFrame(t, params)
  local n = params.numLeds
  if sparks == nil or #sparks ~= n then
    sparks = {}
    for i=1,n do sparks[i]=0 end
  end
  local bi = beats(t, params.bpm)
  if bi ~= lastBeat then
    lastBeat = bi
    local k = math.max(2, math.floor(n/12))
    for j=1,k do sparks[math.random(1,n)] = 255 end
  end
  local fade = math.max(3, math.floor(params.bpm / 120 * 15))
  for i=0,n-1 do
    local s = sparks[i+1]
    sparks[i+1] = math.max(0, s - fade)
    local bg_r = math.floor(params.r * 0.1)
    local bg_g = math.floor(params.g * 0.1)
    local bg_b = math.floor(params.b * 0.1)
    setPixel(i,
      math.min(255, bg_r + math.floor(s * params.r / 255)),
      math.min(255, bg_g + math.floor(s * params.g / 255)),
      math.min(255, bg_b + math.floor(s * params.b / 255)))
  end
end
)LUA",

// ── 11 Lava ── ondulation très lente, 1 cycle / 8 beats ───────────────
R"LUA(
function onFrame(t, params)
  local n = params.numLeds
  local spd = params.bpm / 60.0 / 8.0 * 2 * math.pi
  for i=0,n-1 do
    local wave = math.sin(i/n*4.0 + t*spd) * 0.4
           + math.sin(i/n*7.0 - t*spd*1.3) * 0.3
           + 0.5
    wave = math.max(0, math.min(1, wave))
    setPixel(i,
      math.floor(wave * params.r),
      math.floor(wave * wave * params.g * 0.5),
      0)
  end
end
)LUA",

// ── 12 Aurora ── 1 cycle / 8 beats ────────────────────────────────────
R"LUA(
function onFrame(t, params)
  local n = params.numLeds
  local spd = params.bpm / 60.0 / 8.0 * 2 * math.pi
  for i=0,n-1 do
    local x = i / n
    local wave1 = math.sin(x*3.14 + t*spd) * 0.5 + 0.5
    local wave2 = math.sin(x*6.28 - t*spd*0.6 + 1.0) * 0.5 + 0.5
    local green = math.floor(wave1 * params.g)
    local purple_r = math.floor(wave2 * params.r * 0.6)
    local purple_b = math.floor(wave2 * params.b)
    setPixel(i, purple_r, green, purple_b)
  end
end
)LUA",

// ── 13 Sunrise ── très lent, 1 cycle / 16 beats ───────────────────────
R"LUA(
function onFrame(t, params)
  local n = params.numLeds
  local phase = (math.sin(t * params.bpm / 60.0 / 16.0 * 2 * math.pi) + 1.0) / 2.0
  for i=0,n-1 do
    local pos = i / n
    local r = math.floor(math.min(255, phase * 255 + pos * 60))
    local g = math.floor(phase * phase * 180 * pos)
    local b = math.floor(phase * phase * phase * 120 * pos)
    setPixel(i, r, g, b)
  end
end
)LUA",

// ── 14 Strobe ── flash sur chaque beat (15% du cycle allumé) ──────────
R"LUA(
function onFrame(t, params)
  local n = params.numLeds
  local phase = beat(t, params.bpm)
  if phase < 0.15 then
    for i=0,n-1 do setPixel(i, params.r, params.g, params.b) end
  else
    for i=0,n-1 do setPixel(i, 0, 0, 0) end
  end
end
)LUA",

// ── 15 Matrix ── avance une ligne par demi-beat (8-notes) ─────────────
R"LUA(
local trail = nil
local lastBeat = -1
function onFrame(t, params)
  local n = params.numLeds
  if trail == nil or #trail ~= n then
    trail = {}
    for i=1,n do trail[i]=math.random(0,255) end
  end
  local bi = beats(t, params.bpm * 2)
  if bi ~= lastBeat then
    lastBeat = bi
    for i=n,2,-1 do trail[i]=trail[i-1] end
    if math.random() < 0.4 then trail[1]=255 else trail[1]=0 end
  end
  local fadeStep = pulseExp(t, params.bpm * 2, 1.5) * 0.3 + 0.7
  for i=0,n-1 do
    local v = trail[i+1]
    local g = math.floor(v * fadeStep * params.g / 255)
    setPixel(i, 0, g, 0)
  end
end
)LUA",

// ── 16 Liquid ── remplissage smooth one-shot, fige plein à la fin ─────
// Ease-out quadratique : remplit vite au début, ralentit à la fin pour
// un "smooth stop" naturel. Le niveau déborde au-delà de n pour que le
// halo de surface s'estompe de lui-même → pleine intensité figée.
// Durée : 2 beats. Pour redéclencher : changer d'effet puis revenir.
R"LUA(
local startT = nil
function onFrame(t, params)
  local n = params.numLeds
  if startT == nil then startT = t end
  local fillBeats = 2.0
  local fillDuration = fillBeats * 60.0 / params.bpm
  local progress = math.min(1.0, (t - startT) / fillDuration)
  local smooth = 1 - (1 - progress) * (1 - progress)
  local fadeWidth = 4.0
  local level = smooth * (n + fadeWidth)
  for i=0,n-1 do
    local dist = level - i
    local fill
    if dist >= fadeWidth then
      fill = 1.0
    elseif dist > 0 then
      fill = dist / fadeWidth
    else
      fill = 0.0
    end
    local surface = math.max(0, 1 - math.abs(dist) / 2.0)
    local r = math.floor(params.r * (fill + 0.4 * surface))
    local g = math.floor(params.g * (fill + 0.4 * surface))
    local b = math.floor(params.b * (fill + 0.1 * surface))
    setPixel(i, math.min(255, r), math.min(255, g), math.min(255, b))
  end
end
)LUA",

// ── 17 RGB Strobe ── rotation R→V→B, à 255 BPM = saturation framerate ─
// Facteur x14 : à 255 BPM → 255*14/60 ≈ 60 transitions/s (limite 60fps)
R"LUA(
function onFrame(t, params)
  local n = params.numLeds
  local bi = beats(t, params.bpm * 14)
  local phase = bi % 3
  local r, g, b = 0, 0, 0
  if     phase == 0 then r = 255
  elseif phase == 1 then g = 255
  else                   b = 255 end
  for i=0,n-1 do setPixel(i, r, g, b) end
end
)LUA",

// ── 18 Blink 1/2 ── 1 flash toutes les 2 beats, fade out exp ──────────
R"LUA(
function onFrame(t, params)
  local n = params.numLeds
  local since = (t * params.bpm / 60.0) % 2.0
  local intensity = math.exp(-since * 2.5)
  local r = math.floor(params.r * intensity)
  local g = math.floor(params.g * intensity)
  local b = math.floor(params.b * intensity)
  for i=0,n-1 do setPixel(i, r, g, b) end
end
)LUA",

// ── 19 Blink 1/4 ── 1 flash toutes les 4 beats, fade out exp ──────────
R"LUA(
function onFrame(t, params)
  local n = params.numLeds
  local since = (t * params.bpm / 60.0) % 4.0
  local intensity = math.exp(-since * 2.5)
  local r = math.floor(params.r * intensity)
  local g = math.floor(params.g * intensity)
  local b = math.floor(params.b * intensity)
  for i=0,n-1 do setPixel(i, r, g, b) end
end
)LUA",

// ── 20 Blink 2/4 ── flashs sur temps 1 et 3 d'une mesure 4/4 ──────────
R"LUA(
function onFrame(t, params)
  local n = params.numLeds
  local pos = (t * params.bpm / 60.0) % 4.0
  local since = pos < 2.0 and pos or (pos - 2.0)
  local intensity = math.exp(-since * 2.5)
  local r = math.floor(params.r * intensity)
  local g = math.floor(params.g * intensity)
  local b = math.floor(params.b * intensity)
  for i=0,n-1 do setPixel(i, r, g, b) end
end
)LUA",

// ── 21 Blink 3/4 ── flashs sur temps 1, 2, 3 d'une mesure 4/4 ─────────
R"LUA(
function onFrame(t, params)
  local n = params.numLeds
  local pos = (t * params.bpm / 60.0) % 4.0
  local since
  if     pos < 1.0 then since = pos
  elseif pos < 2.0 then since = pos - 1.0
  else               since = pos - 2.0
  end
  local intensity = math.exp(-since * 2.5)
  local r = math.floor(params.r * intensity)
  local g = math.floor(params.g * intensity)
  local b = math.floor(params.b * intensity)
  for i=0,n-1 do setPixel(i, r, g, b) end
end
)LUA",

// ── 22 Meteor Bounce ── aller-retour en 4 beats, tail inversée ────────
// La trainée suit toujours derrière le point selon la direction courante
R"LUA(
function onFrame(t, params)
  local n = params.numLeds
  local tail = 12
  local span = n + tail
  local phase = (t * params.bpm / 60.0 / 4.0) % 1.0
  local pos, dir
  if phase < 0.5 then
    pos = (phase * 2.0) * span
    dir = 1
  else
    pos = (1.0 - (phase - 0.5) * 2.0) * span
    dir = -1
  end
  for i=0,n-1 do
    local dist = (dir == 1) and (pos - i) or (i - pos)
    local v = 0
    if dist >= 0 and dist < tail then
      v = math.floor(255 * (1 - dist / tail) ^ 2)
    end
    setPixel(i,
      math.floor(v * params.r / 255),
      math.floor(v * params.g / 255),
      math.floor(v * params.b / 255))
  end
end
)LUA"

}; // fin BUILTIN_EFFECTS

static const char* BUILTIN_NAMES[22] = {
  "Thunder", "Fire", "Rainbow", "Ocean", "Forest", "Police",
  "Disco", "Meteor", "Breathing", "Twinkle", "Lava", "Aurora",
  "Sunrise", "Strobe", "Matrix", "Liquid", "RGB Strobe",
  "Blink 1/2", "Blink 1/4", "Blink 2/4", "Blink 3/4",
  "Meteor Bounce"
};
