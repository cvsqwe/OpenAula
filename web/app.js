(function () {
'use strict';

const SVGNS = 'http://www.w3.org/2000/svg';

const state = {
  layout: null,
  appState: null,
  profiles: null,
  remap: null,
  calibration: null,
};


// How long the user is considered "busy" (actively dragging/typing) after
// the last interaction - the periodic state resync skips updates during
// this window so it can't yank a slider out from under a mid-drag user.
const BUSY_GRACE_MS = 600;
let lastInteractionAt = 0;
function markBusy() { lastInteractionAt = Date.now(); }
function isBusy() { return Date.now() - lastInteractionAt < BUSY_GRACE_MS; }

// Every effect the engine knows (ids match core/StateFormat.cpp), grouped
// the way the effect picker lists them. `color: false` marks effects with
// their own fixed palette, where the layer colour does nothing.
const EFFECT_GROUPS = ['Still', 'Ambient', 'Motion', 'Reactive', 'System'];

const EFFECTS = [
  { id: 'custom',      group: 'Still',    name: 'Canvas',     desc: 'Your hand-painted keys', color: false },
  { id: 'gradient',    group: 'Still',    name: 'Horizon',    desc: 'A soft gradient drawn from your colour' },
  { id: 'off',         group: 'Still',    name: 'Blackout',   desc: 'Lights out on this layer', color: false },

  { id: 'breathing',   group: 'Ambient',  name: 'Breath',     desc: 'Slow inhale, slow exhale' },
  { id: 'aurora',      group: 'Ambient',  name: 'Aurora',     desc: 'Northern lights in your hue' },
  { id: 'starlight',   group: 'Ambient',  name: 'Starfield',  desc: 'Keys twinkle at random' },
  { id: 'heartbeat',   group: 'Ambient',  name: 'Pulse',      desc: 'A double heartbeat thump' },
  { id: 'colorcycle',  group: 'Ambient',  name: 'Chroma',     desc: 'Every hue, drifting slowly', color: false },
  { id: 'rainbowwave', group: 'Ambient',  name: 'Prism',      desc: 'A rainbow washing across', color: false },
  { id: 'spiral',      group: 'Ambient',  name: 'Vortex',     desc: 'A turning rainbow pinwheel', color: false },
  { id: 'fire',        group: 'Ambient',  name: 'Ember',      desc: 'Flickering firelight', color: false },

  { id: 'wave',        group: 'Motion',   name: 'Tide',       desc: 'Light rolls side to side' },
  { id: 'bounce',      group: 'Motion',   name: 'Pendulum',   desc: 'A beam swinging end to end' },
  { id: 'ripple',      group: 'Motion',   name: 'Echo',       desc: 'Rings spreading from the centre' },
  { id: 'raindrop',    group: 'Motion',   name: 'Rain',       desc: 'Drops fall and fade' },
  { id: 'matrix',      group: 'Motion',   name: 'Cascade',    desc: 'Digital rain, column by column' },
  { id: 'comet',       group: 'Motion',   name: 'Meteor',     desc: 'A bright streak with a tail' },
  { id: 'snake',       group: 'Motion',   name: 'Serpent',    desc: 'A trail winding through every key' },
  { id: 'sweep',       group: 'Motion',   name: 'Wipe',       desc: 'A hard-edged sweep' },
  { id: 'alternating', group: 'Motion',   name: 'Checker',    desc: 'A blinking chessboard' },
  { id: 'strobe',      group: 'Motion',   name: 'Flash',      desc: 'Sharp on / off strobe' },
  { id: 'fireworks',   group: 'Motion',   name: 'Bloom',      desc: 'Bursts opening at random', color: false },
  { id: 'confetti',    group: 'Motion',   name: 'Confetti',   desc: 'Random keys, random hues', color: false },

  { id: 'afterglow',   group: 'Reactive', name: 'Afterglow',  desc: 'Keys glow where you type' },
  { id: 'splash',      group: 'Reactive', name: 'Splash',     desc: 'Every keystroke sends a ripple' },
  { id: 'indicators',  group: 'Reactive', name: 'Lock Light', desc: 'Caps Lock lights up while on' },

  { id: 'cpu',         group: 'System',   name: 'Processor',  desc: 'CPU load as a level meter', color: false },
  { id: 'memory',      group: 'System',   name: 'Memory',     desc: 'RAM use filling from the bottom' },
  { id: 'thermal',     group: 'System',   name: 'Thermal',    desc: 'Cool blue to hot red with CPU heat', color: false },
  { id: 'network',     group: 'System',   name: 'Traffic',    desc: 'Sparkles with network activity' },
  { id: 'clock',       group: 'System',   name: 'Clock',      desc: 'F-keys show the hour, digits the minutes' },
];

const effectById = (id) => EFFECTS.find((e) => e.id === id) || EFFECTS[0];

const BLENDS = [
  { value: 'normal',   label: 'Cover' },
  { value: 'add',      label: 'Add' },
  { value: 'lighten',  label: 'Lighten' },
  { value: 'multiply', label: 'Tint' },
];

const PRESETS = ['#7c5cff', '#00d6ff', '#2fd47a', '#ffd23f', '#ff5470', '#ff8a3d', '#ffffff'];


// ---------- tiny helpers ----------

const $ = (id) => document.getElementById(id);

async function apiGet(path) {
  const res = await fetch(path);
  if (!res.ok) throw new Error(path + ' -> ' + res.status);
  return res.json();
}

async function apiPost(path, body) {
  const res = await fetch(path, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(body || {}),
  });
  if (!res.ok) throw new Error(path + ' -> ' + res.status);
  return res.json();
}

let toastTimer = null;
function toast(msg) {
  const t = $('toast');
  t.textContent = msg;
  t.classList.add('show');
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => t.classList.remove('show'), 2200);
}

function clamp(v, lo, hi) { return Math.max(lo, Math.min(hi, v)); }

function hex2(n) { return clamp(Math.round(n), 0, 255).toString(16).padStart(2, '0'); }
function rgbToHex(c) { return '#' + hex2(c.r) + hex2(c.g) + hex2(c.b); }
function scaleColor(c, factor) {
  return { r: c.r * factor, g: c.g * factor, b: c.b * factor };
}
function hexToRgb(hex) {
  hex = hex.replace('#', '');
  return {
    r: parseInt(hex.substring(0, 2), 16) || 0,
    g: parseInt(hex.substring(2, 4), 16) || 0,
    b: parseInt(hex.substring(4, 6), 16) || 0,
  };
}

function hsvToRgb(h, s, v) {
  h = ((h % 360) + 360) % 360;
  const c = v * s;
  const x = c * (1 - Math.abs((h / 60) % 2 - 1));
  const m = v - c;
  let r, g, b;

  if (h < 60)       { r = c; g = x; b = 0; }
  else if (h < 120) { r = x; g = c; b = 0; }
  else if (h < 180) { r = 0; g = c; b = x; }
  else if (h < 240) { r = 0; g = x; b = c; }
  else if (h < 300) { r = x; g = 0; b = c; }
  else              { r = c; g = 0; b = x; }

  return { r: (r + m) * 255, g: (g + m) * 255, b: (b + m) * 255 };
}

function rgbToHsv(r, g, b) {
  r /= 255; g /= 255; b /= 255;
  const max = Math.max(r, g, b), min = Math.min(r, g, b);
  const d = max - min;
  let h = 0;

  if (d !== 0) {
    if (max === r) h = 60 * (((g - b) / d) % 6);
    else if (max === g) h = 60 * ((b - r) / d + 2);
    else h = 60 * ((r - g) / d + 4);
  }

  if (h < 0) h += 360;

  const s = max === 0 ? 0 : d / max;
  return { h, s, v: max };
}

// ---------- effect preview math ----------
// Mirrors core/LightingEngine.cpp's computeFrame() as closely as JS allows,
// so the live browser preview shows the same animation the daemon drives
// on the real hardware instead of a static accent-colour tint.

// h in [0,1) - matches LightingEngine.cpp's hsvColor(), NOT the 0-360
// degree convention hsvToRgb() above uses for the colour picker.
function hueToRgb01(h) {
  const sector = ((h % 1 + 1) % 1) * 6;
  const i = Math.floor(sector);
  const f = sector - i;
  const p = 0, q = 1 - f, tt = f;
  let r, g, b;

  switch (i % 6) {
    case 0: r = 1; g = tt; b = p; break;
    case 1: r = q; g = 1; b = p; break;
    case 2: r = p; g = 1; b = tt; break;
    case 3: r = p; g = q; b = 1; break;
    case 4: r = tt; g = p; b = 1; break;
    default: r = 1; g = p; b = q; break;
  }

  return { r: r * 255, g: g * 255, b: b * 255 };
}

function triangleWave(t, period) {
  const half = period / 2;
  let phase = t % period;
  if (phase < 0) phase += period;
  const tri = half - Math.abs(phase - half);
  return tri / half;
}

// Cheap deterministic pseudo-random 0..1, ported bit-for-bit (using
// Math.imul for the same wraparound 32-bit multiply C++ does) from
// LightingEngine.cpp's hash01() - same keys twinkle/flicker/sparkle in the
// same pattern the daemon would show, not an independent random stream.
function hash01(x, salt) {
  let h = (Math.imul(x, 374761393) + Math.imul(salt, 668265263)) >>> 0;
  h = Math.imul(h ^ (h >>> 13), 1274126177) >>> 0;
  h = (h ^ (h >>> 16)) >>> 0;
  return (h & 0xFFFFFF) / 0xFFFFFF;
}

function scaleColorPreview(c, k) {
  k = clamp(k, 0, 1);
  return { r: c.r * k, g: c.g * k, b: c.b * k };
}

const BLACK = { r: 0, g: 0, b: 0 };
const WHITE = { r: 255, g: 255, b: 255 };
const NO_SIGNALS = { cpu: 0, memory: 0, temperature: 0, network: 0, hour: 0, minute: 0, second: 0, capsLock: false, keyAge: [] };

function mod(a, m) { return ((a % m) + m) % m; }

// h, s, v all 0..1 - LightingEngine.cpp's fromHsv().
function hsv01(h, s, v) { return hsvToRgb(mod(h, 1) * 360, clamp(s, 0, 1), clamp(v, 0, 1)); }

function shiftHue(c, shift) {
  const hsv = rgbToHsv(c.r, c.g, c.b);
  return hsvToRgb(hsv.h + shift * 360, hsv.s, hsv.v);
}

function mixColor(a, b, k) {
  k = clamp(k, 0, 1);
  return { r: a.r + (b.r - a.r) * k, g: a.g + (b.g - a.g) * k, b: a.b + (b.b - a.b) * k };
}

