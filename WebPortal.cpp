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

WebServer server(80);
DNSServer dnsServer;
volatile int8_t pendingSound = -1;
static bool webRoutesSet = false;
static bool customAnimationUploadResult = false;

void pumpWeb() {
  if (apMode)
    dnsServer.processNextRequest();
  server.handleClient();
}

// ============================================================
// WIFI & AP MANAGEMENT
// ============================================================
static const wifi_power_t WIFI_TX = WIFI_POWER_8_5dBm;

void createAPName() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char s[5];
  snprintf(s, sizeof(s), "%02X%02X", mac[4], mac[5]);
  apSSID = "MyRobot-" + String(s);
}

void startAP() {
  createAPName();
  WiFi.persistent(false);
  WiFi.disconnect(true, false);
  delay(150);
  WiFi.mode(WIFI_OFF);
  delay(250);
  WiFi.mode(WIFI_AP);
  delay(250);
  WiFi.setTxPower(WIFI_TX);
  WiFi.softAPConfig(apIP, apIP, apMask);
  bool ok = false;
  for (int i = 0; i < 5 && !ok; i++) {
    ok = WiFi.softAP(apSSID.c_str(), AP_PASSWORD, AP_CHANNEL, 0, 4);
    if (!ok) {
      WiFi.mode(WIFI_OFF);
      delay(300);
      WiFi.mode(WIFI_AP);
      delay(300);
      WiFi.setTxPower(WIFI_TX);
      WiFi.softAPConfig(apIP, apIP, apMask);
    }
  }
  apMode = ok;
  if (ok)
    dnsServer.start(53, "*", apIP);
}

void startAPParallel() {
  createAPName();
  WiFi.softAPConfig(apIP, apIP, apMask);
  bool ok = WiFi.softAP(apSSID.c_str(), AP_PASSWORD, AP_CHANNEL, 0, 4);
  apMode = ok;
  if (ok) {
    dnsServer.start(53, "*", apIP);
    Serial.printf("[AP] Parallel AP up: %s -> http://192.168.4.1\n", apSSID.c_str());
  }
}

bool connectHome() {
  if (!wifiSSID.length())
    return false;
  Serial.printf("[WIFI] Connecting to %s", wifiSSID.c_str());
  WiFi.persistent(false);
  WiFi.disconnect(true, false);
  delay(100);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
  WiFi.setTxPower(WIFI_TX);
  uint32_t t = millis();
  int animStep = 0;
  while (WiFi.status() != WL_CONNECTED && millis() - t < 15000) {
    drawConnectingFrame(wifiSSID.c_str(), animStep++);
    delay(160);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    apMode = false;
    WiFi.setSleep(false);
    drawConnectedNotice(wifiSSID.c_str(), 800);
    return true;
  }
  return false;
}

void maintainWiFi() {
  static uint32_t lastTry = 0;
  if (!wifiSSID.length())
    return;
  if (WiFi.status() == WL_CONNECTED && !timeSynced) {
    if (millis() - lastNtpTryAt > 60000) {
      lastNtpTryAt = millis();
      syncTimeFromNTP();
    }
  }
  if (WiFi.status() == WL_CONNECTED) {
    static uint32_t lastWeatherAttempt = 0;
    if ((lastWeatherFetchAt == 0 || millis() - lastWeatherFetchAt > 30UL * 60UL * 1000UL) &&
        (millis() - lastWeatherAttempt > 60000UL || lastWeatherAttempt == 0)) {
      lastWeatherAttempt = millis();
      fetchWeather();
    }
    return;
  }
  if (millis() - lastTry < 15000)
    return;
  lastTry = millis();
  WiFi.reconnect();
}

bool syncTimeFromNTP() {
  if (WiFi.status() != WL_CONNECTED)
    return false;
  Serial.println("[NTP] Syncing...");
  configTime(TZ_OFFSET_SEC, TZ_DST_SEC, NTP_SERVER_1, NTP_SERVER_2);
  struct tm timeinfo;
  int tries = 0;
  while (!getLocalTime(&timeinfo, 500) && tries < 20) {
    tries++;
    delay(250);
  }
  if (tries >= 20) {
    Serial.println("[NTP] Failed");
    timeSynced = false;
    return false;
  }
  timeSynced = true;
  Serial.printf("[NTP] %02d:%02d:%02d %02d/%02d/%04d\n", timeinfo.tm_hour,
                timeinfo.tm_min, timeinfo.tm_sec, timeinfo.tm_mday,
                timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);
  return true;
}

bool suspendLocalNet() {
  bool apWasUp = apMode;
  if (apWasUp) {
    dnsServer.stop();
    WiFi.softAPdisconnect(true);
    apMode = false;
    delay(300);
  }
  server.stop();
  delay(100);
  return apWasUp;
}

