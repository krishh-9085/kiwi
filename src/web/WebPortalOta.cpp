#include "web/WebPortal.h"
#include "config/AppConfig.h"
#include "config/ConfigManager.h"
#include "hardware/AudioEngine.h"
#include "face/FaceEngine.h"
#include "services/AiService.h"
#include "services/MemoryService.h"
#include "animation/AnimationEngine.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Update.h>
#include <time.h>
#include <Preferences.h>
#include "services/WeatherService.h"

void handleOtaPage() {
  static const char OTA_HTML[] PROGMEM = R"rawliteral(<!doctype html>
<html lang="en">

<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
  <title>Kiwi Robot · Firmware OTA</title>
  <link rel="icon"
    href="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 64 64'%3E%3Crect width='64' height='64' rx='12' fill='%2306080d'/%3E%3Crect x='14' y='22' width='10' height='10' fill='%23f2b45c'/%3E%3Crect x='40' y='22' width='10' height='10' fill='%23f2b45c'/%3E%3Cpath d='M20 42h24' stroke='%23f2b45c' stroke-width='4'/%3E%3C/svg%3E">
  <style>
    :root {
      color-scheme: dark;
      --void: #06080d;
      --panel: #0d121b;
      --well: #090c13;
      --line: #1d2533;
      --line-strong: #2c3648;
      --ink: #ece6d9;
      --muted: #8a94a6;
      --astro: #ff4a3d;
      --astro-glow: rgba(255, 74, 61, .35);
      --amber: #f2b45c;
      --amber-dim: rgba(242, 180, 92, .16);
      --amber-glow: rgba(242, 180, 92, .38);
      --ok: #8fd19e;
      --glass: rgba(13, 18, 27, .6);
      --glass-edge: rgba(70, 84, 108, .55);
      --display: "Avenir Next", "Helvetica Neue", sans-serif;
      --body: "Helvetica Neue", Arial, sans-serif;
      --mono: "SF Mono", Menlo, monospace;
      --cut: 14px;
      --cut-sm: 7px;
    }

    * {
      box-sizing: border-box
    }

    body {
      margin: 0;
      font: 14px/1.5 var(--body);
      color: var(--ink);
      background: var(--void);
      min-height: 100vh;
      overflow-x: hidden
    }

    body::before {
      content: "";
      position: fixed;
      inset: 0;
      z-index: -1;
      pointer-events: none;
      background-image: linear-gradient(rgba(242, 180, 92, .035) 1px, transparent 1px), linear-gradient(90deg, rgba(242, 180, 92, .035) 1px, transparent 1px);
      background-size: 48px 48px;
      -webkit-mask-image: linear-gradient(#000, transparent 55%);
      mask-image: linear-gradient(#000, transparent 55%)
    }

    #sky {
      position: fixed;
      inset: 0;
      width: 100%;
      height: 100%;
      z-index: -2;
      pointer-events: none
    }

    .wrap {
      width: min(1260px, 100%);
      margin: 0 auto;
      padding: 24px
    }

    /* Bridge header */
    .bridge {
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 16px;
      flex-wrap: wrap;
      padding: 8px 0 20px
    }

    .id {
      display: flex;
      align-items: center;
      gap: 14px
    }

    .bezel {
      width: 64px;
      height: 48px;
      border-radius: 12px;
      background: #000;
      border: 1px solid rgba(242, 180, 92, .55);
      box-shadow: 0 0 18px var(--amber-glow), inset 0 0 12px #000;
      display: flex;
      align-items: center;
      justify-content: center;
      gap: 9px;
      position: relative;
      overflow: hidden
    }

    .bezel::after {
      content: "";
      position: absolute;
      inset: 0;
      background: repeating-linear-gradient(0deg, rgba(255, 255, 255, .03) 0 1px, transparent 1px 3px);
      pointer-events: none
    }

    .bezel i {
      width: 9px;
      height: 12px;
      background: var(--amber);
      box-shadow: 0 0 8px var(--amber-glow);
      animation: blink 5s infinite
    }

    .unit {
      font: 600 10px/1 var(--display);
      letter-spacing: .22em;
      text-transform: uppercase;
      color: var(--muted)
    }

    h1.brand {
      margin: 4px 0 2px;
      font: 600 17px/1.1 var(--display);
      letter-spacing: .28em;
      text-transform: uppercase
    }

    .nav {
      display: flex;
      gap: 8px;
      flex-wrap: wrap;
      align-items: center
    }

    .nb {
      font: 500 12px var(--body);
      color: var(--ink);
      text-decoration: none;
      background: var(--well);
      border: 1px solid var(--line-strong);
      padding: 8px 14px;
      clip-path: polygon(var(--cut-sm) 0, 100% 0, 100% calc(100% - var(--cut-sm)), calc(100% - var(--cut-sm)) 100%, 0 100%, 0 var(--cut-sm));
      transition: border-color .15s, background .15s, transform .1s;
      display: inline-flex;
      align-items: center
    }

    .nb:hover {
      border-color: rgba(242, 180, 92, .6);
      background: rgba(242, 180, 92, .06)
    }

    .nb:active {
      transform: scale(.97)
    }

    .nb.on {
      background: var(--amber);
      border-color: var(--amber);
      color: #14100a;
      font-weight: 600
    }

    .nb:focus-visible,
    .btn:focus-visible,
    .file-label:focus-visible {
      outline: 2px solid var(--amber);
      outline-offset: 2px
    }

    /* Hull card */
    .container {
      width: min(680px, 100%);
      margin: 24px auto 0
    }

    .hull {
      position: relative;
      background: linear-gradient(180deg, rgba(242, 180, 92, .06), transparent 30%), var(--glass);
      border: 1px solid var(--glass-edge);
      padding: clamp(24px, 5vw, 36px);
      text-align: center;
      backdrop-filter: blur(18px);
      -webkit-backdrop-filter: blur(18px);
      clip-path: polygon(var(--cut) 0, 100% 0, 100% calc(100% - var(--cut)), calc(100% - var(--cut)) 100%, 0 100%, 0 var(--cut))
    }

    .hull::before {
      content: "";
      position: absolute;
      top: 0;
      left: var(--cut);
      width: 36px;
      height: 2px;
      background: var(--amber);
      box-shadow: 0 0 8px var(--amber-glow)
    }

    .glyph {
      width: 56px;
      height: 56px;
      margin: 0 auto 16px;
      display: flex;
      align-items: center;
      justify-content: center;
      background: #000;
      border: 1px solid rgba(242, 180, 92, .5);
      box-shadow: 0 0 16px var(--amber-glow), inset 0 0 10px #000;
      color: var(--amber)
    }

    h2 {
      font: 600 15px/1.2 var(--display);
      letter-spacing: .2em;
      text-transform: uppercase;
      margin: 0 0 8px
    }

    .sub {
      color: var(--muted);
      font-size: 13px;
      margin: 0 0 22px;
      line-height: 1.5
    }

    .sub code {
      font: 12px var(--mono);
      color: var(--amber)
    }

    /* File picker: recessed well with dashed amber border */
    .file-picker {
      display: flex;
      flex-direction: column;
      align-items: center;
      gap: 12px;
      padding: 24px 16px;
      margin: 18px 0 8px;
      background: var(--well);
      border: 1px dashed rgba(242, 180, 92, .45);
      transition: border-color .15s, box-shadow .15s
    }

    .file-picker:hover,
    .file-picker:focus-within {
      border-color: var(--amber);
      box-shadow: 0 0 0 3px rgba(242, 180, 92, .12)
    }

    .file-picker input {
      position: absolute;
      width: 1px;
      height: 1px;
      opacity: 0;
      pointer-events: none
    }

    .file-label {
      display: inline-flex;
      align-items: center;
      justify-content: center;
      padding: 9px 20px;
      cursor: pointer;
      background: var(--panel);
      border: 1px solid var(--line-strong);
      color: var(--ink);
      font: 600 11px var(--display);
      letter-spacing: .18em;
      text-transform: uppercase;
      clip-path: polygon(var(--cut-sm) 0, 100% 0, 100% calc(100% - var(--cut-sm)), calc(100% - var(--cut-sm)) 100%, 0 100%, 0 var(--cut-sm));
      transition: border-color .15s, background .15s, transform .1s
    }

    .file-label:hover {
      border-color: rgba(242, 180, 92, .6);
      background: rgba(242, 180, 92, .06)
    }

    .file-label:active {
      transform: scale(.97)
    }

    .file-name {
      font: 12px var(--mono);
      color: var(--muted);
      word-break: break-all;
      max-width: 90%
    }

    .file-name.set {
      color: var(--ink)
    }

    .file-help {
      color: var(--muted);
      font-size: 12px;
      margin-bottom: 18px
    }

    /* Primary action */
    .btn {
      width: 100%;
      height: 46px;
      border: 0;
      cursor: pointer;
      background: var(--amber);
      color: #14100a;
      font: 600 12px var(--display);
      letter-spacing: .22em;
      text-transform: uppercase;
      clip-path: polygon(var(--cut-sm) 0, 100% 0, 100% calc(100% - var(--cut-sm)), calc(100% - var(--cut-sm)) 100%, 0 100%, 0 var(--cut-sm));
      transition: background .15s, transform .1s
    }

    .btn:hover {
      background: #ffc87a
    }

    .btn:active {
      transform: scale(.98)
    }

    .btn:disabled {
      background: var(--well);
      color: #4d5669;
      border: 1px solid var(--line);
      cursor: not-allowed;
      clip-path: none
    }

    /* Progress */
    #prg {
      margin-top: 22px;
      width: 100%;
      height: 8px;
      background: var(--well);
      border: 1px solid var(--line);
      display: none;
      position: relative;
      overflow: hidden
    }

    #bar {
      height: 100%;
      width: 0%;
      background: var(--amber);
      box-shadow: 0 0 10px var(--amber-glow);
      transition: width .15s ease
    }

    #stat {
      margin-top: 14px;
      font: 12px var(--mono);
      min-height: 20px;
      color: var(--amber)
    }

    /* Partition notice */
    .notice {
      margin-top: 26px;
      padding: 14px 16px;
      background: var(--well);
      border: 1px solid var(--line);
      border-left: 2px solid var(--amber);
      text-align: left;
      font-size: 13px;
      line-height: 1.55;
      color: var(--muted)
    }

    .notice b {
      color: var(--ink);
      font-family: var(--display);
      letter-spacing: .14em;
      text-transform: uppercase;
      font-size: 11px
    }

    .notice code {
      font: 12px var(--mono);
      color: var(--amber)
    }

    @keyframes blink {

      0%,
      92%,
      100% {
        opacity: 1
      }

      96% {
        opacity: .25
      }
    }

    @media(max-width:640px) {
      .wrap {
        padding: 16px
      }

      .bezel {
        width: 52px;
        height: 40px
      }

      h1.brand {
        font-size: 15px
      }

      .nav {
        width: 100%;
        display: grid;
        grid-template-columns: repeat(4, minmax(0, 1fr))
      }

      .nb {
        padding: 8px 4px;
        font-size: 11px;
        justify-content: center
      }
    }

    @media(prefers-reduced-motion:reduce) {

      *,
      *::before,
      *::after {
        animation: none !important;
        transition: none !important
      }
    }
  </style>