// t is already speed-scaled elapsed seconds, exactly like the `t` param
// LightingEngine::computeFrame receives from daemon/main.cpp.
function computePreviewFrame(mode, t, keys, baseColors, activeColor, sys) {
  const n = keys.length;
  const frame = new Array(n);
  sys = sys || NO_SIGNALS;

  const maxOf = (fn) => { let m = 0; for (const k of keys) m = Math.max(m, fn(k)); return m; };

  switch (mode) {
    case 'breathing': {
      const k = 0.1 + 0.9 * (Math.sin(t * 2.0) + 1.0) / 2.0;
      for (let i = 0; i < n; i++) frame[i] = scaleColorPreview(activeColor, k);
      return frame;
    }
    case 'colorcycle': {
      for (let i = 0; i < n; i++) {
        const hue = (t * 0.15 + keys[i].x * 0.03) % 1;
        frame[i] = hueToRgb01(hue);
      }
      return frame;
    }
    case 'bounce': {
      const maxX = maxOf((k) => k.x + k.w);
      const period = 4.0, falloff = 2.2;
      const pos = triangleWave(t, period) * maxX;
      for (let i = 0; i < n; i++) {
        const center = keys[i].x + keys[i].w / 2;
        const dist = Math.abs(center - pos);
        const b = Math.max(0, 1 - dist / falloff);
        frame[i] = scaleColorPreview(activeColor, b);
      }
      return frame;
    }
    case 'wave': {
      const waveLength = 6.0;
      for (let i = 0; i < n; i++) {
        const center = keys[i].x + keys[i].w / 2;
        const phase = center / waveLength - t * 0.35;
        const b = 0.15 + 0.85 * (Math.sin(2 * Math.PI * phase) + 1) / 2;
        frame[i] = scaleColorPreview(activeColor, b);
      }
      return frame;
    }
    case 'ripple': {
      const maxX = maxOf((k) => k.x + k.w), maxY = maxOf((k) => k.y + k.h);
      const cx = maxX / 2, cy = maxY / 2, ringSpacing = 1.6;
      for (let i = 0; i < n; i++) {
        const kx = keys[i].x + keys[i].w / 2, ky = keys[i].y + keys[i].h / 2;
        const dist = Math.hypot(kx - cx, ky - cy);
        const phase = dist / ringSpacing - t * 0.6;
        const b = 0.15 + 0.85 * (Math.sin(2 * Math.PI * phase) + 1) / 2;
        frame[i] = scaleColorPreview(activeColor, b);
      }
      return frame;
    }
    case 'starlight': {
      for (let i = 0; i < n; i++) {
        const seed = hash01(keys[i].ledIndex, 17);
        const period = 1.5 + seed * 3.0;
        const phase = seed * 11.0;
        let twinklePos = (t + phase) % period;
        if (twinklePos < 0) twinklePos += period;
        twinklePos /= period;
        const dist = Math.abs(twinklePos - 0.5) * 2.0;
        const b = Math.pow(Math.max(0, 1 - dist), 6.0) * 0.9 + 0.04;
        frame[i] = scaleColorPreview(activeColor, b);
      }
      return frame;
    }
    case 'raindrop': {
      const maxY = maxOf((k) => k.y + k.h);
      for (let i = 0; i < n; i++) {
        const colSeed = hash01(Math.round(keys[i].x * 3.0), 41);
        const period = 1.2 + colSeed * 2.2;
        const phase = colSeed * 13.0;
        let localT = (t + phase) % period;
        if (localT < 0) localT += period;
        const dropY = (localT / period) * (maxY + 2.5) - 1.5;
        const ky = keys[i].y + keys[i].h / 2;
        const dist = dropY - ky;
        const b = (dist >= 0 && dist < 1.4) ? (1 - dist / 1.4) : 0;
        frame[i] = scaleColorPreview(activeColor, b);
      }
      return frame;
    }
    case 'comet': {
      const maxX = maxOf((k) => k.x + k.w), maxY = maxOf((k) => k.y + k.h);
      const pathLen = maxX + maxY * 1.3, period = 3.0;
      let posFrac = (t / period) % 1;
      if (posFrac < 0) posFrac += 1;
      const pos = posFrac * (pathLen + 6.0) - 3.0;
      for (let i = 0; i < n; i++) {
        const coord = keys[i].x + keys[i].y * 1.3;
        const dist = pos - coord;
        const b = (dist >= 0 && dist < 5.0) ? Math.pow(1 - dist / 5.0, 2.0) : 0;
        frame[i] = scaleColorPreview(activeColor, b);
      }
      return frame;
    }
    case 'fire': {
      const maxY = maxOf((k) => k.y + k.h);
      const flickerStep = Math.floor(t * 6.0);
      for (let i = 0; i < n; i++) {
        const heightFactor = 1 - (keys[i].y / Math.max(1, maxY)) * 0.5;
        const n1 = hash01(keys[i].ledIndex, flickerStep);
        const n2 = hash01(keys[i].ledIndex, flickerStep + 1000);
        const flicker = 0.55 + 0.45 * (n1 * 0.7 + n2 * 0.3);
        const heat = clamp(heightFactor * flicker, 0, 1);
        frame[i] = { r: 255 * heat, g: 90 * heat * heat, b: 12 * heat * heat };
      }
      return frame;
    }
    case 'rainbowwave': {
      for (let i = 0; i < n; i++) {
        const hue = (keys[i].x * 0.025 + keys[i].y * 0.07 + t * 0.2) % 1;
        frame[i] = hueToRgb01(hue);
      }
      return frame;
    }
    case 'heartbeat': {
      const pulse = (x, center, width) => {
        const d = Math.abs(x - center);
        return d < width ? Math.pow(Math.cos((d / width) * Math.PI / 2.0), 2.0) : 0;
      };
      const period = 1.6;
      const localT = t % period;
      let b = 0.06 + 0.55 * pulse(localT, 0.10, 0.10) + 0.92 * pulse(localT, 0.34, 0.14);
      b = clamp(b, 0, 1);
      for (let i = 0; i < n; i++) frame[i] = scaleColorPreview(activeColor, b);
      return frame;
    }
    case 'strobe': {
      const period = 0.6;
      const phase = (t % period) / period;
      const b = phase < 0.5 ? 1.0 : 0.05;
      for (let i = 0; i < n; i++) frame[i] = scaleColorPreview(activeColor, b);
      return frame;
    }
    case 'alternating': {
      const period = 1.0;
      const phase = (t % period) / period;
      const onA = phase < 0.5;
      for (let i = 0; i < n; i++) {
        const groupA = ((Math.floor(keys[i].x) + Math.floor(keys[i].y)) % 2) === 0;
        const b = (groupA === onA) ? 1.0 : 0.08;
        frame[i] = scaleColorPreview(activeColor, b);
      }
      return frame;
    }
    case 'confetti': {
      for (let i = 0; i < n; i++) {
        const seed = hash01(keys[i].ledIndex, 71);
        const period = 0.6 + seed * 1.8;
        const phase = (t % period) / period;
        const cycle = Math.floor(t / period);
        const b = Math.pow(Math.max(0, 1 - phase * 4.0), 3.0);
        const hue = hash01(keys[i].ledIndex + cycle * 977, 133);
        frame[i] = scaleColorPreview(hueToRgb01(hue), b);
      }
      return frame;
    }
    case 'snake': {
      const period = 3.0, tailLen = 8.0;
      const pos = ((t % period) / period) * n;
      for (let i = 0; i < n; i++) {
        let d = pos - i;
        if (d < 0) d += n;
        const b = d < tailLen ? (1 - d / tailLen) : 0;
        frame[i] = scaleColorPreview(activeColor, b);
      }
      return frame;
    }
    case 'spiral': {
      const maxX = maxOf((k) => k.x + k.w), maxY = maxOf((k) => k.y + k.h);
      const cx = maxX / 2, cy = maxY / 2;
      for (let i = 0; i < n; i++) {
        const kx = keys[i].x + keys[i].w / 2, ky = keys[i].y + keys[i].h / 2;
        const angle = Math.atan2(ky - cy, kx - cx) / (2 * Math.PI);
        const dist = Math.hypot(kx - cx, ky - cy);
        let hue = (angle + dist * 0.12 - t * 0.25) % 1;
        if (hue < 0) hue += 1;
        frame[i] = hueToRgb01(hue);
      }
      return frame;
    }
    case 'fireworks': {
      const maxX = maxOf((k) => k.x + k.w), maxY = maxOf((k) => k.y + k.h);
      const boardR = Math.hypot(maxX, maxY) / 2;
      const period = 1.4;
      const w = Math.floor(t / period);
      const localT = t - w * period;
      const ox = hash01(w, 201) * maxX;
      const oy = hash01(w, 202) * maxY;
      const hue = hash01(w, 203);
      const ringR = (localT / period) * (boardR * 1.3);
      const thickness = 1.4;
      const fade = Math.max(0, 1 - localT / period);
      const burstColor = hueToRgb01(hue);
      for (let i = 0; i < n; i++) {
        const kx = keys[i].x + keys[i].w / 2, ky = keys[i].y + keys[i].h / 2;
        const dist = Math.hypot(kx - ox, ky - oy);
        const ringDist = Math.abs(dist - ringR);
        const b = Math.max(0, 1 - ringDist / thickness) * fade;
        frame[i] = scaleColorPreview(burstColor, b);
      }
      return frame;
    }
    case 'sweep': {
      const maxX = maxOf((k) => k.x + k.w), maxY = maxOf((k) => k.y + k.h);
      const diagMax = maxX + maxY;
      const period = 2.0;
      const phase = (t % period) / period;
      const pos = phase * (diagMax + 4) - 2;
      for (let i = 0; i < n; i++) {
        const coord = keys[i].x + keys[i].y;
        const b = coord < pos ? 1.0 : 0.05;
        frame[i] = scaleColorPreview(activeColor, b);
      }
      return frame;
    }
    case 'custom':
      for (let i = 0; i < n; i++) frame[i] = baseColors[i] || BLACK;
      return frame;
    case 'off':
      for (let i = 0; i < n; i++) frame[i] = BLACK;
      return frame;
    case 'aurora': {
      for (let i = 0; i < n; i++) {
        const x = keys[i].x + keys[i].w / 2, y = keys[i].y + keys[i].h / 2;
        const v = Math.sin(x * 0.45 + t * 0.7) + Math.sin(y * 0.9 - t * 0.5) + Math.sin((x + y) * 0.3 + t * 0.35);
        const k = (v + 3) / 6;
        frame[i] = scaleColorPreview(shiftHue(activeColor, (k - 0.5) * 0.35), 0.3 + 0.7 * k);
      }
      return frame;
    }
    case 'matrix': {
      const maxY = maxOf((k) => k.y + k.h);
      for (let i = 0; i < n; i++) {
        const colSeed = hash01(Math.round((keys[i].x + keys[i].w / 2) * 2), 59);
        const period = 1.6 + colSeed * 1.6;
        const localT = mod(t + colSeed * 17, period);
        const headY = (localT / period) * (maxY + 5) - 1;
        const d = headY - (keys[i].y + keys[i].h / 2);
        if (d < 0 || d > 4) { frame[i] = BLACK; continue; }
        const c = scaleColorPreview(activeColor, Math.pow(1 - d / 4, 1.6));
        frame[i] = d < 0.7 ? mixColor(c, WHITE, 0.45) : c;
      }
      return frame;
    }
    case 'gradient': {
      const maxX = maxOf((k) => k.x + k.w);
      const drift = Math.sin(t * 0.3) * 0.06;
      for (let i = 0; i < n; i++) {
        const pos = (keys[i].x + keys[i].w / 2) / Math.max(1, maxX);
        frame[i] = shiftHue(activeColor, pos * 0.33 + drift);
      }
      return frame;
    }
    case 'afterglow':
      for (let i = 0; i < n; i++) {
        const age = sys.keyAge[i] ?? 1e9;
        frame[i] = scaleColorPreview(activeColor, age < 1.4 ? Math.pow(1 - age / 1.4, 2) : 0);
      }
      return frame;
    case 'splash': {
      const life = 1.1, speedUnits = 10, width = 1.3;
      const live = [];
      for (let j = 0; j < n; j++) {
        const age = sys.keyAge[j] ?? 1e9;
        if (age < life) live.push([keys[j].x + keys[j].w / 2, keys[j].y + keys[j].h / 2, age]);
      }
      for (let i = 0; i < n; i++) {
        const kx = keys[i].x + keys[i].w / 2, ky = keys[i].y + keys[i].h / 2;
        let best = 0;
        for (const [ox, oy, age] of live) {
          const dist = Math.hypot(kx - ox, ky - oy);
          const ring = Math.max(0, 1 - Math.abs(dist - age * speedUnits) / width);
          best = Math.max(best, ring * (1 - age / life));
        }
        frame[i] = scaleColorPreview(activeColor, best);
      }
      return frame;
    }
    case 'cpu': {
      const maxX = maxOf((k) => k.x + k.w);
      for (let i = 0; i < n; i++) {
        const pos = (keys[i].x + keys[i].w / 2) / Math.max(1, maxX);
        frame[i] = pos > sys.cpu ? BLACK : hsv01(0.33 * (1 - pos), 1, 1);
      }
      return frame;
    }
    case 'memory': {
      const maxY = maxOf((k) => k.y + k.h);
      for (let i = 0; i < n; i++) {
        const fromBottom = 1 - (keys[i].y + keys[i].h / 2) / Math.max(1, maxY);
        frame[i] = fromBottom <= sys.memory ? activeColor : BLACK;
      }
      return frame;
    }
    case 'thermal': {
      const breathe = 0.8 + 0.2 * Math.sin(t * (1.5 + sys.temperature * 4));
      const c = hsv01(0.62 * (1 - sys.temperature), 1, breathe);
      for (let i = 0; i < n; i++) frame[i] = c;
      return frame;
    }
    case 'network': {
      const step = Math.floor(t * 8), within = t * 8 - step;
      const chance = 0.03 + sys.network * 0.55;
      for (let i = 0; i < n; i++) {
        const b = hash01(keys[i].ledIndex, step * 31 + 7) < chance ? 1 - within : 0;
        frame[i] = scaleColorPreview(activeColor, 0.06 + 0.94 * b);
      }
      return frame;
    }
    case 'clock': {
      for (let i = 0; i < n; i++) frame[i] = BLACK;
      const find = (label) => keys.findIndex((k) => k.label === label);
      const dim = scaleColorPreview(activeColor, 0.08);
      for (let f = 1; f <= 12; f++) { const k = find('F' + f); if (k >= 0) frame[k] = dim; }
      const hour12 = sys.hour % 12 === 0 ? 12 : sys.hour % 12;
      const hk = find('F' + hour12);
      if (hk >= 0) frame[hk] = activeColor;
      const tens = find(String(Math.floor(sys.minute / 10) % 10));
      const units = find(String(sys.minute % 10));
      if (tens >= 0) frame[tens] = activeColor;
      if (units >= 0) frame[units] = units === tens ? WHITE : shiftHue(activeColor, 0.5);
      const esc = find('Esc');
      if (esc >= 0) frame[esc] = scaleColorPreview(activeColor, sys.second % 2 === 0 ? 0.9 : 0.15);
      return frame;
    }
    case 'indicators': {
      for (let i = 0; i < n; i++) frame[i] = BLACK;
      const caps = keys.findIndex((k) => k.label === 'Caps');
      if (caps >= 0 && sys.capsLock) frame[caps] = activeColor;
      return frame;
    }
    default:
      for (let i = 0; i < n; i++) frame[i] = activeColor;
      return frame;
  }
}

