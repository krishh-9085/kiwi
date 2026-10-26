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

// HTML ESCAPE & TEMPLATES
// ============================================================
static String esc(String s) {
  s.replace("&", "&amp;");
  s.replace("<", "&lt;");
  s.replace(">", "&gt;");
  s.replace("\"", "&quot;");
  return s;
}

static String escJson(String s) {
  s.replace("\\", "\\\\");
  s.replace("\"", "\\\"");
  s.replace("\r", "");
  s.replace("\n", "\\n");
  return s;
}

static String configPage() {
  String h;
  h.reserve(28000);
  h = R"rawliteral(
<!doctype html>
<html lang="en"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<title>Kiwi Robot · System Console</title>
<link rel="icon" href="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 64 64'%3E%3Crect width='64' height='64' rx='18' fill='%230071e3'/%3E%3Ccircle cx='22' cy='27' r='6' fill='white'/%3E%3Ccircle cx='42' cy='27' r='6' fill='white'/%3E%3Cpath d='M18 42c8 7 20 7 28 0' fill='none' stroke='white' stroke-width='5' stroke-linecap='round'/%3E%3C/svg%3E">
<style>
:root{
  --bg:#f5f5f7;--surface:#ffffff;--surface-raised:rgba(255,255,255,0.9);
  --border:rgba(60,60,67,0.18);--border-focus:#0071e3;
  --text:#1d1d1f;--secondary:#3a3a3c;--tertiary:#6e6e73;
  --blue:#0071e3;--blue-dark:#005bb5;
  --green:#34c759;--red:#ff3b30;--amber:#ff9500;
  --radius-sm:8px;--radius-md:12px;--radius-lg:16px;
  --shadow-sm:0 1px 3px rgba(0,0,0,.08),0 1px 2px rgba(0,0,0,.06);
  --shadow-md:0 4px 16px rgba(0,0,0,.08),0 2px 6px rgba(0,0,0,.05);
  --shadow-lg:0 12px 32px rgba(0,0,0,.10),0 4px 12px rgba(0,0,0,.06);
}
*{box-sizing:border-box;-webkit-font-smoothing:antialiased;-moz-osx-font-smoothing:grayscale}
html{scroll-behavior:smooth}
body{
  margin:0;
  font-family:-apple-system,BlinkMacSystemFont,'SF Pro Text','Helvetica Neue',sans-serif;
  font-size:15px;line-height:1.47059;
  background:var(--bg);color:var(--text);
  -webkit-text-size-adjust:100%;
}

/* ── Navigation ──────────────────────────────────────────────────────── */
nav{
  display:flex;flex-wrap:wrap;align-items:center;justify-content:space-between;
  gap:10px;padding:10px clamp(16px,3vw,32px);
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
a.nav-btn:active{transform:scale(0.97);transition-duration:60ms}
a.nav-btn.primary{background:var(--blue);border-color:var(--blue);color:#fff}
a.nav-btn.primary:hover{background:var(--blue-dark);border-color:var(--blue-dark)}

/* ── Layout ──────────────────────────────────────────────────────────── */
.container{
  width:min(1240px,100%);
  margin:0 auto;
  padding:28px clamp(16px,3vw,32px) 64px;
}

/* ── Status Grid ─────────────────────────────────────────────────────── */
.status-grid{
  display:grid;
  grid-template-columns:repeat(auto-fit,minmax(180px,1fr));
  gap:10px;margin-bottom:20px;
}
.status-item{
  background:var(--surface);
  border:1px solid var(--border);
  border-radius:var(--radius-md);
  padding:14px 16px;
  box-shadow:var(--shadow-sm);
  transition:box-shadow 150ms ease;
}
.status-item:hover{box-shadow:var(--shadow-md)}
.status-label{
  font-size:10px;font-weight:600;
  text-transform:uppercase;letter-spacing:.08em;
  color:var(--tertiary);margin-bottom:5px;
}
.status-val{font-size:13px;font-weight:600;color:var(--text);font-variant-numeric:tabular-nums}
.status-val.good{color:var(--green)}

/* ── Form Grid & Cards ───────────────────────────────────────────────── */
.form-grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:16px;align-items:start}
.form-grid>.card{min-width:0}
.card{
  background:var(--surface);
  border:1px solid var(--border);
  border-radius:var(--radius-md);
  padding:20px;
  box-shadow:var(--shadow-sm);
  min-width:0;
}
.card-title{
  font-size:11px;font-weight:700;
  text-transform:uppercase;letter-spacing:.08em;
  color:var(--tertiary);
  border-bottom:1px solid var(--border);
  padding-bottom:10px;margin:0 0 16px;
}
h3{
  font-size:11px;font-weight:600;
  text-transform:uppercase;letter-spacing:.06em;
  color:var(--tertiary);
  margin:16px 0 6px;
}

/* ── Form Elements ───────────────────────────────────────────────────── */
label{
  display:block;
  font-size:12px;font-weight:500;
  color:var(--tertiary);
  margin:12px 0 5px;
}
input,select,textarea{
  width:100%;box-sizing:border-box;
  background:#fff;
  border:1px solid rgba(60,60,67,0.25);
  color:var(--text);
  padding:9px 12px;
  border-radius:var(--radius-sm);
  font-size:14px;
  font-family:inherit;
  outline:none;
  appearance:auto;
  transition:border-color 120ms ease,box-shadow 120ms ease;
}
input:focus,select:focus,textarea:focus{
  border-color:var(--border-focus);
  box-shadow:0 0 0 3px rgba(0,113,227,0.15);
}
input[type=range]{padding:0;height:20px;accent-color:var(--blue);min-height:auto}
textarea{min-height:88px;resize:vertical;line-height:1.5}
small{color:var(--tertiary);font-size:12px;display:block;margin-top:5px;line-height:1.4}

/* ── Toggle Switch ───────────────────────────────────────────────────── */
.toggle{display:inline-flex;align-items:center;gap:9px;cursor:pointer;user-select:none;font-size:14px;font-weight:500;color:var(--text)}
.toggle input{display:none}
.switch{
  width:36px;height:20px;
  background:rgba(120,120,128,0.32);
  border-radius:999px;position:relative;
  transition:background 200ms ease;flex-shrink:0;
}
.switch::after{
  content:'';width:17px;height:17px;background:#fff;
  border-radius:50%;position:absolute;
  top:1.5px;left:1.5px;
  transition:transform 200ms ease;
  box-shadow:0 1px 3px rgba(0,0,0,0.25);
}
.toggle input:checked + .switch{background:var(--green)}
.toggle input:checked + .switch::after{transform:translateX(16px)}

/* ── Buttons ─────────────────────────────────────────────────────────── */
button.save-btn{
  width:100%;background:var(--blue);color:#fff;
  border:0;border-radius:var(--radius-sm);
  padding:14px;font-size:15px;font-weight:600;
  letter-spacing:-.01em;
  cursor:pointer;
  transition:background 120ms ease,transform 60ms ease;
}
button.save-btn:hover{background:var(--blue-dark)}
button.save-btn:active{transform:scale(0.98)}
.save-wrap{grid-column:1/-1;margin-top:4px}

.qbtn{
  background:#fff;
  border:1px solid rgba(60,60,67,0.22);
  border-radius:var(--radius-sm);
  padding:8px 14px;
  color:var(--text);
  cursor:pointer;
  font-size:13px;font-weight:500;
  font-family:inherit;
  transition:background 120ms ease,border-color 120ms ease,transform 60ms ease;
}
.qbtn:hover{background:var(--bg);border-color:rgba(60,60,67,0.35)}
.qbtn:active{transform:scale(0.97)}
.qbtn.primary{background:var(--blue);border-color:var(--blue);color:#fff}
.qbtn.primary:hover{background:var(--blue-dark);border-color:var(--blue-dark)}

/* ── Status Pill & Dot ───────────────────────────────────────────────── */
.status-pill{
  display:inline-flex;align-items:center;gap:6px;
  padding:5px 11px;
  border-radius:var(--radius-sm);
  font-size:12px;font-weight:500;
  background:var(--surface);
  border:1px solid var(--border);
  color:var(--secondary);
}
.dot{width:7px;height:7px;border-radius:50%;background:var(--green)}
.dot.busy{background:var(--amber)}
.dot.rec{background:var(--red);animation:pulseDot 1s ease-in-out infinite}
.dot.sleep{background:#8e8e93}
@keyframes pulseDot{0%,100%{opacity:1}50%{opacity:.35}}

/* ── Mic Button ──────────────────────────────────────────────────────── */
.mic-btn{
  background:#fff;
  border:1px solid rgba(60,60,67,0.22);
  color:var(--text);
  border-radius:var(--radius-md);
  padding:14px 16px;
  display:flex;align-items:center;gap:12px;
  cursor:pointer;
  transition:background 120ms ease,border-color 120ms ease,box-shadow 120ms ease,transform 60ms ease;
  user-select:none;width:100%;
  font-family:inherit;
}
.mic-btn:hover{background:var(--bg);border-color:var(--blue);box-shadow:0 0 0 3px rgba(0,113,227,0.08)}
.mic-btn:active{transform:scale(0.98)}
.mic-btn.recording{
  background:#fff0f0;
  border-color:var(--red);
  box-shadow:0 0 0 3px rgba(255,59,48,0.1);
  animation:pulseMic 1.5s ease-in-out infinite;
}
@keyframes pulseMic{0%,100%{box-shadow:0 0 0 3px rgba(255,59,48,.10)}50%{box-shadow:0 0 0 5px rgba(255,59,48,.18)}}
.mic-icon{
  font-size:20px;
  display:flex;align-items:center;justify-content:center;
  width:38px;height:38px;
  background:var(--bg);
  border-radius:var(--radius-sm);
  flex-shrink:0;
}

/* ── Live Console ────────────────────────────────────────────────────── */
.console-columns{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:14px}
.live-console{border:1px solid var(--border)}

/* ── OLED File ───────────────────────────────────────────────────────── */
.oled-file{
  max-width:260px!important;padding:8px!important;
  background:#fff!important;
  border:1px solid rgba(60,60,67,0.2)!important;
  color:var(--text)!important;
  border-radius:var(--radius-sm)!important;
  font-family:inherit;
}
.oled-file::file-selector-button{
  margin-right:8px;padding:6px 10px;
  border:0;border-radius:5px;
  background:var(--blue);color:#fff;font-weight:600;cursor:pointer;
  font-family:inherit;font-size:12px;
}

/* ── Responsive ──────────────────────────────────────────────────────── */
.status-grid{grid-template-columns:repeat(5,minmax(0,1fr))}
@media(max-width:1100px){.status-grid{grid-template-columns:repeat(3,minmax(0,1fr))}}
@media(max-width:900px){
  .form-grid{grid-template-columns:1fr}
  .save-wrap{grid-column:auto}
  .console-columns{grid-template-columns:1fr}
}
@media(max-width:700px){
  nav{flex-wrap:wrap;height:auto;padding:12px 16px}
  .brand{width:100%}
  .nav-actions{width:100%;display:grid;grid-template-columns:repeat(3,minmax(0,1fr))}
  a.nav-btn{justify-content:center;height:36px}
  .container{padding-top:20px}
  .status-grid{grid-template-columns:repeat(2,minmax(0,1fr))}
  .card{padding:16px}
  .status-item{padding:12px}
}
@media(max-width:480px){
  .nav-actions{grid-template-columns:repeat(2,minmax(0,1fr))}
  .status-grid{grid-template-columns:1fr}
}
@media(prefers-reduced-transparency:reduce){
  nav{background:var(--bg);backdrop-filter:none;-webkit-backdrop-filter:none}
}
@media(prefers-reduced-motion:reduce){
  html{scroll-behavior:auto}
  *,*::before,*::after{animation-duration:.01ms!important;transition-duration:.01ms!important}
}
</style></head><body>
<nav>
  <div class="brand">
    <span class="brand-badge">KIWI</span>
    <span>System Console</span>
  </div>
  <div class="nav-actions">
    <a class="nav-btn primary" href="/">Dashboard</a>
    <a class="nav-btn" href="/faces">Faces</a>
    <a class="nav-btn" href="/wiring">Wiring</a>
    <a class="nav-btn" href="/update">OTA</a>
  </div>
</nav>

<div class="container">
  <div class="status-grid">
    <div class="status-item">
      <div class="status-label">Hotspot SSID</div>
      <div class="status-val">%AP%</div>
    </div>
    <div class="status-item">
      <div class="status-label">Setup URL / IP</div>
      <div class="status-val">192.168.4.1</div>
    </div>
    <div class="status-item">
      <div class="status-label">Active Brain</div>
      <div class="status-val">%LLMPROV% / %ACTIVEMODEL%</div>
    </div>
    <div class="status-item">
      <div class="status-label">Speech Engine</div>
      <div class="status-val">%STTPROV% / %STTLANGNAME%</div>
    </div>
    <div class="status-item">
      <div class="status-label">Voice Synthesis</div>
      <div class="status-val">%TTSPROV%</div>
    </div>
  </div>

  <!-- Live AI Voice Interaction & Serial Console Deck -->
  <div class="card live-console" style="margin-bottom:24px">
    <div style="display:flex;align-items:center;justify-content:space-between;border-bottom:1px solid var(--border);padding-bottom:12px;margin-bottom:16px;flex-wrap:wrap;gap:10px">
      <div style="display:flex;align-items:center;gap:10px">
        <span style="font-size:16px">🎙️</span>
        <span style="font-size:13px;font-weight:600;color:var(--text);letter-spacing:-0.01em">Live Voice Interaction &amp; Serial Monitor</span>
      </div>
      <div style="display:flex;align-items:center;gap:8px">
        <span class="status-pill" id="liveStateBadge"><span class="dot" id="liveStateDot"></span><span id="liveStateText">Ready</span></span>
      </div>
    </div>

    <div class="console-columns">
      <!-- Voice & Text Input Column -->
      <div style="display:flex;flex-direction:column;gap:12px">
        <button type="button" id="btnWebMic" class="mic-btn">
          <span class="mic-icon">🎙️</span>
          <div style="text-align:left">
            <div id="micBtnTitle" style="font-size:14px;font-weight:600">Start Voice Recording</div>
            <div id="micBtnSub" style="font-size:12px;color:var(--tertiary)">Stops shortly after you finish speaking</div>
          </div>
        </button>

        <div style="display:flex;gap:8px;flex-wrap:wrap">
          <input type="text" id="textPromptIn" placeholder="Type prompt or question for Kiwi..." style="flex:1">
          <button type="button" id="btnSendPrompt" class="qbtn primary" style="padding:0 16px;white-space:nowrap">Send</button>
        </div>

        <div style="background:var(--bg);border:1px solid var(--border);border-radius:var(--radius-sm);padding:12px">
          <div style="font-size:10px;font-weight:700;letter-spacing:0.04em;color:var(--blue);text-transform:uppercase;margin-bottom:4px">Speech-To-Text Transcription</div>
          <div id="boxQuestion" style="font-size:13px;color:var(--text);font-family:ui-monospace,SFMono-Regular,Menlo,monospace;min-height:22px;word-break:break-word">Awaiting voice input or prompt...</div>
        </div>

        <div style="background:var(--bg);border:1px solid var(--border);border-radius:var(--radius-sm);padding:12px">
          <div style="font-size:10px;font-weight:700;letter-spacing:0.04em;color:var(--green);text-transform:uppercase;margin-bottom:4px">Kiwi AI Response</div>
          <div id="boxAnswer" style="font-size:13px;color:var(--text);font-family:ui-monospace,SFMono-Regular,Menlo,monospace;min-height:22px;word-break:break-word">Awaiting response...</div>
        </div>
      </div>

      <!-- Live Serial Monitor Terminal Window (macOS Terminal styled) -->
      <div style="background:#1c1c1e;border:1px solid rgba(0,0,0,0.15);border-radius:var(--radius-md);padding:12px;display:flex;flex-direction:column;box-shadow:var(--shadow-sm)">
        <div style="display:flex;align-items:center;justify-content:space-between;border-bottom:1px solid rgba(255,255,255,0.08);padding-bottom:8px;margin-bottom:8px">
          <div style="display:flex;align-items:center;gap:6px">
            <span style="display:inline-block;width:10px;height:10px;border-radius:50%;background:#ff5f56"></span>
            <span style="display:inline-block;width:10px;height:10px;border-radius:50%;background:#ffbd2e"></span>
            <span style="display:inline-block;width:10px;height:10px;border-radius:50%;background:#27c93f"></span>
            <span style="margin-left:8px;font-size:11px;font-family:ui-monospace,SFMono-Regular,Menlo,monospace;color:#8e8e93">ESP32-C3 · 115200 BAUD</span>
          </div>
          <div style="display:flex;align-items:center;gap:10px">
            <label style="font-size:11px;color:#8e8e93;display:inline-flex;align-items:center;gap:4px;cursor:pointer;margin:0"><input type="checkbox" id="autoScrollCheck" checked style="width:auto"> Auto-scroll</label>
            <button type="button" id="btnClearLog" style="background:rgba(255,255,255,0.08);border:1px solid rgba(255,255,255,0.12);color:#aeaeb2;border-radius:5px;padding:2px 8px;font-size:11px;cursor:pointer">Clear</button>
          </div>
        </div>
        <div id="serialTerminal" style="flex:1;min-height:210px;max-height:250px;overflow-y:auto;background:#000000;border:1px solid rgba(255,255,255,0.06);border-radius:6px;padding:10px;font-family:ui-monospace,SFMono-Regular,Menlo,monospace;font-size:11.5px;line-height:1.45;color:#30d158;white-space:pre-wrap;word-break:break-all"></div>
      </div>
    </div>
  </div>

  <form method="POST" action="/save" class="form-grid">
    <!-- Wi-Fi -->
    <div class="card">
      <div class="card-title">Network Configuration</div>
      <label>Wi-Fi SSID</label><input name="ssid" value="%SSID%" required>
      <label>Wi-Fi Password</label><input name="pass" type="password" value="%PASS%">
    </div>

    <!-- Speech-to-Text -->
    <div class="card">
      <div class="card-title">Speech-to-Text (STT)</div>
      <label>STT Provider</label>
      <select name="sttprovider" id="sttProvSel" onchange="toggleStt()">
        <option value="sarvam" %STT_SARVAM_SEL%>Sarvam AI (saaras:v3)</option>
        <option value="groq" %STT_GROQ_SEL%>Groq Whisper (fastest)</option>
        <option value="openai" %STT_OPENAI_SEL%>OpenAI Whisper</option>
      </select>
      <label>STT Language</label>
      <select name="sttlang">
        <option value="auto" %LANG_AUTO_SEL%>Auto (Hindi + English)</option>
        <option value="hi" %LANG_HI_SEL%>Force Hindi</option>
        <option value="en" %LANG_EN_SEL%>Force English</option>
      </select>
      <div id="sarvamBox">
        <h3>Sarvam AI Settings</h3>
        <label>Endpoint</label><input name="stturl" value="%STTURL%">
        <label>API Key</label><input name="sttkey" type="password" value="%STTKEY%">
        <label>Model</label>
        <select name="sttmodel">
          <option value="saaras:v3" %STT_SAARAS_SEL%>saaras:v3</option>
          <option value="saarika:v2" %STT_SAARIKA_SEL%>saarika:v2</option>
        </select>
      </div>
      <div id="groqSttBox">
        <h3>Groq Whisper Settings</h3>
        <label>Endpoint</label><input name="groqstturl" value="%GROQSTTURL%">
        <label>API Key</label><input name="groqsttkey" type="password" value="%GROQSTTKEY%">
        <label>Model</label>
        <select name="groqsttmodel">
          <option value="whisper-large-v3-turbo" %GW3T_SEL%>whisper-large-v3-turbo (fast)</option>
          <option value="whisper-large-v3" %GW3_SEL%>whisper-large-v3 (accurate)</option>
          <option value="distil-whisper-large-v3-en" %GDW_SEL%>distil-whisper-large-v3-en</option>
        </select>
      </div>
      <div id="openaiSttBox">
        <h3>OpenAI Whisper Settings</h3>
        <label>Endpoint</label><input name="opaistturl" value="%OPENAIURL%">
        <label>API Key</label><input name="opaisttkey" type="password" value="%OPENAIKEY%">
        <label>Model</label>
        <select name="opaisttmodel">
          <option value="whisper-1" %OW1_SEL%>whisper-1</option>
          <option value="gpt-4o-mini-transcribe" %OG4M_SEL%>gpt-4o-mini-transcribe</option>
          <option value="gpt-4o-transcribe" %OG4_SEL%>gpt-4o-transcribe</option>
        </select>
      </div>
    </div>

    <!-- AI Brain -->
    <div class="card">
      <div class="card-title">AI Intelligence (LLM)</div>
      <label>LLM Provider</label>
      <select name="llmprovider" id="provSel" onchange="toggleModels()">
        <option value="groq" %GROQ_SEL%>Groq (Ultra-Fast)</option>
        <option value="gemini" %GEMINI_SEL%>Google Gemini</option>
        <option value="openai" %OPENAI_SEL%>OpenAI GPT</option>
      </select>
      <div id="groqBox">
        <h3>Groq Model &amp; Credentials</h3>
        <label>Model</label>
        <select name="groqmodel">
          <option value="openai/gpt-oss-20b" %G20B_SEL%>openai/gpt-oss-20b</option>
          <option value="openai/gpt-oss-120b" %G120B_SEL%>openai/gpt-oss-120b</option>
          <option value="qwen/qwen3-32b" %GQ32B_SEL%>qwen/qwen3-32b</option>
          <option value="llama-3.3-70b-versatile" %GL33_SEL%>llama-3.3-70b-versatile</option>
        </select>
        <label>Endpoint</label><input name="groqurl" value="%GROQURL%">
        <label>API Key</label><input name="groqkey" type="password" value="%GROQKEY%">
      </div>
      <div id="geminiBox">
        <h3>Gemini Model &amp; Credentials</h3>
        <label>Model</label>
        <select name="llmmodel" id="geminiModelSel" onchange="updateGeminiUrl()">
          <option value="gemini-2.0-flash-lite" %GE20L_SEL%>gemini-2.0-flash-lite</option>
          <option value="gemini-2.5-flash" %GE25_SEL%>gemini-2.5-flash</option>
          <option value="gemini-2.5-flash-lite" %GE25L_SEL%>gemini-2.5-flash-lite</option>
        </select>
        <label>Endpoint</label><input name="llmurl" id="geminiUrlIn" value="%LLMURL%">
        <label>API Key</label><input name="llmkey" type="password" value="%LLMKEY%">
      </div>
      <div id="openaiBox">
        <h3>OpenAI GPT Model &amp; Credentials</h3>
        <label>Model</label>
        <select name="openaimodel" id="openaiModelSel" onchange="updateOpenAiUrl()">
          <option value="gpt-4o-mini" %GPT4OM_SEL%>gpt-4o-mini (fast, recommended)</option>
          <option value="gpt-4.1-mini" %GPT41M_SEL%>gpt-4.1-mini (quality)</option>
          <option value="gpt-4.1-nano" %GPT41N_SEL%>gpt-4.1-nano (fastest)</option>
          <option value="gpt-4o" %GPT4O_SEL%>gpt-4o (higher quality)</option>
        </select>
        <label>Endpoint</label><input name="openaiurl" id="openaiUrlIn" value="%OPENAI_LLM_URL%">
        <label>API Key</label><input name="openaikey" type="password" value="%OPENAI_LLM_KEY%">
      </div>
    </div>

    <!-- Text-to-Speech -->
    <div class="card">
      <div class="card-title">Text-to-Speech (TTS)</div>
      <label>TTS Provider</label>
      <select name="ttsprovider" id="ttsProvSel" onchange="toggleTts()">
        <option value="sarvam" %TTS_SARVAM_SEL%>Sarvam AI (bulbul:v3)</option>
        <option value="openai" %TTS_OPENAI_SEL%>OpenAI TTS</option>
        <option value="elevenlabs" %TTS_ELEVEN_SEL%>ElevenLabs</option>
      </select>
      <div id="ttsSarvamBox">
        <h3>Sarvam TTS Settings</h3>
        <label>Endpoint</label><input name="ttsurl" value="%TTSURL%">
        <label>API Key</label><input name="ttskey" type="password" value="%TTSKEY%">
        <label>Model</label><input name="ttsmodel" value="%TTSMODEL%">
        <label>Voice</label>
        <select name="ttsvoice">
%SARVAM_VOICE_OPTIONS%
        </select>
      </div>
      <div id="ttsOpenaiBox">
        <h3>OpenAI TTS Settings</h3>
        <label>Endpoint</label><input name="openaittsurl" value="%OPENAITTSURL%">
        <label>API Key</label><input name="openaittskey" type="password" value="%OPENAITTSKEY%">
        <label>Model</label><input name="openaittsmodel" value="%OPENAITTSMODEL%">
        <label>Voice</label>
        <select name="openaittsvoice">
          <option value="alloy" %OA_ALLOY_SEL%>alloy</option>
          <option value="echo" %OA_ECHO_SEL%>echo</option>
          <option value="fable" %OA_FABLE_SEL%>fable</option>
          <option value="onyx" %OA_ONYX_SEL%>onyx</option>
          <option value="nova" %OA_NOVA_SEL%>nova</option>
          <option value="shimmer" %OA_SHIMMER_SEL%>shimmer</option>
        </select>
      </div>
      <div id="ttsElevenBox">
        <h3>ElevenLabs Settings</h3>
        <label>Endpoint</label><input name="elevurl" value="%ELEVURL%">
        <label>API Key</label><input name="elevkey" type="password" value="%ELEVKEY%">
        <label>Model</label><input name="elevmodel" value="%ELEVMODEL%">
        <label>Voice ID</label><input name="elevvoice" value="%ELEVVOICE%">
      </div>
    </div>

    <!-- Audio & Sleep -->
    <div class="card">
      <div class="card-title">Audio &amp; Sleep Schedule</div>
      <label>Speaker Volume: <span id="volLabel" style="color:#60a5fa">%VOL%</span>%</label>
      <input type="range" name="vol" min="0" max="100" value="%VOL%"
             oninput="document.getElementById('volLabel').textContent=this.value">

      <label style="margin-top:20px"><input type="checkbox" name="circ_en" value="1" %CIRC_EN_CHECKED% style="width:auto;margin-right:8px"> Enable Night Sleep Schedule</label>
      <label>Bedtime Hour (0-23)</label>
      <input type="number" name="circ_start" min="0" max="23" value="%CIRC_START%">
      <label>Wake-up Hour (0-23)</label>
      <input type="number" name="circ_end" min="0" max="23" value="%CIRC_END%">
      <small>Screen dims and Kiwi sleeps during scheduled hours. Hold top sensor 7-10s to toggle manually.</small>

      <div style="margin-top:20px;padding:12px;background:#090d16;border:1px solid #334155;border-radius:8px">
        <label class="toggle" style="margin:0">
          <input type="checkbox" name="boredom" id="rootBoredomToggle" value="1" %BOREDOM_CHECKED% onchange="toggleBoredomLive(this.checked)">
          <span class="switch"></span>
          <span>Enable Autonomous Idle Desk Chatter &amp; Quirks</span>
        </label>
        <div style="margin-top:10px;display:flex;align-items:center;gap:10px;flex-wrap:wrap">
          <label style="font-size:12px;color:#94a3b8;margin:0">Idle Chatter Time:</label>
          <select name="boredom_mins" id="boredomMinsSel" onchange="setBoredomIntervalLive(this.value)" style="width:auto;margin:0;padding:6px 12px;font-size:13px;background:#0f172a;color:#38bdf8;border:1px solid #334155;border-radius:6px;cursor:pointer">
            <option value="1" %BM_1_SEL%>Every 1 Minute (Lively)</option>
            <option value="2" %BM_2_SEL%>Every 2 Minutes (Default)</option>
            <option value="3" %BM_3_SEL%>Every 3 Minutes</option>
            <option value="5" %BM_5_SEL%>Every 5 Minutes</option>
            <option value="10" %BM_10_SEL%>Every 10 Minutes</option>
            <option value="15" %BM_15_SEL%>Every 15 Minutes</option>
            <option value="30" %BM_30_SEL%>Every 30 Minutes</option>
          </select>
        </div>
        <small style="margin-top:8px;display:block">When left idle, Kiwi makes natural witty remarks, health/tea reminders, and desk banter instead of repeating robotic sounds.</small>
      </div>
    </div>

    <!-- Weather & Personality -->
    <div class="card">
      <div class="card-title">Weather &amp; Personality</div>
      <label>City Name</label><input name="wthr_city" value="%WTHR_CITY%">
      <label>Latitude</label><input name="wthr_lat" value="%WTHR_LAT%">
      <label>Longitude</label><input name="wthr_lon" value="%WTHR_LON%">
      <label style="margin-top:14px">OLED Face &amp; Eye Skin</label>
      <select name="skin" id="skinSel" onchange="toggleSkinLive(this.value)">
        <option value="default" %SKIN_DEFAULT_SEL%>Default (Clean Round Vector)</option>
        <option value="cyberpunk" %SKIN_CYBER_SEL%>Cyberpunk Visor &amp; HUD</option>
        <option value="cat" %SKIN_CAT_SEL%>Kawaii Cat Neko &amp; Ears</option>
        <option value="retro" %SKIN_RETRO_SEL%>Retro 8-Bit Arcade</option>
        <option value="anime" %SKIN_ANIME_SEL%>Anime Shounen (Starry Glint)</option>
        <option value="hearts" %SKIN_HEARTS_SEL%>Lovestruck Heart Eyes</option>
        <option value="steampunk" %SKIN_STEAM_SEL%>Steampunk Aviator Goggles</option>
        <option value="demon" %SKIN_DEMON_SEL%>Demon Spooky (Fiery Horns)</option>
        <option value="matrix" %SKIN_MATRIX_SEL%>Matrix Code Stream (Terminal)</option>
        <option value="cyborg" %SKIN_CYBORG_SEL%>Cyborg T-800 (Targeting Aperture)</option>
        <option value="mecha" %SKIN_MECHA_SEL%>Mecha Gundam (V-Fin Crest &amp; Core)</option>
        <option value="sunglasses" %SKIN_SHADES_SEL%>Boss Shades (Cool Sunglasses)</option>
        <option value="puppy" %SKIN_PUPPY_SEL%>Kawaii Puppy (Floppy Ears &amp; Boba)</option>
        <option value="hypno" %SKIN_HYPNO_SEL%>Hypno Spiral (Mesmerizing Vortex)</option>
        <option value="alien" %SKIN_ALIEN_SEL%>Cosmic Alien (UFO Orbit &amp; Antenna)</option>
        <option value="ninja" %SKIN_NINJA_SEL%>Shinobi Ninja (Headband &amp; Stealth)</option>
        <option value="round" %SKIN_ROUND_SEL%>Round Eyes (Classic Ocular)</option>
        <option value="kiwi" %SKIN_KIWI_SEL%>Desk Buddy (Cute Block)</option>
      </select>
      <label style="margin-top:16px">Robot System Prompt</label>
      <textarea name="prompt">%PROMPT%</textarea>
    </div>

    <!-- Personal Life Diary & Memory -->
    <div class="card">
      <div class="card-title">Personal Life Diary &amp; Long-Term Memory</div>
      <label>Your Name (Owner)</label>
      <input name="mem_name" value="%MEM_NAME%" placeholder="e.g. Rohil">

      <label style="margin-top:14px">Facts Kiwi Remembers About You:</label>
      <div style="background:var(--bg);border:1px solid var(--border);border-radius:var(--radius-sm);padding:12px;margin-bottom:12px;max-height:160px;overflow-y:auto">
        %MEM_LIST_HTML%
      </div>

      <label>Add New Memory Fact / Event</label>
      <input name="new_mem" placeholder="e.g. Coding test tomorrow / Loves chai">
      <small>Kiwi automatically learns facts while chatting, remembers them across reboots in Flash, and follows up on events!</small>

      <div style="display:flex;gap:10px;margin-top:14px">
        <button type="button" class="qbtn" style="flex:1" onclick="clearMemories()">Clear Memories</button>
      </div>
    </div>

    <div class="save-wrap">
      <button type="submit" class="save-btn">Save Configuration &amp; Restart</button>
    </div>
  </form>
</div>

<script>
function toggleSkinLive(v){
  fetch('/do?skin='+encodeURIComponent(v)).catch(function(){});
}
function toggleBoredomLive(val){
  fetch('/do?boredom='+(val?'1':'0')).catch(function(){});
}
function setBoredomIntervalLive(val){
  fetch('/do?boredom_mins='+encodeURIComponent(val)).catch(function(){});
}
function clearMemories(){
  if(confirm("Are you sure you want to clear all Kiwi's memories?")){
    fetch('/do?clearmem=1').then(function(){ alert("All memories cleared!"); location.reload(); });
  }
}
function toggleModels(){
  var p = document.getElementById('provSel').value;
  document.getElementById('groqBox').style.display = (p==='groq')?'block':'none';
  document.getElementById('geminiBox').style.display = (p==='gemini')?'block':'none';
  document.getElementById('openaiBox').style.display = (p==='openai')?'block':'none';
}
function updateGeminiUrl(){
  var m = document.getElementById('geminiModelSel').value;
  var u = document.getElementById('geminiUrlIn');
  if(u && (u.value.includes('generativelanguage.googleapis.com') || u.value === '')){
    u.value = 'https://generativelanguage.googleapis.com/v1beta/models/' + m + ':generateContent';
  }
  function updateOpenAiUrl(){
    var u = document.getElementById('openaiUrlIn');
    if(u && (u.value.includes('api.openai.com') || u.value === '')){
      u.value = 'https://api.openai.com/v1/chat/completions';
    }
  }
}
function toggleStt(){
  var p = document.getElementById('sttProvSel').value;
  document.getElementById('sarvamBox').style.display = (p==='sarvam')?'block':'none';
  document.getElementById('groqSttBox').style.display = (p==='groq')?'block':'none';
  document.getElementById('openaiSttBox').style.display = (p==='openai')?'block':'none';
}
function toggleTts(){
  var p = document.getElementById('ttsProvSel').value;
  document.getElementById('ttsSarvamBox').style.display = (p==='sarvam')?'block':'none';
  document.getElementById('ttsOpenaiBox').style.display = (p==='openai')?'block':'none';
  document.getElementById('ttsElevenBox').style.display = (p==='elevenlabs')?'block':'none';
}
toggleModels(); toggleStt(); toggleTts();

var autoScroll = true;
var lastLogCount = 0;
var chkAuto = document.getElementById('autoScrollCheck');
if(chkAuto) chkAuto.addEventListener('change', function(){ autoScroll = this.checked; });

var btnClear = document.getElementById('btnClearLog');
if(btnClear) {
  btnClear.addEventListener('click', function(){
    fetch('/do?clearlog=1').then(function(){
      var t = document.getElementById('serialTerminal');
      if(t) t.innerHTML = '';
      lastLogCount = 0;
    }).catch(function(){});
  });
}

var btnMic = document.getElementById('btnWebMic');
if(btnMic) {
  btnMic.addEventListener('click', function(){
    fetch('/do?record=1').then(function(r){ return r.json(); }).then(function(j){
      if(!j.ok && j.reason === 'busy') alert('Kiwi is busy processing audio/AI.');
    }).catch(function(){});
  });
}

var btnSend = document.getElementById('btnSendPrompt');
var promptIn = document.getElementById('textPromptIn');
if(btnSend && promptIn) {
  btnSend.addEventListener('click', sendPrompt);
  promptIn.addEventListener('keypress', function(e){
    if(e.key === 'Enter') { e.preventDefault(); sendPrompt(); }
  });
}

function sendPrompt(){
  var inp = document.getElementById('textPromptIn');
  var txt = inp.value.trim();
  if(!txt) return;
  fetch('/do?ask=' + encodeURIComponent(txt))
    .then(function(r){ return r.json(); })
    .then(function(j){
      if(j.ok) inp.value = '';
      else if(j.reason === 'busy') alert('Kiwi is currently busy!');
    }).catch(function(){});
}

function pollConsole(){
  fetch('/status').then(function(r){ return r.json(); }).then(function(j){
    var mic = document.getElementById('btnWebMic');
    var badgeDot = document.getElementById('liveStateDot');
    var badgeTxt = document.getElementById('liveStateText');
    var micSub = document.getElementById('micBtnSub');
    
    var state = j.state || (j.busy ? 'Busy Processing' : 'Ready');
    if(badgeTxt) badgeTxt.textContent = state;

    if(state.indexOf('Listening') >= 0 || state.indexOf('Recording') >= 0) {
      if(mic) mic.classList.add('recording');
      if(badgeDot) badgeDot.className = 'dot rec';
      if(micSub) micSub.textContent = 'Listening to microphone now...';
    } else if(j.busy) {
      if(mic) mic.classList.remove('recording');
      if(badgeDot) badgeDot.className = 'dot busy';
      if(micSub) micSub.textContent = 'AI processing request...';
    } else if(j.sleeping) {
      if(mic) mic.classList.remove('recording');
      if(badgeDot) badgeDot.className = 'dot sleep';
      if(micSub) micSub.textContent = 'Robot sleeping · Tap mic or head to wake';
    } else {
      if(mic) mic.classList.remove('recording');
      if(badgeDot) badgeDot.className = 'dot';
      if(micSub) micSub.textContent = 'Stops shortly after you finish speaking';
    }

    if(j.question) {
      var qEl = document.getElementById('boxQuestion');
      if(qEl) qEl.textContent = j.question;
    }
    if(j.answer) {
      var aEl = document.getElementById('boxAnswer');
      if(aEl) aEl.textContent = j.answer;
    }

    if(j.logs && Array.isArray(j.logs)) {
      var term = document.getElementById('serialTerminal');
      if(term && (j.logs.length !== lastLogCount || term.innerHTML === '')) {
        var h = '';
        for(var i=0; i<j.logs.length; i++){
          var line = j.logs[i];
          var col = '#4ade80';
          if(line.indexOf('[REC]') >= 0) col = '#f43f5e';
          else if(line.indexOf('[STT]') >= 0) col = '#38bdf8';
          else if(line.indexOf('[LLM]') >= 0) col = '#fbbf24';
          else if(line.indexOf('[TTS]') >= 0) col = '#c084fc';
          else if(line.indexOf('Error') >= 0 || line.indexOf('fail') >= 0 || line.indexOf('Failed') >= 0) col = '#ef4444';
          else if(line.indexOf('[CMD]') >= 0) col = '#a3e635';
          h += '<div style="color:' + col + '">' + line + '</div>';
        }
        term.innerHTML = h;
        lastLogCount = j.logs.length;
        if(autoScroll) term.scrollTop = term.scrollHeight;
      }
    }
  }).catch(function(){});
}
setInterval(pollConsole, 900);
pollConsole();
</script>
</body></html>
)rawliteral";

  String langName = (sttLang == "hi") ? "Hindi" : (sttLang == "en") ? "English" : "Auto (HI+EN)";

  h.replace("%AP%", esc(apSSID));
  h.replace("%LLMPROV%", esc(llmProvider));
  h.replace("%ACTIVEMODEL%", esc(llmProvider == "groq" ? groqModel : llmModel));
  h.replace("%STTPROV%", esc(sttProvider));
  h.replace("%STTLANGNAME%", esc(langName));
  h.replace("%TTSPROV%", esc(ttsProvider));
  h.replace("%VOL%", String(speakerVolume));

  h.replace("%STT_SARVAM_SEL%", sttProvider == "sarvam" ? "selected" : "");
  h.replace("%STT_GROQ_SEL%", sttProvider == "groq" ? "selected" : "");
  h.replace("%STT_OPENAI_SEL%", sttProvider == "openai" ? "selected" : "");
  h.replace("%LANG_AUTO_SEL%", sttLang == "auto" ? "selected" : "");
  h.replace("%LANG_HI_SEL%", sttLang == "hi" ? "selected" : "");
  h.replace("%LANG_EN_SEL%", sttLang == "en" ? "selected" : "");

  h.replace("%STTURL%", esc(sttEndpoint));
  h.replace("%STTKEY%", esc(sttKey));
  h.replace("%STT_SAARAS_SEL%", sttModel == "saaras:v3" ? "selected" : "");
  h.replace("%STT_SAARIKA_SEL%", sttModel == "saarika:v2" ? "selected" : "");
  h.replace("%GROQSTTURL%", esc(groqSttEndpoint));
  h.replace("%GROQSTTKEY%", esc(groqSttKey));
  h.replace("%GW3T_SEL%", groqSttModel == "whisper-large-v3-turbo" ? "selected" : "");
  h.replace("%GW3_SEL%", groqSttModel == "whisper-large-v3" ? "selected" : "");
  h.replace("%GDW_SEL%", groqSttModel == "distil-whisper-large-v3-en" ? "selected" : "");
  h.replace("%OPENAIURL%", esc(openaiSttEndpoint));
  h.replace("%OPENAIKEY%", esc(openaiSttKey));
  h.replace("%OW1_SEL%", openaiSttModel == "whisper-1" ? "selected" : "");
  h.replace("%OG4M_SEL%", openaiSttModel == "gpt-4o-mini-transcribe" ? "selected" : "");
  h.replace("%OG4_SEL%", openaiSttModel == "gpt-4o-transcribe" ? "selected" : "");

  h.replace("%GROQ_SEL%", llmProvider == "groq" ? "selected" : "");
  h.replace("%GEMINI_SEL%", llmProvider == "gemini" ? "selected" : "");
  h.replace("%OPENAI_SEL%", llmProvider == "openai" ? "selected" : "");
  h.replace("%G20B_SEL%", groqModel == "openai/gpt-oss-20b" ? "selected" : "");
  h.replace("%G120B_SEL%", groqModel == "openai/gpt-oss-120b" ? "selected" : "");
  h.replace("%GQ32B_SEL%", groqModel == "qwen/qwen3-32b" ? "selected" : "");
  h.replace("%GL33_SEL%", groqModel == "llama-3.3-70b-versatile" ? "selected" : "");
  h.replace("%GE20L_SEL%", (llmModel == "gemini-2.0-flash-lite" || llmModel == "gemini-3.5-flash-lite") ? "selected" : "");
  h.replace("%GE25_SEL%", llmModel == "gemini-2.5-flash" ? "selected" : "");
  h.replace("%GE25L_SEL%", llmModel == "gemini-2.5-flash-lite" ? "selected" : "");
  h.replace("%GPT4OM_SEL%", llmModel == "gpt-4o-mini" ? "selected" : "");
  h.replace("%GPT41M_SEL%", llmModel == "gpt-4.1-mini" ? "selected" : "");
  h.replace("%GPT41N_SEL%", llmModel == "gpt-4.1-nano" ? "selected" : "");
  h.replace("%GPT4O_SEL%", llmModel == "gpt-4o" ? "selected" : "");

  h.replace("%TTS_SARVAM_SEL%", ttsProvider == "sarvam" ? "selected" : "");
  h.replace("%TTS_OPENAI_SEL%", ttsProvider == "openai" ? "selected" : "");
  h.replace("%TTS_ELEVEN_SEL%", ttsProvider == "elevenlabs" ? "selected" : "");
  h.replace("%TTSURL%", esc(ttsEndpoint));
  h.replace("%TTSKEY%", esc(ttsKey));
  h.replace("%TTSMODEL%", esc(ttsModel));
  h.replace("%TTSVOICE%", esc(ttsVoice));
  static const char *const sarvamVoices[] = {
    "aditya", "ritu", "ashutosh", "priya", "neha", "rahul", "pooja", "rohan", "simran",
    "kavya", "amit", "dev", "ishita", "shreya", "ratan", "varun", "manan", "sumit",
    "roopa", "kabir", "aayan", "shubh", "advait", "anand", "tanya", "tarun", "sunny",
    "mani", "gokul"
  };
  String voiceOpts;
  voiceOpts.reserve(1600);
  for (const char *v : sarvamVoices) {
    bool sel = (ttsVoice.equalsIgnoreCase(v)) || (ttsVoice == "" && strcmp(v, "priya") == 0);
    voiceOpts += "          <option value=\"";
    voiceOpts += v;
    voiceOpts += "\"";
    if (sel) voiceOpts += " selected";
    voiceOpts += ">";
    String disp = v;
    if (disp.length() > 0) disp[0] = toupper(disp[0]);
    voiceOpts += disp;
    if (strcmp(v, "priya") == 0) voiceOpts += " (Default)";
    voiceOpts += "</option>\n";
  }
  h.replace("%SARVAM_VOICE_OPTIONS%", voiceOpts);

  h.replace("%OPENAITTSURL%", esc(openaiTtsEndpoint));
  h.replace("%OPENAITTSKEY%", esc(openaiTtsKey));
  h.replace("%OPENAITTSMODEL%", esc(openaiTtsModel));
  h.replace("%OPENAITTSVOICE%", esc(openaiTtsVoice));
  h.replace("%OA_ALLOY_SEL%", (openaiTtsVoice == "alloy" || openaiTtsVoice == "") ? "selected" : "");
  h.replace("%OA_ECHO_SEL%", openaiTtsVoice == "echo" ? "selected" : "");
  h.replace("%OA_FABLE_SEL%", openaiTtsVoice == "fable" ? "selected" : "");
  h.replace("%OA_ONYX_SEL%", openaiTtsVoice == "onyx" ? "selected" : "");
  h.replace("%OA_NOVA_SEL%", openaiTtsVoice == "nova" ? "selected" : "");
  h.replace("%OA_SHIMMER_SEL%", openaiTtsVoice == "shimmer" ? "selected" : "");

  h.replace("%ELEVURL%", esc(elevenEndpoint));
  h.replace("%ELEVKEY%", esc(elevenKey));
  h.replace("%ELEVMODEL%", esc(elevenModel));
  h.replace("%ELEVVOICE%", esc(elevenVoice));

  h.replace("%SSID%", esc(wifiSSID));
  h.replace("%PASS%", esc(wifiPassword));
  h.replace("%REPLANG%", esc(replyLang));
  h.replace("%PROMPT%", esc(systemPrompt));
  h.replace("%LLMURL%", esc(llmEndpoint));
  h.replace("%LLMKEY%", esc(llmKey));
  h.replace("%OPENAI_LLM_URL%", esc(llmEndpoint));
  h.replace("%OPENAI_LLM_KEY%", esc(llmKey));
  h.replace("%GROQURL%", esc(groqEndpoint));
  h.replace("%GROQKEY%", esc(groqKey));

  h.replace("%CIRC_EN_CHECKED%", circadianSleepEnabled ? "checked" : "");
  h.replace("%CIRC_START%", String(circadianSleepStart));
  h.replace("%BOREDOM_CHECKED%", boredomEnabled ? "checked" : "");
  h.replace("%BM_1_SEL%", boredomMinutes == 1 ? "selected" : "");
  h.replace("%BM_2_SEL%", boredomMinutes == 2 ? "selected" : "");
  h.replace("%BM_3_SEL%", boredomMinutes == 3 ? "selected" : "");
  h.replace("%BM_5_SEL%", boredomMinutes == 5 ? "selected" : "");
  h.replace("%BM_10_SEL%", boredomMinutes == 10 ? "selected" : "");
  h.replace("%BM_15_SEL%", boredomMinutes == 15 ? "selected" : "");
  h.replace("%BM_30_SEL%", boredomMinutes == 30 ? "selected" : "");
  h.replace("%WTHR_CITY%", esc(weatherCity));
  h.replace("%WTHR_LAT%", esc(weatherLat));
  h.replace("%WTHR_LON%", esc(weatherLon));
  h.replace("%SKIN_DEFAULT_SEL%", currentEyeSkin == SKIN_DEFAULT ? "selected" : "");
  h.replace("%SKIN_CYBER_SEL%", currentEyeSkin == SKIN_CYBERPUNK ? "selected" : "");
  h.replace("%SKIN_CAT_SEL%", currentEyeSkin == SKIN_CAT ? "selected" : "");
  h.replace("%SKIN_RETRO_SEL%", currentEyeSkin == SKIN_RETRO8BIT ? "selected" : "");
  h.replace("%SKIN_ANIME_SEL%", currentEyeSkin == SKIN_ANIME ? "selected" : "");
  h.replace("%SKIN_HEARTS_SEL%", currentEyeSkin == SKIN_HEARTS ? "selected" : "");
  h.replace("%SKIN_STEAM_SEL%", currentEyeSkin == SKIN_STEAMPUNK ? "selected" : "");
  h.replace("%SKIN_DEMON_SEL%", currentEyeSkin == SKIN_DEMON ? "selected" : "");
  h.replace("%SKIN_MATRIX_SEL%", currentEyeSkin == SKIN_MATRIX ? "selected" : "");
  h.replace("%SKIN_CYBORG_SEL%", currentEyeSkin == SKIN_CYBORG ? "selected" : "");
  h.replace("%SKIN_MECHA_SEL%", currentEyeSkin == SKIN_MECHA ? "selected" : "");
  h.replace("%SKIN_SHADES_SEL%", currentEyeSkin == SKIN_SUNGLASSES ? "selected" : "");
  h.replace("%SKIN_PUPPY_SEL%", currentEyeSkin == SKIN_PUPPY ? "selected" : "");
  h.replace("%SKIN_HYPNO_SEL%", currentEyeSkin == SKIN_HYPNO ? "selected" : "");
  h.replace("%SKIN_ALIEN_SEL%", currentEyeSkin == SKIN_ALIEN ? "selected" : "");
  h.replace("%SKIN_NINJA_SEL%", currentEyeSkin == SKIN_NINJA ? "selected" : "");
  h.replace("%SKIN_ROUND_SEL%", currentEyeSkin == SKIN_ROUND ? "selected" : "");
  h.replace("%SKIN_KIWI_SEL%", currentEyeSkin == SKIN_KIWI ? "selected" : "");

  h.replace("%MEM_NAME%", esc(getOwnerName()));
  String memListHtml;
  if (kiwiMemory.memoryCount == 0) {
    memListHtml = "<div style=\"color:#86868b;font-size:13px;font-style:italic;padding:4px 0\">No memories saved yet. Talk to Kiwi or type one above!</div>";
  } else {
    for (int i = 0; i < kiwiMemory.memoryCount; i++) {
      memListHtml += "<div style=\"font-size:13px;color:#1d1d1f;padding:6px 0;border-bottom:1px solid rgba(0,0,0,0.06)\">" + esc(kiwiMemory.memories[i]) + "</div>";
    }
  }
  if (kiwiMemory.pendingFollowUp.length() > 0 && !kiwiMemory.followUpTriggered) {
    memListHtml += "<div style=\"font-size:12px;color:#ff9500;padding:6px 0;font-weight:600\">Pending Follow-up: " + esc(kiwiMemory.pendingFollowUp) + "</div>";
  }
  h.replace("%MEM_LIST_HTML%", memListHtml);

  return h;
}

void handleRoot() { server.send(200, "text/html; charset=utf-8", configPage()); }
