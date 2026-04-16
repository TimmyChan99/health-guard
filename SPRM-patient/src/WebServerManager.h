#pragma once
#include <WebServer.h> 
#include "Config.h"
#include "AlertService.h"

inline WebServer server(80);

// Thresholds
inline float tempMin = 20.0;
inline float tempMax = 35.0;

inline int bpSys = 120;   // systolic
inline int bpDia = 80;    // diastolic

extern long mqttIntervalSeconds;

// ================= HTML UI =================
const char page[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0" />
  <meta name="theme-color" content="#0d5d8e" />
  <title>Patient Monitor</title>
  <style>
    @import url('https://fonts.googleapis.com/css2?family=Montserrat:wght@500;600;700;800&display=swap');

    :root {
      --bg-top: #f8fdff;
      --bg-bottom: #edf7fb;
      --card: rgba(255, 255, 255, 0.88);
      --card-border: rgba(64, 126, 168, 0.14);
      --ink: #12314a;
      --muted: #5f7b91;
      --deep-blue: #0d5d8e;
      --teal: #0e9bb5;
      --teal-soft: #25b9c9;
      --field: #f1f9ff;
      --field-border: #b7d9ef;
      --success: #16c47c;
      --danger: #e34b5f;
      --danger-deep: #c92f48;
      --shadow: 0 20px 45px rgba(13, 71, 109, 0.12), 0 6px 18px rgba(13, 71, 109, 0.06);
      --ease: cubic-bezier(.2, .8, .2, 1);
      --radius-xl: 28px;
      --radius-lg: 22px;
      --radius-md: 18px;
    }

    * { box-sizing: border-box; }
    html, body { min-height: 100%; }
    body {
      margin: 0;
      font-family: "Montserrat", sans-serif;
      color: var(--ink);
      background:
        radial-gradient(circle at 12% 8%, rgba(14, 155, 181, 0.16), transparent 24%),
        radial-gradient(circle at 88% 18%, rgba(37, 185, 201, 0.10), transparent 22%),
        radial-gradient(circle at 82% 92%, rgba(227, 75, 95, 0.08), transparent 20%),
        linear-gradient(180deg, var(--bg-top) 0%, var(--bg-bottom) 100%);
      overflow-x: hidden;
    }
    body::before {
      content: "";
      position: fixed;
      inset: 0;
      pointer-events: none;
      background-image: linear-gradient(rgba(18, 49, 74, 0.025) 1px, transparent 1px), linear-gradient(90deg, rgba(18, 49, 74, 0.025) 1px, transparent 1px);
      background-size: 58px 58px;
      mask-image: radial-gradient(circle at center, rgba(0,0,0,0.75), transparent 85%);
      opacity: 0.55;
    }

    input, button { font: inherit; -webkit-tap-highlight-color: transparent; }

    .dashboard {
      width: min(calc(100% - 1rem), 1040px);
      margin: 0 auto;
      padding: calc(1rem + env(safe-area-inset-top)) 0 calc(1.25rem + env(safe-area-inset-bottom));
      position: relative;
      z-index: 1;
    }

    .hero-panel {
      position: relative;
      overflow: hidden;
      text-align: center;
      padding: 1.4rem 1rem 1rem;
      margin-bottom: 1rem;
      border-radius: var(--radius-xl);
      background: linear-gradient(180deg, rgba(255,255,255,0.92), rgba(244,251,255,0.78));
      border: 1px solid rgba(105, 160, 198, 0.16);
      backdrop-filter: blur(18px) saturate(120%);
      -webkit-backdrop-filter: blur(18px) saturate(120%);
      box-shadow: var(--shadow);
      animation: floatIn .7s var(--ease) both;
    }

    .hero-panel::before {
      content: "";
      position: absolute;
      inset: 0;
      background: radial-gradient(circle at 18% 18%, rgba(14, 155, 181, 0.10), transparent 24%), radial-gradient(circle at 82% 20%, rgba(227, 75, 95, 0.08), transparent 22%);
      pointer-events: none;
    }

    .brand-mark {
      width: 82px;
      height: 82px;
      margin: 0 auto 0.95rem;
      border-radius: 26px;
      display: grid;
      place-items: center;
      color: var(--danger);
      background: linear-gradient(180deg, rgba(255,255,255,0.98), rgba(244,249,255,0.86));
      border: 1px solid rgba(227, 75, 95, 0.12);
      box-shadow: 0 18px 36px rgba(227, 75, 95, 0.10), inset 0 1px 0 rgba(255,255,255,0.95);
      position: relative;
    }

    .brand-mark::after {
      content: "";
      position: absolute;
      inset: 10px;
      border-radius: 20px;
      background: radial-gradient(circle, rgba(227, 75, 95, 0.08), transparent 70%);
      z-index: 0;
    }

    .brand-mark svg { width: 38px; height: 38px; position: relative; z-index: 1; fill: currentColor; }

    h1 { margin: 0; font-size: clamp(1.8rem, 5vw, 2.75rem); line-height: 1; letter-spacing: -0.05em; font-weight: 800; }
    .hero-subtitle { margin: 0.5rem auto 0; max-width: 32rem; color: var(--muted); font-size: 0.92rem; line-height: 1.55; font-weight: 600; }

    .header-meta { margin-top: 0.95rem; display: flex; justify-content: center; flex-wrap: wrap; gap: 0.65rem; }

    .status-pill, .ip-chip {
      display: inline-flex;
      align-items: center;
      gap: 0.55rem;
      padding: 0.7rem 0.9rem;
      border-radius: 999px;
      font-size: 0.78rem;
      font-weight: 700;
      backdrop-filter: blur(10px);
      -webkit-backdrop-filter: blur(10px);
    }

    .status-pill { color: #0a8760; background: rgba(236, 255, 248, 0.88); border: 1px solid rgba(22, 196, 124, 0.18); box-shadow: inset 0 1px 0 rgba(255,255,255,0.85); }
    .status-dot { width: 10px; height: 10px; border-radius: 50%; background: var(--success); box-shadow: 0 0 0 0 rgba(22, 196, 124, 0.45); animation: pulse 2s infinite; flex: 0 0 auto; }
    .ip-chip { color: var(--deep-blue); background: rgba(255,255,255,0.82); border: 1px solid rgba(13, 93, 142, 0.12); font-family: ui-monospace, SFMono-Regular, Menlo, monospace; letter-spacing: 0.02em; }

    .ecg-trace { height: 52px; margin-top: 1rem; opacity: 0.55; }
    .ecg-trace svg { width: 100%; height: 100%; display: block; }
    .ecg-trace path { fill: none; stroke: rgba(14, 155, 181, 0.5); stroke-width: 2.5; stroke-linecap: round; stroke-linejoin: round; stroke-dasharray: 620; stroke-dashoffset: 620; animation: drawTrace 3.6s ease forwards 0.35s; }

    .cards-grid { display: grid; gap: 1rem; }

    .card {
      position: relative;
      overflow: hidden;
      padding: 1.15rem;
      border-radius: var(--radius-lg);
      background: linear-gradient(180deg, rgba(255,255,255,0.92), rgba(247,252,255,0.82));
      border: 1px solid var(--card-border);
      backdrop-filter: blur(16px) saturate(120%);
      -webkit-backdrop-filter: blur(16px) saturate(120%);
      box-shadow: var(--shadow);
      animation: floatIn .7s var(--ease) both;
      animation-delay: var(--delay, 0s);
      transition: transform .2s ease, box-shadow .2s ease;
    }

    .card:hover { transform: translateY(-2px); box-shadow: 0 24px 50px rgba(13, 71, 109, 0.14), 0 8px 18px rgba(13, 71, 109, 0.08); }
    .card::before { content: ""; position: absolute; inset: 0 0 auto; height: 1px; background: linear-gradient(90deg, transparent, rgba(255,255,255,0.95), transparent); pointer-events: none; }

    .temp-card { background: linear-gradient(180deg, rgba(255,255,255,0.94), rgba(242,250,255,0.88)); }
    .bp-card { background: linear-gradient(180deg, rgba(255,255,255,0.94), rgba(244,249,255,0.88)); }
    .sync-card { background: linear-gradient(180deg, rgba(255,255,255,0.94), rgba(242,255,252,0.88)); }
    .emergency { background: linear-gradient(180deg, rgba(255,255,255,0.95), rgba(255,244,246,0.90)); border-color: rgba(227, 75, 95, 0.18); }

    .card-head { display: flex; align-items: flex-start; gap: 0.85rem; margin-bottom: 1rem; }

    .icon-orb { width: 48px; height: 48px; border-radius: 16px; display: grid; place-items: center; flex: 0 0 auto; box-shadow: inset 0 1px 0 rgba(255,255,255,0.9); }
    .icon-orb svg { width: 26px; height: 26px; stroke: currentColor; stroke-width: 1.85; stroke-linecap: round; stroke-linejoin: round; fill: none; }
    .icon-orb.temp { color: #168fb3; background: linear-gradient(180deg, #e8f8ff, #f8fdff); }
    .icon-orb.bp { color: #246bcb; background: linear-gradient(180deg, #edf6ff, #f9fbff); }
    .icon-orb.sync { color: #14997f; background: linear-gradient(180deg, #ecfff9, #fbfffe); }
    .icon-orb.alert { color: var(--danger); background: linear-gradient(180deg, #fff0f3, #fff9fa); }

    .card h2 { margin: 0; font-size: 1rem; line-height: 1.2; letter-spacing: -0.02em; font-weight: 800; }
    .card p { margin: 0.32rem 0 0; color: var(--muted); font-size: 0.8rem; line-height: 1.55; font-weight: 600; }
    .card form { margin: 0; }

    .fields { display: grid; gap: 0.75rem; }
    .two-col { grid-template-columns: repeat(2, minmax(0, 1fr)); }
    .three-col { grid-template-columns: repeat(3, minmax(0, 1fr)); }

    .field {
      padding: 0.8rem 0.85rem 0.75rem;
      border-radius: var(--radius-md);
      background: linear-gradient(180deg, #f7fbff, #eef7ff);
      border: 1px solid var(--field-border);
      box-shadow: inset 0 1px 0 rgba(255,255,255,0.9), inset 0 -8px 18px rgba(16, 112, 158, 0.04);
      transition: transform .18s ease, border-color .18s ease, box-shadow .18s ease;
    }

    .field:focus-within { transform: translateY(-1px); border-color: rgba(14, 155, 181, 0.58); box-shadow: 0 0 0 4px rgba(14, 155, 181, 0.11), inset 0 1px 0 rgba(255,255,255,0.95), inset 0 -8px 18px rgba(16, 112, 158, 0.06); }

    .field label { display: flex; align-items: center; justify-content: space-between; gap: 0.5rem; margin-bottom: 0.45rem; color: var(--muted); text-transform: uppercase; letter-spacing: 0.08em; font-size: 0.7rem; font-weight: 800; }
    .field small { color: #7fa1b8; font-size: 0.68rem; font-weight: 700; text-transform: none; letter-spacing: 0; }

    .field input { width: 100%; padding: 0; margin: 0; border: 0; outline: none; background: transparent; color: var(--ink); font-size: 1.12rem; font-weight: 800; letter-spacing: -0.03em; text-align: center; appearance: textfield; }
    .field input::placeholder { color: #9db6c9; }
    .field input::-webkit-outer-spin-button, .field input::-webkit-inner-spin-button { -webkit-appearance: none; margin: 0; }

    .action-btn {
      position: relative;
      overflow: hidden;
      width: 100%;
      height: 50px;
      margin-top: 0.95rem;
      border: none;
      border-radius: var(--radius-md);
      color: #fff;
      font-size: 0.95rem;
      font-weight: 800;
      letter-spacing: -0.02em;
      cursor: pointer;
      background: linear-gradient(135deg, var(--deep-blue), var(--teal));
      box-shadow: 0 16px 28px rgba(13, 93, 142, 0.22), inset 0 1px 0 rgba(255,255,255,0.22);
      transition: transform .18s ease, box-shadow .18s ease, filter .18s ease, background .18s ease;
    }

    .action-btn::before { content: ""; position: absolute; inset: 0; background: linear-gradient(110deg, transparent 20%, rgba(255,255,255,0.36) 50%, transparent 80%); transform: translateX(-140%); transition: transform .8s ease; pointer-events: none; }
    .action-btn:hover::before, .action-btn:focus-visible::before { transform: translateX(140%); }
    .action-btn:hover { transform: translateY(-1px); box-shadow: 0 20px 32px rgba(13, 93, 142, 0.26), inset 0 1px 0 rgba(255,255,255,0.24); }
    .action-btn:active { transform: translateY(1px) scale(0.99); }
    .action-btn:focus-visible { outline: none; box-shadow: 0 0 0 4px rgba(14, 155, 181, 0.16), 0 16px 28px rgba(13, 93, 142, 0.22), inset 0 1px 0 rgba(255,255,255,0.22); }
    .action-btn.is-saved:not(.emergency-button) { background: linear-gradient(135deg, #108873, #14b49d); }

    .emergency-button { background: linear-gradient(135deg, var(--danger-deep), var(--danger)); box-shadow: 0 16px 30px rgba(201, 47, 72, 0.24), inset 0 1px 0 rgba(255,255,255,0.22); animation: breathe 2.6s ease-in-out infinite; }
    .emergency-button:hover { box-shadow: 0 20px 34px rgba(201, 47, 72, 0.28), inset 0 1px 0 rgba(255,255,255,0.22); }
    .emergency-button:focus-visible { box-shadow: 0 0 0 4px rgba(227, 75, 95, 0.16), 0 16px 30px rgba(201, 47, 72, 0.24), inset 0 1px 0 rgba(255,255,255,0.22); }
    .emergency-button.is-sent { animation: none; background: linear-gradient(135deg, #ba2943, #ef6678); }

    .toast { position: fixed; left: 50%; bottom: calc(1rem + env(safe-area-inset-bottom)); transform: translate(-50%, 20px); opacity: 0; pointer-events: none; padding: 0.85rem 1rem; border-radius: 999px; color: #fff; background: rgba(15, 37, 55, 0.94); box-shadow: 0 16px 28px rgba(15, 37, 55, 0.22); font-size: 0.8rem; font-weight: 700; white-space: nowrap; transition: opacity .25s ease, transform .25s ease; z-index: 10; }
    .toast.show { opacity: 1; transform: translate(-50%, 0); }

    @media (min-width: 860px) {
      .dashboard { width: min(calc(100% - 2rem), 1040px); padding-top: 1.4rem; }
      .hero-panel { padding: 1.75rem 1.5rem 1.15rem; }
      .cards-grid { grid-template-columns: repeat(2, minmax(0, 1fr)); }
    }

    @media (max-width: 420px) {
      .dashboard { width: min(calc(100% - 0.75rem), 1040px); }
      .hero-panel, .card { border-radius: 24px; }
      .field { padding-inline: 0.7rem; }
      .field input { font-size: 1.05rem; }
      .action-btn { height: 48px; }
    }

    @media (prefers-reduced-motion: reduce) { *, *::before, *::after { animation: none !important; transition: none !important; scroll-behavior: auto !important; } }

    @keyframes pulse { 0% { box-shadow: 0 0 0 0 rgba(22, 196, 124, 0.45); } 70% { box-shadow: 0 0 0 10px rgba(22, 196, 124, 0); } 100% { box-shadow: 0 0 0 0 rgba(22, 196, 124, 0); } }
    @keyframes drawTrace { to { stroke-dashoffset: 0; } }
    @keyframes floatIn { from { opacity: 0; transform: translateY(16px); } to { opacity: 1; transform: translateY(0); } }
    @keyframes breathe { 0%, 100% { transform: translateY(0); box-shadow: 0 16px 30px rgba(201, 47, 72, 0.24), inset 0 1px 0 rgba(255,255,255,0.22); } 50% { transform: translateY(-1px); box-shadow: 0 20px 34px rgba(201, 47, 72, 0.30), inset 0 1px 0 rgba(255,255,255,0.22); } }
  </style>
</head>
<body>
  <main class="dashboard">
    <section class="hero-panel" aria-label="Patient Monitor header">
      <div class="brand-mark" aria-hidden="true">
        <svg viewBox="0 0 24 24"><path d="M12 20.7s-6.9-4.2-8.7-8.2C1.6 8.8 3.3 5 6.9 5c2.2 0 3.7 1.1 5.1 3 1.4-1.9 2.9-3 5.1-3 3.6 0 5.3 3.8 3.6 7.5-1.8 4-8.7 8.2-8.7 8.2Z"></path></svg>
      </div>
      <h1>Patient Monitor</h1>
      <p class="hero-subtitle">Vital Signs Monitoring System</p>
      <div class="header-meta">
        <span class="status-pill"><span class="status-dot"></span>Connected</span>
        <span class="ip-chip">patient-monitor.local</span>
      </div>
      <div class="ecg-trace" aria-hidden="true">
        <svg viewBox="0 0 600 64" preserveAspectRatio="none"><path d="M0 34H112l16-8 16 20 22-36 18 24 20-8H600"></path></svg>
      </div>
    </section>

    <section class="cards-grid">
      <article class="card temp-card" style="--delay:.08s;">
        <div class="card-head">
          <div class="icon-orb temp" aria-hidden="true">
            <svg viewBox="0 0 24 24"><path d="M10 13.9V6a2 2 0 1 1 4 0v7.9a4 4 0 1 1-4 0Z"></path><path d="M12 10h2"></path><path d="M17.5 6.5h3"></path><path d="M19 5v3"></path></svg>
          </div>
          <div><h2>Body Temperature</h2><p>Set safe minimum and maximum values.</p></div>
        </div>
        <form data-message="Temperature threshold saved.">
          <div class="fields two-col">
            <div class="field"><label for="temp-min"><span>Min</span><small>°C</small></label><input id="temp-min" type="number" step="0.1" min="0" max="50" value="20"></div>
            <div class="field"><label for="temp-max"><span>Max</span><small>°C</small></label><input id="temp-max" type="number" step="0.1" min="0" max="50" value="35"></div>
          </div>
          <button class="action-btn" type="submit" data-success="Saved">Save Threshold</button>
        </form>
      </article>

      <article class="card bp-card" style="--delay:.16s;">
        <div class="card-head">
          <div class="icon-orb bp" aria-hidden="true">
            <svg viewBox="0 0 24 24"><path d="M20.84 4.61a5.5 5.5 0 0 0-7.78 0L12 5.67l-1.06-1.06a5.5 5.5 0 0 0-7.78 7.78l1.06 1.06L12 21.23l7.78-7.78 1.06-1.06a5.5 5.5 0 0 0 0-7.78Z"></path><path d="M12 12v3"></path></svg>
          </div>
          <div><h2>Blood Pressure</h2><p>Configure systolic and diastolic.</p></div>
        </div>
        <form data-message="Blood pressure saved.">
          <div class="fields two-col">
            <div class="field"><label for="bp-sys"><span>SYS</span><small>mmHg</small></label><input id="bp-sys" type="number" min="50" max="240" value="120"></div>
            <div class="field"><label for="bp-dia"><span>DIA</span><small>mmHg</small></label><input id="bp-dia" type="number" min="30" max="160" value="80"></div>
          </div>
          <button class="action-btn" type="submit" data-success="Saved">Save Pressure</button>
        </form>
      </article>

      <article class="card sync-card" style="--delay:.24s;">
        <div class="card-head">
          <div class="icon-orb sync" aria-hidden="true">
            <svg viewBox="0 0 24 24"><circle cx="12" cy="12" r="9"/><path d="M12 7v5l3 2"/><circle cx="12" cy="12" r="2"/></svg>
          </div>
          <div><h2>Data Transmission</h2><p>Choose how often data is pushed.</p></div>
        </div>
        <form data-message="Transmission interval saved.">
          <div class="fields three-col">
            <div class="field"><label for="send-hr"><span>HR</span><small>Hrs</small></label><input id="send-hr" type="number" min="0" max="24" value="0"></div>
            <div class="field"><label for="send-min"><span>MIN</span><small>Min</small></label><input id="send-min" type="number" min="0" max="59" value="0"></div>
            <div class="field"><label for="send-sec"><span>SEC</span><small>Sec</small></label><input id="send-sec" type="number" min="0" max="59" value="1"></div>
          </div>
          <button class="action-btn" type="submit" data-success="Saved">Save Interval</button>
        </form>
      </article>

      <article class="card emergency" style="--delay:.32s;">
        <div class="card-head">
          <div class="icon-orb alert" aria-hidden="true">
            <svg viewBox="0 0 24 24"><path d="M12 4.3 20.1 18H3.9L12 4.3Z"></path><path d="M12 9.5v4.1"></path><path d="M12 15.8h.01"></path></svg>
          </div>
          <div><h2>Emergency Alert</h2><p>Notify care team immediately.</p></div>
        </div>
        <form data-message="Emergency alert dispatched.">
          <button class="action-btn emergency-button" type="submit" data-success="Alert Sent">Trigger Alert</button>
        </form>
      </article>
    </section>
  </main>

  <div class="toast" id="toast" role="status" aria-live="polite"></div>

  <script>
    const toast = document.getElementById('toast');
    let toastTimer;
    const showToast = (message) => { clearTimeout(toastTimer); toast.textContent = message; toast.classList.add('show'); toastTimer = setTimeout(() => toast.classList.remove('show'), 2200); };

    document.querySelectorAll('.card form').forEach((form) => {
      const button = form.querySelector('button[type="submit"]');
      if (!button) return;
      button.dataset.original = button.textContent.trim();

      form.addEventListener('submit', (event) => {
        event.preventDefault();
        const isEmergency = button.classList.contains('emergency-button');
        
        if (isEmergency && !confirm('WARNING: Send emergency alert?')) return;

        const formData = new FormData(form);
        let endpoint = '/';
        if (form.querySelector('#temp-min')) endpoint = '/setTemp?min=' + document.getElementById('temp-min').value + '&max=' + document.getElementById('temp-max').value;
        else if (form.querySelector('#bp-sys')) endpoint = '/setBP?sys=' + document.getElementById('bp-sys').value + '&dia=' + document.getElementById('bp-dia').value;
        else if (form.querySelector('#send-hr')) {
          const h = parseInt(document.getElementById('send-hr').value) || 0;
          const m = parseInt(document.getElementById('send-min').value) || 0;
          const s = parseInt(document.getElementById('send-sec').value) || 0;
          endpoint = '/setInterval?interval=' + (h * 3600 + m * 60 + s);
        }
        else if (isEmergency) endpoint = '/alert';

        fetch(endpoint).then(() => {
          showToast(form.dataset.message || 'Settings saved.');
          button.textContent = button.dataset.success || 'Saved';
          button.classList.add('is-saved');
          if (isEmergency) button.classList.add('is-sent');
          
          setTimeout(() => {
            button.textContent = button.dataset.original;
            button.classList.remove('is-saved', 'is-sent');
          }, 1700);
        });
      });
    });
  </script>
</body>
</html>
)rawliteral";

// ================= ROUTES =================

inline void handleRoot() {
    server.send(200, "text/html", page);
}

// Temperature update
inline void handleSetTemp() {
    if (server.hasArg("min")) tempMin = server.arg("min").toFloat();
    if (server.hasArg("max")) tempMax = server.arg("max").toFloat();

    Serial.printf("Temp updated: min=%.2f max=%.2f\n", tempMin, tempMax);
    server.send(200, "text/plain", "OK");
}

// Blood pressure update
inline void handleSetBP() {
    if (server.hasArg("sys")) bpSys = server.arg("sys").toInt();
    if (server.hasArg("dia")) bpDia = server.arg("dia").toInt();

    Serial.printf("BP updated: %d/%d\n", bpSys, bpDia);
    server.send(200, "text/plain", "OK");
}

// MQTT interval update
inline void handleSetInterval() {
    if (server.hasArg("interval")) {
        long newInterval = server.arg("interval").toInt();
        Serial.printf("Setting MQTT interval to: %ld seconds\n", newInterval);
        mqttIntervalSeconds = newInterval;
        Serial.printf("MQTT interval updated to: %ld seconds\n", mqttIntervalSeconds);
    } else {
        Serial.println("No 'interval' argument received");
    }
    server.send(200, "text/plain", "OK");
}

// Alert trigger (simulate button)
inline void handleAlert() {
    Serial.println("🚨 ALERT TRIGGERED FROM WEB");
    triggerAlert(TOPIC_ALERTS, alertType.emergencyButton);
    server.send(200, "text/plain", "Alert Sent");
}

// ================= INIT =================

inline void handleCaptivePortal() {
    server.sendHeader("Location", "http://patient-monitor.local", true);
    server.send(302, "text/plain", "");
}

inline void initWebServer() {
    server.on("/", handleRoot);
    server.on("/setTemp", handleSetTemp);
    server.on("/setBP", handleSetBP);
    server.on("/setInterval", handleSetInterval);
    server.on("/alert", handleAlert);

    server.onNotFound(handleCaptivePortal);
    server.begin();
}