// JS twin of LightingEngine::composite() - renders the layer stack
// bottom-to-top, each layer only on the keys in its mask.
function compositeLayers(layers, phases, keys, baseColors, sys) {
  const n = keys.length;
  const acc = new Float32Array(n * 3);

  layers.forEach((layer, li) => {
    if (!layer.enabled || layer.effect === 'off') return;
    const o = clamp(layer.opacity, 0, 1);
    const src = computePreviewFrame(layer.effect, phases[li] || 0, keys, baseColors, layer.color, sys);
    const mask = layer.mask;

    for (let i = 0; i < n; i++) {
      if (mask !== '*' && mask[i] !== '1') continue;
      const sc = src[i];
      const sv = [sc.r, sc.g, sc.b];
      for (let c = 0; c < 3; c++) {
        const d = acc[i * 3 + c], v = sv[c];
        let out;
        switch (layer.blend) {
          case 'add':      out = Math.min(255, d + v * o); break;
          case 'lighten':  out = d + (Math.max(d, v) - d) * o; break;
          case 'multiply': out = d * (1 - o) + d * (v / 255) * o; break;
          default:         out = d * (1 - o) + v * o;
        }
        acc[i * 3 + c] = out;
      }
    }
  });

  const frame = new Array(n);
  for (let i = 0; i < n; i++) frame[i] = { r: acc[i * 3], g: acc[i * 3 + 1], b: acc[i * 3 + 2] };
  return frame;
}


function debounce(fn, ms) {
  let t = null;
  return (...args) => {
    clearTimeout(t);
    t = setTimeout(() => fn(...args), ms);
  };
}


// ---------- custom dropdown ----------
// A from-scratch listbox standing in for every <select> in the app
// (effect picker, profile quick-switch, macro key/action/target pickers)
// so they all get one consistent, fully-themed look instead of the
// browser's own barely-stylable native <select> popup.
//
// container: the empty .cdrop element to mount into (from index.html, or
// a freshly created one for dynamic rows like macro steps).
// renderOption(el, item): fills one option row's contents - defaults to
// plain text if omitted.
// renderButtonContent(el, item): fills the closed button's contents
// (item is undefined if nothing is selected yet) - defaults to plain text.
//
// Returns { setOptions(items), setValue(value), getValue(), onChange(fn) }
// where each item is { value, label, ...anything renderOption/Button use }.
function createDropdown(container, { renderOption, renderButton } = {}) {
  container.classList.add('cdrop');
  container.innerHTML = '';

  const btn = document.createElement('button');
  btn.type = 'button';
  btn.className = 'cdrop-btn';

  const content = document.createElement('span');
  content.className = 'cdrop-btn-content';

  const caret = document.createElementNS(SVGNS, 'svg');
  caret.setAttribute('class', 'cdrop-caret');
  caret.setAttribute('viewBox', '0 0 24 24');
  caret.setAttribute('width', '14');
  caret.setAttribute('height', '14');
  caret.innerHTML = '<path d="M6 9l6 6 6-6" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>';

  btn.appendChild(content);
  btn.appendChild(caret);

  const list = document.createElement('div');
  list.className = 'cdrop-list';
  list.setAttribute('role', 'listbox');

  container.appendChild(btn);
  container.appendChild(list);

  let items = [];
  let value = null;
  let changeCb = null;

  function isOpen() { return list.classList.contains('open'); }

  function onDocPointer(e) {
    if (!container.contains(e.target)) close();
  }

  function close() {
    list.classList.remove('open');
    container.classList.remove('open');
    document.removeEventListener('pointerdown', onDocPointer, true);
  }

  function open() {
    list.classList.add('open');
    container.classList.add('open');
    // Flip upward when the list would run off the bottom of the window
    // (the layer pickers live near the bottom of the dock).
    list.classList.remove('up');
    const r = container.getBoundingClientRect();
    const needed = Math.min(list.scrollHeight, parseFloat(getComputedStyle(list).maxHeight) || 300) + 12;
    if (window.innerHeight - r.bottom < needed && r.top > window.innerHeight - r.bottom) list.classList.add('up');

    const sel = list.querySelector('.cdrop-option.selected');
    if (sel) list.scrollTop = sel.offsetTop - list.clientHeight / 2 + sel.offsetHeight / 2;
    setTimeout(() => document.addEventListener('pointerdown', onDocPointer, true), 0);
  }

  btn.addEventListener('click', () => { isOpen() ? close() : open(); });
  btn.addEventListener('keydown', (ev) => { if (ev.key === 'Escape') close(); });

  function renderButtonContent() {
    const current = items.find((i) => i.value === value);
    content.innerHTML = '';

    if (renderButton) renderButton(content, current);
    else content.textContent = current ? current.label : '';
  }

  function renderList() {
    list.innerHTML = '';
    let lastGroup = null;

    items.forEach((item) => {
      // Items carrying a `group` get a small heading whenever the group
      // changes (the effect picker's Still / Ambient / Motion / ... ).
      if (item.group && item.group !== lastGroup) {
        const head = document.createElement('div');
        head.className = 'cdrop-group';
        head.textContent = item.group;
        list.appendChild(head);
        lastGroup = item.group;
      }

      const opt = document.createElement('div');
      opt.className = 'cdrop-option' + (item.value === value ? ' selected' : '');
      opt.setAttribute('role', 'option');
      opt.tabIndex = 0;

      if (renderOption) renderOption(opt, item);
      else opt.textContent = item.label;

      const pick = () => {
        value = item.value;
        renderList();
        renderButtonContent();
        close();
        if (changeCb) changeCb(value);
      };

      opt.addEventListener('click', pick);
      opt.addEventListener('keydown', (ev) => {
        if (ev.key === 'Enter' || ev.key === ' ') { ev.preventDefault(); pick(); }
      });

      list.appendChild(opt);
    });
  }

  return {
    setOptions(newItems) {
      items = newItems;
      renderList();
      renderButtonContent();
    },
    setValue(v) {
      value = v;
      renderList();
      renderButtonContent();
    },
    getValue() { return value; },
    onChange(fn) { changeCb = fn; },
  };
}


// ---------- nav ----------

// Lighting / Remap / Settings are separate views; the top-bar tabs swap
// which one is visible. Only the active view is laid out, so the lighting
// "stage" always gets the whole window to itself.
function wireNav() {
  document.querySelectorAll('.rail-btn[data-section]').forEach((btn) => {
    btn.addEventListener('click', () => showView(btn.dataset.section));
  });

  const fromHash = location.hash.slice(1);
  if (fromHash && $(fromHash) && $(fromHash).classList.contains('view')) showView(fromHash);
}

function showView(id) {
  document.querySelectorAll('.rail-btn[data-section]').forEach((b) => {
    const on = b.dataset.section === id;
    b.classList.toggle('active', on);
    b.setAttribute('aria-selected', on ? 'true' : 'false');
  });

  history.replaceState(null, '', '#' + id);

  document.querySelectorAll('.view').forEach((v) => {
    v.classList.toggle('active', v.id === id);
  });

  window.scrollTo({ top: 0 });
}


// ---------- keyboard preview ----------

// The keyboard preview is the *real* photo (web/assets/aula-f75-black.png)
// with this SVG layered directly on top of it - not a separate abstract
// diagram - so the coloured overlay has to land in actual photo-pixel
// space, not the layout's abstract "key unit" space the backend sends.
//
// KB_PHOTO_W/H are that photo's exact pixel dimensions - the SVG viewBox
// is set to match 1:1 so image pixels and SVG user units are the same
// thing.
//
// KEY_PIXEL_RECTS[ledIndex] = [x0, y0, x1, y1] is each key's measured
// bounding box in that same photo-pixel space, one entry per key in
// buildF75Layout() order (so index == ledIndex). A single affine formula
// (unit coordinate * scale + origin) can't represent this board: the
// F-row's grouping gaps, the nav-cluster gap, and Backspace/Enter/Shift's
// wider keycaps all have their own, mutually inconsistent spacing versus
// the layout's uniform 1u grid. Rather than special-case each of those
// (which is what caused the ring-placement bugs - a partial fix applied to
// one edge of a key but not the other, distorting or misplacing several
// keys), every key's edges here were measured directly from the photo by
// scanning for the dark seams between keycaps. Re-measure these if the
// photo is ever swapped for a different crop or a different keyboard.
const KB_PHOTO_W = 1166;
const KB_PHOTO_H = 513;

const KEY_PIXEL_RECTS = [
  [38.0, 35.5, 105.0, 110.0], [174.0, 35.5, 240.5, 110.0], [240.5, 35.5, 309.0, 110.0],
  [309.0, 35.5, 379.0, 110.0], [379.0, 35.5, 443.5, 110.0], [471.5, 35.5, 539.0, 110.0],
  [539.0, 35.5, 606.5, 110.0], [606.5, 35.5, 674.5, 110.0], [674.5, 35.5, 742.0, 110.0],
  [770.0, 35.5, 834.5, 110.0], [834.5, 35.5, 903.0, 110.0], [903.0, 35.5, 974.0, 110.0],
  [974.0, 35.5, 1038.0, 110.0],

  [37.5, 110.0, 103.5, 191.0], [103.5, 110.0, 172.0, 191.0], [172.0, 110.0, 241.0, 191.0],
  [241.0, 110.0, 310.0, 191.0], [310.0, 110.0, 379.0, 191.0], [379.0, 110.0, 448.0, 191.0],
  [448.0, 110.0, 517.0, 191.0], [517.0, 110.0, 585.5, 191.0], [585.5, 110.0, 654.0, 191.0],
  [654.0, 110.0, 723.0, 191.0], [723.0, 110.0, 791.0, 191.0], [791.0, 110.0, 859.0, 191.0],
  [859.0, 110.0, 928.0, 191.0], [928.0, 110.0, 1064.0, 191.0], [1064.0, 110.0, 1129.5, 191.0],

  [39.0, 191.0, 138.5, 260.0], [138.5, 191.0, 207.0, 260.0], [207.0, 191.0, 276.0, 260.0],
  [276.0, 191.0, 345.0, 260.0], [345.0, 191.0, 414.0, 260.0], [414.0, 191.0, 483.0, 260.0],
  [483.0, 191.0, 551.0, 260.0], [551.0, 191.0, 620.0, 260.0], [620.0, 191.0, 689.0, 260.0],
  [689.0, 191.0, 757.0, 260.0], [757.0, 191.0, 825.0, 260.0], [825.0, 191.0, 893.0, 260.0],
  [893.0, 191.0, 962.0, 260.0], [962.0, 191.0, 1064.0, 260.0], [1064.0, 191.0, 1129.0, 260.0],

  [39.0, 260.0, 154.5, 329.0], [154.5, 260.0, 223.0, 329.0], [223.0, 260.0, 292.0, 329.0],
  [292.0, 260.0, 361.0, 329.0], [361.0, 260.0, 430.0, 329.0], [430.0, 260.0, 499.0, 329.0],
  [499.0, 260.0, 568.0, 329.0], [568.0, 260.0, 636.0, 329.0], [636.0, 260.0, 705.0, 329.0],
  [705.0, 260.0, 773.5, 329.0], [773.5, 260.0, 842.0, 329.0], [842.0, 260.0, 910.0, 329.0],
  [910.0, 260.0, 1063.5, 329.0], [1063.5, 260.0, 1129.0, 329.0],

  [39.0, 329.0, 188.0, 399.0], [188.0, 329.0, 257.5, 399.0], [257.5, 329.0, 326.0, 399.0],
  [326.0, 329.0, 395.0, 399.0], [395.0, 329.0, 464.0, 399.0], [464.0, 329.0, 533.0, 399.0],
  [533.0, 329.0, 602.0, 399.0], [602.0, 329.0, 670.0, 399.0], [670.0, 329.0, 739.0, 399.0],
  [739.0, 329.0, 807.0, 399.0], [807.0, 329.0, 875.5, 399.0], [875.5, 329.0, 995.0, 399.0],
  [995.0, 329.0, 1063.5, 399.0], [1063.5, 329.0, 1129.0, 399.0],

  [39.0, 399.0, 120.0, 468.5], [120.0, 399.0, 207.0, 468.5], [207.0, 399.0, 293.5, 468.5],
  [293.5, 399.0, 722.5, 468.5], [722.5, 399.0, 807.5, 468.5], [807.5, 399.0, 888.5, 468.5],
  [931.5, 399.0, 995.0, 468.5], [995.0, 399.0, 1063.5, 468.5], [1063.5, 399.0, 1128.5, 468.5],
];