void restoreWebAndAP(bool apWasUp) {
  if (apWasUp)
    startAPParallel();
  startWeb();
  Serial.printf("[NET] Free heap after restore: %u\n", (unsigned)ESP.getFreeHeap());
}

// ============================================================
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
<meta name="viewport" content="width=device-width,initial-scale=1">
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
  gap:10px;padding:0 clamp(16px,3vw,32px);
  height:52px;
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
}
function updateGeminiUrl(){
  var m = document.getElementById('geminiModelSel').value;
  var u = document.getElementById('geminiUrlIn');
  if(u && (u.value.includes('generativelanguage.googleapis.com') || u.value === '')){
    u.value = 'https://generativelanguage.googleapis.com/v1beta/models/' + m + ':generateContent';
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
  h.replace("%G20B_SEL%", groqModel == "openai/gpt-oss-20b" ? "selected" : "");
  h.replace("%G120B_SEL%", groqModel == "openai/gpt-oss-120b" ? "selected" : "");
  h.replace("%GQ32B_SEL%", groqModel == "qwen/qwen3-32b" ? "selected" : "");
  h.replace("%GL33_SEL%", groqModel == "llama-3.3-70b-versatile" ? "selected" : "");
  h.replace("%GE20L_SEL%", (llmModel == "gemini-2.0-flash-lite" || llmModel == "gemini-3.5-flash-lite") ? "selected" : "");
  h.replace("%GE25_SEL%", llmModel == "gemini-2.5-flash" ? "selected" : "");
  h.replace("%GE25L_SEL%", llmModel == "gemini-2.5-flash-lite" ? "selected" : "");

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
static const char FACE_PAGE_HTML[] PROGMEM = R"RAWFACE(<!doctype html>
<html lang="en"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
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
  gap:12px;padding:0 clamp(16px,3vw,32px);
  height:52px;
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

static const char WIRING_PAGE_HTML[] PROGMEM = R"RAWWIRING(<!doctype html>
<html lang="en"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=5">
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
  gap:12px;padding:0 clamp(16px,3vw,32px);
  height:52px;
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

void handleRoot() { server.send(200, "text/html; charset=utf-8", configPage()); }
void handleFacesPage() { server.send_P(200, "text/html; charset=utf-8", FACE_PAGE_HTML); }
void handleWiring() { server.send_P(200, "text/html; charset=utf-8", WIRING_PAGE_HTML); }

void handleDoAction() {
  lastUserInteractionAt = millis();

  // Web Mic Audio Record Trigger
  if (server.hasArg("record")) {
    if (busyProcessing) {
      server.send(200, "application/json", "{\"ok\":false,\"reason\":\"busy\"}");
      return;
    }
    startRecording = true;
    addTerminalLog("[WEB] Mic record button clicked -> start listening");
    server.send(200, "application/json", "{\"ok\":true,\"recording\":true}");
    return;
  }

  // Web Text Prompt Trigger
  if (server.hasArg("ask")) {
    String q = server.arg("ask");
    q.trim();
    if (q.length() == 0) {
      server.send(200, "application/json", "{\"ok\":false,\"reason\":\"empty\"}");
      return;
    }
    if (busyProcessing) {
      server.send(200, "application/json", "{\"ok\":false,\"reason\":\"busy\"}");
      return;
    }
    webQueuedQuestion = q;
    addTerminalLog("[WEB] Queued prompt: \"" + q + "\"");
    server.send(200, "application/json", "{\"ok\":true,\"queued\":true}");
    return;
  }

  // Clear Terminal Output
  if (server.hasArg("clearlog")) {
    clearTerminalLog();
    server.send(200, "application/json", "{\"ok\":true,\"cleared\":true}");
    return;
  }

  // Soundboard FX
  if (server.hasArg("sound")) {
    String s = server.arg("sound");
    if (s == "fart") {
      setEmotion(EMOTION_SMUG);
      soundFart();
    } else if (s == "laser") {
      setEmotion(EMOTION_COOL);
      soundLaser();
    } else if (s == "fanfare") {
      setEmotion(EMOTION_PARTY);
      soundFanfare();
    } else if (s == "coin") {
      setEmotion(EMOTION_EXCITED);
      soundCoin();
    } else if (s == "giggle") {
      setEmotion(EMOTION_LAUGHING);
      soundPetGiggle();
    } else if (s == "violin") {
      setEmotion(EMOTION_CRYING);
      soundSadViolin();
    }
    returnIdleAt = millis() + 3000;
    server.send(200, "application/json", "{\"ok\":true}");
    return;
  }

  // Custom OLED Marquee Banner
  if (server.hasArg("animation")) {
    bannerActive = false;
    playAnimation((uint8_t)server.arg("animation").toInt());
    server.send(200, "application/json", "{\"ok\":true}");
    return;
  }
  if (server.hasArg("stopanimation")) {
    stopAnimation();
    setEmotion(EMOTION_IDLE);
    server.send(200, "application/json", "{\"ok\":true}");
    return;
  }
  if (server.hasArg("animations")) {
    server.send(200, "application/json", animationListJson());
    return;
  }
  if (server.hasArg("banner")) {
    stopAnimation();
    bannerText = server.arg("banner");
    bannerActive = true;
    bannerScrollX = SCREEN_WIDTH + 10;
    bannerLoops = 0;
    showClockScreen = false;
    showWeatherScreen = false;
    soundPetSparkle();
    server.send(200, "application/json", "{\"ok\":true}");
    return;
  }
  if (server.hasArg("stopbanner")) {
    bannerActive = false;
    setEmotion(EMOTION_IDLE);
    server.send(200, "application/json", "{\"ok\":true}");
    return;
  }

  // Autonomous Boredom Live Toggle
  if (server.hasArg("boredom")) {
    boredomEnabled = (server.arg("boredom") == "1" || server.arg("boredom") == "true");
    Preferences p;
    p.begin("myrobot", false);
    p.putBool("boredom", boredomEnabled);
    p.end();
    Serial.printf("[PORTAL] Boredom toggle live set to: %d\n", boredomEnabled);
    server.send(200, "application/json", boredomEnabled ? "{\"ok\":true,\"boredom\":true}" : "{\"ok\":true,\"boredom\":false}");
    return;
  }

  // Autonomous Boredom Interval Live Action
  if (server.hasArg("boredom_mins")) {
    int m = server.arg("boredom_mins").toInt();
    if (m >= 1 && m <= 60) {
      boredomMinutes = m;
      Preferences p;
      p.begin("myrobot", false);
      p.putInt("boredom_mins", boredomMinutes);
      p.end();
      resetBoredomInterval();
      Serial.printf("[PORTAL] Boredom interval live set to: %d minutes\n", boredomMinutes);
    }
    server.send(200, "application/json", "{\"ok\":true,\"boredom_mins\":" + String(boredomMinutes) + "}");
    return;
  }

  // Custom OLED Eye Skin Action
  if (server.hasArg("skin")) {
    String s = server.arg("skin");
    s.toLowerCase();
    if (s == "cyberpunk") currentEyeSkin = SKIN_CYBERPUNK;
    else if (s == "cat" || s == "neko") currentEyeSkin = SKIN_CAT;
    else if (s == "retro" || s == "8bit") currentEyeSkin = SKIN_RETRO8BIT;
    else if (s == "anime" || s == "star") currentEyeSkin = SKIN_ANIME;
    else if (s == "hearts" || s == "heart" || s == "love") currentEyeSkin = SKIN_HEARTS;
    else if (s == "steampunk" || s == "goggles") currentEyeSkin = SKIN_STEAMPUNK;
    else if (s == "demon" || s == "devil") currentEyeSkin = SKIN_DEMON;
    else if (s == "matrix" || s == "code" || s == "terminal") currentEyeSkin = SKIN_MATRIX;
    else if (s == "cyborg" || s == "terminator" || s == "t800") currentEyeSkin = SKIN_CYBORG;
    else if (s == "mecha" || s == "gundam" || s == "robot") currentEyeSkin = SKIN_MECHA;
    else if (s == "sunglasses" || s == "shades" || s == "cool") currentEyeSkin = SKIN_SUNGLASSES;
    else if (s == "puppy" || s == "dog" || s == "doggy") currentEyeSkin = SKIN_PUPPY;
    else if (s == "hypno" || s == "spiral" || s == "vortex") currentEyeSkin = SKIN_HYPNO;
    else if (s == "alien" || s == "ufo" || s == "cosmic") currentEyeSkin = SKIN_ALIEN;
    else if (s == "ninja" || s == "shinobi" || s == "naruto") currentEyeSkin = SKIN_NINJA;
    else if (s == "round" || s == "circle" || s == "ocular") currentEyeSkin = SKIN_ROUND;
    else currentEyeSkin = SKIN_DEFAULT;

    Preferences p;
    p.begin("myrobot", false);
    p.putInt("eyeskin", (int)currentEyeSkin);
    p.end();

    soundPetSparkle();
    Serial.printf("[PORTAL] Eye skin switched to: %d\n", (int)currentEyeSkin);
    server.send(200, "application/json", "{\"ok\":true,\"skin\":\"" + s + "\"}");
    return;
  }

  // Memory & Diary Actions
  if (server.hasArg("clearmem")) {
    clearAllMemory();
    server.send(200, "application/json", "{\"ok\":true,\"cleared\":true}");
    return;
  }
  if (server.hasArg("addmem")) {
    addMemoryFact(server.arg("addmem"));
    server.send(200, "application/json", "{\"ok\":true}");
    return;
  }
  if (server.hasArg("setname")) {
    setOwnerName(server.arg("setname"));
    server.send(200, "application/json", "{\"ok\":true}");
    return;
  }

  String e = server.hasArg("e") ? server.arg("e") : "";
  uint32_t ms = server.hasArg("ms") ? (uint32_t)server.arg("ms").toInt() : 60000;
  bool snd = server.hasArg("s") ? (server.arg("s").toInt() != 0) : true;
  if (busyProcessing) {
    server.send(200, "application/json", "{\"ok\":false,\"reason\":\"busy\"}");
    return;
  }
  if (e == "sleep" || e == "sleeping") {
    manualSleep = true;
    inCircadianSleep = true;
    showClockScreen = false;
    showWeatherScreen = false;
    setOledDim(false);
    setEmotion(EMOTION_SLEEPING);
    if (snd) soundPetSleepy();
    server.send(200, "application/json", "{\"ok\":true,\"sleeping\":true}");
    return;
  }
  if (e == "wake") {
    manualSleep = false;
    inCircadianSleep = false;
    setOledDim(false);
    setEmotion(EMOTION_HAPPY);
    if (snd) soundPetSparkle();
    server.send(200, "application/json", "{\"ok\":true,\"sleeping\":false}");
    return;
  }
  if (e == "clock") {
    manualSleep = false;
    inCircadianSleep = false;
    setOledDim(false);
    showClockScreen = true;
    showWeatherScreen = false;
    if (snd) soundClock();
    if (!timeSynced && WiFi.status() == WL_CONNECTED) syncTimeFromNTP();
    server.send(200, "application/json", "{\"ok\":true}");
    return;
  }
  if (e == "weather") {
    manualSleep = false;
    inCircadianSleep = false;
    setOledDim(false);
    showWeatherScreen = true;
    showClockScreen = false;
    if (snd) soundPetSparkle();
    if (lastWeatherFetchAt == 0 || millis() - lastWeatherFetchAt > 10 * 60 * 1000 || weatherCode < 0) {
      fetchWeather();
    }
    server.send(200, "application/json", "{\"ok\":true}");
    return;
  }
  if (e == "clear" || e == "forget") {
    clearConversationHistory();
    server.send(200, "application/json", "{\"ok\":true,\"cleared\":true}");
    return;
  }
  bool ok = triggerEmotionByName(e, ms, snd);
  server.send(200, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false}");
}

void handleStatus() {
  static const char *names[] = {
      "idle", "listening", "thinking", "speaking", "happy", "excited",
      "sad", "confused", "angry", "sleeping", "error", "petted",
      "surprised", "wink", "smug", "curious", "bored", "laughing",
      "scared", "suspicious", "relieved", "proud", "dizzy", "lovestruck",
      "evil", "crying", "hearts", "dead", "cool", "shocked",
      "singing", "party", "nervous", "cute"};
  int idx = (int)currentEmotion;
  if (idx < 0 || idx > 33)
    idx = 0;
  String j = "{\"emotion\":\"" + String(names[idx]) + "\",\"busy\":";
  j += busyProcessing ? "true," : "false,";
  j += "\"sleeping\":";
  j += (inCircadianSleep || manualSleep) ? "true," : "false,";
  j += "\"history\":" + String(getConversationHistoryCount());
  const char *skinNames[] = {
    "default", "cyberpunk", "cat", "retro", "anime", "hearts", "steampunk", "demon",
    "matrix", "cyborg", "mecha", "sunglasses", "puppy", "hypno", "alien", "ninja", "round"
  };
  int skIdx = (int)currentEyeSkin;
  if (skIdx < 0 || skIdx >= 17) skIdx = 0;
  j += ",\"boredom\":" + String(boredomEnabled ? "true" : "false");
  j += ",\"boredom_mins\":" + String(boredomMinutes);
  j += ",\"skin\":\"" + String(skinNames[skIdx]) + "\"";
  j += ",\"state\":\"" + escJson(robotStateDesc) + "\"";
  j += ",\"question\":\"" + escJson(lastTranscribedQuestion) + "\"";
  j += ",\"answer\":\"" + escJson(lastLlmAnswer) + "\"";
  j += ",\"logs\":" + getTerminalJson();
  j += "}";
  server.send(200, "application/json", j);
}

void handleSave() {
  if (!server.hasArg("ssid")) {
    server.send(400, "text/plain", "SSID required");
    return;
  }
  wifiSSID = server.arg("ssid");
  wifiPassword = server.arg("pass");

  if (server.hasArg("sttprovider"))
    sttProvider = normalizeSttProvider(server.arg("sttprovider"));
  if (server.hasArg("sttlang"))
    sttLang = normalizeSttLang(server.arg("sttlang"));

  sttEndpoint = server.arg("stturl");
  sttKey = server.arg("sttkey");
  sttModel = server.arg("sttmodel");
  groqSttEndpoint = server.arg("groqstturl");
  groqSttKey = server.arg("groqsttkey");
  groqSttModel = server.arg("groqsttmodel");
  openaiSttEndpoint = server.arg("opaistturl");
  openaiSttKey = server.arg("opaisttkey");
  openaiSttModel = server.arg("opaisttmodel");

  replyLang = server.arg("replang");
  systemPrompt = server.arg("prompt");

  if (server.hasArg("llmprovider")) {
    llmProvider = server.arg("llmprovider");
    llmProvider.trim();
    llmProvider.toLowerCase();
    if (llmProvider != "gemini" && llmProvider != "groq")
      llmProvider = "groq";
  }
  llmEndpoint = server.arg("llmurl");
  llmKey = server.arg("llmkey");
  llmModel = server.arg("llmmodel");
  groqEndpoint = server.arg("groqurl");
  groqKey = server.arg("groqkey");
  groqModel = server.arg("groqmodel");

  if (server.hasArg("ttsprovider"))
    ttsProvider = normalizeTtsProvider(server.arg("ttsprovider"));
  ttsEndpoint = server.arg("ttsurl");
  ttsKey = server.arg("ttskey");
  ttsModel = server.arg("ttsmodel");
  ttsVoice = server.arg("ttsvoice");
  openaiTtsEndpoint = server.arg("openaittsurl");
  openaiTtsKey = server.arg("openaittskey");
  openaiTtsModel = server.arg("openaittsmodel");
  openaiTtsVoice = server.arg("openaittsvoice");
  elevenEndpoint = server.arg("elevurl");
  elevenKey = server.arg("elevkey");
  elevenModel = server.arg("elevmodel");
  elevenVoice = server.arg("elevvoice");

  if (server.hasArg("vol")) {
    speakerVolume = server.arg("vol").toInt();
    if (speakerVolume < 0)
      speakerVolume = 0;
    if (speakerVolume > 100)
      speakerVolume = 100;
  }

  circadianSleepEnabled = server.hasArg("circ_en");
  if (server.hasArg("circ_start"))
    circadianSleepStart = server.arg("circ_start").toInt();
  if (server.hasArg("circ_end"))
    circadianSleepEnd = server.arg("circ_end").toInt();
  boredomEnabled = server.hasArg("boredom");
  if (server.hasArg("boredom_mins")) {
    int bm = server.arg("boredom_mins").toInt();
    if (bm >= 1 && bm <= 60) {
      boredomMinutes = bm;
      resetBoredomInterval();
    }
  }

  if (server.hasArg("skin")) {
    String s = server.arg("skin");
    s.toLowerCase();
    if (s == "cyberpunk") currentEyeSkin = SKIN_CYBERPUNK;
    else if (s == "cat" || s == "neko") currentEyeSkin = SKIN_CAT;
    else if (s == "retro" || s == "8bit") currentEyeSkin = SKIN_RETRO8BIT;
    else if (s == "anime" || s == "star") currentEyeSkin = SKIN_ANIME;
    else if (s == "hearts" || s == "heart" || s == "love") currentEyeSkin = SKIN_HEARTS;
    else if (s == "steampunk" || s == "goggles") currentEyeSkin = SKIN_STEAMPUNK;
    else if (s == "demon" || s == "devil") currentEyeSkin = SKIN_DEMON;
    else if (s == "matrix" || s == "code" || s == "terminal") currentEyeSkin = SKIN_MATRIX;
    else if (s == "cyborg" || s == "terminator" || s == "t800") currentEyeSkin = SKIN_CYBORG;
    else if (s == "mecha" || s == "gundam" || s == "robot") currentEyeSkin = SKIN_MECHA;
    else if (s == "sunglasses" || s == "shades" || s == "cool") currentEyeSkin = SKIN_SUNGLASSES;
    else if (s == "puppy" || s == "dog" || s == "doggy") currentEyeSkin = SKIN_PUPPY;
    else if (s == "hypno" || s == "spiral" || s == "vortex") currentEyeSkin = SKIN_HYPNO;
    else if (s == "alien" || s == "ufo" || s == "cosmic") currentEyeSkin = SKIN_ALIEN;
    else if (s == "ninja" || s == "shinobi" || s == "naruto") currentEyeSkin = SKIN_NINJA;
    else if (s == "round" || s == "circle" || s == "ocular") currentEyeSkin = SKIN_ROUND;
    else currentEyeSkin = SKIN_DEFAULT;
  }

  if (server.hasArg("mem_name"))
    setOwnerName(server.arg("mem_name"));
  if (server.hasArg("new_mem") && server.arg("new_mem").length() > 0)
    addMemoryFact(server.arg("new_mem"));

  if (server.hasArg("wthr_city"))
    weatherCity = server.arg("wthr_city");
  if (server.hasArg("wthr_lat"))
    weatherLat = server.arg("wthr_lat");
  if (server.hasArg("wthr_lon"))
    weatherLon = server.arg("wthr_lon");
  weatherCity.trim();
  weatherLat.trim();
  weatherLon.trim();

  wifiSSID.trim();
  wifiPassword.trim();
  replyLang.trim();
  sttEndpoint.trim();
  sttKey.trim();
  sttModel.trim();
  groqSttEndpoint.trim();
  groqSttKey.trim();
  groqSttModel.trim();
  openaiSttEndpoint.trim();
  openaiSttKey.trim();
  openaiSttModel.trim();
  llmEndpoint.trim();
  llmKey.trim();
  llmModel.trim();
  groqEndpoint.trim();
  groqKey.trim();
  groqModel.trim();
  ttsEndpoint.trim();
  ttsKey.trim();
  ttsModel.trim();
  ttsVoice.trim();
  openaiTtsEndpoint.trim();
  openaiTtsKey.trim();
  openaiTtsModel.trim();
  openaiTtsVoice.trim();
  elevenEndpoint.trim();
  elevenKey.trim();
  elevenModel.trim();
  elevenVoice.trim();

  saveConfig();
  server.send(200, "text/html",
              "<html><body style='font-family:Arial;text-align:center;background:#111;color:white;padding:50px'>"
              "<h1>Saved</h1><p>Restarting Kiwi...</p></body></html>");
  delay(1200);
  ESP.restart();
}

void handleOtaPage() {
  static const char OTA_HTML[] PROGMEM = R"rawliteral(<!doctype html>
<html lang="en"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Kiwi Robot · Firmware OTA</title>
<link rel="icon" href="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 64 64'%3E%3Crect width='64' height='64' rx='18' fill='%230071e3'/%3E%3Ccircle cx='22' cy='27' r='6' fill='white'/%3E%3Ccircle cx='42' cy='27' r='6' fill='white'/%3E%3Cpath d='M18 42c8 7 20 7 28 0' fill='none' stroke='white' stroke-width='5' stroke-linecap='round'/%3E%3C/svg%3E">
<style>
:root{
  --bg:#f5f5f7;--surface:#ffffff;--border:rgba(60,60,67,0.18);--border-subtle:rgba(0,0,0,0.08);
  --text:#1d1d1f;--secondary:#424245;--tertiary:#86868b;
  --blue:#0071e3;--blue-dark:#005bb5;
  --green:#34c759;--red:#ff3b30;
  --radius-sm:8px;--radius-md:12px;--radius-lg:18px;
  --shadow-card:0 4px 24px rgba(0,0,0,0.06),0 1px 3px rgba(0,0,0,0.04);
}
*{box-sizing:border-box;-webkit-font-smoothing:antialiased;-moz-osx-font-smoothing:grayscale}
body{
  margin:0;
  font-family:-apple-system,BlinkMacSystemFont,'SF Pro Text','Helvetica Neue',sans-serif;
  font-size:14px;line-height:1.47059;
  background:var(--bg);color:var(--text);
  -webkit-text-size-adjust:100%;
}
nav{
  display:flex;flex-wrap:wrap;align-items:center;justify-content:space-between;
  gap:12px;padding:0 clamp(16px,3vw,32px);
  height:52px;
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
a.back{
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
a.back:hover{background:#fff;border-color:rgba(60,60,67,0.3)}
a.back:active{transform:scale(0.97)}
a.back.primary{background:var(--blue);border-color:var(--blue);color:#fff}

.container{
  width:min(680px,100%);
  margin:0 auto;
  padding:40px clamp(16px,3vw,24px) 64px;
}
.box{
  background:var(--surface);
  border:1px solid var(--border);
  border-radius:var(--radius-lg);
  padding:clamp(24px,5vw,40px);
  box-shadow:var(--shadow-card);
  text-align:center;
}
.update-icon{
  width:64px;height:64px;margin:0 auto 18px;
  background:linear-gradient(180deg,#e8f2ff,#d0e5ff);
  border-radius:18px;display:flex;align-items:center;justify-content:center;
  color:var(--blue);box-shadow:0 2px 8px rgba(0,113,227,0.12);
}
h1{
  font-size:22px;font-weight:600;letter-spacing:-0.02em;
  margin:0 0 8px;color:var(--text);
}
p.sub{
  color:var(--secondary);font-size:14px;margin:0 0 24px;line-height:1.5;
}

/* File Picker */
.file-picker{
  display:flex;flex-direction:column;align-items:center;gap:12px;
  padding:24px 16px;margin:20px 0 10px;
  border:2px dashed var(--border);
  border-radius:var(--radius-md);
  background:var(--bg);
  transition:all 120ms ease;
}
.file-picker:hover{border-color:var(--blue);background:rgba(0,113,227,0.02)}
.file-picker input{position:absolute;width:1px;height:1px;opacity:0;pointer-events:none}
.file-picker-label{
  display:inline-flex;align-items:center;justify-content:center;
  padding:10px 22px;border-radius:var(--radius-sm);
  background:#fff;border:1px solid var(--border);
  color:var(--text);font-size:13px;font-weight:600;
  cursor:pointer;transition:all 120ms ease;
  box-shadow:0 1px 3px rgba(0,0,0,0.06);
}
.file-picker-label:hover{background:var(--bg);border-color:rgba(60,60,67,0.3)}
.file-picker-label:active{transform:scale(0.97)}
.file-name{
  font-size:13px;color:var(--secondary);word-break:break-all;max-width:90%;
}
.file-help{
  color:var(--tertiary);font-size:12px;margin-bottom:20px;
}

button{
  width:100%;height:44px;
  background:var(--blue);color:#fff;
  border:0;border-radius:var(--radius-sm);
  font-size:14px;font-weight:600;font-family:inherit;
  cursor:pointer;
  box-shadow:0 2px 8px rgba(0,113,227,0.22);
  transition:background 120ms ease,transform 80ms ease;
}
button:hover{background:var(--blue-dark)}
button:active{transform:scale(0.98)}
button:disabled{background:#d1d1d6;color:#8e8e93;cursor:not-allowed;box-shadow:none}

#prg{
  margin-top:22px;width:100%;height:8px;
  background:#e5e5ea;border-radius:999px;
  overflow:hidden;display:none;
}
#bar{
  height:100%;width:0%;
  background:var(--blue);border-radius:999px;
  transition:width .15s ease;
}
#stat{
  margin-top:14px;font-size:13px;font-weight:600;min-height:20px;
}

.alert-box{
  margin-top:28px;padding:16px 18px;
  background:rgba(0,113,227,0.05);border:1px solid rgba(0,113,227,0.18);
  border-radius:var(--radius-md);text-align:left;
  font-size:13px;line-height:1.5;color:var(--secondary);
}
.alert-box b{color:var(--text)}
</style></head><body>

<nav>
  <div class="brand">
    <span class="brand-badge">KIWI</span>
    <span>Firmware Over-The-Air Update</span>
  </div>
  <div class="nav-actions">
    <a class="back" href="/">Dashboard</a>
    <a class="back" href="/faces">Faces</a>
    <a class="back" href="/wiring">Wiring</a>
    <a class="back primary" href="/update">OTA</a>
  </div>
</nav>

<div class="container">
  <div class="box">
    <div class="update-icon">
      <svg width="32" height="32" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
        <path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"/>
        <polyline points="17 8 12 3 7 8"/>
        <line x1="12" y1="3" x2="12" y2="15"/>
      </svg>
    </div>
    <h1>Flash Firmware Wirelessly</h1>
    <p class="sub">Select the compiled <code>MyRobot_Modular.ino.bin</code> from your build folder to update your Kiwi Robot.</p>

    <form method="POST" action="/update" enctype="multipart/form-data" id="otaForm">
      <div class="file-picker">
        <input type="file" name="update" accept=".bin" required id="fileIn">
        <label class="file-picker-label" for="fileIn">Choose Firmware Binary...</label>
        <span class="file-name" id="fileName">No file selected</span>
      </div>
      <div class="file-help">Accepts compiled .bin application or filesystem binaries.</div>
      <button type="submit" id="btn">Update Firmware</button>
    </form>

    <div id="prg"><div id="bar"></div></div>
    <div id="stat"></div>

    <div class="alert-box">
      <b>Partition Notice:</b><br>
      For OTA updates to succeed, the ESP32 partition table must have OTA partitions enabled (e.g. <b>Minimal SPIFFS (Large APPS with OTA)</b>). If this is your first time switching schemes from <code>huge_app</code>, please flash once via USB cable in the Arduino IDE.
    </div>
  </div>
</div>

<script>
var form=document.getElementById('otaForm');
var fileInput=document.getElementById('fileIn');
var fileName=document.getElementById('fileName');
fileInput.addEventListener('change',function(){
  fileName.textContent=this.files.length ? this.files[0].name : 'No file selected';
  fileName.style.color=this.files.length ? '#1d1d1f' : '#86868b';
  fileName.style.fontWeight=this.files.length ? '600' : '400';
});
form.onsubmit=function(e){
  e.preventDefault();
  var file=document.getElementById('fileIn').files[0];
  if(!file)return;
  var xhr=new XMLHttpRequest();
  var fd=new FormData();
  fd.append('update',file);
  document.getElementById('prg').style.display='block';
  document.getElementById('btn').disabled=true;
  document.getElementById('stat').style.color='#0071e3';
  document.getElementById('stat').textContent='Uploading firmware binary...';
  xhr.upload.onprogress=function(pe){
    if(pe.lengthComputable){
      var pct=Math.round((pe.loaded/pe.total)*100);
      document.getElementById('bar').style.width=pct+'%';
      document.getElementById('stat').textContent='Flashing: '+pct+'%';
    }
  };
  xhr.onload=function(){
    if(xhr.status===200 && xhr.responseText.trim()==='OK'){
      document.getElementById('stat').style.color='#34c759';
      document.getElementById('stat').innerHTML='&#10004; Flash Success! Kiwi is restarting with new firmware...';
      setTimeout(function(){window.location.href='/';},9000);
    }else{
      document.getElementById('stat').style.color='#ff3b30';
      document.getElementById('stat').innerHTML='&#10008; '+(xhr.responseText||'Flash Failed (Check partition table or serial monitor)');
      document.getElementById('btn').disabled=false;
    }
  };
  xhr.onerror=function(){
    document.getElementById('stat').style.color='#ff3b30';
    document.getElementById('stat').innerHTML='&#10008; Network connection interrupted during upload';
    document.getElementById('btn').disabled=false;
  };
  xhr.open('POST','/update');
  xhr.send(fd);
};
</script>
</body></html>)rawliteral";
  server.send_P(200, "text/html; charset=utf-8", OTA_HTML);
}

void processPendingSound() {
  if (pendingSound < 0)
    return;
  int8_t s = pendingSound;
  pendingSound = -1;
  if (!txHandle || busyProcessing)
    return;
  switch (s) {
  case 0: soundPetLove(); break;
  case 1: soundPetGiggle(); break;
  case 2: soundPetShy(); break;
  case 3: soundPetSparkle(); break;
  case 4: soundPetKiss(); break;
  case 5: soundPetCuddle(); break;
  case 6: soundPetPlayful(); break;
  case 7: soundPetSleepy(); break;
  case 8: soundPetProud(); break;
  case 9: soundPetSurprised(); break;
  }
}

void startWeb() {
  if (!webRoutesSet) {
    server.on("/", HTTP_GET, handleRoot);
    server.on("/save", HTTP_POST, handleSave);
    server.on("/faces", HTTP_GET, handleFacesPage);
    server.on("/wiring", HTTP_GET, handleWiring);
    server.on("/do", HTTP_GET, handleDoAction);
    server.on("/animations", HTTP_GET, []() {
      server.send(200, "application/json", animationListJson());
    });
    server.on(
        "/animation-upload", HTTP_POST,
        []() {
          server.send(customAnimationUploadResult ? 200 : 400,
                      "application/json",
                      customAnimationUploadResult
                          ? "{\"ok\":true}"
                          : "{\"ok\":false,\"error\":\"Expected 1024 bitmap bytes\"}");
        },
        []() {
          HTTPUpload &upload = server.upload();
          if (upload.status == UPLOAD_FILE_START) {
            stopAnimation();
            beginCustomAnimationUpload(upload.filename, server.arg("name"));
            customAnimationUploadResult = false;
          } else if (upload.status == UPLOAD_FILE_WRITE) {
            if (!writeCustomAnimationUpload(upload.buf, upload.currentSize))
              customAnimationUploadResult = false;
          } else if (upload.status == UPLOAD_FILE_END) {
            customAnimationUploadResult = finishCustomAnimationUpload();
          } else if (upload.status == UPLOAD_FILE_ABORTED) {
            customAnimationUploadResult = false;
          }
        });
    server.on("/status", HTTP_GET, handleStatus);
    server.on("/update", HTTP_GET, handleOtaPage);
    server.on(
        "/update", HTTP_POST,
        []() {
          server.sendHeader("Connection", "close");
          if (Update.hasError()) {
            server.send(500, "text/plain", String("FAIL: ") + Update.errorString());
          } else {
            server.send(200, "text/plain", "OK");
            delay(1000);
            ESP.restart();
          }
        },
        []() {
          HTTPUpload &upload = server.upload();
          if (upload.status == UPLOAD_FILE_START) {
            Serial.printf("[OTA] Update: %s\n", upload.filename.c_str());
            if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
              Update.printError(Serial);
            }
          } else if (upload.status == UPLOAD_FILE_WRITE) {
            if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
              Update.printError(Serial);
            }
          } else if (upload.status == UPLOAD_FILE_END) {
            if (Update.end(true)) {
              Serial.printf("[OTA] Success: %u bytes\n", upload.totalSize);
            } else {
              Update.printError(Serial);
            }
          }
        });
    webRoutesSet = true;
  }
  server.begin();
}
