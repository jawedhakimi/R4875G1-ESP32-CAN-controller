#ifndef WEB_PAGE_H
#define WEB_PAGE_H

#include <Arduino.h>   // PROGMEM

/* Single self-contained page (no CDN/external assets -- this has to work
   on a LAN with no internet access). Polls /api/status every 500ms and
   posts to /api/... on user action. Served from web_server.cpp. */
static const char WEB_INDEX_HTML[] PROGMEM = R"HTMLPAGE(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Huawei PSU Control</title>
<style>
  :root { color-scheme: dark; }
  * { box-sizing: border-box; }
  body {
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
    background: #14161a; color: #eaeaea; margin: 0; padding: 16px;
  }
  h1 { font-size: 1.3em; margin: 0 0 4px; }
  .sub { color: #8a8f98; font-size: 0.85em; margin-bottom: 16px; }
  .card {
    background: #1f2228; border-radius: 12px; padding: 14px 16px;
    margin-bottom: 12px; border: 1px solid #2c3038;
  }
  .card h2 {
    font-size: 0.95em; text-transform: uppercase; letter-spacing: 0.04em;
    color: #9aa3b2; margin: 0 0 10px;
  }
  .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(110px, 1fr)); gap: 10px; }
  .stat { background: #14161a; border-radius: 8px; padding: 8px 10px; }
  .stat .v { font-size: 1.3em; font-weight: 600; }
  .stat .l { font-size: 0.75em; color: #8a8f98; }
  .row { display: flex; align-items: center; gap: 10px; margin-bottom: 10px; flex-wrap: wrap; }
  .row:last-child { margin-bottom: 0; }
  label { font-size: 0.85em; color: #9aa3b2; min-width: 90px; }
  input[type=number], input[type=text] {
    background: #14161a; border: 1px solid #2c3038; color: #eaeaea;
    border-radius: 6px; padding: 8px; width: 90px; font-size: 1em;
  }
  input[type=checkbox] { width: 18px; height: 18px; }
  button {
    background: #3d6bff; color: white; border: none; border-radius: 6px;
    padding: 9px 14px; font-size: 0.9em; cursor: pointer;
  }
  button:active { background: #2f52cc; }
  button.danger { background: #d9455a; }
  button.toggle-off { background: #3a3f4a; }
  .link-ok { color: #4fd979; }
  .link-bad { color: #d9455a; }
  .card h2 .hint { text-transform: none; letter-spacing: normal; color: #5c6270; font-size: 0.9em; }
  .layout { display: grid; grid-template-columns: 1fr 4fr; gap: 16px; align-items: start; }
  @media (max-width: 800px) {
    .layout { grid-template-columns: 1fr; }
  }
  .var-palette { display: flex; flex-wrap: wrap; gap: 8px; margin-bottom: 12px; }
  .var-chip {
    display: flex; align-items: center; gap: 6px;
    background: #14161a; border: 1px solid #2c3038; border-radius: 20px;
    padding: 6px 12px; font-size: 0.8em; cursor: grab; user-select: none;
  }
  .var-chip:active { cursor: grabbing; }
  .var-chip.active { border-color: #565d6b; background: #1a1d24; }
  .var-chip .dot { width: 8px; height: 8px; border-radius: 50%; flex-shrink: 0; }
  .var-chip b { font-weight: 600; }
  .big-chart-wrap { position: relative; border-radius: 8px; overflow: hidden; }
  .big-chart { width: 100%; height: 440px; display: block; background: #14161a; }
  .big-chart-wrap.dragover { outline: 2px dashed #3d6bff; outline-offset: -2px; }
  .empty-hint {
    position: absolute; top: 50%; left: 50%; transform: translate(-50%, -50%);
    color: #5c6270; font-size: 0.9em; pointer-events: none; text-align: center;
  }
  .legend { display: flex; flex-wrap: wrap; gap: 8px; margin-top: 12px; }
  .legend-chip {
    display: flex; align-items: center; gap: 6px;
    background: #14161a; border-radius: 20px; padding: 6px 8px 6px 12px; font-size: 0.8em;
  }
  .legend-chip .dot { width: 8px; height: 8px; border-radius: 50%; flex-shrink: 0; }
  .legend-chip b { font-weight: 600; }
  .legend-chip button {
    background: none; border: none; color: #8a8f98; cursor: pointer;
    padding: 0 4px; font-size: 1.1em; line-height: 1;
  }
  .legend-chip button:hover { color: #eaeaea; }
  .curve-toggle { display: flex; border: 1px solid #2c3038; border-radius: 8px; overflow: hidden; }
  .curve-toggle button {
    background: #14161a; border: none; border-radius: 0; color: #9aa3b2;
    padding: 8px 14px; font-size: 0.85em; cursor: pointer;
  }
  .curve-toggle button.active { background: #3d6bff; color: white; }
  .profile-toolbar { display: flex; flex-wrap: wrap; gap: 10px; align-items: center; margin: 12px 0; }
  .profile-chart-wrap { position: relative; border-radius: 8px; overflow: hidden; }
  .profile-chart { width: 100%; height: 320px; display: block; background: #14161a; cursor: crosshair; touch-action: none; }
  .profile-chart-wrap.running .profile-chart { cursor: default; opacity: 0.85; }
  .profile-status { display: flex; align-items: center; gap: 8px; margin-top: 12px; font-size: 0.85em; color: #9aa3b2; }
  .profile-status .dot { width: 8px; height: 8px; border-radius: 50%; background: #5c6270; }
  .profile-status.running .dot { background: #4fd979; }
  #toast {
    position: fixed; bottom: 16px; left: 50%; transform: translateX(-50%);
    background: #2c3038; padding: 8px 16px; border-radius: 8px; font-size: 0.85em;
    opacity: 0; transition: opacity 0.3s; pointer-events: none;
  }
  #toast.show { opacity: 1; }
</style>
</head>
<body>

<h1>Huawei PSU Control</h1>
<div class="sub">CAN link: <span id="link">--</span></div>

<div class="layout">
  <div class="sidebar">
    <div class="card">
      <h2>Live Telemetry</h2>
      <div class="grid">
        <div class="stat"><div class="v" id="vout">--</div><div class="l">Vout (V)</div></div>
        <div class="stat"><div class="v" id="iout">--</div><div class="l">Iout (A)</div></div>
        <div class="stat"><div class="v" id="pout">--</div><div class="l">Pout (W)</div></div>
        <div class="stat"><div class="v" id="vin">--</div><div class="l">Vin (V)</div></div>
        <div class="stat"><div class="v" id="iin">--</div><div class="l">Iin (A)</div></div>
        <div class="stat"><div class="v" id="pin">--</div><div class="l">Pin (W)</div></div>
        <div class="stat"><div class="v" id="effi">--</div><div class="l">Efficiency (%)</div></div>
        <div class="stat"><div class="v" id="fin">--</div><div class="l">Line freq (Hz)</div></div>
      </div>
    </div>

    <div class="card">
      <h2>Output</h2>
      <div class="row">
        <span id="outState">--</span>
        <button id="outBtn" onclick="toggleOutput()">Toggle</button>
      </div>
    </div>

    <div class="card">
      <h2>Setpoints</h2>
      <div class="row"><label>Online V</label><input type="number" step="0.1" id="onV"><button onclick="setVal('onV','/api/voltage/online')">Set</button></div>
      <div class="row"><label>Offline V</label><input type="number" step="0.1" id="offV"><button onclick="setVal('offV','/api/voltage/offline')">Set</button></div>
      <div class="row"><label>Online I</label><input type="number" step="0.1" id="onI"><button onclick="setVal('onI','/api/current/online')">Set</button></div>
      <div class="row"><label>Offline I</label><input type="number" step="0.1" id="offI"><button onclick="setVal('offI','/api/current/offline')">Set</button></div>
    </div>

    <div class="card">
      <h2>Fan</h2>
      <div class="row">
        <span id="fanState">--</span>
        <button id="fanBtn" onclick="toggleFan()">Toggle Auto/Manual</button>
      </div>
    </div>

    <div class="card">
      <h2>Run Timer</h2>
      <div class="row">
        <label>HH.MM.SS</label>
        <input type="text" id="timerInput" placeholder="00.00.00" style="width:110px">
        <button onclick="setTimer()">Set</button>
      </div>
      <div class="row">
        <label><input type="checkbox" id="timerEnabled" onchange="toggleTimerEnabled()"> Enabled</label>
        <span id="timerRemaining">--</span>
      </div>
    </div>

    <div class="card">
      <h2>Energy</h2>
      <div class="row">
        <span id="energy">-- kWh</span>
        <button class="danger" onclick="resetEnergy()">Reset</button>
      </div>
    </div>
  </div>

  <div class="main">
    <div class="card">
      <h2>Trends <span class="hint">(drag a variable onto the chart, or tap it)</span></h2>
      <div class="var-palette" id="varPalette">
        <div class="var-chip" draggable="true" data-key="vout"><span class="dot" style="background:#4fd979"></span>Vout <b id="chip-vout">--</b></div>
        <div class="var-chip" draggable="true" data-key="iout"><span class="dot" style="background:#3d6bff"></span>Iout <b id="chip-iout">--</b></div>
        <div class="var-chip" draggable="true" data-key="pout"><span class="dot" style="background:#f5a623"></span>Pout <b id="chip-pout">--</b></div>
        <div class="var-chip" draggable="true" data-key="vin"><span class="dot" style="background:#22d3ee"></span>Vin <b id="chip-vin">--</b></div>
        <div class="var-chip" draggable="true" data-key="iin"><span class="dot" style="background:#f472b6"></span>Iin <b id="chip-iin">--</b></div>
        <div class="var-chip" draggable="true" data-key="pin"><span class="dot" style="background:#facc15"></span>Pin <b id="chip-pin">--</b></div>
        <div class="var-chip" draggable="true" data-key="effi"><span class="dot" style="background:#c77dff"></span>Efficiency <b id="chip-effi">--</b></div>
      </div>
      <div class="big-chart-wrap" id="chartDrop">
        <canvas id="bigChart" class="big-chart"></canvas>
        <div class="empty-hint" id="emptyHint">Drag or tap a variable above to plot it</div>
      </div>
      <div class="legend" id="legend"></div>
    </div>
  </div>
</div>

<div class="card">
  <h2>Output Profile <span class="hint">(schedule voltage/current over time)</span></h2>
  <div class="row">
    <label>Duration</label>
    <input type="text" id="profDuration" placeholder="01.00.00" style="width:110px">
    <button onclick="setProfileDuration()">Set</button>
  </div>
  <div class="profile-toolbar">
    <div class="curve-toggle">
      <button id="curveVoltBtn" class="active" onclick="setActiveCurve('voltage')">Voltage</button>
      <button id="curveCurrBtn" onclick="setActiveCurve('current')">Current</button>
    </div>
    <button onclick="clearActiveCurve()">Clear points</button>
    <button onclick="saveProfile()">Save</button>
    <button id="profRunBtn" class="danger" onclick="toggleProfileRun()">Run</button>
  </div>
  <div class="sub" style="margin-bottom:8px;">Click the chart to add a point on the active curve. Drag a point to move it. Double-click a point to delete it.</div>
  <div class="profile-chart-wrap" id="profChartWrap">
    <canvas id="profChart" class="profile-chart"></canvas>
  </div>
  <div class="profile-status" id="profStatus">
    <span class="dot"></span>
    <span id="profStatusText">Stopped</span>
  </div>
</div>

<div id="toast"></div>

<script>
let outputOn = false;
let fanManual = false;

function toast(msg) {
  const t = document.getElementById('toast');
  t.textContent = msg;
  t.classList.add('show');
  clearTimeout(t._h);
  t._h = setTimeout(() => t.classList.remove('show'), 2000);
}

async function api(path, body) {
  try {
    const opts = { method: body ? 'POST' : 'GET' };
    if (body) {
      opts.headers = { 'Content-Type': 'application/json' };
      opts.body = JSON.stringify(body);
    }
    const r = await fetch(path, opts);
    if (!r.ok) { toast('Error: ' + r.status); return null; }
    return await r.json();
  } catch (e) {
    toast('Request failed');
    return null;
  }
}

function fmt(v, digits) {
  if (v === null || v === undefined) return '--';
  return Number(v).toFixed(digits);
}

/* ---------------------------------------------------------------------
   Trends -- one big canvas, plain JS (no charting library -- this page
   has to be fully self-contained, no CDN). Every variable is buffered in
   the background all the time (cheap -- 150 samples x 7 variables), but
   only the ones in activeSeries are drawn, so dragging a variable onto
   the chart immediately backfills up to 5 minutes of history instead of
   starting from empty. Each series is scaled to its own min/max within
   the visible window -- that's the only sane way to plot volts, amps,
   watts and a percentage on one shared canvas; the legend/chip values are
   what carries the real numbers. A null sample (CAN link down) breaks
   that series' line instead of plotting a false zero.
   --------------------------------------------------------------------- */
const CHART_MAX_POINTS = 600; // ~5 min at the 500ms poll interval below

const SERIES_META = {
  vout: { label: 'Vout (V)',       color: '#4fd979', digits: 2 },
  iout: { label: 'Iout (A)',       color: '#3d6bff', digits: 2 },
  pout: { label: 'Pout (W)',       color: '#f5a623', digits: 2 },
  vin:  { label: 'Vin (V)',        color: '#22d3ee', digits: 2 },
  iin:  { label: 'Iin (A)',        color: '#f472b6', digits: 2 },
  pin:  { label: 'Pin (W)',        color: '#facc15', digits: 2 },
  effi: { label: 'Efficiency (%)', color: '#c77dff', digits: 1 },
};

const histories = {};
Object.keys(SERIES_META).forEach((k) => { histories[k] = []; });

let activeSeries = ['vout', 'pout']; // sensible default so the chart isn't empty on first load

function pushSample(key, v) {
  const num = (v === null || v === undefined || isNaN(v)) ? null : Number(v);
  const arr = histories[key];
  arr.push(num);
  if (arr.length > CHART_MAX_POINTS) arr.shift();
}

const bigCanvas = document.getElementById('bigChart');
const bigCtx = bigCanvas.getContext('2d');

function renderBigChart() {
  const dpr = window.devicePixelRatio || 1;
  const w = bigCanvas.clientWidth, h = bigCanvas.clientHeight;
  if (w === 0 || h === 0) return;

  const pw = Math.round(w * dpr), ph = Math.round(h * dpr);
  if (bigCanvas.width !== pw || bigCanvas.height !== ph) {
    bigCanvas.width = pw;
    bigCanvas.height = ph;
  }
  bigCtx.setTransform(dpr, 0, 0, dpr, 0, 0);
  bigCtx.clearRect(0, 0, w, h);

  bigCtx.strokeStyle = '#22252c';
  bigCtx.lineWidth = 1;
  for (let i = 1; i < 4; i++) {
    const y = Math.round((h / 4) * i) + 0.5;
    bigCtx.beginPath();
    bigCtx.moveTo(0, y);
    bigCtx.lineTo(w, y);
    bigCtx.stroke();
  }

  const n = CHART_MAX_POINTS;
  const stepX = w / (n - 1);

  activeSeries.forEach((key) => {
    const data = histories[key];
    const vals = data.filter((v) => v !== null);
    if (vals.length < 2) return;

    let min = Math.min(...vals), max = Math.max(...vals);
    if (min === max) { min -= 1; max += 1; }
    const pad = (max - min) * 0.1;
    min -= pad; max += pad;

    const offset = n - data.length;
    bigCtx.beginPath();
    bigCtx.lineWidth = 2;
    bigCtx.strokeStyle = SERIES_META[key].color;
    let started = false;

    data.forEach((v, i) => {
      const x = (offset + i) * stepX;
      if (v === null) { started = false; return; }
      const y = h - ((v - min) / (max - min)) * h;
      if (!started) { bigCtx.moveTo(x, y); started = true; }
      else bigCtx.lineTo(x, y);
    });
    bigCtx.stroke();
  });
}

function syncTrendUI() {
  document.querySelectorAll('.var-chip').forEach((chip) => {
    chip.classList.toggle('active', activeSeries.includes(chip.dataset.key));
  });

  const legend = document.getElementById('legend');
  legend.innerHTML = '';
  activeSeries.forEach((key) => {
    const meta = SERIES_META[key];
    const chip = document.createElement('div');
    chip.className = 'legend-chip';

    const dot = document.createElement('span');
    dot.className = 'dot';
    dot.style.background = meta.color;

    const label = document.createElement('span');
    label.textContent = meta.label + ' ';

    const val = document.createElement('b');
    val.id = 'legend-val-' + key;
    val.textContent = '--';

    const btn = document.createElement('button');
    btn.textContent = '×'; // ×
    btn.setAttribute('aria-label', 'Remove ' + meta.label);
    btn.addEventListener('click', () => removeSeries(key));

    chip.appendChild(dot);
    chip.appendChild(label);
    chip.appendChild(val);
    chip.appendChild(btn);
    legend.appendChild(chip);
  });

  document.getElementById('emptyHint').style.display = activeSeries.length ? 'none' : 'block';
  renderBigChart();
}

function addSeries(key) {
  if (!SERIES_META[key] || activeSeries.includes(key)) return;
  activeSeries.push(key);
  syncTrendUI();
}

function removeSeries(key) {
  const idx = activeSeries.indexOf(key);
  if (idx < 0) return;
  activeSeries.splice(idx, 1);
  syncTrendUI();
}

function toggleSeries(key) {
  if (activeSeries.includes(key)) removeSeries(key);
  else addSeries(key);
}

document.querySelectorAll('.var-chip').forEach((chip) => {
  chip.addEventListener('click', () => toggleSeries(chip.dataset.key));
  chip.addEventListener('dragstart', (e) => {
    e.dataTransfer.setData('text/plain', chip.dataset.key);
    e.dataTransfer.effectAllowed = 'copy';
  });
});

const chartDrop = document.getElementById('chartDrop');
chartDrop.addEventListener('dragover', (e) => {
  e.preventDefault();
  chartDrop.classList.add('dragover');
});
chartDrop.addEventListener('dragleave', () => chartDrop.classList.remove('dragover'));
chartDrop.addEventListener('drop', (e) => {
  e.preventDefault();
  chartDrop.classList.remove('dragover');
  addSeries(e.dataTransfer.getData('text/plain'));
});

window.addEventListener('resize', renderBigChart);

syncTrendUI(); // initial paint with the default active series

// Guards against overlapping polls piling up if a request is slow (weak
// WiFi, etc.) -- skip a tick rather than firing a second fetch on top of
// one still in flight.
let refreshInFlight = false;

async function refresh() {
  if (refreshInFlight) return;
  refreshInFlight = true;
  try {
    await doRefresh();
  } finally {
    refreshInFlight = false;
  }
}

async function doRefresh() {
  const s = await api('/api/status');
  if (!s) return;

  document.getElementById('link').textContent = s.link_ok ? 'OK' : 'LOST';
  document.getElementById('link').className = s.link_ok ? 'link-ok' : 'link-bad';

  document.getElementById('vout').textContent = fmt(s.vout, 2);
  document.getElementById('iout').textContent = fmt(s.iout, 2);
  document.getElementById('pout').textContent = fmt(s.pout, 2);
  document.getElementById('vin').textContent = fmt(s.vin, 2);
  document.getElementById('iin').textContent = fmt(s.iin, 2);
  document.getElementById('pin').textContent = fmt(s.pin, 2);
  document.getElementById('effi').textContent = fmt(s.efficiency, 1);
  document.getElementById('fin').textContent = fmt(s.freq, 1);

  pushSample('vout', s.vout);
  pushSample('iout', s.iout);
  pushSample('pout', s.pout);
  pushSample('vin', s.vin);
  pushSample('iin', s.iin);
  pushSample('pin', s.pin);
  pushSample('effi', s.efficiency);

  Object.keys(SERIES_META).forEach((key) => {
    const data = histories[key];
    const last = data.length ? data[data.length - 1] : null;
    const txt = (last === null) ? '--' : fmt(last, SERIES_META[key].digits);
    const chipVal = document.getElementById('chip-' + key);
    if (chipVal) chipVal.textContent = txt;
    const legendVal = document.getElementById('legend-val-' + key);
    if (legendVal) legendVal.textContent = txt;
  });
  renderBigChart();

  outputOn = s.output_on;
  document.getElementById('outState').textContent = outputOn ? 'ON' : 'OFF';
  document.getElementById('outBtn').className = outputOn ? '' : 'toggle-off';

  fanManual = s.fan_manual;
  document.getElementById('fanState').textContent = fanManual ? 'MANUAL' : 'AUTO';

  document.getElementById('energy').textContent = fmt(s.energy_kwh, 4) + ' kWh';

  document.getElementById('timerRemaining').textContent = s.timer_remaining || '--';
  document.getElementById('timerEnabled').checked = !!s.timer_enabled;

  // Don't stomp on values the user is actively editing.
  if (document.activeElement.id !== 'onV') document.getElementById('onV').value = fmt(s.online_v, 2);
  if (document.activeElement.id !== 'offV') document.getElementById('offV').value = fmt(s.offline_v, 2);
  if (document.activeElement.id !== 'onI') document.getElementById('onI').value = fmt(s.online_i, 2);
  if (document.activeElement.id !== 'offI') document.getElementById('offI').value = fmt(s.offline_i, 2);
  if (document.activeElement.id !== 'timerInput') document.getElementById('timerInput').value = s.timer_set || '';

  // Running/elapsed only -- never the duration or point arrays, so this
  // fast poll can't clobber an edit in progress. The full definition only
  // (re)loads via loadProfile(), on page init and after an explicit save.
  if (s.profile_running !== undefined) {
    profileRunning = s.profile_running;
    profileElapsed = s.profile_elapsed_sec;
    updateProfileRunUI();
  }
}

async function toggleOutput() {
  const r = await api('/api/output', { on: !outputOn });
  if (r) toast('Output ' + (r.output_on ? 'ENABLED' : 'DISABLED'));
  refresh();
}

async function toggleFan() {
  const r = await api('/api/fan', { manual: !fanManual });
  if (r) toast('Fan ' + (r.fan_manual ? 'MANUAL' : 'AUTO'));
  refresh();
}

async function setVal(id, path) {
  const val = parseFloat(document.getElementById(id).value);
  if (isNaN(val)) { toast('Enter a number'); return; }
  const r = await api(path, { value: val });
  if (r) toast('Set to ' + fmt(r.value, 2));
  refresh();
}

async function setTimer() {
  const txt = document.getElementById('timerInput').value.trim();
  const r = await api('/api/timer', { hms: txt });
  if (r && r.ok) toast('Timer set');
  else toast('Invalid format, use HH.MM.SS');
  refresh();
}

async function toggleTimerEnabled() {
  const enabled = document.getElementById('timerEnabled').checked;
  await api('/api/timer/enabled', { enabled: enabled });
  refresh();
}

async function resetEnergy() {
  await api('/api/energy/reset', {});
  toast('Energy counter reset');
  refresh();
}

/* ---------------------------------------------------------------------
   Output Profile -- a scheduled voltage/current curve, edited here and
   executed live on the device (psu_profile.cpp). Two independent point
   arrays (voltage, current), each an ordered list of {t, v}. Only the
   "active" curve is editable at a time -- a single click position is
   otherwise ambiguous between "volts" and "amps". The inactive curve
   still draws, dimmed, for context.

   HH.MM.SS matches the existing Run Timer field's format for consistency.
   Point values/limits are mirrored from HuaweiCAN.h here only for local
   axis scaling and input clamping -- the device re-clamps authoritatively
   in psu_profile_set(), so a mismatch here would only be a display
   quirk, never a safety issue.
   --------------------------------------------------------------------- */
const CURVE_LIMITS = {
  voltage: { min: 41.5, max: 58.5, color: '#4fd979' },
  current: { min: 0, max: 75, color: '#3d6bff' },
};
const PROFILE_POINT_HIT_RADIUS = 10; // px

let profileDuration = 3600; // seconds
let voltagePoints = [];     // [{t, v}], t in seconds
let currentPoints = [];
let activeCurve = 'voltage';
let profileRunning = false;
let profileElapsed = 0;

function formatHMS(totalSec) {
  totalSec = Math.max(0, Math.round(totalSec));
  const h = Math.floor(totalSec / 3600);
  const m = Math.floor((totalSec % 3600) / 60);
  const s = totalSec % 60;
  const pad = (n) => String(n).padStart(2, '0');
  return pad(h) + '.' + pad(m) + '.' + pad(s);
}

function parseHMS(str) {
  const parts = str.trim().split('.');
  if (parts.length !== 3) return null;
  const h = parseInt(parts[0], 10), m = parseInt(parts[1], 10), s = parseInt(parts[2], 10);
  if (!Number.isFinite(h) || !Number.isFinite(m) || !Number.isFinite(s)) return null;
  if (h < 0 || m < 0 || m >= 60 || s < 0 || s >= 60) return null;
  return h * 3600 + m * 60 + s;
}

function curvePoints(curve) {
  return curve === 'voltage' ? voltagePoints : currentPoints;
}

// The chart reserves margins for axis labels -- everything below maps
// time/value to/from a "plot rect" inside the canvas rather than the full
// canvas, so labels never overlap the plotted lines/points.
const CHART_MARGIN = { left: 50, right: 8, top: 8, bottom: 20 };

function getPlotRect(w, h) {
  return {
    x: CHART_MARGIN.left,
    y: CHART_MARGIN.top,
    w: Math.max(1, w - CHART_MARGIN.left - CHART_MARGIN.right),
    h: Math.max(1, h - CHART_MARGIN.top - CHART_MARGIN.bottom),
  };
}

function timeToX(t, rect) {
  return rect.x + (t / profileDuration) * rect.w;
}
function xToTime(x, rect) {
  return Math.max(0, Math.min(profileDuration, Math.round(((x - rect.x) / rect.w) * profileDuration)));
}

// Active curve's value axis auto-fits to its own points (padded) so small
// variations stay readable -- points are already clamped to the curve's
// hardware limits when added/dragged, so this never needs to intersect
// with CURVE_LIMITS again.
function getCurveRange(curve) {
  const lim = CURVE_LIMITS[curve];
  const pts = curvePoints(curve);
  if (pts.length === 0) return { min: lim.min, max: lim.max };
  const vals = pts.map((p) => p.v);
  let dmin = Math.min(...vals), dmax = Math.max(...vals);
  if (dmin === dmax) { dmin -= 1; dmax += 1; }
  const pad = (dmax - dmin) * 0.15;
  return { min: dmin - pad, max: dmax + pad };
}

function valueToY(v, range, rect) {
  return rect.y + rect.h - ((v - range.min) / (range.max - range.min)) * rect.h;
}
function yToValue(y, range, rect) {
  return range.min + ((rect.y + rect.h - y) / rect.h) * (range.max - range.min);
}

function curveUnit(curve) {
  return curve === 'voltage' ? 'V' : 'A';
}

const profCanvas = document.getElementById('profChart');
const profCtx = profCanvas.getContext('2d');

function drawProfileCurve(curve, active, rect) {
  const pts = curvePoints(curve);
  if (pts.length === 0) return;

  const range = getCurveRange(curve);
  profCtx.globalAlpha = active ? 1 : 0.3;
  profCtx.strokeStyle = CURVE_LIMITS[curve].color;
  profCtx.lineWidth = 2;
  profCtx.beginPath();
  pts.forEach((p, idx) => {
    const x = timeToX(p.t, rect), y = valueToY(p.v, range, rect);
    if (idx === 0) profCtx.moveTo(x, y);
    else profCtx.lineTo(x, y);
  });
  profCtx.stroke();

  if (active) {
    profCtx.fillStyle = CURVE_LIMITS[curve].color;
    pts.forEach((p) => {
      const x = timeToX(p.t, rect), y = valueToY(p.v, range, rect);
      profCtx.beginPath();
      profCtx.arc(x, y, 5, 0, Math.PI * 2);
      profCtx.fill();
    });

    // Coordinate label next to each point -- "value unit, HH.MM.SS" --
    // so a point's exact position is readable at a glance instead of
    // having to guess it off the axes.
    profCtx.font = '10px sans-serif';
    pts.forEach((p) => {
      const x = timeToX(p.t, rect), y = valueToY(p.v, range, rect);
      const label = p.v.toFixed(2) + curveUnit(curve) + ', ' + formatHMS(p.t);
      const tw = profCtx.measureText(label).width;

      let lx = x - tw / 2;
      lx = Math.max(rect.x, Math.min(rect.x + rect.w - tw, lx));
      let ly = y - 10; // above the point by default
      if (ly < rect.y + 8) ly = y + 18; // flip below if too close to the top

      profCtx.fillStyle = 'rgba(20, 22, 26, 0.85)';
      profCtx.fillRect(lx - 3, ly - 10, tw + 6, 13);
      profCtx.fillStyle = CURVE_LIMITS[curve].color;
      profCtx.fillText(label, lx, ly);
    });
  }
  profCtx.globalAlpha = 1;
}

function renderProfileChart() {
  const dpr = window.devicePixelRatio || 1;
  const w = profCanvas.clientWidth, h = profCanvas.clientHeight;
  if (w === 0 || h === 0) return;

  const pw = Math.round(w * dpr), ph = Math.round(h * dpr);
  if (profCanvas.width !== pw || profCanvas.height !== ph) {
    profCanvas.width = pw;
    profCanvas.height = ph;
  }
  profCtx.setTransform(dpr, 0, 0, dpr, 0, 0);
  profCtx.clearRect(0, 0, w, h);

  const rect = getPlotRect(w, h);
  const range = getCurveRange(activeCurve);
  const gridCount = 5;

  profCtx.font = '10px sans-serif';

  // Y-axis (active curve's value scale) -- horizontal gridlines + labels
  // in the left margin.
  for (let i = 0; i <= gridCount; i++) {
    const y = rect.y + (rect.h / gridCount) * i;
    const v = range.max - ((range.max - range.min) / gridCount) * i;

    profCtx.strokeStyle = '#22252c';
    profCtx.lineWidth = 1;
    profCtx.beginPath();
    profCtx.moveTo(rect.x, y);
    profCtx.lineTo(rect.x + rect.w, y);
    profCtx.stroke();

    profCtx.fillStyle = '#5c6270';
    const label = v.toFixed(1);
    const tw = profCtx.measureText(label).width;
    profCtx.fillText(label, rect.x - tw - 6, y + 3);
  }

  // X-axis (time) -- vertical gridlines + labels along the bottom margin.
  for (let i = 0; i <= gridCount; i++) {
    const x = rect.x + (rect.w / gridCount) * i;
    profCtx.strokeStyle = '#22252c';
    profCtx.lineWidth = 1;
    profCtx.beginPath();
    profCtx.moveTo(x, rect.y);
    profCtx.lineTo(x, rect.y + rect.h);
    profCtx.stroke();

    profCtx.fillStyle = '#5c6270';
    const label = formatHMS(Math.round((profileDuration / gridCount) * i));
    const tw = profCtx.measureText(label).width;
    let lx = x - tw / 2;
    if (i === 0) lx = x;
    if (i === gridCount) lx = x - tw;
    profCtx.fillText(label, lx, h - 4);
  }

  const inactive = activeCurve === 'voltage' ? 'current' : 'voltage';
  drawProfileCurve(inactive, false, rect);
  drawProfileCurve(activeCurve, true, rect);

  if (profileElapsed > 0) {
    const x = timeToX(profileElapsed, rect);
    profCtx.strokeStyle = '#eaeaea';
    profCtx.setLineDash([4, 3]);
    profCtx.beginPath();
    profCtx.moveTo(x, rect.y);
    profCtx.lineTo(x, rect.y + rect.h);
    profCtx.stroke();
    profCtx.setLineDash([]);
  }
}

function getCanvasPos(e) {
  const rect = profCanvas.getBoundingClientRect();
  return { x: e.clientX - rect.left, y: e.clientY - rect.top };
}

function findNearestProfilePoint(curve, x, y, rect) {
  const pts = curvePoints(curve);
  const range = getCurveRange(curve);
  let best = -1, bestDist = Infinity;
  pts.forEach((p, idx) => {
    const d = Math.hypot(timeToX(p.t, rect) - x, valueToY(p.v, range, rect) - y);
    if (d < bestDist) { bestDist = d; best = idx; }
  });
  return bestDist <= PROFILE_POINT_HIT_RADIUS ? best : -1;
}

let dragPoint = null;   // { curve, index }
let dragMoved = false;
let suppressNextClick = false;

profCanvas.addEventListener('pointerdown', (e) => {
  if (profileRunning) return;
  const rect = getPlotRect(profCanvas.clientWidth, profCanvas.clientHeight);
  const { x, y } = getCanvasPos(e);
  const idx = findNearestProfilePoint(activeCurve, x, y, rect);
  if (idx >= 0) {
    dragPoint = { curve: activeCurve, index: idx };
    dragMoved = false;
    profCanvas.setPointerCapture(e.pointerId);
  }
});

profCanvas.addEventListener('pointermove', (e) => {
  if (!dragPoint || profileRunning) return;
  const rect = getPlotRect(profCanvas.clientWidth, profCanvas.clientHeight);
  const { x, y } = getCanvasPos(e);
  const lim = CURVE_LIMITS[dragPoint.curve];
  const range = getCurveRange(dragPoint.curve);

  const t = xToTime(Math.max(rect.x, Math.min(rect.x + rect.w, x)), rect);
  let v = yToValue(Math.max(rect.y, Math.min(rect.y + rect.h, y)), range, rect);
  v = Math.max(lim.min, Math.min(lim.max, v));

  curvePoints(dragPoint.curve)[dragPoint.index] = { t, v };
  dragMoved = true;
  renderProfileChart();
});

profCanvas.addEventListener('pointerup', (e) => {
  if (!dragPoint) return;
  curvePoints(dragPoint.curve).sort((a, b) => a.t - b.t);
  if (dragMoved) suppressNextClick = true;
  dragPoint = null;
  renderProfileChart();
});

profCanvas.addEventListener('click', (e) => {
  if (suppressNextClick) { suppressNextClick = false; return; }
  if (profileRunning) return;

  const rect = getPlotRect(profCanvas.clientWidth, profCanvas.clientHeight);
  const { x, y } = getCanvasPos(e);

  // Clicking on/near an existing point does nothing here -- drag to move
  // it, double-click to delete it. Only an empty spot adds a new point.
  if (findNearestProfilePoint(activeCurve, x, y, rect) >= 0) return;

  const lim = CURVE_LIMITS[activeCurve];
  const range = getCurveRange(activeCurve);
  const t = xToTime(x, rect);
  let v = yToValue(y, range, rect);
  v = Math.max(lim.min, Math.min(lim.max, v));

  const pts = curvePoints(activeCurve);
  pts.push({ t, v });
  pts.sort((a, b) => a.t - b.t);
  renderProfileChart();
});

profCanvas.addEventListener('dblclick', (e) => {
  if (profileRunning) return;
  const rect = getPlotRect(profCanvas.clientWidth, profCanvas.clientHeight);
  const { x, y } = getCanvasPos(e);
  const idx = findNearestProfilePoint(activeCurve, x, y, rect);
  if (idx >= 0) {
    curvePoints(activeCurve).splice(idx, 1);
    renderProfileChart();
  }
});

window.addEventListener('resize', renderProfileChart);

function setActiveCurve(curve) {
  activeCurve = curve;
  document.getElementById('curveVoltBtn').classList.toggle('active', curve === 'voltage');
  document.getElementById('curveCurrBtn').classList.toggle('active', curve === 'current');
  renderProfileChart();
}

function clearActiveCurve() {
  if (profileRunning) { toast('Stop the profile first'); return; }
  if (activeCurve === 'voltage') voltagePoints = [];
  else currentPoints = [];
  renderProfileChart();
}

function setProfileDuration() {
  const secs = parseHMS(document.getElementById('profDuration').value);
  if (secs === null || secs <= 0) { toast('Invalid format, use HH.MM.SS'); return; }
  profileDuration = secs;
  voltagePoints.forEach((p) => { if (p.t > profileDuration) p.t = profileDuration; });
  currentPoints.forEach((p) => { if (p.t > profileDuration) p.t = profileDuration; });
  renderProfileChart();
}

async function saveProfile() {
  if (profileRunning) { toast('Stop the profile first'); return; }
  const body = {
    duration_sec: profileDuration,
    voltage_points: voltagePoints,
    current_points: currentPoints,
  };
  const r = await api('/api/profile', body);
  if (r && r.ok) toast('Profile saved');
}

function updateProfileRunUI() {
  document.getElementById('profChartWrap').classList.toggle('running', profileRunning);
  document.getElementById('profRunBtn').textContent = profileRunning ? 'Stop' : 'Run';
  document.getElementById('profRunBtn').className = profileRunning ? '' : 'danger';
  document.getElementById('profStatus').classList.toggle('running', profileRunning);
  document.getElementById('profStatusText').textContent = profileRunning
    ? 'Running -- ' + formatHMS(profileElapsed) + ' / ' + formatHMS(profileDuration)
    : 'Stopped';
  renderProfileChart();
}

async function toggleProfileRun() {
  const r = await api('/api/profile/run', { run: !profileRunning });
  if (!r) return;
  if (!r.ok) { toast('Nothing to run -- add some points first'); return; }
  profileRunning = r.running;
  profileElapsed = r.elapsed_sec;
  updateProfileRunUI();
}

async function loadProfile() {
  const r = await api('/api/profile');
  if (!r) return;
  profileDuration = r.duration_sec > 0 ? r.duration_sec : 3600;
  voltagePoints = r.voltage_points || [];
  currentPoints = r.current_points || [];
  profileRunning = r.running;
  profileElapsed = r.elapsed_sec;
  document.getElementById('profDuration').value = formatHMS(profileDuration);
  updateProfileRunUI();
}

// Enter submits the same as pressing the adjacent "Set" button, and blurs
// the field afterward so a mobile on-screen keyboard closes.
function onEnter(id, fn) {
  document.getElementById(id).addEventListener('keydown', (e) => {
    if (e.key !== 'Enter') return;
    e.preventDefault();
    document.getElementById(id).blur();
    fn();
  });
}

onEnter('onV', () => setVal('onV', '/api/voltage/online'));
onEnter('offV', () => setVal('offV', '/api/voltage/offline'));
onEnter('onI', () => setVal('onI', '/api/current/online'));
onEnter('offI', () => setVal('offI', '/api/current/offline'));
onEnter('timerInput', setTimer);
onEnter('profDuration', setProfileDuration);

loadProfile();
refresh();
setInterval(refresh, 500); // device-side telemetry itself refreshes ~every 200ms (can_bridge.cpp)
</script>
</body>
</html>
)HTMLPAGE";

#endif