// One entry per layout key: cached DOM refs so painting a frame is just a
// handful of attribute writes, not a full SVG rebuild.
let keyElements = [];
let rafHandle = null;

// Backlight is drawn the way a real board shows it, not as a coloured
// sticker on each keycap: light shines through the legends and spills out
// of the gaps under the caps, while the cap tops stay dark. Both regions
// come from the photo itself (buildPhotoMasks): the legends are its bright
// pixels, the gaps its near-black ones minus each cap's top face.
//
// Two SVGs sit over the photo:
//   #kbLight (screen-blended) - gap underglow, legend bloom
//   #kbSvg   (normal, on top) - the legends recoloured, hover/zone wash, hit targets

// Inner "top face" of a key in photo pixels - the part of the cap that
// stays dark when lit.
function capTop(x0, y0, w, h) {
  return [x0 + w * 0.16, y0 + h * 0.12, w * 0.68, h * 0.62];
}

function buildPhotoMasks(img, rects) {
  const w = KB_PHOTO_W, h = KB_PHOTO_H;
  const canvas = document.createElement('canvas');
  canvas.width = w; canvas.height = h;
  const ctx = canvas.getContext('2d', { willReadFrequently: true });
  ctx.drawImage(img, 0, 0, w, h);
  const src = ctx.getImageData(0, 0, w, h).data;

  const legend = ctx.createImageData(w, h);
  const gap = ctx.createImageData(w, h);

  for (let p = 0, i = 0; p < src.length; p += 4, i++) {
    const lum = 0.2126 * src[p] + 0.7152 * src[p + 1] + 0.0722 * src[p + 2];
    const l = clamp((lum - 70) * 255 / 60, 0, 255);
    const g = clamp((20 - lum) * 255 / 12, 0, 255);
    legend.data[p] = legend.data[p + 1] = legend.data[p + 2] = l; legend.data[p + 3] = 255;
    gap.data[p] = gap.data[p + 1] = gap.data[p + 2] = g; gap.data[p + 3] = 255;
  }

  const toUrl = (data, punchTops) => {
    ctx.putImageData(data, 0, 0);
    if (punchTops) {
      ctx.fillStyle = '#000';
      rects.forEach(([x0, y0, x1, y1]) => {
        const [x, y, cw, ch] = capTop(x0, y0, x1 - x0, y1 - y0);
        ctx.fillRect(x, y, cw, ch);
      });
    }
    return canvas.toDataURL('image/png');
  };

  return { legend: toUrl(legend, false), gap: toUrl(gap, true) };
}

function svgEl(tag, attrs, cls) {
  const el = document.createElementNS(SVGNS, tag);
  for (const k in attrs) el.setAttribute(k, attrs[k]);
  if (cls) el.classList.add(cls);
  return el;
}

function maskDef(id, href) {
  const mask = svgEl('mask', { id, maskUnits: 'userSpaceOnUse', x: 0, y: 0, width: KB_PHOTO_W, height: KB_PHOTO_H });
  const image = svgEl('image', { x: 0, y: 0, width: KB_PHOTO_W, height: KB_PHOTO_H });
  image.setAttribute('href', href);
  mask.appendChild(image);
  return mask;
}

async function buildKeyboardDom() {
  const svg = $('kbSvg');
  const light = $('kbLight');
  svg.innerHTML = '';
  light.innerHTML = '';
  keyElements = [];

  const layout = state.layout;
  if (!layout) return;

  const img = document.querySelector('.keyboard-photo img');
  if (!img.complete) await new Promise((r) => img.addEventListener('load', r, { once: true }));

  const rects = layout.keys.map((k) => KEY_PIXEL_RECTS[k.ledIndex] || [0, 0, 0, 0]);
  const masks = buildPhotoMasks(img, rects);

  for (const el of [svg, light]) el.setAttribute('viewBox', `0 0 ${KB_PHOTO_W} ${KB_PHOTO_H}`);

  const defs = svgEl('defs', {});
  defs.innerHTML =
    '<filter id="kbUnderglow" x="-5%" y="-10%" width="110%" height="120%"><feGaussianBlur stdDeviation="1.8"/></filter>' +
    '<filter id="kbBloom" x="-5%" y="-10%" width="110%" height="120%"><feGaussianBlur stdDeviation="2.6"/></filter>';
  defs.appendChild(maskDef('kbGapMask', masks.gap));
  defs.appendChild(maskDef('kbLegendMask', masks.legend));
  light.appendChild(defs);

  // #kbLight: underglow (blurred, then clipped to the gaps), bloom.
  const glowOuter = svgEl('g', { mask: 'url(#kbGapMask)' }, 'key-underglow-layer');
  const glowInner = svgEl('g', { filter: 'url(#kbUnderglow)' });
  glowOuter.appendChild(glowInner);
  const bloomOuter = svgEl('g', { filter: 'url(#kbBloom)' }, 'key-bloom-layer');
  const bloomInner = svgEl('g', { mask: 'url(#kbLegendMask)' });
  bloomOuter.appendChild(bloomInner);
  light.appendChild(glowOuter);
  light.appendChild(bloomOuter);

  // #kbSvg: recoloured legends, then per-key wash + hit target.
  const legendGroup = svgEl('g', { mask: 'url(#kbLegendMask)' });
  svg.appendChild(legendGroup);
  const keyGroup = svgEl('g', {});
  svg.appendChild(keyGroup);

  layout.keys.forEach((k, i) => {
    const [x0, y0, x1, y1] = rects[i];
    const w = x1 - x0, h = y1 - y0;
    const [tx, ty, tw, th] = capTop(x0, y0, w, h);

    // A thin ring on the key's outer edge - where it meets its
    // neighbours - rather than a filled block, so only a narrow line of
    // light escapes at the base of the cap instead of its whole side.
    const glow = svgEl('rect', { x: x0 + 2, y: y0 + 2, width: w - 4, height: h - 4, rx: 8, fill: 'none', 'stroke-width': 4 });
    glowInner.appendChild(glow);

    const bloom = svgEl('rect', { x: x0, y: y0, width: w, height: h });
    bloomInner.appendChild(bloom);

    const legend = svgEl('rect', { x: x0, y: y0, width: w, height: h });
    legendGroup.appendChild(legend);

    const group = svgEl('g', {}, 'key-group');
    group.appendChild(svgEl('rect', { x: tx, y: ty, width: tw, height: th, rx: 6 }, 'key-wash'));
    const hit = svgEl('rect', { x: x0, y: y0, width: w, height: h }, 'key-hit');
    hit.dataset.index = i;
    group.appendChild(hit);
    keyGroup.appendChild(group);

    keyElements.push({ key: k, group, glow, bloom, legend });
  });

  wireKeyboardPointer(svg);
  updateToolUI();
}


// ---------- live signals for reactive / system previews ----------

// Browser-side stand-ins for what the daemon reads from the real machine:
// key presses come from this page's own keydown events (so typing here
// previews Afterglow/Splash), metrics from /api/system.
const signals = { ...NO_SIGNALS, keyAge: [] };
let keyPressAt = [];

// KeyboardEvent.code for each visual key, in buildF75Layout() order.
const KEY_EVENT_CODES = [
  'Escape', 'F1', 'F2', 'F3', 'F4', 'F5', 'F6', 'F7', 'F8', 'F9', 'F10', 'F11', 'F12',
  'Backquote', 'Digit1', 'Digit2', 'Digit3', 'Digit4', 'Digit5', 'Digit6', 'Digit7', 'Digit8', 'Digit9', 'Digit0',
  'Minus', 'Equal', 'Backspace', 'Delete',
  'Tab', 'KeyQ', 'KeyW', 'KeyE', 'KeyR', 'KeyT', 'KeyY', 'KeyU', 'KeyI', 'KeyO', 'KeyP',
  'BracketLeft', 'BracketRight', 'Backslash', 'PageUp',
  'CapsLock', 'KeyA', 'KeyS', 'KeyD', 'KeyF', 'KeyG', 'KeyH', 'KeyJ', 'KeyK', 'KeyL',
  'Semicolon', 'Quote', 'Enter', 'PageDown',
  'ShiftLeft', 'KeyZ', 'KeyX', 'KeyC', 'KeyV', 'KeyB', 'KeyN', 'KeyM',
  'Comma', 'Period', 'Slash', 'ShiftRight', 'ArrowUp', 'End',
  'ControlLeft', 'MetaLeft', 'AltLeft', 'Space', 'Fn', 'ControlRight', 'ArrowLeft', 'ArrowDown', 'ArrowRight',
];

function wireKeySignals() {
  document.addEventListener('keydown', (ev) => {
    signals.capsLock = ev.getModifierState && ev.getModifierState('CapsLock');
    const i = KEY_EVENT_CODES.indexOf(ev.code);
    if (i >= 0) keyPressAt[i] = performance.now();
  });
  document.addEventListener('keyup', (ev) => {
    signals.capsLock = ev.getModifierState && ev.getModifierState('CapsLock');
  });
}

function layersUse(test) {
  return layers.some((l) => l.enabled && test(l.effect));
}

const SYSTEM_EFFECTS = new Set(['cpu', 'memory', 'thermal', 'network']);

async function pollSystemSignals() {
  if (!layersUse((e) => SYSTEM_EFFECTS.has(e))) return;
  try {
    Object.assign(signals, await apiGet('/api/system'));
  } catch (e) {
    // preview just keeps the last numbers
  }
}

function currentSignals(now) {
  const n = keyElements.length;
  if (signals.keyAge.length !== n) signals.keyAge = new Array(n);
  for (let i = 0; i < n; i++) {
    signals.keyAge[i] = keyPressAt[i] === undefined ? 1e9 : (now - keyPressAt[i]) / 1000;
  }
  const d = new Date();
  signals.hour = d.getHours(); signals.minute = d.getMinutes(); signals.second = d.getSeconds();
  return signals;
}


// ---------- rendering ----------

// Legend colour of a key that isn't lit - close to the photo's own.
const UNLIT_LEGEND = { r: 118, g: 118, b: 124 };

let phases = [];
let lastFrameAt = performance.now();
let frameCount = 0;

function resetAnimationClock() {
  phases = [];
}