</head>

<body>
  <canvas id="sky" aria-hidden="true"></canvas>

  <div class="wrap">
    <header class="bridge">
      <div class="id">
        <div class="bezel" aria-hidden="true"><i></i><i></i></div>
        <div>
          <div class="unit">Kiwi Unit · Firmware</div>
          <h1 class="brand">Over-The-Air Update</h1>
        </div>
      </div>
      <nav class="nav" aria-label="Pages">
        <a class="nb" href="/">Dashboard</a>
        <a class="nb" href="/faces">Faces</a>
        <a class="nb on" href="/update" aria-current="page">OTA</a>
      </nav>
    </header>

    <main class="container">
      <section class="hull" aria-labelledby="otaTitle">
        <div class="glyph" aria-hidden="true">
          <svg width="26" height="26" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"
            stroke-linecap="square">
            <path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4" />
            <polyline points="17 8 12 3 7 8" />
            <line x1="12" y1="3" x2="12" y2="15" />
          </svg>
        </div>
        <h2 id="otaTitle">Flash Firmware Wirelessly</h2>
        <p class="sub">Select the compiled <code>MyRobot_Modular.ino.bin</code> from your build folder to update your
          Kiwi Robot.</p>

        <form method="POST" action="/update" enctype="multipart/form-data" id="otaForm">
          <div class="file-picker">
            <input type="file" name="update" accept=".bin" required id="fileIn">
            <label class="file-label" for="fileIn" tabindex="0">Choose Firmware Binary</label>
            <span class="file-name" id="fileName" aria-live="polite">No file selected</span>
          </div>
          <div class="file-help">Accepts compiled .bin application or filesystem binaries.</div>
          <button type="submit" id="btn" class="btn">Update Firmware</button>
        </form>

        <div id="prg" role="progressbar" aria-label="Upload progress" aria-valuemin="0" aria-valuemax="100">
          <div id="bar"></div>
        </div>
        <div id="stat" role="status" aria-live="polite"></div>

        <div class="notice">
          <b>Partition Notice</b><br>
          For OTA updates to succeed, the ESP32 partition table must have OTA partitions enabled (e.g. <b
            style="font-family:inherit;letter-spacing:0;text-transform:none;font-size:13px;color:var(--ink)">Minimal
            SPIFFS (Large APPS with OTA)</b>). If this is your first time switching schemes from <code>huge_app</code>,
          please flash once via USB cable in the Arduino IDE.
        </div>
      </section>
    </main>
  </div>

  <script>
    /* ── Starfield (paused when hidden, static under reduced motion) ── */
    (function () {
      var c = document.getElementById('sky'), x = c.getContext('2d'), stars = [], W, H, raf;
      var reduce = window.matchMedia('(prefers-reduced-motion: reduce)').matches;
      function size() {
        W = c.width = innerWidth; H = c.height = innerHeight; stars = [];
        var n = Math.min(220, Math.round(W * H / 9000));
        for (var i = 0; i < n; i++)stars.push({ x: Math.random() * W, y: Math.random() * H, r: Math.random() * 1.2 + .2, s: Math.random() * .15 + .02, p: Math.random() * 6.28 });
      }
      function draw(t) {
        x.clearRect(0, 0, W, H);
        for (var i = 0; i < stars.length; i++) {
          var s = stars[i];
          if (!reduce) { s.y += s.s; if (s.y > H) { s.y = 0; s.x = Math.random() * W; } }
          var a = .35 + .35 * Math.sin(t / 1200 + s.p);
          x.fillStyle = 'rgba(236,230,217,' + a.toFixed(3) + ')';
          x.beginPath(); x.arc(s.x, s.y, s.r, 0, 6.283); x.fill();
        }
        if (!reduce) raf = requestAnimationFrame(draw);
      }
      size(); window.addEventListener('resize', size);
      if (reduce) { draw(0); } else {
        raf = requestAnimationFrame(draw);
        document.addEventListener('visibilitychange', function () {
          if (document.hidden) { cancelAnimationFrame(raf); } else { raf = requestAnimationFrame(draw); }
        });
      }
    })();

    /* ── OTA logic (unchanged behaviour) ── */
    var form = document.getElementById('otaForm');
    var fileInput = document.getElementById('fileIn');
    var fileName = document.getElementById('fileName');
    var stat = document.getElementById('stat');
    var btn = document.getElementById('btn');
    fileInput.addEventListener('change', function () {
      var has = this.files.length > 0;
      fileName.textContent = has ? this.files[0].name : 'No file selected';
      fileName.classList.toggle('set', has);
    });
    form.onsubmit = function (e) {
      e.preventDefault();
      var file = fileInput.files[0];
      if (!file) return;
      var xhr = new XMLHttpRequest();
      var fd = new FormData();
      fd.append('update', file);
      document.getElementById('prg').style.display = 'block';
      btn.disabled = true;
      stat.style.color = 'var(--amber)';
      stat.textContent = 'Uploading firmware binary...';
      xhr.upload.onprogress = function (pe) {
        if (pe.lengthComputable) {
          var pct = Math.round((pe.loaded / pe.total) * 100);
          document.getElementById('bar').style.width = pct + '%';
          stat.textContent = 'Flashing: ' + pct + '%';
        }
      };
      xhr.onload = function () {
        if (xhr.status === 200 && xhr.responseText.trim() === 'OK') {
          stat.style.color = 'var(--ok)';
          stat.innerHTML = '&#10004; Flash Success! Kiwi is restarting with new firmware...';
          setTimeout(function () { window.location.href = '/'; }, 9000);
        } else {
          stat.style.color = 'var(--astro)';
          stat.innerHTML = '&#10008; ' + (xhr.responseText || 'Flash Failed (Check partition table or serial monitor)');
          btn.disabled = false;
        }
      };
      xhr.onerror = function () {
        stat.style.color = 'var(--astro)';
        stat.innerHTML = '&#10008; Network connection interrupted during upload';
        btn.disabled = false;
      };
      xhr.open('POST', '/update');
      xhr.send(fd);
    };
  </script>
</body>

</html>)rawliteral";
  server.send_P(200, "text/html; charset=utf-8", OTA_HTML);
}

