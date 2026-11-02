#include "animation/AnimationEngine.h"
#include "config/AppConfig.h"
#include "config/ConfigManager.h"
#include "face/FaceEngine.h"
#include "hardware/AudioEngine.h"
#include "services/AiService.h"
#include "services/MemoryService.h"
#include "services/WeatherService.h"
#include "web/WebPortal.h"
#include <DNSServer.h>
#include <Preferences.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>
#include <time.h>


void handleOtaPage() {
  static const char OTA_HTML[] PROGMEM = R"rawliteral(<!doctype html>
<html lang="en">

<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
  <meta name="color-scheme" content="dark">
  <title>Kiwi Robot · Firmware OTA</title>
  <link rel="apple-touch-icon" sizes="180x180" href="/apple-touch-icon.png">
  <link rel="icon" type="image/png" href="/apple-touch-icon.png">
  <meta name="apple-mobile-web-app-title" content="Kiwi">
  <meta name="apple-mobile-web-app-capable" content="yes">
  <meta name="mobile-web-app-capable" content="yes">
  <meta name="apple-mobile-web-app-status-bar-style" content="black-translucent">
  <meta name="theme-color" content="#06080d">
  <link rel="icon"
    href="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 64 64'%3E%3Crect width='64' height='64' rx='12' fill='%2306080d'/%3E%3Crect x='14' y='22' width='10' height='10' fill='%23f2b45c'/%3E%3Crect x='40' y='22' width='10' height='10' fill='%23f2b45c'/%3E%3Cpath d='M20 42h24' stroke='%23f2b45c' stroke-width='4'/%3E%3C/svg%3E">
  <style>
    :root {
      color-scheme: dark;
      --void: #06080d;
      --panel: #0d121b;
      --panel-2: #111826;
      --well: #090c13;
      --line: #1d2533;
      --line-strong: #2c3648;
      --ink: #ece6d9;
      --muted: #a0abbd;
      --astro: #ff4a3d;
      --astro-glow: rgba(255, 74, 61, .35);
      --amber: #f2b45c;
      --amber-dim: rgba(242, 180, 92, .16);
      --amber-glow: rgba(242, 180, 92, .38);
      --ok: #8fd19e;
      --glass: rgba(13, 18, 27, .88);
      --glass-edge: rgba(70, 84, 108, .55);
      --display: "Avenir Next", "Helvetica Neue", sans-serif;
      --body: "Helvetica Neue", Arial, sans-serif;
      --mono: "SF Mono", Menlo, monospace;
      --cut: 14px;
      --cut-sm: 7px;
    }

    * {
      box-sizing: border-box;
    }

    body {
      margin: 0;
      font: 14px/1.5 var(--body);
      color: var(--ink);
      background: var(--void);
      min-height: 100vh;
      overflow-x: hidden;
    }

    body::before {
      content: "";
      position: fixed;
      inset: 0;
      z-index: -1;
      pointer-events: none;
      background-image: linear-gradient(rgba(242, 180, 92, .035) 1px, transparent 1px), linear-gradient(90deg, rgba(242, 180, 92, .035) 1px, transparent 1px);
      background-size: 48px 48px;
      -webkit-mask-image: linear-gradient(#000, transparent 65%);
      mask-image: linear-gradient(#000, transparent 65%);
    }

    .wrap {
      width: min(1260px, 100%);
      margin: 0 auto;
      padding: 24px;
      padding-top: max(20px, env(safe-area-inset-top, 20px));
      padding-left: max(16px, env(safe-area-inset-left, 16px));
      padding-right: max(16px, env(safe-area-inset-right, 16px));
      padding-bottom: max(24px, env(safe-area-inset-bottom, 24px));
    }

    /* Bridge header */
    .bridge {
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 16px;
      flex-wrap: wrap;
      padding: 8px 0 20px;
    }

    .id {
      display: flex;
      align-items: center;
      gap: 14px;
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
      overflow: hidden;
    }

    .bezel::after {
      content: "";
      position: absolute;
      inset: 0;
      background: repeating-linear-gradient(0deg, rgba(255, 255, 255, .03) 0 1px, transparent 1px 3px);
      pointer-events: none;
    }

    .bezel i {
      width: 9px;
      height: 12px;
      background: var(--amber);
      box-shadow: 0 0 8px var(--amber-glow);
      animation: blink 5s infinite;
    }

    .unit {
      font: 600 12px/1 var(--display);
      letter-spacing: .2em;
      text-transform: uppercase;
      color: var(--muted);
    }

    h1.brand {
      margin: 4px 0 2px;
      font: 600 18px/1.1 var(--display);
      letter-spacing: .25em;
      text-transform: uppercase;
    }

    .nav {
      display: flex;
      gap: 8px;
      flex-wrap: wrap;
      align-items: center;
    }

    .nb {
      font: 600 13px var(--body);
      color: var(--ink);
      text-decoration: none;
      background: var(--well);
      border: 1px solid var(--line-strong);
      padding: 10px 16px;
      min-height: 44px;
      clip-path: polygon(var(--cut-sm) 0, 100% 0, 100% calc(100% - var(--cut-sm)), calc(100% - var(--cut-sm)) 100%, 0 100%, 0 var(--cut-sm));
      transition: border-color .15s, background .15s, transform .1s;
      display: inline-flex;
      align-items: center;
    }

    .nb:hover {
      border-color: rgba(242, 180, 92, .6);
      background: rgba(242, 180, 92, .06);
    }

    .nb:active {
      transform: scale(.97);
    }

    .nb.on {
      background: var(--amber);
      border-color: var(--amber);
      color: #14100a;
      font-weight: 600;
    }

    .nb:focus-visible,
    .btn:focus-visible,
    .file-label:focus-visible {
      outline: 2px solid var(--amber);
      outline-offset: 2px;
    }

    /* Container card */
    .container {
      width: min(680px, 100%);
      margin: 24px auto 0;
    }

    .hull {
      position: relative;
      background: linear-gradient(180deg, rgba(242, 180, 92, .06), transparent 30%), var(--glass);
      border: 1px solid var(--glass-edge);
      padding: clamp(24px, 5vw, 36px);
      text-align: center;
      clip-path: polygon(var(--cut) 0, 100% 0, 100% calc(100% - var(--cut)), calc(100% - var(--cut)) 100%, 0 100%, 0 var(--cut));
    }

    .hull::before {
      content: "";
      position: absolute;
      top: 0;
      left: var(--cut);
      width: 36px;
      height: 2px;
      background: var(--amber);
      box-shadow: 0 0 8px var(--amber-glow);
    }

    .fw-meta-pill {
      display: inline-flex;
      align-items: center;
      gap: 8px;
      padding: 4px 14px;
      border-radius: 999px;
      background: var(--well);
      border: 1px solid var(--line-strong);
      font: 600 12px var(--mono);
      color: var(--amber);
      margin-bottom: 16px;
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
      color: var(--amber);
    }

    h2 {
      font: 600 16px/1.2 var(--display);
      letter-spacing: .2em;
      text-transform: uppercase;
      margin: 0 0 8px;
    }

    .sub {
      color: var(--muted);
      font-size: 13px;
      margin: 0 0 20px;
      line-height: 1.5;
    }

    .sub code {
      font: 12px var(--mono);
      color: var(--amber);
    }

    .warning-banner {
      display: flex;
      align-items: center;
      justify-content: center;
      gap: 10px;
      background: rgba(255, 74, 61, 0.12);
      border: 1px solid rgba(255, 74, 61, 0.35);
      color: #ff9d94;
      font-size: 12px;
      padding: 10px 14px;
      margin-bottom: 20px;
      text-align: left;
    }

    .warning-banner svg {
      flex-shrink: 0;
      stroke: var(--astro);
    }

    /* File picker: recessed dropzone */
    .file-picker {
      display: flex;
      flex-direction: column;
      align-items: center;
      gap: 12px;
      padding: 28px 16px;
      margin: 18px 0 8px;
      background: var(--well);
      border: 2px dashed rgba(242, 180, 92, .45);
      transition: border-color .15s, background .15s, box-shadow .15s;
      cursor: pointer;
    }

    .file-picker:hover,
    .file-picker.dragover,
    .file-picker:focus-within {
      border-color: var(--amber);
      background: rgba(242, 180, 92, .04);
      box-shadow: 0 0 0 3px rgba(242, 180, 92, .15);
    }

    .file-picker input {
      position: absolute;
      width: 1px;
      height: 1px;
      opacity: 0;
      pointer-events: none;
    }

    .file-label {
      display: inline-flex;
      align-items: center;
      justify-content: center;
      padding: 11px 24px;
      min-height: 44px;
      cursor: pointer;
      background: var(--panel);
      border: 1px solid var(--line-strong);
      color: var(--ink);
      font: 600 12px var(--display);
      letter-spacing: .18em;
      text-transform: uppercase;
      clip-path: polygon(var(--cut-sm) 0, 100% 0, 100% calc(100% - var(--cut-sm)), calc(100% - var(--cut-sm)) 100%, 0 100%, 0 var(--cut-sm));
      transition: border-color .15s, background .15s, transform .1s;
    }

    .file-label:hover {
      border-color: rgba(242, 180, 92, .6);
      background: rgba(242, 180, 92, .06);
    }

    .file-label:active {
      transform: scale(.97);
    }

    .file-name {
      font: 12px var(--mono);
      color: var(--muted);
      word-break: break-all;
      max-width: 90%;
    }

    .file-name.set {
      color: var(--ink);
      font-weight: 600;
    }

    .file-help {
      color: var(--muted);
      font-size: 12px;
      margin-bottom: 20px;
    }

    /* Primary action */
    .btn {
      width: 100%;
      min-height: 48px;
      border: 0;
      cursor: pointer;
      background: var(--amber);
      color: #14100a;
      font: 600 13px var(--display);
      letter-spacing: .22em;
      text-transform: uppercase;
      clip-path: polygon(var(--cut-sm) 0, 100% 0, 100% calc(100% - var(--cut-sm)), calc(100% - var(--cut-sm)) 100%, 0 100%, 0 var(--cut-sm));
      transition: background .15s, transform .1s, opacity .15s;
    }

    .btn:hover:not(:disabled) {
      background: #ffc87a;
    }

    .btn:active:not(:disabled) {
      transform: scale(.98);
    }

    .btn:disabled {
      background: var(--well);
      color: #5b6579;
      border: 1px solid var(--line);
      cursor: not-allowed;
      clip-path: none;
    }

    /* Progress */
    #prg {
      margin-top: 24px;
      width: 100%;
      height: 10px;
      background: var(--well);
      border: 1px solid var(--line);
      display: none;
      position: relative;
      overflow: hidden;
    }

    #bar {
      height: 100%;
      width: 0%;
      background: var(--amber);
      box-shadow: 0 0 12px var(--amber-glow);
      transition: width .15s ease;
    }

    #stat {
      margin-top: 16px;
      font: 13px var(--mono);
      min-height: 24px;
      color: var(--amber);
    }

    /* Collapsible partition help */
    .notice-details {
      margin-top: 28px;
      background: var(--well);
      border: 1px solid var(--line);
      border-left: 2px solid var(--amber);
      text-align: left;
    }

    .notice-details summary {
      padding: 12px 16px;
      cursor: pointer;
      font: 600 12px var(--display);
      letter-spacing: .14em;
      text-transform: uppercase;
      color: var(--ink);
      user-select: none;
    }

    .notice-details summary:hover {
      color: var(--amber);
    }

    .notice-body {
      padding: 0 16px 14px;
      font-size: 13px;
      line-height: 1.55;
      color: var(--muted);
    }

    .notice-body code {
      font: 12px var(--mono);
      color: var(--amber);
    }

    .noscript-warn {
      padding: 12px;
      background: #451a1a;
      color: #ffb4b4;
      text-align: center;
      font-weight: 600;
      border-bottom: 1px solid #782a2a;
    }

    @keyframes blink {

      0%,
      92%,
      100% {
        opacity: 1;
      }

      96% {
        opacity: .25;
      }
    }

    @media(max-width:768px) {
      .wrap {
        padding: 12px;
        padding-top: calc(env(safe-area-inset-top, 0px) + 8px);
        padding-left: max(12px, env(safe-area-inset-left, 12px));
        padding-right: max(12px, env(safe-area-inset-right, 12px));
        padding-bottom: max(16px, env(safe-area-inset-bottom, 16px));
      }

      .bridge {
        padding: 2px 0 8px;
        gap: 8px;
      }

      .bezel {
        width: 44px;
        height: 34px;
        border-radius: 8px;
        gap: 6px;
      }

      .bezel i {
        width: 6px;
        height: 9px;
      }

      .id {
        gap: 10px;
      }

      h1.brand {
        font-size: 17px;
        margin: 0;
        letter-spacing: .16em;
      }

      .id .unit {
        font-size: 10px;
        letter-spacing: .12em;
      }

      .nav {
        width: 100%;
        display: grid;
        grid-template-columns: repeat(3, minmax(0, 1fr));
        gap: 6px;
      }

      .nb {
        padding: 6px 4px;
        font-size: 12px;
        min-height: 36px;
        justify-content: center;
      }

      .hull {
        padding: 18px 14px;
      }
    }

    @media(prefers-reduced-motion:reduce) {

      *,
      *::before,
      *::after {
        animation: none !important;
        transition: none !important;
      }
    }
  </style>
