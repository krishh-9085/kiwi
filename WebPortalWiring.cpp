#include "WebPortal.h"
#include "Config.h"
#include "ConfigManager.h"
#include "AudioEngine.h"
#include "FaceEngine.h"
#include "AiService.h"
#include "MemoryService.h"
#include "AnimationEngine.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Update.h>
#include <time.h>
#include <Preferences.h>
#include "WeatherService.h"

static const char WIRING_PAGE_HTML[] PROGMEM = R"RAWWIRING(<!doctype html>
<html lang="en"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=5,viewport-fit=cover">
<title>Kiwi Robot · Hardware Wiring Blueprint</title>
<link rel="icon" href="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 64 64'%3E%3Crect width='64' height='64' rx='18' fill='%230071e3'/%3E%3Ccircle cx='22' cy='27' r='6' fill='white'/%3E%3Ccircle cx='42' cy='27' r='6' fill='white'/%3E%3Cpath d='M18 42c8 7 20 7 28 0' fill='none' stroke='white' stroke-width='5' stroke-linecap='round'/%3E%3C/svg%3E">
<style>
:root{
  --bg:#f5f5f7;--surface:#ffffff;--surface-raised:rgba(255,255,255,0.92);
  --border:rgba(60,60,67,0.18);--border-subtle:rgba(0,0,0,0.08);
  --text:#1d1d1f;--secondary:#424245;--tertiary:#86868b;
  --blue:#0071e3;--blue-dark:#005bb5;--blue-subtle:rgba(0,113,227,0.08);
  --green:#34c759;--amber:#ff9500;--red:#ff3b30;--indigo:#5856d6;
  --radius-sm:8px;--radius-md:12px;--radius-lg:16px;
  --shadow-sm:0 1px 3px rgba(0,0,0,.06),0 1px 2px rgba(0,0,0,.04);
  --shadow-md:0 4px 16px rgba(0,0,0,.06),0 2px 6px rgba(0,0,0,.04);
  --shadow-card:0 2px 10px rgba(0,0,0,.04),0 1px 2px rgba(0,0,0,.02);
}
*{box-sizing:border-box;-webkit-font-smoothing:antialiased;-moz-osx-font-smoothing:grayscale}
html{scroll-behavior:smooth}
body{
  margin:0;
  font-family:-apple-system,BlinkMacSystemFont,'SF Pro Text','Helvetica Neue',sans-serif;
  font-size:14px;line-height:1.47059;
  background:var(--bg);color:var(--text);
  -webkit-text-size-adjust:100%;
}

