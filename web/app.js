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

let wheelHue = 260, wheelSat = 0.64, wheelVal = 1.0;
let wheelBase = null;

// How long the user is considered "busy" (actively dragging/typing) after
// the last interaction - the periodic state resync skips updates during
// this window so it can't yank a slider out from under a mid-drag user.
const BUSY_GRACE_MS = 600;
let lastInteractionAt = 0;
function markBusy() { lastInteractionAt = Date.now(); }
function isBusy() { return Date.now() - lastInteractionAt < BUSY_GRACE_MS; }

const EFFECTS = [
  { id: 'custom',      name: 'Custom',       desc: 'Your own per-key design',      css: 'linear-gradient(135deg,#7c5cff,#00d6ff)' },
  { id: 'breathing',   name: 'Breathing',    desc: 'Fades in and out',             css: 'linear-gradient(135deg,#7c5cff,#241a52)' },
  { id: 'colorcycle',  name: 'Colour Cycle', desc: 'Smooth hue rotation',          css: 'linear-gradient(90deg,#ff5470,#ffd23f,#2fd47a,#00d6ff,#7c5cff)' },
  { id: 'bounce',      name: 'Bounce',       desc: 'Light bounces across keys',    css: 'linear-gradient(135deg,#00d6ff,#0a3a4a)' },
  { id: 'wave',        name: 'Wave',         desc: 'Colour sweeps side to side',   css: 'linear-gradient(90deg,#00d6ff,#0d0e11,#00d6ff)' },
  { id: 'ripple',      name: 'Ripple',       desc: 'Radiates out from a point',    css: 'radial-gradient(circle,#00d6ff,#0d0e11)' },
  { id: 'starlight',   name: 'Starlight',    desc: 'Random keys twinkle',          css: 'radial-gradient(circle at 30% 30%,#fff,#7c5cff 45%,#0d0e11)' },
  { id: 'raindrop',    name: 'Raindrop',     desc: 'Drops of colour fall & fade',  css: 'linear-gradient(180deg,#00d6ff,#0d0e11)' },
  { id: 'comet',       name: 'Comet',        desc: 'A trail streaks across',       css: 'linear-gradient(120deg,#fff,#00d6ff,#0d0e11)' },
  { id: 'fire',        name: 'Fire',         desc: 'Flickering embers',            css: 'linear-gradient(0deg,#ff5470,#ffb347,#ffd23f)' },
  { id: 'rainbowwave', name: 'Rainbow Wave', desc: 'A rainbow sweeps across',      css: 'linear-gradient(90deg,#ff5470,#ffd23f,#2fd47a,#00d6ff,#7c5cff)' },
  { id: 'heartbeat',   name: 'Heartbeat',    desc: 'Pulses like a heartbeat',      css: 'radial-gradient(circle,#ff5470,#3a0d1a)' },
  { id: 'strobe',      name: 'Strobe',       desc: 'Sharp on/off flash',           css: 'linear-gradient(135deg,#fff,#0d0e11)' },
  { id: 'alternating', name: 'Alternating',  desc: 'Checkerboard blink',           css: 'linear-gradient(45deg,#00d6ff 25%,#0d0e11 25%,#0d0e11 50%,#00d6ff 50%,#00d6ff 75%,#0d0e11 75%)' },
  { id: 'confetti',    name: 'Confetti',     desc: 'Random keys flash random hues', css: 'radial-gradient(circle at 30% 30%,#ff5470,transparent 40%),radial-gradient(circle at 70% 60%,#2fd47a,transparent 40%),radial-gradient(circle at 40% 80%,#00d6ff,transparent 40%),#0d0e11' },
  { id: 'snake',       name: 'Snake',        desc: 'A lit trail chases across',    css: 'linear-gradient(90deg,#0d0e11,#7c5cff,#00d6ff,#0d0e11)' },
  { id: 'spiral',      name: 'Spiral',       desc: 'A rotating rainbow pinwheel',  css: 'conic-gradient(from 0deg,#ff5470,#ffd23f,#2fd47a,#00d6ff,#7c5cff,#ff5470)' },
  { id: 'fireworks',   name: 'Fireworks',    desc: 'Rings burst from random spots', css: 'radial-gradient(circle at 35% 35%,#ffd23f,transparent 55%),radial-gradient(circle at 65% 65%,#ff5470,transparent 55%),#0d0e11' },
  { id: 'sweep',       name: 'Sweep',        desc: 'A hard-edged colour wipe',      css: 'linear-gradient(120deg,#00d6ff 50%,#0d0e11 50%)' },
  { id: 'off',         name: 'Off',          desc: 'Backlight disabled',           css: 'linear-gradient(135deg,#26272c,#0d0e11)' },
];