</head>

<body>
  <noscript>
    <div class="noscript-warn">JavaScript is required to perform OTA firmware updates.</div>
  </noscript>

  <div class="wrap">
    <header class="bridge">
      <div class="id">
        <div class="bezel" aria-hidden="true"><i></i><i></i></div>
        <div>
          <div class="unit">Kiwi Unit · <span id="fwVer">v1.0</span> · Firmware</div>
          <h1 class="brand">Over-The-Air Update</h1>
        </div>
      </div>
      <nav class="nav" aria-label="Main Pages">
        <a class="nb" href="/">Dashboard</a>
        <a class="nb" href="/faces">Faces</a>
        <a class="nb on" href="/update" aria-current="page">OTA</a>
      </nav>
    </header>

    <main class="container">
      <section class="hull" aria-labelledby="otaTitle">
        <div class="fw-meta-pill" id="fwPill">
          <span>Current Firmware:</span>
          <strong id="fwVersion">v1.0</strong>
        </div>

        <div class="glyph" aria-hidden="true">
          <svg width="26" height="26" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"
            stroke-linecap="square">
            <path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4" />
            <polyline points="17 8 12 3 7 8" />
            <line x1="12" y1="3" x2="12" y2="15" />
          </svg>
        </div>
        <h2 id="otaTitle">Flash Firmware Wirelessly</h2>
        <p class="sub">Select or drag &amp; drop the compiled <code>MyRobot_Modular.ino.bin</code> from your Arduino IDE
          or PlatformIO build output.</p>

        <div class="warning-banner" role="alert">
          <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke-width="2" aria-hidden="true">
            <path d="M10.29 3.86L1.82 18a2 2 0 0 0 1.71 3h16.94a2 2 0 0 0 1.71-3L13.71 3.86a2 2 0 0 0-3.42 0z" />
            <line x1="12" y1="9" x2="12" y2="13" />
            <line x1="12" y1="17" x2="12.01" y2="17" />
          </svg>
          <span><b>Crucial:</b> Do not power off Kiwi or disconnect Wi-Fi while firmware write is active.</span>
        </div>

        <form method="POST" action="/update" enctype="multipart/form-data" id="otaForm">
          <div class="file-picker" id="dropZone" tabindex="0" role="region" aria-label="Firmware file drop zone">
            <input type="file" name="update" accept=".bin" required id="fileIn">
            <label class="file-label" for="fileIn" id="fileLabel" tabindex="0">Choose Firmware Binary</label>
            <span class="file-name" id="fileName" aria-live="polite">Drag &amp; drop .bin file here or click to
              browse</span>
          </div>
          <div class="file-help">Accepts compiled .bin binary files (maximum 3.0 MB).</div>
          <button type="submit" id="btn" class="btn">Update Firmware</button>
        </form>

        <div id="prg" role="progressbar" aria-label="Upload progress" aria-valuemin="0" aria-valuemax="100"
          aria-valuenow="0">
          <div id="bar"></div>
        </div>
        <div id="stat" role="status" aria-live="polite"></div>

        <details class="notice-details">
          <summary>Partition Scheme &amp; Troubleshooting Guide</summary>
          <div class="notice-body">
            For OTA updates to succeed, the ESP32 partition table must have dual OTA app partitions enabled (e.g.
            <b>Minimal SPIFFS / Large APPS with OTA</b> or Kiwi dual 1.9MB custom partitions).
            <br><br>
            If your board was previously running a single <code>huge_app</code> scheme without OTA slots, please flash
            once via USB cable in the Arduino IDE before using wireless updates.
          </div>
        </details>
      </section>
    </main>
  </div>

  <script>
    (function () {
      'use strict';

      const form = document.getElementById('otaForm');
      const fileInput = document.getElementById('fileIn');
      const fileLabel = document.getElementById('fileLabel');
      const fileName = document.getElementById('fileName');
      const dropZone = document.getElementById('dropZone');
      const stat = document.getElementById('stat');
      const btn = document.getElementById('btn');
      const prg = document.getElementById('prg');
      const bar = document.getElementById('bar');
      const fwVersion = document.getElementById('fwVersion');

      // Fetch version dynamically if possible
      fetch('/status', { cache: 'no-store' })
        .then(function (r) { return r.json(); })
        .then(function (j) {
          if (j && j.version && fwVersion) {
            fwVersion.textContent = j.version;
          }
        })
        .catch(function () { });

      function handleFileSelected(file) {
        if (!file) {
          fileName.textContent = 'Drag & drop .bin file here or click to browse';
          fileName.classList.remove('set');
          return;
        }

        if (!file.name.toLowerCase().endsWith('.bin')) {
          stat.style.color = 'var(--astro)';
          stat.textContent = 'Invalid file type. Please select a compiled .bin file.';
          fileInput.value = '';
          fileName.textContent = 'Invalid file (must end in .bin)';
          fileName.classList.remove('set');
          return;
        }

        // Limit check: 3MB max for OTA partition
        if (file.size > 3145728) {
          stat.style.color = 'var(--astro)';
          stat.textContent = 'File too large (' + (file.size / 1048576).toFixed(1) + ' MB). Maximum size is 3.0 MB.';
          fileInput.value = '';
          fileName.textContent = 'File exceeds 3 MB limit';
          fileName.classList.remove('set');
          return;
        }

        stat.textContent = '';
        fileName.textContent = file.name + ' (' + (file.size / 1024).toFixed(0) + ' KB)';
        fileName.classList.add('set');
      }

      fileInput.addEventListener('change', function () {
        if (this.files && this.files.length > 0) {
          handleFileSelected(this.files[0]);
        }
      });

      // Keyboard accessibility for label and dropzone
      function triggerPicker(e) {
        if (e.key === 'Enter' || e.key === ' ') {
          e.preventDefault();
          fileInput.click();
        }
      }
      fileLabel.addEventListener('keydown', triggerPicker);
      dropZone.addEventListener('keydown', triggerPicker);

      // Drag and drop handlers
      ['dragenter', 'dragover'].forEach(function (eventName) {
        dropZone.addEventListener(eventName, function (e) {
          e.preventDefault();
          e.stopPropagation();
          dropZone.classList.add('dragover');
        });
      });

      ['dragleave', 'drop'].forEach(function (eventName) {
        dropZone.addEventListener(eventName, function (e) {
          e.preventDefault();
          e.stopPropagation();
          dropZone.classList.remove('dragover');
        });
      });

      dropZone.addEventListener('drop', function (e) {
        var dt = e.dataTransfer;
        if (dt && dt.files && dt.files.length > 0) {
          fileInput.files = dt.files;
          handleFileSelected(dt.files[0]);
        }
      });

      function pollForReboot() {
        stat.style.color = 'var(--amber)';
        stat.textContent = 'Verifying and rebooting Kiwi... Polling system status...';
        var attempts = 0;
        var maxAttempts = 30;

        var interval = setInterval(function () {
          attempts++;
          fetch('/status', { cache: 'no-store' })
            .then(function (r) {
              if (r.ok) {
                clearInterval(interval);
                stat.style.color = 'var(--ok)';
                stat.textContent = '✓ Kiwi rebooted successfully! Redirecting to Dashboard...';
                setTimeout(function () { window.location.href = '/'; }, 1200);
              }
            })
            .catch(function () {
              if (attempts >= maxAttempts) {
                clearInterval(interval);
                stat.style.color = 'var(--amber)';
                stat.textContent = 'Device is taking longer than expected to reboot. Please refresh manually.';
              }
            });
        }, 2500);
      }

      form.onsubmit = function (e) {
        e.preventDefault();
        var file = fileInput.files[0];
        if (!file) return;

        if (!file.name.toLowerCase().endsWith('.bin')) {
          stat.style.color = 'var(--astro)';
          stat.textContent = 'Only .bin files are accepted.';
          return;
        }

        var xhr = new XMLHttpRequest();
        var fd = new FormData();
        fd.append('update', file);

        prg.style.display = 'block';
        prg.setAttribute('aria-valuenow', '0');
        btn.disabled = true;
        stat.style.color = 'var(--amber)';
        stat.textContent = 'Uploading firmware binary to ESP32 flash...';

        xhr.upload.onprogress = function (pe) {
          if (pe.lengthComputable) {
            var pct = Math.round((pe.loaded / pe.total) * 100);
            bar.style.width = pct + '%';
            prg.setAttribute('aria-valuenow', String(pct));
            if (pct < 100) {
              stat.textContent = 'Uploading firmware: ' + pct + '%';
            } else {
              stat.textContent = 'Upload complete (100%). Writing to flash partition & verifying checksum...';
            }
          }
        };

        xhr.onload = function () {
          var resp = (xhr.responseText || '').trim();
          var isSuccess = (xhr.status === 200 && (resp === 'OK' || resp.indexOf('OK') !== -1 || resp === ''));
          if (isSuccess) {
            stat.style.color = 'var(--ok)';
            bar.style.width = '100%';
            prg.setAttribute('aria-valuenow', '100');
            stat.textContent = '✓ Flash write verified! Kiwi is rebooting into the new firmware...';
            // Allow 4 seconds for the ESP32 to restart and come back up, then poll
            setTimeout(pollForReboot, 4000);
          } else {
            stat.style.color = 'var(--astro)';
            stat.textContent = '✗ ' + (resp || 'Flash Update Failed (Code: ' + xhr.status + ')');
            btn.disabled = false;
          }
        };

        xhr.onerror = function () {
          stat.style.color = 'var(--astro)';
          stat.textContent = '✗ Connection lost during firmware upload. Check Wi-Fi connection.';
          btn.disabled = false;
        };

        xhr.open('POST', '/update');
        xhr.send(fd);
      };
    })();
  </script>
</body>

</html>)rawliteral";
  server.send_P(200, "text/html; charset=utf-8", OTA_HTML);
}