// Runs every animation frame: advances each layer's clock by its own
// speed (same as daemon/main.cpp), composites the stack, and writes the
// colours straight onto the cached key elements.
function paintKeyboard() {
  if (!keyElements.length || !state.appState) return;

  const now = performance.now();
  const dt = Math.min(0.25, (now - lastFrameAt) / 1000);
  lastFrameAt = now;

  if (phases.length !== layers.length) phases.length = layers.length;
  for (let i = 0; i < layers.length; i++) phases[i] = (phases[i] || 0) + dt * Math.max(0.05, layers[i].speed);

  const s = state.appState;
  const frame = compositeLayers(layers, phases, state.layout.keys, s.customColors || [], currentSignals(now));
  const brightness = s.brightness;
  const zone = tool === 'zone' ? layers[selectedLayer] : null;

  let sr = 0, sg = 0, sb = 0;

  keyElements.forEach(({ group, glow, bloom, legend }, i) => {
    let c = scaleColor(frame[i], brightness);
    sr += c.r; sg += c.g; sb += c.b;

    if (zone) {
      const inZone = zone.mask === '*' || zone.mask[i] === '1';
      group.classList.toggle('in-zone', inZone);
      if (!inZone) c = scaleColor(c, 0.22);
    }

    const hex = rgbToHex(c);
    glow.setAttribute('stroke', hex);
    bloom.setAttribute('fill', hex);

    // A lit legend glows in the key's colour (pushed a little towards
    // white at full power, like a real LED behind translucent plastic);
    // an unlit one keeps the photo's own pale grey.
    const level = Math.max(c.r, c.g, c.b) / 255;
    const lit = level > 0 ? mixColor(scaleColor(c, 1 / level), WHITE, 0.28 * level) : UNLIT_LEGEND;
    legend.setAttribute('fill', rgbToHex(mixColor(UNLIT_LEGEND, lit, Math.min(1, level * 2))));
  });

  // The stage's ambient glow follows what the board is actually showing.
  if (++frameCount % 8 === 0) {
    const n = keyElements.length;
    const avg = { r: sr / n, g: sg / n, b: sb / n };
    const peak = Math.max(avg.r, avg.g, avg.b, 1);
    document.documentElement.style.setProperty('--live', rgbToHex(scaleColor(avg, Math.min(3, 200 / peak))));
    document.documentElement.style.setProperty('--live-strength', String(clamp(peak / 120, 0.15, 1)));
  }
}

function startAnimationLoop() {
  if (rafHandle) return;

  const tick = () => {
    paintKeyboard();
    rafHandle = requestAnimationFrame(tick);
  };

  rafHandle = requestAnimationFrame(tick);
}

function renderKeyboard() {
  paintKeyboard();
}


// ---------- keyboard tools: paint / layer keys ----------

let tool = 'paint';
let brush = { r: 255, g: 255, b: 255 };
let erasing = false;
let stroke = null;   // { kind: 'paint' | 'zone', value, touched:Set }

// Quick key sets for the "Layer keys" tool, by layout label.
const ZONES = [
  { id: 'all', label: 'All' },
  { id: 'none', label: 'None' },
  { id: 'invert', label: 'Invert' },
  { id: 'letters', label: 'Letters', labels: 'QWERTYUIOPASDFGHJKLZXCVBNM'.split('') },
  { id: 'numbers', label: 'Numbers', labels: '`1234567890-='.split('') },
  { id: 'frow', label: 'F-row', labels: ['Esc', 'F1', 'F2', 'F3', 'F4', 'F5', 'F6', 'F7', 'F8', 'F9', 'F10', 'F11', 'F12'] },
  { id: 'wasd', label: 'WASD', labels: ['W', 'A', 'S', 'D'] },
  { id: 'arrows', label: 'Arrows', labels: ['Up', 'Down', 'Left', 'Right'] },
  { id: 'mods', label: 'Modifiers', labels: ['Tab', 'Caps', 'Shift', 'Ctrl', 'Win', 'Alt', 'Fn', 'Space', 'Enter', 'Backspace'] },
  { id: 'nav', label: 'Nav', labels: ['Delete', 'PgUp', 'PgDn', 'End'] },
];

function maskHas(mask, i) { return mask === '*' || mask[i] === '1'; }

function maskSet(mask, i, on) {
  const n = keyElements.length;
  const arr = mask === '*' ? new Array(n).fill('1') : mask.padEnd(n, '0').split('');
  arr[i] = on ? '1' : '0';
  const out = arr.join('');
  return out.indexOf('0') === -1 ? '*' : out;
}

function maskCount(mask) {
  if (mask === '*') return keyElements.length || 80;
  let c = 0;
  for (const ch of mask) if (ch === '1') c++;
  return c;
}

function applyZone(id) {
  const layer = layers[selectedLayer];
  if (!layer) return;
  const n = keyElements.length;
  const keys = state.layout.keys;

  if (id === 'all') layer.mask = '*';
  else if (id === 'none') layer.mask = '0'.repeat(n);
  else if (id === 'invert') {
    let out = '';
    for (let i = 0; i < n; i++) out += maskHas(layer.mask, i) ? '0' : '1';
    layer.mask = out.indexOf('0') === -1 ? '*' : out;
  } else {
    const zone = ZONES.find((z) => z.id === id);
    let out = '';
    for (let i = 0; i < n; i++) out += zone.labels.includes(keys[i].label) ? '1' : '0';
    layer.mask = out;
  }

  markBusy();
  renderLayers();
  updateToolUI();
  pushLayers();
}

// Topmost Canvas layer (where brush strokes land), creating one on top of
// the stack - covering no keys yet - if there isn't one. Painting a key
// adds it to that layer's mask, so paint always shows over the effects
// below; erasing removes it again and reveals them.
function canvasLayerIndex() {
  for (let i = layers.length - 1; i >= 0; i--) {
    if (layers[i].effect === 'custom') return i;
  }
  layers.push(newLayer('custom', { mask: '0'.repeat(keyElements.length) }));
  toast('Added a Canvas layer for your paint');
  return layers.length - 1;
}

function paintKey(i) {
  const li = canvasLayerIndex();
  const layer = layers[li];
  layer.enabled = true;

  if (erasing) {
    layer.mask = maskSet(layer.mask, i, false);
  } else {
    state.appState.customColors[i] = { ...brush };
    layer.mask = maskSet(layer.mask, i, true);
  }
}

function keyIndexAt(ev) {
  const el = document.elementFromPoint(ev.clientX, ev.clientY);
  return el && el.classList.contains('key-hit') ? Number(el.dataset.index) : -1;
}

function applyStroke(i) {
  if (i < 0 || !stroke || stroke.touched.has(i)) return;
  stroke.touched.add(i);

  if (stroke.kind === 'paint') {
    paintKey(i);
  } else {
    const layer = layers[selectedLayer];
    if (layer) layer.mask = maskSet(layer.mask, i, stroke.value);
  }
  markBusy();
}

function wireKeyboardPointer(svg) {
  svg.addEventListener('pointerdown', (ev) => {
    const i = keyIndexAt(ev);
    if (i < 0) return;
    ev.preventDefault();

    const key = state.layout.keys[i];

    if (state.calibration && state.calibration.active) {
      handleCalibrationKeyClick(key);
      return;
    }

    // Alt-click: eyedropper - take that key's painted colour as the brush.
    if (ev.altKey) {
      const c = state.appState.customColors[i];
      if (c) setBrush(c);
      return;
    }

    if (tool === 'paint') {
      stroke = { kind: 'paint', touched: new Set() };
    } else {
      const layer = layers[selectedLayer];
      if (!layer) return;
      stroke = { kind: 'zone', value: !maskHas(layer.mask, i), touched: new Set() };
    }

    svg.setPointerCapture(ev.pointerId);
    applyStroke(i);
  });

  svg.addEventListener('pointermove', (ev) => {
    const i = keyIndexAt(ev);
    keyElements.forEach((k, idx) => k.group.classList.toggle('hover', idx === i));
    if (stroke) applyStroke(i);
  });

  svg.addEventListener('pointerleave', () => {
    keyElements.forEach((k) => k.group.classList.remove('hover'));
  });

  const end = () => {
    if (!stroke) return;
    const wasPaint = stroke.kind === 'paint';
    stroke = null;
    renderLayers();
    updateToolUI();
    pushLayers(wasPaint);
  };

  svg.addEventListener('pointerup', end);
  svg.addEventListener('pointercancel', end);
}

function setBrush(c) {
  brush = { r: Math.round(c.r), g: Math.round(c.g), b: Math.round(c.b) };
  erasing = false;
  updateToolUI();
}

function setTool(t) {
  tool = t;
  updateToolUI();
}

function updateToolUI() {
  document.querySelectorAll('#toolSeg [data-tool]').forEach((b) => {
    b.classList.toggle('active', b.dataset.tool === tool);
  });

  $('paintPanel').hidden = tool !== 'paint';
  $('zonePanel').hidden = tool !== 'zone';
  $('kbSvg').classList.toggle('tool-zone', tool === 'zone');

  const hex = rgbToHex(brush);
  $('brushSwatch').style.background = hex;
  $('brushHex').textContent = hex.toUpperCase();
  $('eraseBtn').classList.toggle('active', erasing);
  $('brushSwatch').classList.toggle('erasing', erasing);

  const layer = layers[selectedLayer];
  $('zoneInfo').innerHTML = layer
    ? `<strong>${effectById(layer.effect).name}</strong> · ${maskCount(layer.mask)} keys`
    : 'No layer selected';

  $('keyboardHint').textContent = tool === 'paint'
    ? 'Click or drag across keys to paint them · Alt-click picks a key’s colour'
    : 'Click or drag to choose which keys the selected layer covers';
}

function wireTools() {
  document.querySelectorAll('#toolSeg [data-tool]').forEach((b) => {
    b.addEventListener('click', () => setTool(b.dataset.tool));
  });

  // The brush swatch opens the same picker the inspector uses, in a popover.
  const pop = $('brushPop');
  const brushPicker = createColorPicker($('brushPicker'), (rgb) => setBrush(rgb));
  const closePop = (ev) => {
    if (ev && (pop.contains(ev.target) || $('brushSwatchBtn').contains(ev.target))) return;
    pop.hidden = true;
    document.removeEventListener('pointerdown', closePop, true);
  };
  $('brushSwatchBtn').addEventListener('click', () => {
    if (!pop.hidden) { closePop(); return; }
    brushPicker.set(brush);
    pop.hidden = false;
    document.addEventListener('pointerdown', closePop, true);
  });
  pop.addEventListener('keydown', (ev) => { if (ev.key === 'Escape') { closePop(); $('brushSwatchBtn').focus(); } });

  const presets = $('brushPresets');
  PRESETS.forEach((hex) => {
    const dot = document.createElement('button');
    dot.type = 'button';
    dot.className = 'preset-swatch';
    dot.style.background = hex;
    dot.title = hex;
    dot.addEventListener('click', () => setBrush(hexToRgb(hex)));
    presets.appendChild(dot);
  });

  $('eraseBtn').addEventListener('click', () => { erasing = !erasing; updateToolUI(); });

  const chips = $('zoneChips');
  ZONES.forEach((z) => {
    const b = document.createElement('button');
    b.type = 'button';
    b.className = 'zone-chip';
    b.textContent = z.label;
    b.addEventListener('click', () => applyZone(z.id));
    chips.appendChild(b);
  });
}


// ---------- layers ----------

// Working copy of the stack (same shape as /api/state's `layers`), edited
// locally and pushed back debounced - index 0 is the bottom layer.
let layers = [];
let selectedLayer = 0;

function newLayer(effect, extra) {
  return {
    effect,
    color: { r: 124, g: 92, b: 255 },
    speed: 1,
    opacity: 1,
    blend: 'normal',
    enabled: true,
    mask: '*',
    ...extra,
  };
}

function syncLayersFromState() {
  const src = (state.appState && state.appState.layers) || [];
  layers = src.map((l) => ({ ...l, color: { ...l.color } }));
  selectedLayer = clamp(selectedLayer, 0, Math.max(0, layers.length - 1));
}

const pushLayers = (() => {
  let timer = null;
  let withColors = false;

  return (includeColors) => {
    withColors = withColors || !!includeColors;
    markBusy();
    clearTimeout(timer);
    timer = setTimeout(async () => {
      const body = { layers };
      if (withColors) body.customColors = state.appState.customColors;
      withColors = false;
      try {
        const res = await apiPost('/api/layers', body);
        // Keep the local stack: it may already be newer than this reply.
        state.appState = { ...res, layers };
      } catch (e) {
        toast('Could not reach openaula-webd');
      }
    }, 140);
  };
})();

