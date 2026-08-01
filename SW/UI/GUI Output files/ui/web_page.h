#ifndef WEB_PAGE_H
#define WEB_PAGE_H

#include <Arduino.h>   // PROGMEM

/* Single self-contained page (no CDN/external assets -- this has to work
   on a LAN with no internet access). Polls /api/status every 2s and posts
   to /api/... on user action. Served from web_server.cpp. */
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

async function refresh() {
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

refresh();
setInterval(refresh, 2000);
</script>
</body>
</html>
)HTMLPAGE";

#endif