const PRESETS = ['#7c5cff', '#00d6ff', '#2fd47a', '#ffd23f', '#ff5470', '#ffffff'];


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
// degree convention hsvToRgb() above uses for the colour wheel.
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

// t is already speed-scaled elapsed seconds, exactly like the `t` param
// LightingEngine::computeFrame receives from daemon/main.cpp.
function computePreviewFrame(mode, t, keys, baseColors, activeColor) {
  const n = keys.length;
  const frame = new Array(n);

  const maxOf = (fn) => { let m = 0; for (const k of keys) m = Math.max(m, fn(k)); return m; };

  switch (mode) {
    case 'breathing': {
      const k = 0.1 + 0.9 * (Math.sin(t * 2.0) + 1.0) / 2.0;
      for (let i = 0; i < n; i++) frame[i] = scaleColorPreview(baseColors[i] || { r: 24, g: 24, b: 28 }, k);
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
    default:
      for (let i = 0; i < n; i++) frame[i] = activeColor;
      return frame;
  }
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

    items.forEach((item) => {
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

// Lighting / Macros & Remap / Settings are all permanently in the DOM now
// (see index.html's .page-section elements) - there's no view to switch
// to, so the rail just smooth-scrolls the already-visible section into
// place, the same way it already did for the Profiles sidebar.
function wireNav() {
  document.querySelectorAll('.rail-btn[data-section]').forEach((btn) => {
    btn.addEventListener('click', () => scrollToSection(btn.dataset.section, btn));
  });
}

function scrollToSection(id, activeBtn) {
  document.querySelectorAll('.rail-btn[data-section]').forEach((b) => {
    b.classList.toggle('active', b === activeBtn);
  });

  const el = $(id);
  if (!el) return;

  el.scrollIntoView({ behavior: 'smooth', block: 'start' });

  if (id === 'profilesSide') {
    el.animate(
      [{ boxShadow: '0 0 0 0 rgba(0,214,255,0)' }, { boxShadow: '0 0 0 2px rgba(0,214,255,.6) inset' }, { boxShadow: '0 0 0 0 rgba(0,214,255,0)' }],
      { duration: 900 }
    );
  }
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

function ensureGlowFilter(svg) {
  if (svg.querySelector('#keyGlowBlur')) return;
  const defs = document.createElementNS(SVGNS, 'defs');
  defs.innerHTML =
    '<filter id="keyGlowBlur" x="-80%" y="-80%" width="260%" height="260%">' +
    '<feGaussianBlur stdDeviation="4"/>' +
    '</filter>';
  svg.appendChild(defs);
}

// One entry per layout key: cached DOM refs so painting a frame is just a
// handful of attribute writes, not a full SVG rebuild - the animated
// preview repaints every frame, so rebuilding the whole tree each time
// would both be wasteful and reset hover/selection state constantly.
let keyElements = [];
let selectedLedIndex = null;
let animStart = performance.now();
let rafHandle = null;

function resetAnimationClock() {
  animStart = performance.now();
}

// Builds the SVG once per layout load. Each key gets two stroked (never
// filled) rings well inside its own bounds - a crisp one and a wider,
// blurred one behind it for ambient bleed - plus a separate highlight
// outline used only for hover/selection feedback. Two things this fixes
// vs. earlier attempts:
//   - insetting the rings (rather than stroking each key's *full* border)
//     means two adjacent keys' rings never land on the same seam, so
//     neither can double up on the other's stroke opacity there - that
//     doubling-up was what made the very first version look uneven.
//   - stroking (never filling) the ring means a key with the near-black
//     "unset" default custom colour reads as a faint, thin edge rather
//     than an opaque block covering the keycap - filling was what caused
//     the solid black squares in the next attempt.
function buildKeyboardDom() {
  const svg = $('kbSvg');
  svg.innerHTML = '';
  svg.setAttribute('viewBox', `0 0 ${KB_PHOTO_W} ${KB_PHOTO_H}`);
  ensureGlowFilter(svg);

  keyElements = [];

  const layout = state.layout;
  if (!layout) return;

  layout.keys.forEach((k) => {
    const rect = KEY_PIXEL_RECTS[k.ledIndex] || [0, 0, 0, 0];
    const [x0, y0, x1, y1] = rect;
    const w = x1 - x0, h = y1 - y0;

    const group = document.createElementNS(SVGNS, 'g');
    group.classList.add('key-group');

    // Invisible full-key hit target - handles clicks/hover, never itself
    // painted, so the photo's own keycap stays fully visible. Always
    // clickable now, regardless of mode: clicking a key while a
    // non-Custom effect is active switches into Custom automatically
    // (see openKeyPopover) rather than silently doing nothing.
    const hit = document.createElementNS(SVGNS, 'rect');
    hit.setAttribute('x', x0); hit.setAttribute('y', y0);
    hit.setAttribute('width', w); hit.setAttribute('height', h);
    hit.classList.add('key-hit', 'editable');
    hit.addEventListener('click', (ev) => onKeyClick(ev, k, group));
    hit.addEventListener('mouseenter', () => group.classList.add('hover'));
    hit.addEventListener('mouseleave', () => group.classList.remove('hover'));

    // Wider, blurred ring (ambient bleed) - inset a bit less than the
    // crisp ring so its blur has room to spread without ever reaching
    // the neighbouring key's own inset area.
    const insetBlur = { x: w * 0.18, y: h * 0.20 };
    const blur = document.createElementNS(SVGNS, 'rect');
    blur.setAttribute('x', x0 + insetBlur.x); blur.setAttribute('y', y0 + insetBlur.y);
    blur.setAttribute('width', Math.max(0, w - insetBlur.x * 2));
    blur.setAttribute('height', Math.max(0, h - insetBlur.y * 2));
    blur.setAttribute('rx', 4);
    blur.classList.add('key-glow-blur');
    blur.setAttribute('filter', 'url(#keyGlowBlur)');

    // Crisp inner ring - the actual per-key colour reads clearly here.
    const insetRing = { x: w * 0.27, y: h * 0.29 };
    const ring = document.createElementNS(SVGNS, 'rect');
    ring.setAttribute('x', x0 + insetRing.x); ring.setAttribute('y', y0 + insetRing.y);
    ring.setAttribute('width', Math.max(0, w - insetRing.x * 2));
    ring.setAttribute('height', Math.max(0, h - insetRing.y * 2));
    ring.setAttribute('rx', 3);
    ring.classList.add('key-glow');

    const highlight = document.createElementNS(SVGNS, 'rect');
    highlight.setAttribute('x', x0 + insetRing.x); highlight.setAttribute('y', y0 + insetRing.y);
    highlight.setAttribute('width', Math.max(0, w - insetRing.x * 2));
    highlight.setAttribute('height', Math.max(0, h - insetRing.y * 2));
    highlight.setAttribute('rx', 3);
    highlight.classList.add('key-highlight');

    group.appendChild(hit);
    group.appendChild(blur);
    group.appendChild(ring);
    group.appendChild(highlight);
    svg.appendChild(group);

    keyElements.push({ key: k, group, blur, ring });
  });

  $('keyboardHint').textContent = 'Click any key to paint it and switch to the Custom effect.';
}

// Runs every animation frame (and on demand after any state change) -
// computes this instant's colour for every key and writes it straight to
// the cached DOM elements from buildKeyboardDom(). Custom mode is static
// (just the saved per-key design); Off hides everything; every other mode
// is animated live via computePreviewFrame(), the JS port of
// core/LightingEngine.cpp, so the preview shows the same motion the
// daemon drives on the real hardware instead of a flat accent tint.
function paintKeyboard() {
  if (!keyElements.length) return;

  const s = state.appState;
  const mode = s ? s.mode : 'custom';
  const brightness = s ? s.brightness : 1;
  const accent = s ? s.activeColor : { r: 124, g: 92, b: 255 };
  const customColors = s ? s.customColors : [];
  const backlightOff = mode === 'off';

  let colors = null;
  if (!backlightOff && mode !== 'custom') {
    const t = (performance.now() - animStart) / 1000 * (s ? s.speed : 1);
    colors = computePreviewFrame(mode, t, state.layout.keys, customColors, accent);
  }

  keyElements.forEach(({ key: k, group, blur, ring }, i) => {
    group.classList.toggle('selected', k.ledIndex === selectedLedIndex);

    if (backlightOff) {
      blur.setAttribute('stroke', 'none');
      ring.setAttribute('stroke', 'none');
      return;
    }

    const raw = mode === 'custom'
      ? (customColors[k.ledIndex] || { r: 24, g: 24, b: 28 })
      : colors[i];
    const c = scaleColor(raw, brightness);
    const hex = rgbToHex(c);

    ring.setAttribute('stroke', hex);
    ring.setAttribute('stroke-width', 2.5);
    ring.setAttribute('stroke-opacity', 0.95);

    blur.setAttribute('stroke', hex);
    blur.setAttribute('stroke-width', 7);
    blur.setAttribute('stroke-opacity', 0.55);
  });
}

function startAnimationLoop() {
  if (rafHandle) return;

  const tick = () => {
    paintKeyboard();
    rafHandle = requestAnimationFrame(tick);
  };

  rafHandle = requestAnimationFrame(tick);
}

// Kept as the name the rest of the file calls after any state-changing
// action - now just triggers an immediate repaint rather than a full SVG
// rebuild, since buildKeyboardDom() only needs to run once per layout.
function renderKeyboard() {
  paintKeyboard();
}

// Only one popover can be open at a time - tracked here so opening a new
// key's popover always tears down the previous key's document-level
// listener instead of leaving it stacked up (each click used to leak one).
let closePopoverListener = null;

function closeKeyPopover() {
  $('keyPopover').classList.remove('show');
  if (closePopoverListener) {
    document.removeEventListener('pointerdown', closePopoverListener, true);
    closePopoverListener = null;
  }
  selectedLedIndex = null;
}

// Every key click goes through here first: during a calibration session
// (see the Settings panel) a click means "this is the key that just lit
// up", not "edit this key's colour" - the wizard hijacks the same keyboard
// preview rather than drawing a second one.
function onKeyClick(ev, key, group) {
  if (state.calibration && state.calibration.active) {
    handleCalibrationKeyClick(key);
    return;
  }

  openKeyPopover(ev, key, group);
}

// Clicking any key edits its colour, same as any other per-key RGB
// software - it's not gated behind first manually selecting the Custom
// effect. If a different effect is currently active, the first click
// switches into Custom (seeding it from whatever the animation currently
// shows for that key, so the switch doesn't visually jump) and then opens
// the popover exactly as if Custom had already been selected.
async function openKeyPopover(ev, key, group) {
  closeKeyPopover();
  markBusy();

  selectedLedIndex = key.ledIndex;
  if (group) group.classList.add('selected');

  if (state.appState && state.appState.mode !== 'custom') {
    try {
      const res = await apiPost('/api/mode', { mode: 'custom' });
      state.appState = res;
      resetAnimationClock();
      highlightActiveEffect();
      renderKeyboard();
    } catch (e) {
      toast('Could not reach openaula-webd');
      return;
    }
  }

  const pop = $('keyPopover');
  const input = $('keyColorInput');
  const current = (state.appState.customColors[key.ledIndex]) || { r: 24, g: 24, b: 28 };

  input.value = rgbToHex(current);
  $('keyPopoverLabel').textContent = key.label;

  // Default to opening below the key; flip above it if there isn't room
  // (bottom-row keys would otherwise render the popover off-screen).
  const rect = ev.currentTarget.getBoundingClientRect();
  const popW = 150, popH = 60;
  const left = clamp(rect.left, 8, window.innerWidth - popW - 8);
  const openBelow = rect.bottom + 8 + popH <= window.innerHeight;
  const top = openBelow ? rect.bottom + 8 : rect.top - popH - 8;

  pop.style.left = left + 'px';
  pop.style.top = Math.max(8, top) + 'px';
  pop.classList.add('show');

  const commit = debounce(async (hex) => {
    markBusy();
    const rgb = hexToRgb(hex);
    const res = await apiPost('/api/custom-color', { index: key.ledIndex, ...rgb });
    state.appState = res;
    renderKeyboard();
  }, 60);

  input.oninput = () => { markBusy(); commit(input.value); };

  closePopoverListener = (e) => {
    if (!pop.contains(e.target) && e.target !== ev.currentTarget) {
      closeKeyPopover();
    }
  };

  setTimeout(() => document.addEventListener('pointerdown', closePopoverListener, true), 0);
}


// ---------- effect dropdown ----------

let effectDropdown = null;

function renderEffectOption(el, fx) {
  const swatch = document.createElement('span');
  swatch.className = 'cdrop-swatch';
  swatch.style.background = fx.css;
  el.appendChild(swatch);

  const label = document.createElement('span');
  label.textContent = fx.name;
  el.appendChild(label);
}

function buildEffectSelect() {
  effectDropdown = createDropdown($('effectDropdown'), {
    renderOption: (el, item) => renderEffectOption(el, item.fx),
    renderButton: (el, item) => { if (item) renderEffectOption(el, item.fx); },
  });

  effectDropdown.setOptions(EFFECTS.map((fx) => ({ value: fx.id, label: fx.name, fx })));

  effectDropdown.onChange(async (mode) => {
    const fx = EFFECTS.find((e) => e.id === mode);
    updateEffectPreview();
    try {
      const res = await apiPost('/api/mode', { mode });
      state.appState = res;
      resetAnimationClock();
      renderKeyboard();
      toast(fx.name + ' applied');
    } catch (e) {
      toast('Could not reach openaula-webd');
    }
  });
}

function updateEffectPreview() {
  const mode = effectDropdown.getValue();
  const fx = EFFECTS.find((e) => e.id === mode) || EFFECTS[0];
  $('effectDescText').textContent = fx.desc;
}

function highlightActiveEffect() {
  const mode = state.appState ? state.appState.mode : 'custom';
  effectDropdown.setValue(mode);
  updateEffectPreview();
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

  const postSpeed = debounce(async (v) => {
    state.appState = await apiPost('/api/speed', { speed: v / 10 });
  }, 120);

  brightness.addEventListener('input', () => {
    markBusy();
    $('brightnessValue').textContent = brightness.value + '%';
    if (state.appState) state.appState.brightness = Number(brightness.value) / 100;
    renderKeyboard();
    postBrightness(Number(brightness.value));
  });

  speed.addEventListener('input', () => {
    markBusy();
    $('speedValue').textContent = (speed.value / 10).toFixed(1) + 'x';
    postSpeed(Number(speed.value));
  });
}


// ---------- colour wheel ----------

function buildWheelBase() {
  const canvas = $('wheel');
  const ctx = canvas.getContext('2d');
  const size = canvas.width;
  const cx = size / 2, cy = size / 2, radius = size / 2;

  const img = ctx.createImageData(size, size);

  for (let y = 0; y < size; y++) {
    for (let x = 0; x < size; x++) {
      const dx = x - cx, dy = y - cy;
      const dist = Math.sqrt(dx * dx + dy * dy);
      const idx = (y * size + x) * 4;

      if (dist > radius) {
        img.data[idx + 3] = 0;
        continue;
      }

      const angle = Math.atan2(dy, dx) * 180 / Math.PI;
      const hue = (angle + 360) % 360;
      const sat = Math.min(1, dist / radius);

      const rgb = hsvToRgb(hue, sat, 1);
      img.data[idx] = rgb.r;
      img.data[idx + 1] = rgb.g;
      img.data[idx + 2] = rgb.b;
      img.data[idx + 3] = 255;
    }
  }

  wheelBase = img;
  drawWheel();
}

function drawWheel() {
  const canvas = $('wheel');
  const ctx = canvas.getContext('2d');
  if (!wheelBase) return;

  ctx.putImageData(wheelBase, 0, 0);

  const size = canvas.width, cx = size / 2, cy = size / 2, radius = size / 2;
  const dist = wheelSat * radius;
  const angle = wheelHue * Math.PI / 180;
  const mx = cx + Math.cos(angle) * dist;
  const my = cy + Math.sin(angle) * dist;

  ctx.beginPath();
  ctx.arc(mx, my, 6, 0, Math.PI * 2);
  ctx.strokeStyle = '#ffffff';
  ctx.lineWidth = 2;
  ctx.stroke();
  ctx.beginPath();
  ctx.arc(mx, my, 6, 0, Math.PI * 2);
  ctx.strokeStyle = 'rgba(0,0,0,.45)';
  ctx.lineWidth = 1;
  ctx.stroke();
}

function updateSwatch() {
  const rgb = hsvToRgb(wheelHue, wheelSat, wheelVal);
  const hex = rgbToHex(rgb);
  $('swatchPreview').style.background = hex;
  $('swatchHex').textContent = hex;
  $('railAccentDot').style.background = hex;
  $('railAccentDot').style.boxShadow = `0 0 12px 2px ${hex}`;
  updateValueSliderVisual();
  return rgb;
}

function updateValueSliderVisual() {
  const pct = clamp(wheelVal * 100, 0, 100);
  $('valueFill').style.height = pct + '%';
  $('valueThumb').style.bottom = pct + '%';
  $('valueSlider').setAttribute('aria-valuenow', Math.round(pct));
}

const postColor = debounce((rgb) => {
  apiPost('/api/color', rgb).then((res) => { state.appState = res; renderKeyboard(); });
}, 90);

function wireWheel() {
  buildWheelBase();

  const canvas = $('wheel');
  let dragging = false;

  const pick = (ev) => {
    const rect = canvas.getBoundingClientRect();
    const scaleX = canvas.width / rect.width;
    const scaleY = canvas.height / rect.height;
    const x = (ev.clientX - rect.left) * scaleX;
    const y = (ev.clientY - rect.top) * scaleY;
    const cx = canvas.width / 2, cy = canvas.height / 2, radius = canvas.width / 2;
    const dx = x - cx, dy = y - cy;
    const dist = Math.min(radius, Math.sqrt(dx * dx + dy * dy));

    wheelHue = (Math.atan2(dy, dx) * 180 / Math.PI + 360) % 360;
    wheelSat = dist / radius;

    markBusy();
    drawWheel();
    postColor(updateSwatch());
  };

  canvas.addEventListener('pointerdown', (ev) => { dragging = true; canvas.setPointerCapture(ev.pointerId); pick(ev); });
  canvas.addEventListener('pointermove', (ev) => { if (dragging) pick(ev); });
  canvas.addEventListener('pointerup', () => { dragging = false; });
  canvas.addEventListener('pointercancel', () => { dragging = false; });

  wireValueSlider();
}

function wireValueSlider() {
  const slider = $('valueSlider');
  let dragging = false;

  const setFromClientY = (clientY) => {
    const rect = slider.getBoundingClientRect();
    const pct = clamp((rect.bottom - clientY) / rect.height, 0, 1);
    wheelVal = pct;
    markBusy();
    postColor(updateSwatch());
  };

  slider.addEventListener('pointerdown', (ev) => {
    dragging = true;
    slider.setPointerCapture(ev.pointerId);
    slider.focus();
    setFromClientY(ev.clientY);
  });
  slider.addEventListener('pointermove', (ev) => { if (dragging) setFromClientY(ev.clientY); });
  slider.addEventListener('pointerup', () => { dragging = false; });
  slider.addEventListener('pointercancel', () => { dragging = false; });

  slider.addEventListener('keydown', (ev) => {
    const step = ev.shiftKey ? 0.1 : 0.02;
    if (ev.key === 'ArrowUp' || ev.key === 'ArrowRight') { wheelVal = clamp(wheelVal + step, 0, 1); }
    else if (ev.key === 'ArrowDown' || ev.key === 'ArrowLeft') { wheelVal = clamp(wheelVal - step, 0, 1); }
    else if (ev.key === 'Home') { wheelVal = 0; }
    else if (ev.key === 'End') { wheelVal = 1; }
    else { return; }

    ev.preventDefault();
    markBusy();
    postColor(updateSwatch());
  });
}

function buildPresets() {
  const row = $('presetRow');
  row.innerHTML = '';

  PRESETS.forEach((hex) => {
    const dot = document.createElement('button');
    dot.type = 'button';
    dot.className = 'preset-swatch';
    dot.style.background = hex;
    dot.title = hex;
    dot.addEventListener('click', () => {
      const rgb = hexToRgb(hex);
      const hsv = rgbToHsv(rgb.r, rgb.g, rgb.b);
      wheelHue = hsv.h; wheelSat = hsv.s; wheelVal = hsv.v;
      markBusy();
      drawWheel();
      postColor(updateSwatch());
    });
    row.appendChild(dot);
  });
}


// ---------- profiles ----------

let profileDropdown = null;

function wireProfiles() {
  $('addProfileBtn').addEventListener('click', async () => {
    const name = prompt('Save current lighting as profile:', 'New Profile');
    if (!name) return;

    const res = await apiPost('/api/profiles/save', { name });
    state.profiles = res;
    renderProfiles();
    toast('Profile "' + name + '" saved');
  });

  profileDropdown = createDropdown($('profileDropdown'));
  profileDropdown.onChange((index) => applyProfile(Number(index)));
}

// Shared by the sidebar's profile cards and the quick-select dropdown in
// the Lighting section, so picking a profile either way stays in sync.
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

function renderProfileDropdown() {
  const profiles = state.profiles ? state.profiles.profiles : [];
  const activeIndex = state.profiles ? state.profiles.activeIndex : -1;

  if (profiles.length === 0) {
    profileDropdown.setOptions([{ value: '-1', label: 'No profiles saved yet' }]);
    profileDropdown.setValue('-1');
    return;
  }

  profileDropdown.setOptions(profiles.map((p, i) => ({ value: String(i), label: p.name })));
  profileDropdown.setValue(String(activeIndex));
}

function renderProfiles() {
  const list = $('profileList');
  list.innerHTML = '';

  const profiles = state.profiles ? state.profiles.profiles : [];
  const activeIndex = state.profiles ? state.profiles.activeIndex : -1;

  renderProfileDropdown();

  if (profiles.length === 0) {
    const empty = document.createElement('div');
    empty.className = 'profile-empty';
    empty.textContent = 'No saved profiles yet. Set up your lighting, then hit + to save it.';
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
    renameBtn.textContent = '✎';
    renameBtn.title = 'Rename';
    renameBtn.addEventListener('click', async (ev) => {
      ev.stopPropagation();
      const newName = prompt('Rename profile:', p.name);
      if (!newName || newName === p.name) return;
      const res = await apiPost('/api/profiles/rename', { index: i, name: newName });
      state.profiles = res;
      renderProfiles();
    });

    const deleteBtn = document.createElement('button');
    deleteBtn.type = 'button';
    deleteBtn.textContent = '✕';
    deleteBtn.title = 'Delete';
    deleteBtn.addEventListener('click', async (ev) => {
      ev.stopPropagation();
      if (!confirm('Delete profile "' + p.name + '"?')) return;
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
    removeBtn.textContent = '✕';
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
    deleteBtn.textContent = '✕';
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
// slider/wheel or typing in the key colour popover) so a network refresh
// can't yank a control out from under their pointer.
async function refreshState() {
  if (isBusy()) return;

  try {
    const previousMode = state.appState ? state.appState.mode : null;
    const fresh = await apiGet('/api/state');
    state.appState = fresh;
    // Only restart the preview's animation clock when the mode actually
    // changed (e.g. another tab/profile switched it) - resetting it on
    // every 5s poll would make the animation stutter back to t=0
    // constantly even while a single effect stays selected.
    if (fresh.mode !== previousMode) resetAnimationClock();
    applyStateToControls();
    renderKeyboard();
  } catch (e) {
    // Handled by pollStatus's own error path already covering reachability.
  }
}


// ---------- apply loaded state to controls ----------

function applyStateToControls() {
  const s = state.appState;
  if (!s) return;

  $('brightnessSlider').value = Math.round(s.brightness * 100);
  $('brightnessValue').textContent = Math.round(s.brightness * 100) + '%';

  $('speedSlider').value = Math.round(s.speed * 10);
  $('speedValue').textContent = s.speed.toFixed(1) + 'x';

  const hsv = rgbToHsv(s.activeColor.r, s.activeColor.g, s.activeColor.b);
  wheelHue = hsv.h; wheelSat = hsv.s; wheelVal = hsv.v || 1;
  drawWheel();
  updateSwatch();

  $('calibrationText').textContent = s.calibrated
    ? 'This keyboard has been calibrated — per-key colours map to the correct physical LEDs.'
    : 'Not calibrated yet - per-key colours may land on the wrong keys until this keyboard has been calibrated.';

  highlightActiveEffect();
  renderDaemonStatus(s.daemonRunning);
}


// ---------- boot ----------

function init() {
  wireNav();
  wireSliders();
  wireProfiles();
  wireDaemonControls();
  wireMacros();
  buildEffectSelect();
  buildPresets();

  Promise.all([apiGet('/api/layout'), apiGet('/api/state'), apiGet('/api/profiles'), apiGet('/api/remap')])
    .then(([layout, appState, profiles, remap]) => {
      state.layout = layout;
      state.appState = appState;
      state.profiles = profiles;
      state.remap = remap;

      wireWheel();
      buildKeyboardDom();
      resetAnimationClock();
      startAnimationLoop();
      applyStateToControls();
      renderProfiles();
      populateKeySelects();
      renderRemapEngineStatus();
      loadBindingIntoEditor();
      renderBindingList();
    })
    .catch(() => toast('Could not reach openaula-webd'));

  setInterval(pollStatus, 4000);
  setInterval(refreshState, 5000);
}

document.addEventListener('DOMContentLoaded', init);

})();