function effectItems() {
  return EFFECTS.map((fx) => ({ value: fx.id, label: fx.name, group: fx.group, fx }));
}

function renderEffectOption(el, item) {
  const name = document.createElement('span');
  name.className = 'fx-name';
  name.textContent = item.fx.name;
  el.appendChild(name);

  const desc = document.createElement('span');
  desc.className = 'fx-desc';
  desc.textContent = item.fx.desc;
  el.appendChild(desc);
}

const ICON_EYE = '<svg viewBox="0 0 24 24" width="16" height="16"><path d="M2 12s3.6-7 10-7 10 7 10 7-3.6 7-10 7S2 12 2 12Z" fill="none" stroke="currentColor" stroke-width="1.7"/><circle cx="12" cy="12" r="3" fill="none" stroke="currentColor" stroke-width="1.7"/></svg>';
const ICON_EYE_OFF = '<svg viewBox="0 0 24 24" width="16" height="16"><path d="M3 3l18 18M10.6 5.1A10.7 10.7 0 0 1 12 5c6.4 0 10 7 10 7a17 17 0 0 1-3.2 4.1M6.6 6.6A16.6 16.6 0 0 0 2 12s3.6 7 10 7a9.8 9.8 0 0 0 5.4-1.6" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round"/></svg>';
const ICON_UP = '<svg viewBox="0 0 24 24" width="14" height="14"><path d="M6 15l6-6 6 6" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/></svg>';
const ICON_DOWN = '<svg viewBox="0 0 24 24" width="14" height="14"><path d="M6 9l6 6 6-6" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/></svg>';
const ICON_PEN = '<svg viewBox="0 0 24 24" width="13" height="13"><path d="M14.5 4.5l5 5L9 20H4v-5L14.5 4.5Z" fill="none" stroke="currentColor" stroke-width="2"/></svg>';
const ICON_X = '<svg viewBox="0 0 24 24" width="14" height="14"><path d="M6 6l12 12M18 6L6 18" stroke="currentColor" stroke-width="2" stroke-linecap="round"/></svg>';

function iconButton(svg, title, onClick) {
  const b = document.createElement('button');
  b.type = 'button';
  b.className = 'layer-icon';
  b.title = title;
  b.innerHTML = svg;
  b.addEventListener('click', (ev) => { ev.stopPropagation(); onClick(); });
  return b;
}

// Only restyles the existing rows (no rebuild), so a click that both
// selects a row and opens its effect picker keeps the picker open.
function selectLayer(i) {
  selectedLayer = i;
  document.querySelectorAll('#layerList .layer-row').forEach((row) => {
    row.classList.toggle('selected', Number(row.dataset.index) === i);
  });
  renderBlend();
  $('effectDescText').textContent = layers[i] ? effectById(layers[i].effect).desc : '';
  loadSelectedLayerIntoControls();
  updateToolUI();
}

function moveLayer(i, dir) {
  const j = i + dir;
  if (j < 0 || j >= layers.length) return;
  [layers[i], layers[j]] = [layers[j], layers[i]];
  [phases[i], phases[j]] = [phases[j], phases[i]];
  selectedLayer = j;
  renderLayers();
  pushLayers();
}

function removeLayer(i) {
  if (layers.length <= 1) { toast('Keep at least one layer'); return; }
  layers.splice(i, 1);
  phases.splice(i, 1);
  selectedLayer = clamp(selectedLayer >= i ? selectedLayer - 1 : selectedLayer, 0, layers.length - 1);
  renderLayers();
  loadSelectedLayerIntoControls();
  pushLayers();
}

function addLayer() {
  if (layers.length >= 16) { toast('That’s plenty of layers'); return; }
  const base = layers[selectedLayer];
  const layer = newLayer('starlight', {
    color: base ? { ...base.color } : undefined,
    blend: layers.length ? 'lighten' : 'normal',
  });
  layers.splice(selectedLayer + 1, 0, layer);
  phases.splice(selectedLayer + 1, 0, 0);
  selectedLayer += layers.length > 1 ? 1 : 0;
  renderLayers();
  loadSelectedLayerIntoControls();
  pushLayers();
}

// Rows are listed top-of-stack first, like any layers panel.
function renderLayers() {
  const list = $('layerList');
  list.innerHTML = '';

  for (let i = layers.length - 1; i >= 0; i--) {
    const layer = layers[i];
    const row = document.createElement('div');
    row.className = 'layer-row' + (i === selectedLayer ? ' selected' : '') + (layer.enabled ? '' : ' muted');
    row.dataset.index = i;
    row.addEventListener('click', () => { if (i !== selectedLayer) selectLayer(i); });

    const num = document.createElement('span');
    num.className = 'layer-num';
    num.textContent = String(i + 1).padStart(2, '0');
    row.appendChild(num);

    row.appendChild(iconButton(layer.enabled ? ICON_EYE : ICON_EYE_OFF, layer.enabled ? 'Hide layer' : 'Show layer', () => {
      layer.enabled = !layer.enabled;
      renderLayers();
      pushLayers();
    }));

    const fx = effectById(layer.effect);
    const dot = document.createElement('span');
    dot.className = 'layer-dot' + (fx.color === false ? ' palette' : '');
    if (fx.color !== false) dot.style.background = rgbToHex(layer.color);
    row.appendChild(dot);

    const dropHost = document.createElement('div');
    dropHost.className = 'layer-effect';
    row.appendChild(dropHost);

    const drop = createDropdown(dropHost, {
      renderOption: renderEffectOption,
      renderButton: (el, item) => { if (item) el.textContent = item.fx.name; },
    });
    drop.setOptions(effectItems());
    drop.setValue(layer.effect);
    drop.onChange((effect) => {
      layer.effect = effect;
      selectedLayer = i;
      renderLayers();
      loadSelectedLayerIntoControls();
      updateToolUI();
      pushLayers();
      pollSystemSignals();
    });

    const scope = document.createElement('button');
    scope.type = 'button';
    scope.className = 'layer-scope';
    scope.textContent = layer.mask === '*' ? 'All keys' : maskCount(layer.mask) + ' keys';
    scope.title = 'Choose which keys this layer covers';
    scope.addEventListener('click', (ev) => {
      ev.stopPropagation();
      selectedLayer = i;
      renderLayers();
      loadSelectedLayerIntoControls();
      setTool('zone');
    });
    row.appendChild(scope);

    const actions = document.createElement('div');
    actions.className = 'layer-actions';
    actions.appendChild(iconButton(ICON_UP, 'Move up', () => moveLayer(i, 1)));
    actions.appendChild(iconButton(ICON_DOWN, 'Move down', () => moveLayer(i, -1)));
    actions.appendChild(iconButton(ICON_X, 'Remove layer', () => removeLayer(i)));
    row.appendChild(actions);

    list.appendChild(row);
  }

  renderBlend();

  const sel = layers[selectedLayer];
  $('effectDescText').textContent = sel ? effectById(sel.effect).desc : '';
}

function renderBlend() {
  const seg = $('blendSeg');
  const layer = layers[selectedLayer];
  seg.innerHTML = '';

  BLENDS.forEach((b) => {
    const btn = document.createElement('button');
    btn.type = 'button';
    btn.textContent = b.label;
    btn.classList.toggle('active', !!layer && layer.blend === b.value);
    btn.addEventListener('click', () => {
      if (!layer) return;
      layer.blend = b.value;
      renderBlend();
      pushLayers();
    });
    seg.appendChild(btn);
  });
}

// Colour, speed and opacity always edit the selected layer.
function loadSelectedLayerIntoControls() {
  const layer = layers[selectedLayer];
  if (!layer) return;

  layerPicker.set(layer.color);
  showLayerColor(layer.color);

  const usesColor = effectById(layer.effect).color !== false;
  $('colorCell').classList.toggle('disabled', !usesColor);
  $('colorNote').textContent = usesColor ? '' : effectById(layer.effect).name + ' uses its own palette';

  $('speedSlider').value = Math.round(layer.speed * 10);
  $('speedValue').textContent = layer.speed.toFixed(1) + 'x';
  $('opacitySlider').value = Math.round(layer.opacity * 100);
  $('opacityValue').textContent = Math.round(layer.opacity * 100) + '%';
}

function wireLayers() {
  $('addLayerBtn').addEventListener('click', addLayer);
}


// ---------- sliders ----------

function wireSliders() {
  const brightness = $('brightnessSlider');
  const speed = $('speedSlider');

  const postBrightness = debounce(async (v) => {
    const res = await apiPost('/api/brightness', { brightness: v / 100 });
    state.appState = res;
    renderKeyboard();
  }, 120);

  brightness.addEventListener('input', () => {
    markBusy();
    $('brightnessValue').textContent = brightness.value + '%';
    if (state.appState) state.appState.brightness = Number(brightness.value) / 100;
    renderKeyboard();
    postBrightness(Number(brightness.value));
  });

  speed.addEventListener('input', () => {
    const layer = layers[selectedLayer];
    if (!layer) return;
    layer.speed = speed.value / 10;
    $('speedValue').textContent = layer.speed.toFixed(1) + 'x';
    pushLayers();
  });

  const opacity = $('opacitySlider');
  opacity.addEventListener('input', () => {
    const layer = layers[selectedLayer];
    if (!layer) return;
    layer.opacity = opacity.value / 100;
    $('opacityValue').textContent = opacity.value + '%';
    pushLayers();
  });
}


// ---------- colour picker ----------

// One picker component used everywhere a colour is chosen (the selected
// layer's colour in the inspector, and the paint brush): a square of
// saturation (x) by brightness (y), a vertical hue strip, a hex field and
// preset swatches - no native <input type=color> dialog.
function createColorPicker(host, onInput) {
  host.classList.add('picker');
  host.innerHTML =
    '<div class="picker-sv" tabindex="0" role="slider" aria-label="Saturation and brightness">' +
      '<canvas width="240" height="160"></canvas><span class="picker-thumb"></span></div>' +
    '<div class="picker-hue" tabindex="0" role="slider" aria-label="Hue"><span class="picker-hue-thumb"></span></div>' +
    '<div class="picker-foot">' +
      '<span class="picker-swatch"></span>' +
      '<input class="picker-hex" maxlength="7" spellcheck="false" aria-label="Hex colour">' +
      '<div class="picker-presets"></div>' +
    '</div>';

  const sv = host.querySelector('.picker-sv');
  const canvas = sv.querySelector('canvas');
  const thumb = sv.querySelector('.picker-thumb');
  const hueStrip = host.querySelector('.picker-hue');
  const hueThumb = host.querySelector('.picker-hue-thumb');
  const swatch = host.querySelector('.picker-swatch');
  const hexInput = host.querySelector('.picker-hex');
  const presets = host.querySelector('.picker-presets');

  let h = 0, sat = 1, val = 1;

  const drawSquare = () => {
    const ctx = canvas.getContext('2d');
    const w = canvas.width, ht = canvas.height;
    ctx.fillStyle = rgbToHex(hsvToRgb(h, 1, 1));
    ctx.fillRect(0, 0, w, ht);
    const white = ctx.createLinearGradient(0, 0, w, 0);
    white.addColorStop(0, '#fff'); white.addColorStop(1, 'rgba(255,255,255,0)');
    ctx.fillStyle = white; ctx.fillRect(0, 0, w, ht);
    const black = ctx.createLinearGradient(0, 0, 0, ht);
    black.addColorStop(0, 'rgba(0,0,0,0)'); black.addColorStop(1, '#000');
    ctx.fillStyle = black; ctx.fillRect(0, 0, w, ht);
  };

  const render = () => {
    const hex = rgbToHex(hsvToRgb(h, sat, val));
    thumb.style.left = (sat * 100) + '%';
    thumb.style.top = ((1 - val) * 100) + '%';
    thumb.style.background = hex;
    hueThumb.style.top = (h / 360 * 100) + '%';
    swatch.style.background = hex;
    if (document.activeElement !== hexInput) hexInput.value = hex.toUpperCase();
  };

  const emit = () => {
    markBusy();
    render();
    onInput(hsvToRgb(h, sat, val));
  };

  const drag = (el, apply) => {
    el.addEventListener('pointerdown', (ev) => {
      el.setPointerCapture(ev.pointerId);
      apply(ev);
      const move = (e) => apply(e);
      const up = () => { el.removeEventListener('pointermove', move); el.removeEventListener('pointerup', up); };
      el.addEventListener('pointermove', move);
      el.addEventListener('pointerup', up);
    });
  };

  drag(sv, (ev) => {
    const r = sv.getBoundingClientRect();
    sat = clamp((ev.clientX - r.left) / r.width, 0, 1);
    val = clamp(1 - (ev.clientY - r.top) / r.height, 0, 1);
    emit();
  });

  drag(hueStrip, (ev) => {
    const r = hueStrip.getBoundingClientRect();
    h = clamp((ev.clientY - r.top) / r.height, 0, 1) * 359.9;
    drawSquare();
    emit();
  });

  sv.addEventListener('keydown', (ev) => {
    const step = ev.shiftKey ? 0.1 : 0.02;
    if (ev.key === 'ArrowLeft') sat = clamp(sat - step, 0, 1);
    else if (ev.key === 'ArrowRight') sat = clamp(sat + step, 0, 1);
    else if (ev.key === 'ArrowUp') val = clamp(val + step, 0, 1);
    else if (ev.key === 'ArrowDown') val = clamp(val - step, 0, 1);
    else return;
    ev.preventDefault();
    emit();
  });

  hueStrip.addEventListener('keydown', (ev) => {
    const step = ev.shiftKey ? 20 : 4;
    if (ev.key === 'ArrowUp' || ev.key === 'ArrowLeft') h = (h - step + 360) % 360;
    else if (ev.key === 'ArrowDown' || ev.key === 'ArrowRight') h = (h + step) % 360;
    else return;
    ev.preventDefault();
    drawSquare();
    emit();
  });

  const set = (rgb) => {
    const hsv = rgbToHsv(rgb.r, rgb.g, rgb.b);
    // Keep the current hue for greys/black, where HSV has none.
    if (hsv.s > 0.001 && hsv.v > 0.001) h = hsv.h;
    sat = hsv.s; val = hsv.v;
    drawSquare();
    render();
  };

  hexInput.addEventListener('input', () => {
    const v = hexInput.value.trim();
    if (/^#?[0-9a-f]{6}$/i.test(v)) {
      set(hexToRgb(v.startsWith('#') ? v : '#' + v));
      onInput(hsvToRgb(h, sat, val));
      markBusy();
    }
  });
  hexInput.addEventListener('blur', render);
  hexInput.addEventListener('keydown', (ev) => { if (ev.key === 'Enter') hexInput.blur(); });

  PRESETS.forEach((hex) => {
    const b = document.createElement('button');
    b.type = 'button';
    b.className = 'preset-swatch';
    b.style.background = hex;
    b.title = hex.toUpperCase();
    b.addEventListener('click', () => { set(hexToRgb(hex)); emit(); });
    presets.appendChild(b);
  });

  drawSquare();
  render();
  return { set };
}

