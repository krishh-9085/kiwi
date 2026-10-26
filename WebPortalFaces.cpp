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

static const char FACE_PAGE_HTML[] PROGMEM = R"RAWFACE(<!doctype html>
<html lang="en"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<title>Kiwi Robot · Face &amp; Emotion Studio</title>
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
.status-pill{
  display:inline-flex;align-items:center;gap:6px;
  padding:4px 10px;border-radius:20px;
  font-size:12px;font-weight:600;
  background:rgba(0,0,0,0.04);border:1px solid var(--border-subtle);
  color:var(--text);
}
.dot{width:7px;height:7px;border-radius:50%;background:var(--green)}
.dot.busy{background:var(--amber)}
.dot.sleep{background:var(--indigo)}
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

/* ── Quick Action Utility Deck ───────────────────────────────────────── */
.actions-bar{
  display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));
  gap:10px;margin-bottom:20px;
}
.qbtn{
  background:var(--surface);
  border:1px solid var(--border);
  border-radius:var(--radius-md);
  padding:12px 14px;
  color:var(--text);
  cursor:pointer;
  display:inline-flex;align-items:center;justify-content:center;gap:8px;
  font-family:inherit;font-size:13px;font-weight:600;
  box-shadow:var(--shadow-sm);
  transition:background 120ms ease,border-color 120ms ease,transform 80ms ease,box-shadow 120ms ease;
  user-select:none;
}
.qbtn:hover{background:var(--bg);border-color:rgba(60,60,67,0.3);box-shadow:var(--shadow-md)}
.qbtn:active{transform:scale(0.97)}
.qbtn.primary{background:var(--blue);border-color:var(--blue);color:#fff;box-shadow:0 2px 8px rgba(0,113,227,0.24)}
.qbtn.primary:hover{background:var(--blue-dark);border-color:var(--blue-dark)}

/* ── Studio Cards ────────────────────────────────────────────────────── */
.studio-card{
  background:var(--surface);
  border:1px solid var(--border);
  border-radius:var(--radius-lg);
  padding:20px clamp(16px,3vw,24px);
  margin-bottom:20px;
  box-shadow:var(--shadow-card);
}
.studio-title{
  font-size:12px;font-weight:700;letter-spacing:0.04em;text-transform:uppercase;
  color:var(--tertiary);margin-bottom:12px;
  display:flex;align-items:center;justify-content:space-between;
}
.studio-title span{display:inline-flex;align-items:center;gap:6px}

/* Inputs */
input[type="text"]{
  background:#fff;border:1px solid var(--border);
  color:var(--text);border-radius:var(--radius-sm);
  padding:10px 14px;font-size:13px;font-family:inherit;
  outline:none;transition:border-color 120ms ease,box-shadow 120ms ease;
}
input[type="text"]:focus{
  border-color:var(--blue);
  box-shadow:0 0 0 3px rgba(0,113,227,0.14);
}

.oled-file{
  max-width:260px;padding:8px;
  background:#fff;border:1px solid var(--border);
  color:var(--text);border-radius:var(--radius-sm);
  font-family:inherit;font-size:12px;
}
.oled-file::file-selector-button{
  margin-right:8px;padding:6px 12px;
  border:0;border-radius:6px;
  background:var(--blue);color:#fff;font-weight:600;cursor:pointer;
  font-family:inherit;font-size:12px;
}

/* ── Skin Grid ───────────────────────────────────────────────────────── */
.skin-grid{
  display:grid;grid-template-columns:repeat(auto-fill,minmax(140px,1fr));
  gap:8px;margin-bottom:14px;
}
.skin-btn{
  background:var(--bg);border:1px solid var(--border-subtle);
  border-radius:var(--radius-sm);padding:9px 12px;
  font-size:12px;font-weight:500;color:var(--text);
  cursor:pointer;text-align:left;font-family:inherit;
  transition:all 120ms ease;display:flex;align-items:center;justify-content:space-between;
}
.skin-btn:hover{background:#fff;border-color:var(--blue);box-shadow:var(--shadow-sm)}
.skin-btn:active{transform:scale(0.97)}
.skin-btn.primary{
  background:var(--blue);border-color:var(--blue);color:#fff;font-weight:600;
  box-shadow:0 2px 8px rgba(0,113,227,0.2);
}

/* ── Options / Autonomous Deck ───────────────────────────────────────── */
.options-bar{
  display:flex;align-items:center;justify-content:space-between;
  background:var(--bg);border:1px solid var(--border-subtle);
  border-radius:var(--radius-md);padding:12px 18px;
  font-size:13px;color:var(--secondary);
}
.toggle{display:inline-flex;align-items:center;gap:10px;cursor:pointer;user-select:none;font-weight:500;color:var(--text)}
.toggle input{display:none}
.switch{width:36px;height:20px;background:#d1d1d6;border-radius:999px;position:relative;transition:background 180ms ease}
.switch::after{content:'';width:16px;height:16px;background:#fff;border-radius:50%;position:absolute;top:2px;left:2px;box-shadow:0 1px 3px rgba(0,0,0,0.2);transition:transform 180ms ease}
.toggle input:checked + .switch{background:var(--green)}
.toggle input:checked + .switch::after{transform:translateX(16px)}
select.select-sm{
  background:#fff;border:1px solid var(--border);border-radius:var(--radius-sm);
  padding:6px 12px;font-size:12px;font-weight:500;color:var(--text);outline:none;
}

/* ── Segmented Control Tabs ──────────────────────────────────────────── */
.tabs-wrap{margin-bottom:18px;overflow-x:auto;-webkit-overflow-scrolling:touch;scrollbar-width:none}
.tabs-wrap::-webkit-scrollbar{display:none}
.tabs{
  display:inline-flex;background:rgba(118,118,128,0.12);
  padding:3px;border-radius:10px;gap:2px;
}
.tab{
  padding:7px 16px;border-radius:8px;font-size:13px;font-weight:500;
  color:var(--secondary);cursor:pointer;white-space:nowrap;user-select:none;
  transition:all 140ms ease;
}
.tab:hover{color:var(--text)}
.tab.active{
  background:#fff;color:var(--text);font-weight:600;
  box-shadow:0 1px 3px rgba(0,0,0,0.12),0 1px 1px rgba(0,0,0,0.06);
}

/* ── Expression Cards Grid ───────────────────────────────────────────── */
.grid{
  display:grid;grid-template-columns:repeat(auto-fill,minmax(130px,1fr));
  gap:10px;
}
.ecard{
  background:var(--surface);
  border:1px solid var(--border-subtle);
  border-radius:var(--radius-md);
  padding:16px 10px 12px;
  cursor:pointer;
  display:flex;flex-direction:column;align-items:center;text-align:center;gap:6px;
  user-select:none;
  box-shadow:var(--shadow-sm);
  transition:transform 100ms ease,box-shadow 120ms ease,border-color 120ms ease;
}
.ecard:hover{
  border-color:rgba(0,113,227,0.35);
  box-shadow:0 6px 18px rgba(0,0,0,0.06);
  transform:translateY(-1px);
}
.ecard:active{
  transform:scale(0.96);
  box-shadow:0 1px 2px rgba(0,0,0,0.04);
}
.e-ico{font-size:32px;line-height:1;margin-bottom:2px}
.e-title{font-size:13px;font-weight:600;color:var(--text);letter-spacing:-0.01em}
.e-desc{font-size:11px;color:var(--tertiary);line-height:1.2}

/* ── Dynamic Toast Capsule ───────────────────────────────────────────── */
#toast{
  position:fixed;top:20px;left:50%;
  transform:translateX(-50%) translateY(-60px);
  background:rgba(29,29,31,0.88);
  backdrop-filter:saturate(180%) blur(20px);
  -webkit-backdrop-filter:saturate(180%) blur(20px);
  color:#fff;padding:9px 22px;border-radius:999px;
  font-size:13px;font-weight:500;
  box-shadow:0 10px 30px rgba(0,0,0,0.18);
  opacity:0;
  transition:transform 300ms cubic-bezier(0.16,1,0.3,1),opacity 200ms ease;
  pointer-events:none;z-index:999;
}
#toast.show{transform:translateX(-50%) translateY(0);opacity:1}

@media(max-width:640px){
  nav{align-items:flex-start}
  .brand{width:100%}
  .nav-actions{width:100%;justify-content:flex-start}
  a.nav-btn{padding:0 10px}
  .grid{grid-template-columns:repeat(auto-fill,minmax(105px,1fr));gap:8px}
  .ecard{padding:12px 6px 10px}
  .e-ico{font-size:26px}
  .actions-bar{grid-template-columns:repeat(2,1fr)}
  .options-bar{flex-direction:column;align-items:flex-start;gap:12px}
}
</style></head><body>

<nav>
  <div class="brand">
    <span class="brand-badge">KIWI</span>
    <span>Face &amp; Emotion Studio</span>
  </div>
  <div class="nav-actions">
    <div class="status-pill"><span class="dot" id="statusDot"></span><span id="statusTxt">Ready</span></div>
    <a class="nav-btn" href="/">Dashboard</a>
    <a class="nav-btn primary" href="/faces">Faces</a>
    <a class="nav-btn" href="/wiring">Wiring</a>
    <a class="nav-btn" href="/update">OTA</a>
  </div>
</nav>

<div class="container">
  <!-- Action Controls Bar -->
  <div class="actions-bar">
    <button class="qbtn primary" id="btnSurprise">Surprise Me</button>
    <button class="qbtn" id="btnSleep"><span id="sleepIco">🌙</span> <span id="sleepTxt">Sleep Mode</span></button>
    <button class="qbtn" id="btnClock">Show Clock</button>
    <button class="qbtn" id="btnWeather">Show Weather</button>
  </div>

  <!-- Custom OLED Marquee Banner -->
  <div class="studio-card">
    <div class="studio-title">
      <span>Marquee Text Display</span>
    </div>
    <div style="display:flex;gap:10px;align-items:center;flex-wrap:wrap">
      <input type="text" id="bannerIn" placeholder="Type marquee banner message..." style="flex:1;min-width:220px">
      <button class="qbtn primary" id="btnSendBanner" style="padding:10px 18px">Display</button>
      <button class="qbtn" id="btnStopBanner" style="padding:10px 18px">Stop</button>
    </div>
  </div>

  <!-- Named OLED Animations -->
  <div class="studio-card">
    <div class="studio-title">
      <span>OLED Animations</span>
      <span style="font-size:11px;color:var(--tertiary);text-transform:none;font-weight:400">Up to 4 custom slots</span>
    </div>
    <div id="oledAnimSlots" style="display:grid;grid-template-columns:repeat(auto-fit,minmax(140px,1fr));gap:8px;margin-bottom:12px">
      <div style="color:var(--tertiary);font-size:13px;padding:8px 0">Loading animation slots...</div>
    </div>
    <div style="display:flex;gap:10px;align-items:center;flex-wrap:wrap;padding-top:8px;border-top:1px solid var(--border-subtle)">
      <input type="file" id="oledAnimFile" class="oled-file" accept=".ino,.bin,.raw">
      <input type="text" id="oledAnimName" maxlength="24" placeholder="Slot name" style="max-width:180px">
      <button class="qbtn primary" id="btnUploadOledAnim" style="padding:8px 16px">Upload Slot</button>
      <button class="qbtn" id="btnStopOledAnim" style="padding:8px 16px">Stop Animation</button>
    </div>
  </div>

  <!-- OLED Eye Skin Studio Bar (17 Styles) -->
  <div class="studio-card">
    <div class="studio-title">
      <span>Eye &amp; Face Skin Theme</span>
      <span id="activeSkinLabel" style="font-size:12px;color:var(--blue);font-weight:600">Active: Default</span>
    </div>
    <div class="skin-grid">
      <button class="skin-btn primary" data-skin="default"><span>Default Vector</span></button>
      <button class="skin-btn" data-skin="cyberpunk"><span>Cyberpunk</span></button>
      <button class="skin-btn" data-skin="cat"><span>Kawaii Cat</span></button>
      <button class="skin-btn" data-skin="retro"><span>Retro 8-Bit</span></button>
      <button class="skin-btn" data-skin="anime"><span>Anime Glint</span></button>
      <button class="skin-btn" data-skin="hearts"><span>Heart Eyes</span></button>
      <button class="skin-btn" data-skin="steampunk"><span>Steampunk</span></button>
      <button class="skin-btn" data-skin="demon"><span>Demon</span></button>
      <button class="skin-btn" data-skin="matrix"><span>Matrix Code</span></button>
      <button class="skin-btn" data-skin="cyborg"><span>Cyborg T-800</span></button>
      <button class="skin-btn" data-skin="mecha"><span>Mecha Gundam</span></button>
      <button class="skin-btn" data-skin="sunglasses"><span>Boss Shades</span></button>
      <button class="skin-btn" data-skin="puppy"><span>Kawaii Puppy</span></button>
      <button class="skin-btn" data-skin="hypno"><span>Hypno Spiral</span></button>
      <button class="skin-btn" data-skin="alien"><span>Cosmic Alien</span></button>
      <button class="skin-btn" data-skin="ninja"><span>Shinobi Ninja</span></button>
      <button class="skin-btn" data-skin="round"><span>Round Ocular</span></button>
      <button class="skin-btn" data-skin="kiwi"><span>Desk Buddy (Cute Block)</span></button>
    </div>

    <div class="options-bar">
      <label class="toggle" title="Toggle autonomous idle expressions & remarks">
        <input type="checkbox" id="bToggle">
        <span class="switch"></span>
        <span>Autonomous Idle Remarks</span>
      </label>
      <div style="display:flex;align-items:center;gap:8px">
        <span style="font-size:12px;color:var(--tertiary)">Interval:</span>
        <select id="boredomMinsDeck" class="select-sm" onchange="setBoredomIntervalLive(this.value)">
          <option value="1">1 minute</option>
          <option value="2">2 minutes</option>
          <option value="3">3 minutes</option>
          <option value="5" selected>5 minutes</option>
          <option value="10">10 minutes</option>
          <option value="15">15 minutes</option>
          <option value="30">30 minutes</option>
        </select>
      </div>
    </div>
  </div>

  <!-- Segmented Expression Tabs -->
  <div class="tabs-wrap">
    <div class="tabs">
      <div class="tab active" data-cat="all">All (42)</div>
      <div class="tab" data-cat="pet">Pet (10)</div>
      <div class="tab" data-cat="core">Core (11)</div>
      <div class="tab" data-cat="expressive">Expressive (11)</div>
      <div class="tab" data-cat="fun">Wild &amp; Fun (10)</div>
    </div>
  </div>

  <!-- Cards Grid -->
  <div class="grid" id="cardGrid">
    <!-- Pet Moods -->
    <div class="ecard" data-cat="pet" data-e="love"><span class="e-ico">💖</span><span class="e-title">Love</span><span class="e-desc">Heart flutter</span></div>
    <div class="ecard" data-cat="pet" data-e="giggle"><span class="e-ico">🤭</span><span class="e-title">Giggle</span><span class="e-desc">Musical laugh</span></div>
    <div class="ecard" data-cat="pet" data-e="shy"><span class="e-ico">🙈</span><span class="e-title">Shy</span><span class="e-desc">Blushing look</span></div>
    <div class="ecard" data-cat="pet" data-e="sparkle"><span class="e-ico">✨</span><span class="e-title">Sparkle</span><span class="e-desc">Glinting stars</span></div>
    <div class="ecard" data-cat="pet" data-e="kiss"><span class="e-ico">💋</span><span class="e-title">Kiss</span><span class="e-desc">Blowing kisses</span></div>
    <div class="ecard" data-cat="pet" data-e="cuddle"><span class="e-ico">🫂</span><span class="e-title">Cuddle</span><span class="e-desc">Sleepy cozy</span></div>
    <div class="ecard" data-cat="pet" data-e="playful"><span class="e-ico">🎾</span><span class="e-title">Playful</span><span class="e-desc">Bouncing wink</span></div>
    <div class="ecard" data-cat="pet" data-e="sleepy"><span class="e-ico">🥱</span><span class="e-title">Sleepy</span><span class="e-desc">Heavy eyelids</span></div>
    <div class="ecard" data-cat="pet" data-e="proud"><span class="e-ico">🦁</span><span class="e-title">Proud</span><span class="e-desc">Chest puff</span></div>
    <div class="ecard" data-cat="pet" data-e="surprised"><span class="e-ico">😲</span><span class="e-title">Surprise</span><span class="e-desc">Gasp reaction</span></div>

    <!-- Core Faces -->
    <div class="ecard" data-cat="core" data-e="idle"><span class="e-ico">😐</span><span class="e-title">Idle</span><span class="e-desc">Blinking calm</span></div>
    <div class="ecard" data-cat="core" data-e="happy"><span class="e-ico">😄</span><span class="e-title">Happy</span><span class="e-desc">Sweet smile</span></div>
    <div class="ecard" data-cat="core" data-e="excited"><span class="e-ico">🤩</span><span class="e-title">Excited</span><span class="e-desc">Big bouncy</span></div>
    <div class="ecard" data-cat="core" data-e="sad"><span class="e-ico">😢</span><span class="e-title">Sad</span><span class="e-desc">Droopy frown</span></div>
    <div class="ecard" data-cat="core" data-e="angry"><span class="e-ico">😡</span><span class="e-title">Angry</span><span class="e-desc">Fierce brow</span></div>
    <div class="ecard" data-cat="core" data-e="confused"><span class="e-ico">😕</span><span class="e-title">Confused</span><span class="e-desc">Tilted glance</span></div>
    <div class="ecard" data-cat="core" data-e="thinking"><span class="e-ico">🤔</span><span class="e-title">Thinking</span><span class="e-desc">Processing AI</span></div>
    <div class="ecard" data-cat="core" data-e="sleeping"><span class="e-ico">😴</span><span class="e-title">Sleeping</span><span class="e-desc">Z Z snore</span></div>
    <div class="ecard" data-cat="core" data-e="listening"><span class="e-ico">👂</span><span class="e-title">Listening</span><span class="e-desc">Audio waves</span></div>
    <div class="ecard" data-cat="core" data-e="speaking"><span class="e-ico">🗣️</span><span class="e-title">Speaking</span><span class="e-desc">Talking mouth</span></div>
    <div class="ecard" data-cat="core" data-e="error"><span class="e-ico">⚠️</span><span class="e-title">Error</span><span class="e-desc">Buzzer alert</span></div>

    <!-- Expressive Faces -->
    <div class="ecard" data-cat="expressive" data-e="wink"><span class="e-ico">😉</span><span class="e-title">Wink</span><span class="e-desc">Cheeky glance</span></div>
    <div class="ecard" data-cat="expressive" data-e="smug"><span class="e-ico">😏</span><span class="e-title">Smug</span><span class="e-desc">Corner smirk</span></div>
    <div class="ecard" data-cat="expressive" data-e="curious"><span class="e-ico">🧐</span><span class="e-title">Curious</span><span class="e-desc">Wide search</span></div>
    <div class="ecard" data-cat="expressive" data-e="bored"><span class="e-ico">😑</span><span class="e-title">Bored</span><span class="e-desc">Half lids</span></div>
    <div class="ecard" data-cat="expressive" data-e="laughing"><span class="e-ico">🤣</span><span class="e-title">Laughing</span><span class="e-desc">Joy bounce</span></div>
    <div class="ecard" data-cat="expressive" data-e="scared"><span class="e-ico">😱</span><span class="e-title">Scared</span><span class="e-desc">Shivering eyes</span></div>
    <div class="ecard" data-cat="expressive" data-e="suspicious"><span class="e-ico">🤨</span><span class="e-title">Suspicious</span><span class="e-desc">Squinting side</span></div>
    <div class="ecard" data-cat="expressive" data-e="relieved"><span class="e-ico">😌</span><span class="e-title">Relieved</span><span class="e-desc">Soft sigh</span></div>
    <div class="ecard" data-cat="expressive" data-e="proud"><span class="e-ico">👑</span><span class="e-title">Regal</span><span class="e-desc">Head held high</span></div>
    <div class="ecard" data-cat="expressive" data-e="dizzy"><span class="e-ico">💫</span><span class="e-title">Dizzy</span><span class="e-desc">Orbiting eyes</span></div>
    <div class="ecard" data-cat="expressive" data-e="lovestruck"><span class="e-ico">😍</span><span class="e-title">Lovestruck</span><span class="e-desc">Pulsing glow</span></div>

    <!-- Wild & Fun Faces -->
    <div class="ecard" data-cat="fun" data-e="cool"><span class="e-ico">😎</span><span class="e-title">Cool</span><span class="e-desc">Sunglasses</span></div>
    <div class="ecard" data-cat="fun" data-e="evil"><span class="e-ico">😈</span><span class="e-title">Devil</span><span class="e-desc">Spiky horns</span></div>
    <div class="ecard" data-cat="fun" data-e="crying"><span class="e-ico">😭</span><span class="e-title">Crying</span><span class="e-desc">Tear streams</span></div>
    <div class="ecard" data-cat="fun" data-e="hearts"><span class="e-ico">💖</span><span class="e-title">Hearts</span><span class="e-desc">Floating hearts</span></div>
    <div class="ecard" data-cat="fun" data-e="dead"><span class="e-ico">😵</span><span class="e-title">Dead / KO</span><span class="e-desc">Cross X eyes</span></div>
    <div class="ecard" data-cat="fun" data-e="shocked"><span class="e-ico">🤯</span><span class="e-title">Shocked</span><span class="e-desc">Tiny pupils</span></div>
    <div class="ecard" data-cat="fun" data-e="singing"><span class="e-ico">🎶</span><span class="e-title">Singing</span><span class="e-desc">Music notes</span></div>
    <div class="ecard" data-cat="fun" data-e="party"><span class="e-ico">🥳</span><span class="e-title">Party</span><span class="e-desc">Confetti dance</span></div>
    <div class="ecard" data-cat="fun" data-e="nervous"><span class="e-ico">😰</span><span class="e-title">Nervous</span><span class="e-desc">Sweat drops</span></div>
    <div class="ecard" data-cat="fun" data-e="cute"><span class="e-ico">🥺</span><span class="e-title">Cute</span><span class="e-desc">Puppy pleading</span></div>
  </div>
</div>

<div id="toast">Expression Triggered</div>

<script>
var isSleeping = false;
function showToast(msg){
  var t = document.getElementById('toast');
  t.textContent = msg;
  t.classList.add('show');
  setTimeout(function(){t.classList.remove('show');},1600);
}

function fire(name, label){
  fetch('/do?e='+encodeURIComponent(name)+'&ms=60000&s=1')
    .then(function(r){return r.json();})
    .then(function(j){
      showToast(label || name);
      poll();
    }).catch(function(){});
}

document.querySelectorAll('.ecard[data-e]').forEach(function(c){
  c.addEventListener('click',function(){
    var e = this.getAttribute('data-e');
    var t = this.querySelector('.e-title').textContent;
    fire(e, t);
  });
});

document.getElementById('btnSurprise').addEventListener('click',function(){ fire('random','Surprise'); });
document.getElementById('btnClock').addEventListener('click',function(){ fire('clock','Clock'); });
document.getElementById('btnWeather').addEventListener('click',function(){ fire('weather','Weather'); });
document.getElementById('btnSleep').addEventListener('click',function(){
  fetch('/do?sleep='+(isSleeping?'0':'1'))
    .then(function(r){return r.json();})
    .then(function(){
      poll();
    }).catch(function(){});
});

document.querySelectorAll('.tab').forEach(function(t){
  t.addEventListener('click',function(){
    document.querySelectorAll('.tab').forEach(function(x){x.classList.remove('active');});
    this.classList.add('active');
    var cat = this.getAttribute('data-cat');
    document.querySelectorAll('.ecard').forEach(function(card){
      if(cat==='all' || card.getAttribute('data-cat')===cat){
        card.style.display = 'flex';
      }else{
        card.style.display = 'none';
      }
    });
  });
});

document.getElementById('btnSendBanner').addEventListener('click',function(){
  var inp = document.getElementById('bannerIn');
  var txt = inp.value.trim();
  if(!txt) return;
  fetch('/do?banner='+encodeURIComponent(txt))
    .then(function(r){return r.json();})
    .then(function(){showToast('Banner displayed');})
    .catch(function(){});
});

document.getElementById('btnStopBanner').addEventListener('click',function(){
  fetch('/do?stopbanner=1')
    .then(function(r){return r.json();})
    .then(function(){showToast('Banner stopped');})
    .catch(function(){});
});

function refreshOledAnimations(){
  fetch('/animations').then(function(r){return r.json();}).then(function(items){
    var box = document.getElementById('oledAnimSlots');
    if(!items || !items.length){
      box.innerHTML = '<div style="color:var(--tertiary);font-size:12px;padding:6px 0">No custom animations saved yet.</div>';
      return;
    }
    box.innerHTML = items.map(function(a){
      return '<button class="qbtn anim-btn" style="padding:9px 12px;font-size:12px;justify-content:flex-start" onclick="playOledAnimation('+a.slot+')"><span>▶</span> <span>'+a.name+'</span></button>';
    }).join('');
  }).catch(function(){document.getElementById('oledAnimSlots').textContent='Unable to load animations';});
}

function playOledAnimation(slot){
  fetch('/do?animation='+slot).then(function(){showToast('OLED animation playing');});
}

document.getElementById('btnStopOledAnim').addEventListener('click',function(){
  fetch('/do?stopanimation=1').then(function(){showToast('OLED animation stopped');});
});

document.getElementById('btnUploadOledAnim').addEventListener('click',function(){
  var f = document.getElementById('oledAnimFile').files[0];
  var name = document.getElementById('oledAnimName').value.trim();
  if(!f){ showToast('Select an animation file first'); return; }
  f.arrayBuffer().then(function(buf){
    var bytes = new Uint8Array(buf);
    var body = '';
    if(bytes.length === 1024){
      body = String.fromCharCode.apply(null, Array.from(bytes));
    }else{
      body = new TextDecoder('utf-8').decode(bytes);
    }
    return fetch('/animation-upload?name='+encodeURIComponent(name),{method:'POST',body:body})
      .then(function(r){return r.json().then(function(x){return {ok:r.ok,data:x};});})
      .then(function(x){
        if(!x.ok) throw new Error(x.data && x.data.error ? x.data.error : 'Upload failed');
        showToast('Uploaded to slot '+(x.data.slot+1));
        refreshOledAnimations();
      });
  }).catch(function(){showToast('Upload failed: use a 1024-byte .bin or .ino');});
});

document.querySelectorAll('.skin-btn').forEach(function(b){
  b.addEventListener('click', function(){
    var sk = this.getAttribute('data-skin');
    fetch('/do?skin=' + encodeURIComponent(sk))
      .then(function(r){ return r.json(); })
      .then(function(){
        document.querySelectorAll('.skin-btn').forEach(function(x){ x.classList.remove('primary'); });
        b.classList.add('primary');
        var lbl = document.getElementById('activeSkinLabel');
        if(lbl) lbl.textContent = 'Active: ' + b.textContent.trim();
        showToast('Skin: ' + b.textContent.trim());
      })
      .catch(function(){});
  });
});

var bToggle = document.getElementById('bToggle');
if(bToggle){
  bToggle.addEventListener('change', function(){
    var en = this.checked ? '1' : '0';
    fetch('/do?boredom=' + en)
      .then(function(r){ return r.json(); })
      .then(function(){
        showToast(en === '1' ? 'Idle remarks active' : 'Idle remarks disabled');
      })
      .catch(function(){});
  });
}

function setBoredomIntervalLive(val){
  fetch('/do?boredom_mins=' + encodeURIComponent(val))
    .then(function(r){ return r.json(); })
    .then(function(){
      showToast('Interval set: ' + val + 'm');
    })
    .catch(function(){});
}

function poll(){
  fetch('/status').then(function(r){return r.json();}).then(function(j){
    var dot = document.getElementById('statusDot');
    var txt = document.getElementById('statusTxt');
    var slpTxt = document.getElementById('sleepTxt');
    var slpIco = document.getElementById('sleepIco');
    if(!dot || !txt) return;
    isSleeping = !!j.sleeping;
    if(j.skin){
      var activeName = j.skin;
      document.querySelectorAll('.skin-btn').forEach(function(b){
        if(b.getAttribute('data-skin') === j.skin){
          b.classList.add('primary');
          activeName = b.textContent.trim();
        } else {
          b.classList.remove('primary');
        }
      });
      var lbl = document.getElementById('activeSkinLabel');
      if(lbl) lbl.textContent = 'Active: ' + activeName;
    }
    if(j.boredom !== undefined){
      var bT = document.getElementById('bToggle');
      if(bT) bT.checked = !!j.boredom;
    }
    if(j.boredom_mins !== undefined){
      var bM = document.getElementById('boredomMinsDeck');
      if(bM) bM.value = String(j.boredom_mins);
    }
    if(j.sleeping){
      dot.className = 'dot sleep';
      txt.textContent = 'Sleeping';
      slpTxt.textContent = 'Wake Up';
      slpIco.textContent = '☀️';
    }else if(j.busy){
      dot.className = 'dot busy';
      txt.textContent = 'Busy';
      slpTxt.textContent = 'Sleep Mode';
      slpIco.textContent = '🌙';
    }else{
      dot.className = 'dot';
      txt.textContent = (j.emotion==='idle'?'Ready':j.emotion);
      slpTxt.textContent = 'Sleep Mode';
      slpIco.textContent = '🌙';
    }
  }).catch(function(){});
}
setInterval(poll,2000);poll();refreshOledAnimations();
</script>
</body></html>)RAWFACE";


void handleFacesPage() { server.send_P(200, "text/html; charset=utf-8", FACE_PAGE_HTML); }