/* ── Navigation ──────────────────────────────────────────────────────── */
nav{
  display:flex;flex-wrap:wrap;align-items:center;justify-content:space-between;
  gap:12px;padding:10px clamp(16px,3vw,32px);
  min-height:52px;height:auto;
  background:rgba(245,245,247,0.82);
  backdrop-filter:saturate(180%) blur(20px);
  -webkit-backdrop-filter:saturate(180%) blur(20px);
  border-bottom:1px solid var(--border);
  position:sticky;top:0;z-index:100;
}
.brand{display:flex;align-items:center;gap:10px;font-size:15px;font-weight:600;color:var(--text);letter-spacing:-0.01em}
.brand-badge{
  background:var(--blue);color:#fff;
  padding:3px 8px;border-radius:5px;
  font-size:10px;font-weight:700;letter-spacing:.04em;
  text-transform:uppercase;
}
.nav-actions{display:flex;align-items:center;gap:8px;flex-wrap:wrap}
a.nav-btn{
  display:inline-flex;align-items:center;
  padding:0 14px;height:32px;
  border-radius:var(--radius-sm);
  font-size:13px;font-weight:500;
  text-decoration:none;
  border:1px solid rgba(60,60,67,0.2);
  background:rgba(255,255,255,0.82);
  color:var(--text);
  transition:background 120ms ease,border-color 120ms ease;
  white-space:nowrap;
}
a.nav-btn:hover{background:#fff;border-color:rgba(60,60,67,0.3)}
a.nav-btn:active{transform:scale(0.97)}
a.nav-btn.primary{background:var(--blue);border-color:var(--blue);color:#fff}
a.nav-btn.primary:hover{background:var(--blue-dark);border-color:var(--blue-dark)}

/* ── Container ───────────────────────────────────────────────────────── */
.container{
  width:min(1240px,100%);
  margin:0 auto;
  padding:24px clamp(16px,3vw,32px) 64px;
}

/* ── Hero ────────────────────────────────────────────────────────────── */
.hero{
  background:var(--surface);
  border:1px solid var(--border);
  border-radius:var(--radius-lg);
  padding:28px clamp(20px,4vw,36px);
  margin-bottom:20px;
  box-shadow:var(--shadow-card);
}
.hero h1{
  font-size:clamp(20px,3.5vw,26px);font-weight:600;
  letter-spacing:-0.02em;margin:0 0 8px;color:var(--text);
}
.hero p{
  color:var(--secondary);font-size:14px;margin:0;line-height:1.55;
  max-width:760px;
}

/* ── Filter Bar ──────────────────────────────────────────────────────── */
.filter-bar{display:flex;gap:8px;flex-wrap:wrap;margin-bottom:18px}
.f-btn{
  background:var(--surface);border:1px solid var(--border);
  color:var(--secondary);font-size:12px;font-weight:600;font-family:inherit;
  padding:7px 14px;border-radius:var(--radius-sm);cursor:pointer;
  transition:all 120ms ease;box-shadow:var(--shadow-sm);
}
.f-btn:hover{background:var(--bg);color:var(--text);border-color:rgba(60,60,67,0.3)}
.f-btn:active{transform:scale(0.97)}
.f-btn.active{background:var(--blue);border-color:var(--blue);color:#fff}

/* ── Cards ───────────────────────────────────────────────────────────── */
.card{
  background:var(--surface);
  border:1px solid var(--border);
  border-radius:var(--radius-lg);
  padding:24px;
  margin-bottom:20px;
  box-shadow:var(--shadow-card);
}
.card-title{
  font-size:13px;font-weight:700;letter-spacing:0.04em;text-transform:uppercase;
  color:var(--tertiary);margin-bottom:16px;
  display:flex;align-items:center;justify-content:space-between;flex-wrap:wrap;gap:8px;
}

/* ── Dual Header Pinout Card ─────────────────────────────────────────── */
.pinout-split{display:grid;grid-template-columns:1fr;gap:16px}
@media(min-width:768px){.pinout-split{grid-template-columns:1fr 1fr}}
.header-box{
  background:var(--bg);
  border:1px solid var(--border-subtle);
  border-radius:var(--radius-md);
  padding:16px;
}
.header-box-title{
  font-size:11px;font-weight:700;letter-spacing:0.05em;text-transform:uppercase;
  color:var(--secondary);margin-bottom:12px;
  display:flex;align-items:center;justify-content:space-between;
}
.pin-row{
  display:flex;align-items:center;justify-content:space-between;gap:10px;
  padding:8px 10px;border-bottom:1px solid rgba(0,0,0,0.04);
  font-size:12px;transition:background 100ms ease;border-radius:6px;
}
.pin-row:last-child{border-bottom:none}
.pin-row:hover{background:rgba(0,0,0,0.03)}
.pin-left{display:flex;align-items:center;gap:10px;min-width:0;overflow:hidden}
.pin-num{
  font-family:ui-monospace,SFMono-Regular,Menlo,monospace;
  font-size:10.5px;font-weight:700;padding:2px 7px;border-radius:5px;min-width:65px;text-align:center;
}
.pin-target{color:var(--text);font-weight:500;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.pin-wire{
  font-family:ui-monospace,SFMono-Regular,Menlo,monospace;
  font-size:10.5px;padding:2px 7px;border-radius:5px;font-weight:600;flex-shrink:0;
}

/* ── SVG Schematic CAD Viewer (macOS Dark Canvas Window) ─────────────── */
.svg-wrapper{
  background:#141720;
  border:1px solid rgba(0,0,0,0.15);
  border-radius:var(--radius-md);
  overflow:hidden;
  box-shadow:var(--shadow-md);
}
.svg-toolbar{
  display:flex;align-items:center;justify-content:space-between;gap:8px;
  padding:10px 14px;background:#1e2330;border-bottom:1px solid rgba(255,255,255,0.08);
}
.zoom-btns{display:flex;gap:6px}
.z-btn{
  background:rgba(255,255,255,0.08);border:1px solid rgba(255,255,255,0.12);
  color:#e2e8f0;padding:4px 10px;border-radius:6px;font-size:12px;font-weight:600;
  cursor:pointer;transition:all 100ms ease;font-family:inherit;
}
.z-btn:hover{background:rgba(255,255,255,0.16);color:#fff}
.z-btn:active{transform:scale(0.96)}
.svg-viewport{
  width:100%;overflow-x:auto;overflow-y:hidden;-webkit-overflow-scrolling:touch;
  padding:14px;background:#0d1017;
}
.svg-viewport svg{display:block;width:100%;max-width:100%;height:auto;margin:0 auto}

/* ── Grid Layout for Module Cards ────────────────────────────────────── */
.grid-2{display:grid;grid-template-columns:1fr;gap:16px;align-items:stretch}
@media(min-width:768px){.grid-2{grid-template-columns:1fr 1fr}}
.mod-card{
  background:var(--surface);border:1px solid var(--border);
  border-radius:var(--radius-md);padding:18px;min-width:0;
  box-shadow:var(--shadow-card);
}
.mod-title{
  font-size:14px;font-weight:600;color:var(--text);
  display:flex;align-items:center;gap:8px;margin-bottom:12px;
}
.quick-heading{margin:28px 0 14px}
.quick-heading h2{margin:0;color:var(--text);font-size:18px;font-weight:600;letter-spacing:-0.01em}
.quick-heading p{margin:4px 0 0;color:var(--secondary);font-size:13px}

/* ── Tables & Sticky Horizontal Scroll ───────────────────────────────── */
.table-scroll{
  overflow-x:auto;-webkit-overflow-scrolling:touch;
  border-radius:var(--radius-md);border:1px solid var(--border);
  background:var(--surface);
}
table{width:100%;border-collapse:collapse;font-size:12px;text-align:left}
th{
  background:var(--bg);color:var(--secondary);
  font-size:11px;font-weight:700;text-transform:uppercase;letter-spacing:0.04em;
  padding:10px 14px;border-bottom:1px solid var(--border);
}
td{padding:10px 14px;border-bottom:1px solid rgba(0,0,0,0.05);color:var(--text)}
tr:last-child td{border-bottom:none}
tr:hover td{background:rgba(0,0,0,0.02)}
.sticky-col{position:sticky;left:0;background:var(--surface);z-index:2;border-right:1px solid var(--border)}
tr:hover .sticky-col{background:rgba(0,0,0,0.02)}
.mono{font-family:ui-monospace,SFMono-Regular,Menlo,monospace;font-weight:600}
.tag{display:inline-block;padding:2px 6px;border-radius:5px;font-size:10px;font-weight:600;font-family:ui-monospace,SFMono-Regular,Menlo,monospace}
.tag.pwr{background:rgba(255,59,48,0.12);color:#ff3b30}
.tag.gnd{background:rgba(142,142,147,0.15);color:#636366}
.tag.i2c{background:rgba(255,149,0,0.12);color:#d97706}
.tag.i2s{background:rgba(0,113,227,0.12);color:#0071e3}
.tag.touch{background:rgba(88,86,214,0.12);color:#5856d6}

.note-box{
  background:rgba(0,113,227,0.05);border-left:4px solid var(--blue);
  border-radius:0 var(--radius-sm) var(--radius-sm) 0;
  padding:12px 16px;margin-top:14px;font-size:13px;line-height:1.5;color:var(--secondary);
}
.note-box.warn{border-left-color:var(--amber);background:rgba(255,149,0,0.06)}
.note-box.tip{border-left-color:var(--green);background:rgba(52,199,89,0.06)}
.note-box strong{color:var(--text)}
.scroll-hint{display:block;font-size:11px;color:var(--tertiary);margin-bottom:6px;text-align:right}

@media(max-width:640px){
  nav{align-items:flex-start}
  .brand{width:100%}
  .nav-actions{width:100%;justify-content:flex-start}
  a.nav-btn{padding:0 10px}
  .hero{padding:20px 16px}
  .card{padding:16px}
  .nav-actions{gap:4px}
  a.nav-btn{padding:0 10px;font-size:12px}
}
</style>
</head><body>

<nav>
  <div class="brand">
    <span class="brand-badge">KIWI</span>
    <span>Hardware Wiring Blueprint</span>
  </div>
  <div class="nav-actions">
    <a class="nav-btn" href="/">Dashboard</a>
    <a class="nav-btn" href="/faces">Faces</a>
    <a class="nav-btn primary" href="/wiring">Wiring</a>
    <a class="nav-btn" href="/update">OTA</a>
  </div>
</nav>

<div class="container">
<div class="hero">
    <h1>Kiwi Wiring Control</h1>
    <p>Simple connection guide for the ESP32-C3, microphone, amplifier, OLED, and touch sensors.</p>
    <div class="badges">
      <span class="badge">MCU: ESP32-C3 Super Mini (160MHz)</span>
      <span class="badge cyan">Audio: Shared I2S (GPIO5/6)</span>
      <span class="badge green">Display: I2C (GPIO8/9 @ 400kHz)</span>
      <span class="badge amber">Touch: GPIO3 (Wake) + GPIO20/21</span>
    </div>
  </div>

  <!-- Interactive Category Filter -->
  <div class="filter-bar">
    <button class="f-btn active" onclick="filterMod('all')">All Pins (16)</button>
    <button class="f-btn" onclick="filterMod('audio')">🎙️ I2S Audio Bus (Mic &amp; Amp)</button>
    <button class="f-btn" onclick="filterMod('i2c')">📺 OLED Display (I2C)</button>
    <button class="f-btn" onclick="filterMod('touch')">👆 Touch Sensors (3x)</button>
    <button class="f-btn" onclick="filterMod('pwr')">⚡ Power &amp; Rails</button>
  </div>

  <!-- Physical Super Mini Dual-Header Card -->
  <div class="card wiring-visual">
    <div class="card-title">
      <span>Physical ESP32-C3 Super Mini Pinout View</span>
      <span style="font-size:11px;color:#94a3b8;font-weight:600">Left vs Right Header Orientation</span>
    </div>
    <div class="pinout-split">
      <!-- Left Header Pins 1-8 -->
      <div class="header-box">
        <div class="header-box-title">
          <span>LEFT HEADER (PINS 1–8)</span>
          <span style="font-size:10px;color:#94a3b8">USB-C at Top</span>
        </div>
        <div class="pin-row" data-cat="pwr">
          <div class="pin-left"><span class="pin-num" style="background:rgba(255,59,48,0.12);color:#ff3b30">5V (VBUS)</span><span class="pin-target">MAX98357A VIN</span></div>
          <span class="pin-wire" style="background:rgba(255,59,48,0.08);color:#ff3b30">5.0V Power</span>
        </div>
        <div class="pin-row" data-cat="pwr">
          <div class="pin-left"><span class="pin-num" style="background:rgba(142,142,147,0.15);color:#636366">GND</span><span class="pin-target">All Modules Ground</span></div>
          <span class="pin-wire" style="background:rgba(142,142,147,0.10);color:#636366">Common GND</span>
        </div>
        <div class="pin-row" data-cat="pwr">
          <div class="pin-left"><span class="pin-num" style="background:rgba(255,149,0,0.12);color:#ff9500">3V3</span><span class="pin-target">OLED, Mic, Touch VCC</span></div>
          <span class="pin-wire" style="background:rgba(255,149,0,0.08);color:#ff9500">3.3V Logic</span>
        </div>
        <div class="pin-row" data-cat="other">
          <div class="pin-left"><span class="pin-num" style="background:rgba(0,0,0,0.05);color:#8e8e93">GPIO0</span><span class="pin-target">Unused (Boot strap)</span></div>
          <span class="pin-wire" style="background:rgba(0,0,0,0.03);color:#8e8e93">Free</span>
        </div>
        <div class="pin-row" data-cat="other">
          <div class="pin-left"><span class="pin-num" style="background:rgba(0,0,0,0.05);color:#8e8e93">GPIO1</span><span class="pin-target">Unused (ADC)</span></div>
          <span class="pin-wire" style="background:rgba(0,0,0,0.03);color:#8e8e93">Free</span>
        </div>
        <div class="pin-row" data-cat="other">
          <div class="pin-left"><span class="pin-num" style="background:rgba(0,0,0,0.05);color:#8e8e93">GPIO2</span><span class="pin-target">Onboard Blue LED</span></div>
          <span class="pin-wire" style="background:rgba(0,0,0,0.03);color:#8e8e93">Internal</span>
        </div>
        <div class="pin-row" data-cat="touch">
          <div class="pin-left"><span class="pin-num" style="background:rgba(255,149,0,0.12);color:#d97706">GPIO3</span><span class="pin-target">Head Touch (Wake)</span></div>
          <span class="pin-wire" style="background:rgba(255,149,0,0.08);color:#d97706">RTC Touch #1</span>
        </div>
        <div class="pin-row" data-cat="audio">
          <div class="pin-left"><span class="pin-num" style="background:rgba(0,113,227,0.12);color:#0071e3">GPIO4</span><span class="pin-target">INMP441 Mic SD</span></div>
          <span class="pin-wire" style="background:rgba(0,113,227,0.08);color:#0071e3">I2S Audio RX</span>
        </div>
      </div>

      <!-- Right Header Pins 9-16 -->
      <div class="header-box">
        <div class="header-box-title">
          <span>RIGHT HEADER (PINS 9–16)</span>
          <span style="font-size:10px;color:#94a3b8">Antenna at Bottom</span>
        </div>
        <div class="pin-row" data-cat="audio">
          <div class="pin-left"><span class="pin-num" style="background:#083344;color:#38bdf8">GPIO5</span><span class="pin-target">Mic SCK &amp; Amp BCLK</span></div>
          <span class="pin-wire" style="background:#041c26;color:#38bdf8">Shared BCLK</span>
        </div>
        <div class="pin-row" data-cat="audio">
          <div class="pin-left"><span class="pin-num" style="background:rgba(88,86,214,0.12);color:#5856d6">GPIO6</span><span class="pin-target">Mic WS &amp; Amp LRC</span></div>
          <span class="pin-wire" style="background:#200336;color:#c084fc">Shared WS/LRC</span>
        </div>
        <div class="pin-row" data-cat="audio">
          <div class="pin-left"><span class="pin-num" style="background:rgba(255,45,85,0.12);color:#ff2d55">GPIO7</span><span class="pin-target">MAX98357A DIN</span></div>
          <span class="pin-wire" style="background:#2b0313;color:#f472b6">I2S Audio TX</span>
        </div>
        <div class="pin-row" data-cat="i2c">
          <div class="pin-left"><span class="pin-num" style="background:#422006;color:#facc15">GPIO8</span><span class="pin-target">SSD1306 OLED SDA</span></div>
          <span class="pin-wire" style="background:#261202;color:#facc15">I2C Data</span>
        </div>
        <div class="pin-row" data-cat="i2c">
          <div class="pin-left"><span class="pin-num" style="background:#052e16;color:#4ade80">GPIO9</span><span class="pin-target">SSD1306 OLED SCL</span></div>
          <span class="pin-wire" style="background:#031c0c;color:#4ade80">I2C Clock</span>
        </div>
        <div class="pin-row" data-cat="other">
          <div class="pin-left"><span class="pin-num" style="background:rgba(0,0,0,0.05);color:#8e8e93">GPIO10</span><span class="pin-target">Unused (FSPIQ)</span></div>
          <span class="pin-wire" style="background:rgba(0,0,0,0.03);color:#8e8e93">Free</span>
        </div>
        <div class="pin-row" data-cat="touch">
          <div class="pin-left"><span class="pin-num" style="background:rgba(255,149,0,0.12);color:#d97706">GPIO20</span><span class="pin-target">Left Cheek Touch</span></div>
          <span class="pin-wire" style="background:rgba(255,149,0,0.08);color:#d97706">Touch #2</span>
        </div>
        <div class="pin-row" data-cat="touch">
          <div class="pin-left"><span class="pin-num" style="background:rgba(255,149,0,0.12);color:#d97706">GPIO21</span><span class="pin-target">Right Cheek Touch</span></div>
          <span class="pin-wire" style="background:rgba(255,149,0,0.08);color:#d97706">Touch #3</span>
        </div>
      </div>
    </div>
  </div>

  <!-- Interactive SVG Architecture Schematic with Responsive Zoom Deck -->
  <div class="card wiring-visual">
    <div class="card-title">
      <span>Interactive Visual Schematic Diagram</span>
      <span style="font-size:11px;color:#94a3b8;font-weight:600">Auto-scales to screen · Zoom &amp; Pan enabled</span>
    </div>
    <div class="svg-wrapper">
      <div class="svg-toolbar">
        <span>Schematic View Controls</span>
        <div class="zoom-btns">
          <button class="z-btn" onclick="zoomSvg(1.25)">+ Zoom In</button>
          <button class="z-btn" onclick="zoomSvg(0.8)">&minus; Zoom Out</button>
          <button class="z-btn" onclick="resetSvg()">&#8634; Reset</button>
        </div>
      </div>
      <div class="svg-viewport" id="svgBox">
        <svg id="schematicSvg" viewBox="0 0 980 560" xmlns="http://www.w3.org/2000/svg">
          <defs>
            <filter id="glow" x="-20%" y="-20%" width="140%" height="140%">
              <feGaussianBlur stdDeviation="3" result="blur" />
              <feComposite in="SourceGraphic" in2="blur" operator="over" />
            </filter>
          </defs>

          <!-- Subtle Background Grid -->
          <g stroke="#0c121d" stroke-width="1">
            <line x1="0" y1="90" x2="980" y2="90" />
            <line x1="0" y1="180" x2="980" y2="180" />
            <line x1="0" y1="270" x2="980" y2="270" />
            <line x1="0" y1="360" x2="980" y2="360" />
            <line x1="0" y1="450" x2="980" y2="450" />
            <line x1="160" y1="0" x2="160" y2="560" />
            <line x1="320" y1="0" x2="320" y2="560" />
            <line x1="490" y1="0" x2="490" y2="560" />
            <line x1="660" y1="0" x2="660" y2="560" />
            <line x1="820" y1="0" x2="820" y2="560" />
          </g>

          <!-- ==================== WIRE TRACES (PERFECTLY ALIGNED) ==================== -->

          <!-- 1. 5V Rail (Red) -> ESP32 5V (410, 70) to MAX98357A VIN (710, 305) -->
          <path d="M 410 70 H 380 V 12 H 680 V 305 H 710" fill="none" stroke="#ef4444" stroke-width="2.5" />

          <!-- 2. 3.3V Power Rail (Orange) -> ESP32 3V3 (410, 130) to OLED VCC (270, 90), Touch VCC (270, 455), Mic VDD (710, 65) -->
          <path d="M 410 130 H 350" fill="none" stroke="#f97316" stroke-width="2.5" />
          <path d="M 350 130 V 90 H 270" fill="none" stroke="#f97316" stroke-width="2.5" />
          <path d="M 350 130 V 455 H 270" fill="none" stroke="#f97316" stroke-width="2.5" />
          <path d="M 350 90 V 22 H 670 V 65 H 710" fill="none" stroke="#f97316" stroke-width="2.5" />
          <circle cx="350" cy="130" r="3.5" fill="#f97316" />
          <circle cx="350" cy="90" r="3.5" fill="#f97316" />

          <!-- 3. GND Bus (Slate Grey Dashed) -> ESP32 GND (410, 100) to OLED (270, 60), Touch (270, 490), Mic (710, 95), Amp (710, 335) -->
          <path d="M 410 100 H 335" fill="none" stroke="#64748b" stroke-width="2" stroke-dasharray="4 3" />
          <path d="M 335 100 V 60 H 270" fill="none" stroke="#64748b" stroke-width="2" stroke-dasharray="4 3" />
          <path d="M 335 100 V 490 H 270" fill="none" stroke="#64748b" stroke-width="2" stroke-dasharray="4 3" />
          <path d="M 335 60 V 32 H 660 V 95 H 710" fill="none" stroke="#64748b" stroke-width="2" stroke-dasharray="4 3" />
          <path d="M 660 95 V 335 H 710" fill="none" stroke="#64748b" stroke-width="2" stroke-dasharray="4 3" />
          <circle cx="335" cy="100" r="3" fill="#64748b" />
          <circle cx="335" cy="60" r="3" fill="#64748b" />
          <circle cx="660" cy="95" r="3" fill="#64748b" />

          <!-- 4. Mic SD (Blue) -> ESP32 IO4 (410, 280) to INMP441 SD (710, 125) -->
          <path d="M 410 280 H 370 V 510 H 690 V 125 H 710" fill="none" stroke="#38bdf8" stroke-width="2.5" />

          <!-- 5. Shared I2S BCLK (Cyan) -> ESP32 IO5 (570, 130) to Mic SCK (710, 155) & Amp BCLK (710, 395) -->
          <path d="M 570 130 H 635 V 155 H 710" fill="none" stroke="#06b6d4" stroke-width="2.5" />
          <path d="M 635 155 V 395 H 710" fill="none" stroke="#06b6d4" stroke-width="2.5" />
          <circle cx="635" cy="155" r="4" fill="#06b6d4" />

          <!-- 6. Shared I2S WS/LRC (Purple) -> ESP32 IO6 (570, 160) to Mic WS (710, 185) & Amp LRC (710, 425) -->
          <path d="M 570 160 H 648 V 185 H 710" fill="none" stroke="#c084fc" stroke-width="2.5" />
          <path d="M 648 185 V 425 H 710" fill="none" stroke="#c084fc" stroke-width="2.5" />
          <circle cx="648" cy="185" r="4" fill="#c084fc" />

          <!-- 7. Amp DIN (Pink) -> ESP32 IO7 (570, 190) to MAX98357A DIN (710, 365) -->
          <path d="M 570 190 H 660 V 365 H 710" fill="none" stroke="#f472b6" stroke-width="2.5" />

          <!-- 8. I2C SDA (Yellow) -> ESP32 IO8 (570, 220) to OLED SDA (270, 150) -->
          <path d="M 570 220 H 620 V 525 H 300 V 150 H 270" fill="none" stroke="#facc15" stroke-width="2.5" />

          <!-- 9. I2C SCL (Green) -> ESP32 IO9 (570, 250) to OLED SCL (270, 120) -->
          <path d="M 570 250 H 605 V 540 H 285 V 120 H 270" fill="none" stroke="#4ade80" stroke-width="2.5" />

          <!-- 10. Touch Top / Wake (Amber) -> ESP32 IO3 (410, 250) to Touch TOP (270, 350) -->
          <path d="M 410 250 H 360 V 350 H 270" fill="none" stroke="#fbbf24" stroke-width="2.5" />

          <!-- 11. Touch Left Cheek (Amber) -> ESP32 IO20 (570, 310) to Touch LEFT (270, 385) -->
          <path d="M 570 310 H 590 V 475 H 315 V 385 H 270" fill="none" stroke="#fbbf24" stroke-width="2" />

          <!-- 12. Touch Right Cheek (Amber) -> ESP32 IO21 (570, 340) to Touch RIGHT (270, 420) -->
          <path d="M 570 340 H 582 V 490 H 305 V 420 H 270" fill="none" stroke="#fbbf24" stroke-width="2" />

          <!-- ==================== MODULE 1: SSD1306 128x64 OLED (TOP LEFT) ==================== -->
          <rect x="40" y="30" width="230" height="160" rx="10" fill="#0c1017" stroke="#1f2d47" stroke-width="1.5" />
          <rect x="55" y="45" width="120" height="70" rx="4" fill="#030712" stroke="#2563eb" stroke-width="1" />
          <text x="115" y="76" fill="#38bdf8" font-size="11" font-family="monospace" text-anchor="middle" font-weight="700">SSD1306 OLED</text>
          <text x="115" y="93" fill="#64748b" font-size="9" font-family="monospace" text-anchor="middle">128x64 (0x3C)</text>

          <!-- OLED Pin Circles & Labels -->
          <circle cx="270" cy="60" r="4.5" fill="#64748b" />
          <text x="255" y="64" fill="#94a3b8" font-size="10" font-family="monospace" text-anchor="end">GND</text>

          <circle cx="270" cy="90" r="4.5" fill="#f97316" />
          <text x="255" y="94" fill="#f97316" font-size="10" font-family="monospace" font-weight="700" text-anchor="end">VCC (3.3V)</text>

          <circle cx="270" cy="120" r="4.5" fill="#4ade80" />
          <text x="255" y="124" fill="#4ade80" font-size="10" font-family="monospace" font-weight="700" text-anchor="end">SCL (GPIO9)</text>

          <circle cx="270" cy="150" r="4.5" fill="#facc15" />
          <text x="255" y="154" fill="#facc15" font-size="10" font-family="monospace" font-weight="700" text-anchor="end">SDA (GPIO8)</text>

          <!-- ==================== MODULE 2: 3x TOUCH SENSORS (BOTTOM LEFT) ==================== -->
          <rect x="40" y="310" width="230" height="210" rx="10" fill="#0c1017" stroke="#1f2d47" stroke-width="1.5" />
          <text x="155" y="333" fill="#f8fafc" font-size="12" font-weight="700" text-anchor="middle">3x Touch Sensors (TTP223)</text>

          <!-- Visual Touch Badges -->
          <rect x="55" y="348" width="55" height="22" rx="4" fill="#171e2e" stroke="#2a3952" stroke-width="1" />
          <text x="82" y="363" fill="#fbbf24" font-size="9" font-family="monospace" text-anchor="middle">👆 HEAD</text>
          <rect x="55" y="383" width="55" height="22" rx="4" fill="#171e2e" stroke="#2a3952" stroke-width="1" />
          <text x="82" y="398" fill="#fbbf24" font-size="9" font-family="monospace" text-anchor="middle">👈 L-CHEEK</text>
          <rect x="55" y="418" width="55" height="22" rx="4" fill="#171e2e" stroke="#2a3952" stroke-width="1" />
          <text x="82" y="433" fill="#fbbf24" font-size="9" font-family="monospace" text-anchor="middle">👉 R-CHEEK</text>

          <!-- Touch Pin Circles & Labels -->
          <circle cx="270" cy="350" r="4.5" fill="#fbbf24" />
          <text x="255" y="354" fill="#fbbf24" font-size="10" font-family="monospace" font-weight="700" text-anchor="end">TOP (Head) &rarr; IO3</text>

          <circle cx="270" cy="385" r="4.5" fill="#fbbf24" />
          <text x="255" y="389" fill="#fbbf24" font-size="10" font-family="monospace" font-weight="700" text-anchor="end">LEFT &rarr; IO20</text>

          <circle cx="270" cy="420" r="4.5" fill="#fbbf24" />
          <text x="255" y="424" fill="#fbbf24" font-size="10" font-family="monospace" font-weight="700" text-anchor="end">RIGHT &rarr; IO21</text>

          <circle cx="270" cy="455" r="4.5" fill="#f97316" />
          <text x="255" y="459" fill="#f97316" font-size="10" font-family="monospace" text-anchor="end">VCC &rarr; 3.3V</text>

          <circle cx="270" cy="490" r="4.5" fill="#64748b" />
          <text x="255" y="494" fill="#94a3b8" font-size="10" font-family="monospace" text-anchor="end">GND &rarr; GND</text>

          <!-- ==================== CENTRAL MCU: ESP32-C3 SUPER MINI ==================== -->
          <rect x="410" y="40" width="160" height="340" rx="12" fill="#070a10" stroke="#334155" stroke-width="2" />
          <!-- USB-C Port -->
          <rect x="450" y="22" width="80" height="24" rx="4" fill="#1e293b" stroke="#475569" stroke-width="1" />
          <text x="490" y="38" fill="#cbd5e1" font-size="9" font-family="monospace" text-anchor="middle">USB-C (5V)</text>

          <!-- Microcontroller IC -->
          <rect x="440" y="145" width="100" height="110" rx="6" fill="#020408" stroke="#1e293b" stroke-width="1.5" />
          <text x="490" y="195" fill="#f8fafc" font-size="11" font-family="monospace" text-anchor="middle" font-weight="700">ESP32-C3</text>
          <text x="490" y="212" fill="#64748b" font-size="9" font-family="monospace" text-anchor="middle">SUPER MINI</text>
          <circle cx="490" cy="115" r="3.5" fill="#3b82f6" filter="url(#glow)" />
          <text x="490" y="360" fill="#94a3b8" font-size="9" font-family="monospace" text-anchor="middle">PCB ANTENNA</text>

          <!-- Left Header Pins (Pins 1–8) -->
          <circle cx="410" cy="70" r="5" fill="#ef4444" />
          <text x="422" y="74" fill="#ef4444" font-size="9" font-family="monospace" font-weight="700">5V (VBUS)</text>

          <circle cx="410" cy="100" r="5" fill="#64748b" />
          <text x="422" y="104" fill="#94a3b8" font-size="9" font-family="monospace">GND</text>

          <circle cx="410" cy="130" r="5" fill="#f97316" />
          <text x="422" y="134" fill="#f97316" font-size="9" font-family="monospace" font-weight="700">3V3</text>

          <circle cx="410" cy="160" r="4" fill="#334155" />
          <text x="422" y="164" fill="#64748b" font-size="9" font-family="monospace">IO0</text>

          <circle cx="410" cy="190" r="4" fill="#334155" />
          <text x="422" y="194" fill="#64748b" font-size="9" font-family="monospace">IO1</text>

          <circle cx="410" cy="220" r="4" fill="#334155" />
          <text x="422" y="224" fill="#64748b" font-size="9" font-family="monospace">IO2 (LED)</text>

          <circle cx="410" cy="250" r="5" fill="#fbbf24" />
          <text x="422" y="254" fill="#fbbf24" font-size="9" font-family="monospace" font-weight="700">IO3 (Top)</text>

          <circle cx="410" cy="280" r="5" fill="#38bdf8" />
          <text x="422" y="284" fill="#38bdf8" font-size="9" font-family="monospace" font-weight="700">IO4 (MIC SD)</text>

          <!-- Right Header Pins (Pins 9–16) with text-anchor="end" to avoid clipping -->
          <circle cx="570" cy="130" r="5" fill="#06b6d4" />
          <text x="558" y="134" fill="#06b6d4" font-size="9" font-family="monospace" font-weight="700" text-anchor="end">IO5 (BCLK)</text>

          <circle cx="570" cy="160" r="5" fill="#c084fc" />
          <text x="558" y="164" fill="#c084fc" font-size="9" font-family="monospace" font-weight="700" text-anchor="end">IO6 (WS/LRC)</text>

          <circle cx="570" cy="190" r="5" fill="#f472b6" />
          <text x="558" y="194" fill="#f472b6" font-size="9" font-family="monospace" font-weight="700" text-anchor="end">IO7 (AMP DIN)</text>

          <circle cx="570" cy="220" r="5" fill="#facc15" />
          <text x="558" y="224" fill="#facc15" font-size="9" font-family="monospace" font-weight="700" text-anchor="end">IO8 (SDA)</text>

          <circle cx="570" cy="250" r="5" fill="#4ade80" />
          <text x="558" y="254" fill="#4ade80" font-size="9" font-family="monospace" font-weight="700" text-anchor="end">IO9 (SCL)</text>

          <circle cx="570" cy="280" r="4" fill="#334155" />
          <text x="558" y="284" fill="#64748b" font-size="9" font-family="monospace" text-anchor="end">IO10</text>

          <circle cx="570" cy="310" r="5" fill="#fbbf24" />
          <text x="558" y="314" fill="#fbbf24" font-size="9" font-family="monospace" font-weight="700" text-anchor="end">IO20 (Left)</text>

          <circle cx="570" cy="340" r="5" fill="#fbbf24" />
          <text x="558" y="344" fill="#fbbf24" font-size="9" font-family="monospace" font-weight="700" text-anchor="end">IO21 (Right)</text>

          <!-- ==================== MODULE 3: INMP441 MIC (TOP RIGHT) ==================== -->
          <rect x="710" y="30" width="230" height="185" rx="10" fill="#0c1017" stroke="#1f2d47" stroke-width="1.5" />
          <text x="825" y="52" fill="#f8fafc" font-size="12" font-weight="700" text-anchor="middle">INMP441 I2S Mic (RX)</text>

          <!-- Mic Pin Circles & Labels -->
          <circle cx="710" cy="65" r="4.5" fill="#f97316" />
          <text x="725" y="69" fill="#f97316" font-size="10" font-family="monospace">VDD &rarr; 3.3V</text>

          <circle cx="710" cy="95" r="4.5" fill="#64748b" />
          <text x="725" y="99" fill="#94a3b8" font-size="10" font-family="monospace">GND &rarr; GND</text>

          <circle cx="710" cy="125" r="4.5" fill="#38bdf8" />
          <text x="725" y="129" fill="#38bdf8" font-size="10" font-family="monospace" font-weight="700">SD  &rarr; GPIO4 (Data)</text>

          <circle cx="710" cy="155" r="4.5" fill="#06b6d4" />
          <text x="725" y="159" fill="#06b6d4" font-size="10" font-family="monospace" font-weight="700">SCK &rarr; GPIO5 (BCLK)</text>

          <circle cx="710" cy="185" r="4.5" fill="#c084fc" />
          <text x="725" y="189" fill="#c084fc" font-size="10" font-family="monospace" font-weight="700">WS  &rarr; GPIO6 (LRC)</text>

          <text x="825" y="205" fill="#64748b" font-size="9" font-family="monospace" text-anchor="middle">L/R &rarr; GND (Left Ch)</text>

          <!-- ==================== MODULE 4: MAX98357A AMP (BOTTOM RIGHT) ==================== -->
          <rect x="710" y="270" width="230" height="245" rx="10" fill="#0c1017" stroke="#1f2d47" stroke-width="1.5" />
          <text x="825" y="292" fill="#f8fafc" font-size="12" font-weight="700" text-anchor="middle">MAX98357A Amp (TX)</text>

          <!-- Amp Pin Circles & Labels -->
          <circle cx="710" cy="305" r="4.5" fill="#ef4444" />
          <text x="725" y="309" fill="#ef4444" font-size="10" font-family="monospace" font-weight="700">VIN  &rarr; 5V (VBUS)</text>

          <circle cx="710" cy="335" r="4.5" fill="#64748b" />
          <text x="725" y="339" fill="#94a3b8" font-size="10" font-family="monospace">GND  &rarr; GND</text>

          <circle cx="710" cy="365" r="4.5" fill="#f472b6" />
          <text x="725" y="369" fill="#f472b6" font-size="10" font-family="monospace" font-weight="700">DIN  &rarr; GPIO7 (Data)</text>

          <circle cx="710" cy="395" r="4.5" fill="#06b6d4" />
          <text x="725" y="399" fill="#06b6d4" font-size="10" font-family="monospace" font-weight="700">BCLK &rarr; GPIO5 (Shared)</text>

          <circle cx="710" cy="425" r="4.5" fill="#c084fc" />
          <text x="725" y="429" fill="#c084fc" font-size="10" font-family="monospace" font-weight="700">LRC  &rarr; GPIO6 (Shared)</text>

          <circle cx="710" cy="455" r="4.5" fill="#64748b" />
          <text x="725" y="459" fill="#64748b" font-size="10" font-family="monospace">GAIN &rarr; GND (12dB)</text>

          <!-- Mini Speaker Graphic -->
          <rect x="835" y="445" width="85" height="50" rx="4" fill="#020408" stroke="#334155" stroke-width="1" />
          <text x="877" y="468" fill="#38bdf8" font-size="10" font-family="monospace" text-anchor="middle" font-weight="700">🔊 SPEAKER</text>
          <text x="877" y="484" fill="#64748b" font-size="9" font-family="monospace" text-anchor="middle">8Ω 2W Mini</text>
        </svg>
      </div>
    </div>
  </div>

  <!-- COMPLETE WIRING MATRIX TABLE -->
  <div class="card wiring-matrix">
    <div class="card-title">
      <span>Master Pinout Assignment &amp; Voltage Matrix</span>
      <span style="font-size:11px;color:#94a3b8;font-weight:600">16 ESP32-C3 Pins</span>
    </div>
    <span class="scroll-hint">&larr; Swipe horizontally to view voltage &amp; notes &rarr;</span>
    <div class="table-scroll">
      <table id="matrixTable">
        <thead>
          <tr>
            <th class="sticky-col">ESP32-C3 Pin</th>
            <th>Header Side</th>
            <th>Target Module</th>
            <th>Module Pin</th>
            <th>Signal Type</th>
            <th>Voltage</th>
            <th>Hardware Functional Notes</th>
          </tr>
        </thead>
        <tbody>
          <tr data-cat="pwr">
            <td class="sticky-col mono" style="color:#ef4444">5V (VBUS)</td>
            <td>Left #1</td>
            <td>MAX98357A Amplifier</td>
            <td class="mono">VIN</td>
            <td><span class="tag pwr">POWER</span></td>
            <td>5.0V</td>
            <td>Supplies high-current power for 3.2W audio amplifier. Avoids 3.3V rail sagging.</td>
          </tr>
          <tr data-cat="pwr">
            <td class="sticky-col mono" style="color:#94a3b8">GND</td>
            <td>Left #2</td>
            <td>All Modules (OLED, Mic, Amp, Touch)</td>
            <td class="mono">GND / L/R / GAIN</td>
            <td><span class="tag gnd">GROUND</span></td>
            <td>0V</td>
            <td>Common ground bus. Tie INMP441 L/R and MAX98357A GAIN to this ground.</td>
          </tr>
          <tr data-cat="pwr">
            <td class="sticky-col mono" style="color:#f97316">3V3</td>
            <td>Left #3</td>
            <td>SSD1306 OLED, INMP441 Mic, 3x Touch</td>
            <td class="mono">VCC / VDD</td>
            <td><span class="tag pwr">POWER</span></td>
            <td>3.3V</td>
            <td>Main 3.3V logic rail. <strong>DO NOT connect INMP441 to 5V!</strong></td>
          </tr>
          <tr data-cat="other">
            <td class="sticky-col mono">GPIO0</td>
            <td>Left #4</td>
            <td>Unused (Strapping)</td>
            <td class="mono">—</td>
            <td><span class="tag gnd">STRAP</span></td>
            <td>3.3V</td>
            <td>Boot mode pin. Kept free for clean boot sequence.</td>
          </tr>
          <tr data-cat="other">
            <td class="sticky-col mono">GPIO1</td>
            <td>Left #5</td>
            <td>Unused</td>
            <td class="mono">—</td>
            <td><span class="tag gnd">ADC</span></td>
            <td>3.3V</td>
            <td>Available for external expansion / analog sensors.</td>
          </tr>
          <tr data-cat="other">
            <td class="sticky-col mono">GPIO2</td>
            <td>Left #6</td>
            <td>Onboard Blue LED</td>
            <td class="mono">LED</td>
            <td><span class="tag gnd">GPIO</span></td>
            <td>3.3V</td>
            <td>Internal status LED (active LOW / HIGH depending on board revision).</td>
          </tr>
          <tr data-cat="touch">
            <td class="sticky-col mono" style="color:#fbbf24">GPIO3</td>
            <td>Left #7</td>
            <td>Touch Sensor #1 (Top / Head)</td>
            <td class="mono">SIG / OUT</td>
            <td><span class="tag touch">TOUCH / WAKE</span></td>
            <td>3.3V</td>
            <td><strong>Head Tap / Action Sensor.</strong> Deep-sleep RTC wakeup pin. Active HIGH.</td>
          </tr>
          <tr data-cat="audio">
            <td class="sticky-col mono" style="color:#38bdf8">GPIO4</td>
            <td>Left #8</td>
            <td>INMP441 Microphone</td>
            <td class="mono">SD</td>
            <td><span class="tag i2s">I2S AUDIO RX</span></td>
            <td>3.3V</td>
            <td>Serial audio data in from microphone (24kHz 16-bit mono stream).</td>
          </tr>
          <tr data-cat="audio">
            <td class="sticky-col mono" style="color:#06b6d4">GPIO5</td>
            <td>Right #9</td>
            <td>INMP441 Mic &amp; MAX98357A Amp</td>
            <td class="mono">SCK &amp; BCLK</td>
            <td><span class="tag i2s">I2S BCLK</span></td>
            <td>3.3V</td>
            <td><strong>SHARED BIT CLOCK:</strong> Connect both INMP441 SCK and MAX98357A BCLK here!</td>
          </tr>
          <tr data-cat="audio">
            <td class="sticky-col mono" style="color:#c084fc">GPIO6</td>
            <td>Right #10</td>
            <td>INMP441 Mic &amp; MAX98357A Amp</td>
            <td class="mono">WS &amp; LRC</td>
            <td><span class="tag i2s">I2S WS/LRC</span></td>
            <td>3.3V</td>
            <td><strong>SHARED WORD SELECT:</strong> Connect both INMP441 WS and MAX98357A LRC here!</td>
          </tr>
          <tr data-cat="audio">
            <td class="sticky-col mono" style="color:#f472b6">GPIO7</td>
            <td>Right #11</td>
            <td>MAX98357A Amplifier</td>
            <td class="mono">DIN</td>
            <td><span class="tag i2s">I2S AUDIO TX</span></td>
            <td>3.3V</td>
            <td>Serial audio data out to amplifier DAC.</td>
          </tr>
          <tr data-cat="i2c">
            <td class="sticky-col mono" style="color:#facc15">GPIO8</td>
            <td>Right #12</td>
            <td>SSD1306 128x64 OLED Display</td>
            <td class="mono">SDA</td>
            <td><span class="tag i2c">I2C DATA</span></td>
            <td>3.3V</td>
            <td>I2C Data line for display graphics (400kHz Fast Mode).</td>
          </tr>
          <tr data-cat="i2c">
            <td class="sticky-col mono" style="color:#4ade80">GPIO9</td>
            <td>Right #13</td>
            <td>SSD1306 128x64 OLED Display</td>
            <td class="mono">SCL</td>
            <td><span class="tag i2c">I2C CLK</span></td>
            <td>3.3V</td>
            <td>I2C Clock line for display graphics.</td>
          </tr>
          <tr data-cat="other">
            <td class="sticky-col mono">GPIO10</td>
            <td>Right #14</td>
            <td>Unused (FSPIQ)</td>
            <td class="mono">—</td>
            <td><span class="tag gnd">GPIO</span></td>
            <td>3.3V</td>
            <td>Free GPIO.</td>
          </tr>
          <tr data-cat="touch">
            <td class="sticky-col mono" style="color:#fbbf24">GPIO20</td>
            <td>Right #15</td>
            <td>Touch Sensor #2 (Left Cheek)</td>
            <td class="mono">SIG / OUT</td>
            <td><span class="tag touch">TOUCH / UART_RX</span></td>
            <td>3.3V</td>
            <td><strong>Left Cheek Pet / Tickle Sensor.</strong> Active HIGH.</td>
          </tr>
          <tr data-cat="touch">
            <td class="sticky-col mono" style="color:#fbbf24">GPIO21</td>
            <td>Right #16</td>
            <td>Touch Sensor #3 (Right Cheek)</td>
            <td class="mono">SIG / OUT</td>
            <td><span class="tag touch">TOUCH / UART_TX</span></td>
            <td>3.3V</td>
            <td><strong>Right Cheek Pet / Tickle Sensor.</strong> Active HIGH.</td>
          </tr>
        </tbody>
      </table>
    </div>
  </div>

  <!-- MODULE BY MODULE DETAIL CARDS -->
  <div class="quick-heading">
    <h2>Quick Connections</h2>
    <p>Connect each module exactly as shown below. Shared clock wires are marked clearly.</p>
  </div>
  <div class="grid-2">
    <!-- INMP441 Microphone -->
    <div class="mod-card" data-cat="audio">
      <div class="mod-title">
        <span style="font-size:18px">🎙️</span>
        <span>INMP441 Omnidirectional MEMS Mic</span>
      </div>
      <table style="margin-bottom:12px;min-width:auto">
        <tr><th>Module Pin</th><th>Connect To</th><th>Function</th></tr>
        <tr><td class="mono">VDD</td><td class="mono" style="color:#f97316">3.3V</td><td>Power (3.3V ONLY)</td></tr>
        <tr><td class="mono">GND</td><td class="mono" style="color:#94a3b8">GND</td><td>Common Ground</td></tr>
        <tr><td class="mono">SD</td><td class="mono" style="color:#38bdf8">GPIO4</td><td>Serial Audio Data Out</td></tr>
        <tr><td class="mono">SCK</td><td class="mono" style="color:#06b6d4">GPIO5</td><td>Serial Clock (Shared BCLK)</td></tr>
        <tr><td class="mono">WS</td><td class="mono" style="color:#c084fc">GPIO6</td><td>Word Select (Shared LRC)</td></tr>
        <tr><td class="mono">L/R</td><td class="mono" style="color:#94a3b8">GND</td><td><strong>Left Channel (MANDATORY)</strong></td></tr>
      </table>
      <div class="note-box warn">
        <strong>Critical INMP441 Rule:</strong> The <code>L/R</code> pin MUST be connected directly to <code>GND</code>. Leaving it floating causes channel desync and silence during voice transcription. Never connect VDD to 5V; it will fry the sensor!
      </div>
    </div>

    <!-- MAX98357A Amplifier -->
    <div class="mod-card" data-cat="audio">
      <div class="mod-title">
        <span style="font-size:18px">🔊</span>
        <span>MAX98357A 3.2W Class-D Audio Amp</span>
      </div>
      <table style="margin-bottom:12px;min-width:auto">
        <tr><th>Module Pin</th><th>Connect To</th><th>Function</th></tr>
        <tr><td class="mono">VIN</td><td class="mono" style="color:#ef4444">5V (VBUS)</td><td>Power for 3.2W Output</td></tr>
        <tr><td class="mono">GND</td><td class="mono" style="color:#94a3b8">GND</td><td>Common Ground</td></tr>
        <tr><td class="mono">DIN</td><td class="mono" style="color:#f472b6">GPIO7</td><td>Serial Audio Data In</td></tr>
        <tr><td class="mono">BCLK</td><td class="mono" style="color:#06b6d4">GPIO5</td><td>Bit Clock (Shared BCLK)</td></tr>
        <tr><td class="mono">LRC</td><td class="mono" style="color:#c084fc">GPIO6</td><td>Word Select (Shared WS)</td></tr>
        <tr><td class="mono">GAIN</td><td class="mono" style="color:#94a3b8">GND</td><td>12dB Gain setting</td></tr>
        <tr><td class="mono">SPK + / -</td><td class="mono" style="color:#38bdf8">Speaker</td><td>Connect 8Ω 1.5W–3W Mini Speaker</td></tr>
      </table>
      <div class="note-box tip">
        <strong>Power Recommendation:</strong> Powering the MAX98357A from the ESP32-C3 <code>5V</code> pin (USB VBUS) ensures crisp, loud speech synthesis without causing brownouts on the ESP32 logic rail.
      </div>
    </div>

    <!-- SSD1306 OLED Display -->
    <div class="mod-card" data-cat="i2c">
      <div class="mod-title">
        <span style="font-size:18px">📺</span>
        <span>0.96" SSD1306 128x64 I2C OLED</span>
      </div>
      <table style="margin-bottom:12px;min-width:auto">
        <tr><th>Module Pin</th><th>Connect To</th><th>Function</th></tr>
        <tr><td class="mono">GND</td><td class="mono" style="color:#94a3b8">GND</td><td>Common Ground</td></tr>
        <tr><td class="mono">VCC</td><td class="mono" style="color:#f97316">3.3V</td><td>Power Supply</td></tr>
        <tr><td class="mono">SCL</td><td class="mono" style="color:#4ade80">GPIO9</td><td>I2C Clock (SCL)</td></tr>
        <tr><td class="mono">SDA</td><td class="mono" style="color:#facc15">GPIO8</td><td>I2C Data (SDA)</td></tr>
      </table>
      <div class="note-box">
        <strong>Hardware Address:</strong> Default I2C address is <code>0x3C</code>. The ESP32-C3 internal pull-ups on GPIO8 and GPIO9 are enabled automatically by the firmware driver.
      </div>
    </div>

    <!-- Capacitive Touch Sensors -->
    <div class="mod-card" data-cat="touch">
      <div class="mod-title">
        <span style="font-size:18px">👆</span>
        <span>3x Touch Sensors (TTP223 / Wire)</span>
      </div>
      <table style="margin-bottom:12px;min-width:auto">
        <tr><th>Sensor Location</th><th>ESP32 Pin</th><th>Gesture / Action</th></tr>
        <tr><td>Head (Top)</td><td class="mono" style="color:#fbbf24">GPIO3</td><td>Tap: Voice / Hold: Sleep</td></tr>
        <tr><td>Left Cheek</td><td class="mono" style="color:#fbbf24">GPIO20</td><td>Tap/Pet: Giggles, blushing</td></tr>
        <tr><td>Right Cheek</td><td class="mono" style="color:#fbbf24">GPIO21</td><td>Tap/Pet: Love chimes</td></tr>
        <tr><td>Both Cheeks</td><td class="mono" style="color:#fbbf24">GPIO20+21</td><td>Dual hold 600ms: Surprise</td></tr>
      </table>
      <div class="note-box tip">
        <strong>TTP223 Configuration:</strong> Leave jumpers A and B open (Active HIGH, Momentary mode).
      </div>
    </div>
  </div>

  <!-- Shared Clock Bus Architecture Detail -->
  <div class="card wiring-deep-dive" style="margin-top:18px">
    <div class="card-title">
      <span>Deep Dive: Shared I2S Audio Bus Architecture</span>
      <span class="tag i2s">I2S FULL-DUPLEX</span>
    </div>
    <p style="color:var(--muted);font-size:12px;line-height:1.6">
      The ESP32-C3 has one I2S peripheral controller. In Kiwi's firmware, we achieve both audio capture (from the INMP441) and audio playback (through the MAX98357A) by sharing the Bit Clock (<code>GPIO5</code>) and Word Select / LR Clock (<code>GPIO6</code>).
    </p>
    <div style="background:#05070a;border:1px solid #1c273c;border-radius:8px;padding:12px 14px;font-family:monospace;font-size:11px;color:#38bdf8;line-height:1.6;overflow-x:auto">
      ESP32-C3 GPIO5 (BCLK) ──┬──> INMP441 SCK (Mic Bit Clock)<br>
      &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;└──> MAX98357A BCLK (Amp Bit Clock)<br>
      <br>
      ESP32-C3 GPIO6 (WS)&nbsp;&nbsp;──┬──> INMP441 WS (Mic Word Select)<br>
      &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;└──> MAX98357A LRC (Amp Frame Sync)
    </div>
    <div class="note-box">
      <strong>Physical Wiring Guide:</strong> Solder or twist the INMP441 <code>SCK</code> wire and MAX98357A <code>BCLK</code> wire together and plug into <code>GPIO5</code>. Do the same for INMP441 <code>WS</code> and MAX98357A <code>LRC</code> into <code>GPIO6</code>.
    </div>
  </div>
</div>

<script>
var currZoom = 1.0;
var svgEl = document.getElementById('schematicSvg');
var boxEl = document.getElementById('svgBox');

function zoomSvg(factor) {
  currZoom = Math.min(Math.max(currZoom * factor, 0.7), 2.5);
  if (currZoom <= 1.02) {
    svgEl.style.width = '100%';
    svgEl.style.minWidth = '0';
  } else {
    var pct = Math.round(currZoom * 100);
    svgEl.style.width = pct + '%';
    svgEl.style.minWidth = pct + '%';
  }
}

function resetSvg() {
  currZoom = 1.0;
  svgEl.style.width = '100%';
  svgEl.style.minWidth = '0';
  if (boxEl) boxEl.scrollLeft = 0;
}

// Mobile Pinch-to-zoom support
var initialDist = 0;
if (boxEl) {
  boxEl.addEventListener('touchstart', function(e) {
    if (e.touches.length === 2) {
      initialDist = Math.hypot(
        e.touches[0].pageX - e.touches[1].pageX,
        e.touches[0].pageY - e.touches[1].pageY
      );
    }
  }, {passive:true});

  boxEl.addEventListener('touchmove', function(e) {
    if (e.touches.length === 2 && initialDist > 0) {
      var dist = Math.hypot(
        e.touches[0].pageX - e.touches[1].pageX,
        e.touches[0].pageY - e.touches[1].pageY
      );
      if (Math.abs(dist - initialDist) > 25) {
        if (dist > initialDist) zoomSvg(1.08);
        else zoomSvg(0.92);
        initialDist = dist;
      }
    }
  }, {passive:true});
}

function filterMod(cat) {
  var btns = document.querySelectorAll('.f-btn');
  btns.forEach(function(b){ b.classList.remove('active'); });
  if (window.event && window.event.target) window.event.target.classList.add('active');

  // Filter Pin rows
  var pRows = document.querySelectorAll('.pin-row');
  pRows.forEach(function(r){
    if (cat === 'all' || r.getAttribute('data-cat') === cat) {
      r.style.display = 'flex';
    } else {
      r.style.display = 'none';
    }
  });

  // Filter Matrix Table
  var tRows = document.querySelectorAll('#matrixTable tbody tr');
  tRows.forEach(function(tr){
    if (cat === 'all' || tr.getAttribute('data-cat') === cat) {
      tr.style.display = '';
    } else {
      tr.style.display = 'none';
    }
  });

  // Filter Module cards
  var mCards = document.querySelectorAll('.mod-card');
  mCards.forEach(function(c){
    if (cat === 'all' || c.getAttribute('data-cat') === cat) {
      c.style.display = 'block';
    } else {
      c.style.display = 'none';
    }
  });
}
</script>
</body></html>)RAWWIRING";


void handleWiring() { server.send_P(200, "text/html; charset=utf-8", WIRING_PAGE_HTML); }