let layerPicker = null;

// Mirrors the selected layer's colour into the header lamp and the
// inspector's hex readout.
function showLayerColor(rgb) {
  const hex = rgbToHex(rgb);
  $('swatchHex').textContent = hex.toUpperCase();
  $('railAccentDot').style.background = hex;
}

function postColor(rgb) {
  const layer = layers[selectedLayer];
  if (!layer) return;
  layer.color = { r: Math.round(rgb.r), g: Math.round(rgb.g), b: Math.round(rgb.b) };
  showLayerColor(layer.color);
  const dot = document.querySelector('.layer-row.selected .layer-dot:not(.palette)');
  if (dot) dot.style.background = rgbToHex(layer.color);
  pushLayers();
}

function wireLayerPicker() {
  layerPicker = createColorPicker($('layerPicker'), postColor);
}


// ---------- modal ----------

// In-page replacement for prompt()/confirm(), styled like the rest of the
// app. Resolves to the entered text (input mode), true/false (confirm
// mode), or null when cancelled.
let modalResolve = null;

function closeModal(result) {
  $('modal').hidden = true;
  const resolve = modalResolve;
  modalResolve = null;
  if (resolve) resolve(result);
}

function openModal({ title, text = '', input = null, okLabel = 'OK', danger = false }) {
  if (modalResolve) closeModal(null);

  $('modalTitle').textContent = title;
  $('modalText').textContent = text;
  $('modalText').hidden = !text;

  const field = $('modalInput');
  field.hidden = input === null;
  field.value = input || '';

  const ok = $('modalOk');
  ok.textContent = okLabel;
  ok.classList.toggle('danger', danger);

  $('modal').hidden = false;
  setTimeout(() => (input === null ? ok : field).focus(), 0);
  if (input !== null) field.select();

  return new Promise((resolve) => { modalResolve = resolve; });
}

function askText(title, initial, okLabel) {
  return openModal({ title, input: initial, okLabel }).then((v) => (v ? v.trim() : null));
}

function askConfirm(title, text, okLabel) {
  return openModal({ title, text, okLabel, danger: true }).then((v) => v === true);
}

function wireModal() {
  const submit = () => closeModal($('modalInput').hidden ? true : $('modalInput').value);
  $('modalOk').addEventListener('click', submit);
  $('modalCancel').addEventListener('click', () => closeModal(null));
  $('modal').addEventListener('pointerdown', (ev) => { if (ev.target === $('modal')) closeModal(null); });
  $('modal').addEventListener('keydown', (ev) => {
    if (ev.key === 'Escape') closeModal(null);
    else if (ev.key === 'Enter') { ev.preventDefault(); submit(); }
  });
}


// ---------- profiles ----------

function wireProfiles() {
  $('addProfileBtn').addEventListener('click', async () => {
    const name = await askText('Save as profile', 'New Profile', 'Save');
    if (!name) return;

    const res = await apiPost('/api/profiles/save', { name });
    state.profiles = res;
    renderProfiles();
    toast('Profile "' + name + '" saved');
  });
}

async function applyProfile(index) {
  const p = state.profiles.profiles[index];
  if (!p) return;

  const res = await apiPost('/api/profiles/apply', { index });
  state.appState = res;
  state.profiles.activeIndex = index;
  resetAnimationClock();
  applyStateToControls();
  renderProfiles();
  renderKeyboard();
  toast('Applied "' + p.name + '"');
}

function renderProfiles() {
  const list = $('profileList');
  list.innerHTML = '';

  const profiles = state.profiles ? state.profiles.profiles : [];
  const activeIndex = state.profiles ? state.profiles.activeIndex : -1;


  if (profiles.length === 0) {
    const empty = document.createElement('div');
    empty.className = 'profile-empty';
    empty.textContent = 'None yet — set up your lighting, then hit Save.';
    list.appendChild(empty);
    return;
  }

  profiles.forEach((p, i) => {
    const card = document.createElement('div');
    card.className = 'profile-card' + (i === activeIndex ? ' active' : '');
    card.tabIndex = 0;

    const name = document.createElement('div');
    name.className = 'profile-card-name';
    name.textContent = p.name;
    card.appendChild(name);

    const actions = document.createElement('div');
    actions.className = 'profile-card-actions';

    const renameBtn = document.createElement('button');
    renameBtn.type = 'button';
    renameBtn.innerHTML = ICON_PEN;
    renameBtn.title = 'Rename';
    renameBtn.addEventListener('click', async (ev) => {
      ev.stopPropagation();
      const newName = await askText('Rename profile', p.name, 'Rename');
      if (!newName || newName === p.name) return;
      const res = await apiPost('/api/profiles/rename', { index: i, name: newName });
      state.profiles = res;
      renderProfiles();
    });

    const deleteBtn = document.createElement('button');
    deleteBtn.type = 'button';
    deleteBtn.innerHTML = ICON_X;
    deleteBtn.title = 'Delete';
    deleteBtn.addEventListener('click', async (ev) => {
      ev.stopPropagation();
      if (!await askConfirm('Delete profile?', '"' + p.name + '" will be removed. This can’t be undone.', 'Delete')) return;
      const res = await apiPost('/api/profiles/delete', { index: i });
      state.profiles = res;
      renderProfiles();
    });

    actions.appendChild(renameBtn);
    actions.appendChild(deleteBtn);
    card.appendChild(actions);

    card.addEventListener('click', () => applyProfile(i));
    card.addEventListener('keydown', (ev) => {
      if (ev.key === 'Enter' || ev.key === ' ') { ev.preventDefault(); applyProfile(i); }
    });

    list.appendChild(card);
  });
}


// ---------- macros & remap ----------

// The working copy of the currently-edited binding's macro steps - kept
// separate from state.remap until "Save binding" is clicked, same as the
// key colour popover only commits on input rather than every keystroke.
let macroSteps = [];

// Same createDropdown() listbox used everywhere else in the app (effect
// picker, profile quick-switch) rather than native <select>s, so the
// Macros & Remap form matches the rest of the app's look instead of
// falling back to the browser's own barely-stylable popup.
let bindKeyDropdown = null;
let bindTypeDropdown = null;
let remapTargetDropdown = null;
let keySelectsPopulated = false;

const BINDING_TYPES = [
  { value: 'passthrough', label: 'Passthrough (normal key)' },
  { value: 'disabled', label: 'Disabled' },
  { value: 'remap', label: 'Remap to another key' },
  { value: 'macro', label: 'Play a macro' },
];

const PRESS_RELEASE = [
  { value: '1', label: 'Press' },
  { value: '0', label: 'Release' },
];

function wireMacros() {
  $('remapEnabledToggle').addEventListener('change', async () => {
    try {
      const res = await apiPost('/api/remap/enabled', { enabled: $('remapEnabledToggle').checked });
      state.remap = res;
      renderRemapEngineStatus();
    } catch (e) {
      toast('Could not reach openaula-webd');
    }
  });

  $('remapdPill').addEventListener('click', async () => {
    const running = state.remap && state.remap.running;
    try {
      await apiPost(running ? '/api/remapd/stop' : '/api/remapd/start', {});
      toast(running ? 'Stopping remap engine…' : 'Starting remap engine…');
      setTimeout(refreshRemap, 700);
    } catch (e) {
      toast('Could not reach openaula-webd');
    }
  });

  bindKeyDropdown = createDropdown($('bindKeyDropdown'));
  bindKeyDropdown.onChange(loadBindingIntoEditor);

  bindTypeDropdown = createDropdown($('bindTypeDropdown'));
  bindTypeDropdown.setOptions(BINDING_TYPES);
  bindTypeDropdown.setValue('passthrough');
  bindTypeDropdown.onChange(updateBindingEditorVisibility);

  remapTargetDropdown = createDropdown($('remapTargetDropdown'));

  $('addMacroStepBtn').addEventListener('click', () => { addMacroStepRow(); });
  $('saveBindingBtn').addEventListener('click', saveBinding);
}

async function refreshRemap() {
  try {
    state.remap = await apiGet('/api/remap');
    populateKeySelects();
    renderRemapEngineStatus();
    loadBindingIntoEditor();
    renderBindingList();
  } catch (e) {
    toast('Could not reach openaula-webd');
  }
}

function renderRemapEngineStatus() {
  const r = state.remap;
  if (!r) return;

  $('remapEnabledToggle').checked = r.enabled;

  const dot = $('remapdDot');
  const label = $('remapdLabel');
  dot.className = 'daemon-dot ' + (r.running ? 'on' : 'off');
  label.textContent = r.running ? 'Engine Active — click to stop' : 'Engine Stopped — click to start';

  $('remapdInstallHint').style.display = r.installed ? 'none' : '';
}

// Both dropdowns offer the exact same set of physical/target keys, so
// they're built once from the first /api/remap response rather than
// re-populated (and losing whatever the user had open) on every refresh.
function populateKeySelects() {
  if (keySelectsPopulated) return;

  const options = state.remap.targetKeys.map((k) => ({ value: String(k.code), label: k.name }));

  bindKeyDropdown.setOptions(options);
  remapTargetDropdown.setOptions(options);

  if (options.length) {
    bindKeyDropdown.setValue(options[0].value);
    remapTargetDropdown.setValue(options[0].value);
  }

  keySelectsPopulated = true;
}

function updateBindingEditorVisibility() {
  const type = bindTypeDropdown.getValue();
  $('remapTargetRow').style.display = type === 'remap' ? '' : 'none';
  $('macroEditor').style.display = type === 'macro' ? '' : 'none';
}

// Selecting a physical key loads whatever binding (if any) already exists
// for it, so "Add" and "Edit" are the same form instead of two flows.
function loadBindingIntoEditor() {
  if (!state.remap || !bindKeyDropdown) return;

  const key = Number(bindKeyDropdown.getValue());
  const existing = state.remap.bindings.find((b) => b.key === key);

  bindTypeDropdown.setValue(existing ? existing.type : 'passthrough');
  remapTargetDropdown.setValue(String(existing && existing.type === 'remap' ? existing.remap : key));

  macroSteps = existing && existing.type === 'macro'
    ? existing.macro.map((s) => ({ code: s.code, press: s.press, delayAfterMs: s.delayAfterMs }))
    : [];

  renderMacroSteps();
  updateBindingEditorVisibility();
}

function addMacroStepRow() {
  const key = Number(bindKeyDropdown.getValue()) || Number(state.remap.targetKeys[0].code);
  macroSteps.push({ code: key, press: true, delayAfterMs: 30 });
  renderMacroSteps();
}

function renderMacroSteps() {
  const container = $('macroSteps');
  container.innerHTML = '';

  macroSteps.forEach((step, i) => {
    const row = document.createElement('div');
    row.className = 'macro-step-row';

    const keyContainer = document.createElement('div');
    const keyDrop = createDropdown(keyContainer);
    keyDrop.setOptions(state.remap.targetKeys.map((k) => ({ value: String(k.code), label: k.name })));
    keyDrop.setValue(String(step.code));
    keyDrop.onChange((v) => { step.code = Number(v); });

    const pressContainer = document.createElement('div');
    const pressDrop = createDropdown(pressContainer);
    pressDrop.setOptions(PRESS_RELEASE);
    pressDrop.setValue(step.press ? '1' : '0');
    pressDrop.onChange((v) => { step.press = v === '1'; });

    const delayInput = document.createElement('input');
    delayInput.type = 'number';
    delayInput.min = '0';
    delayInput.title = 'Delay after this step (ms)';
    delayInput.value = step.delayAfterMs;
    delayInput.addEventListener('input', () => {
      step.delayAfterMs = Math.max(0, Number(delayInput.value) || 0);
    });

    const removeBtn = document.createElement('button');
    removeBtn.type = 'button';
    removeBtn.className = 'macro-step-remove';
    removeBtn.title = 'Remove step';
    removeBtn.innerHTML = ICON_X;
    removeBtn.addEventListener('click', () => {
      macroSteps.splice(i, 1);
      renderMacroSteps();
    });

    row.appendChild(keyContainer);
    row.appendChild(pressContainer);
    row.appendChild(delayInput);
    row.appendChild(removeBtn);
    container.appendChild(row);
  });
}

async function saveBinding() {
  const key = Number(bindKeyDropdown.getValue());
  const type = bindTypeDropdown.getValue();
  const remap = Number(remapTargetDropdown.getValue());

  if (type === 'macro' && macroSteps.length === 0) {
    toast('Add at least one step first');
    return;
  }

  try {
    const res = await apiPost('/api/remap/binding', { key, type, remap, macro: macroSteps });
    state.remap = res;
    renderBindingList();
    toast('Binding saved');
  } catch (e) {
    toast('Could not reach openaula-webd');
  }
}

function renderBindingList() {
  const list = $('bindingList');
  list.innerHTML = '';

  const bindings = state.remap ? state.remap.bindings : [];

  if (bindings.length === 0) {
    const empty = document.createElement('div');
    empty.className = 'binding-empty';
    empty.textContent = 'No custom bindings yet. Pick a key above, choose an action, and save.';
    list.appendChild(empty);
    return;
  }

  bindings.forEach((b) => {
    const row = document.createElement('div');
    row.className = 'binding-row';

    const main = document.createElement('div');
    main.className = 'binding-row-main';

    const keyEl = document.createElement('div');
    keyEl.className = 'binding-row-key';
    keyEl.textContent = b.keyName;

    const detail = document.createElement('div');
    detail.className = 'binding-row-detail';
    detail.textContent =
      b.type === 'remap' ? ('Remapped to ' + b.remapName) :
      b.type === 'macro' ? ('Macro - ' + b.macro.length + ' step' + (b.macro.length === 1 ? '' : 's')) :
      b.type === 'disabled' ? 'Disabled' :
      'Passthrough';

    main.appendChild(keyEl);
    main.appendChild(detail);

    const actions = document.createElement('div');
    actions.className = 'binding-row-actions';

    const editBtn = document.createElement('button');
    editBtn.type = 'button';
    editBtn.className = 'icon-btn';
    editBtn.textContent = 'Edit';
    editBtn.addEventListener('click', () => {
      bindKeyDropdown.setValue(String(b.key));
      loadBindingIntoEditor();
    });

    const deleteBtn = document.createElement('button');
    deleteBtn.type = 'button';
    deleteBtn.className = 'icon-btn';
    deleteBtn.title = 'Remove binding';
    deleteBtn.innerHTML = ICON_X;
    deleteBtn.addEventListener('click', async () => {
      try {
        const res = await apiPost('/api/remap/binding/delete', { key: b.key });
        state.remap = res;
        renderBindingList();
        if (Number(bindKeyDropdown.getValue()) === b.key) loadBindingIntoEditor();
        toast('Binding removed');
      } catch (e) {
        toast('Could not reach openaula-webd');
      }
    });

    actions.appendChild(editBtn);
    actions.appendChild(deleteBtn);

    row.appendChild(main);
    row.appendChild(actions);
    list.appendChild(row);
  });
}


// ---------- calibration wizard ----------

// Walks through every physical LED one at a time (see core/CalibrationSession
// and daemon/main.cpp - while a session is active, the daemon lights only
// that one LED white and ignores whatever effect is otherwise configured)
// and asks "which key just lit up on your physical keyboard?" - answering
// records that key's calibration mapping, same as the deleted Qt GUI's
// CalibrationDialog did, just spread across HTTP calls instead of one
// process holding the HID connection directly.
function wireCalibration() {
  $('recalibrateBtn').addEventListener('click', startCalibration);
  $('calibBackBtn').addEventListener('click', () => calibrationStep('/api/calibration/back'));
  $('calibSkipBtn').addEventListener('click', () => calibrationStep('/api/calibration/skip'));
  $('calibFinishBtn').addEventListener('click', () => calibrationStep('/api/calibration/finish'));
}

async function startCalibration() {
  try {
    state.calibration = await apiPost('/api/calibration/start', {});
    showView('section-lighting');
    renderCalibrationUI();
    toast('Calibration started - watch your physical keyboard');
  } catch (e) {
    toast('Could not reach openaula-webd');
  }
}

async function calibrationStep(path) {
  try {
    state.calibration = await apiPost(path, {});
    renderCalibrationUI();

    if (!state.calibration.active) {
      toast('Calibration finished');
      // The mapping (and possibly the "calibrated" flag) just changed on
      // the backend - resync the full app state so Custom-mode colours
      // land on the right physical keys again.
      const fresh = await apiGet('/api/state');
      state.appState = fresh;
      applyStateToControls();
      renderKeyboard();
    }
  } catch (e) {
    toast('Could not reach openaula-webd');
  }
}

async function handleCalibrationKeyClick(key) {
  if (!state.calibration || !state.calibration.active) return;

  try {
    state.calibration = await apiPost('/api/calibration/map', { ledIndex: key.ledIndex });
    renderCalibrationUI();

    if (!state.calibration.active) {
      toast('Calibration finished');
      const fresh = await apiGet('/api/state');
      state.appState = fresh;
      applyStateToControls();
      renderKeyboard();
    }
  } catch (e) {
    toast('Could not reach openaula-webd');
  }
}

function renderCalibrationUI() {
  const c = state.calibration;
  const bar = $('calibBar');

  if (!c || !c.active) {
    bar.style.display = 'none';
    return;
  }

  bar.style.display = '';
  $('calibStepText').textContent = `LED ${c.step + 1} of ${c.total}`;
  $('calibBackBtn').disabled = c.step <= 0;
}


// ---------- daemon status ----------

function renderDaemonStatus(running) {
  const dot = $('daemonDot');
  const label = $('daemonLabel');
  const sDot = $('settingsDaemonDot');
  const sLabel = $('settingsDaemonLabel');

  dot.className = 'daemon-dot ' + (running ? 'on' : 'off');
  label.textContent = running ? 'Daemon Active' : 'Daemon Offline';

  sDot.className = 'daemon-dot ' + (running ? 'on' : 'off');
  sLabel.textContent = running ? 'Running — click to stop' : 'Stopped — click to start';
}

function wireDaemonControls() {
  const toggle = async () => {
    const running = state.appState && state.appState.daemonRunning;
    await apiPost(running ? '/api/daemon/stop' : '/api/daemon/start', {});
    toast(running ? 'Stopping background daemon…' : 'Starting background daemon…');
    setTimeout(pollStatus, 700);
  };

  $('daemonPill').addEventListener('click', toggle);
  $('settingsDaemonBtn').addEventListener('click', toggle);
}

async function pollStatus() {
  try {
    const status = await apiGet('/api/daemon/status');
    if (state.appState) state.appState.daemonRunning = status.running;
    renderDaemonStatus(status.running);
  } catch (e) {
    // Bridge unreachable - leave the last known status showing rather
    // than flashing an error on every missed poll.
  }
}

// The page only ever mutated its local copy of appState from POST
// responses - if another browser tab changed anything, this
// page would show stale data forever. Poll the real state periodically
// and re-render, but never while the user is mid-interaction (dragging a
// a slider or the colour picker) so a network refresh
// can't yank a control out from under their pointer.
async function refreshState() {
  if (isBusy() || stroke || document.querySelector('.cdrop.open')) return;

  try {
    const fresh = await apiGet('/api/state');
    const layersChanged = JSON.stringify(fresh.layers) !== JSON.stringify(layers);
    state.appState = fresh;
    applyStateToControls(layersChanged);
  } catch (e) {
    // Handled by pollStatus's own error path already covering reachability.
  }
}


// ---------- apply loaded state to controls ----------

// reloadLayers: replace the local layer stack with the server's (on load,
// after applying a profile, or when another tab changed it).
function applyStateToControls(reloadLayers = true) {
  const s = state.appState;
  if (!s) return;

  $('brightnessSlider').value = Math.round(s.brightness * 100);
  $('brightnessValue').textContent = Math.round(s.brightness * 100) + '%';

  if (reloadLayers) {
    syncLayersFromState();
    renderLayers();
    loadSelectedLayerIntoControls();
    updateToolUI();
    pollSystemSignals();
  } else {
    state.appState.layers = layers;
  }

  $('calibrationText').textContent = s.calibrated
    ? 'This keyboard has been calibrated — per-key colours map to the correct physical LEDs.'
    : 'Not calibrated yet - per-key colours may land on the wrong keys until this keyboard has been calibrated.';

  renderDaemonStatus(s.daemonRunning);
}


// ---------- boot ----------

function init() {
  wireNav();
  wireSliders();
  wireProfiles();
  wireDaemonControls();
  wireMacros();
  wireCalibration();
  wireLayers();
  wireTools();
  wireKeySignals();
  wireLayerPicker();
  wireModal();

  Promise.all([apiGet('/api/layout'), apiGet('/api/state'), apiGet('/api/profiles'), apiGet('/api/remap')])
    .then(([layout, appState, profiles, remap]) => {
      state.layout = layout;
      state.appState = appState;
      state.profiles = profiles;
      state.remap = remap;

      buildKeyboardDom();
      startAnimationLoop();
      applyStateToControls();
      renderProfiles();
      populateKeySelects();
      renderRemapEngineStatus();
      loadBindingIntoEditor();
      renderBindingList();
    })
    .catch(() => toast('Could not reach openaula-webd'));

  // A calibration session lives on the backend, so it survives a page
  // reload - pick it back up instead of leaving the keyboard stuck
  // lighting one LED with no wizard on screen.
  apiGet('/api/calibration')
    .then((calib) => {
      state.calibration = calib;
      if (calib.active) showView('section-lighting');
      renderCalibrationUI();
    })
    .catch(() => {});

  setInterval(pollStatus, 4000);
  setInterval(refreshState, 5000);
  setInterval(pollSystemSignals, 1000);
}

document.addEventListener('DOMContentLoaded', init);

})();
