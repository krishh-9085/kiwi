import os
import sys

# Ensure UTF-8 output on Windows consoles
if sys.platform == "win32":
    try:
        sys.stdout.reconfigure(encoding="utf-8")
        sys.stderr.reconfigure(encoding="utf-8")
    except Exception:
        pass

import json

FIRMWARE_VERSION = "v1.0"
import time
import urllib.request
import urllib.parse
import urllib.error
import webbrowser
from http.server import HTTPServer, BaseHTTPRequestHandler
import argparse

DEV_DIR = os.path.dirname(os.path.abspath(__file__))
if DEV_DIR not in sys.path:
    sys.path.insert(0, DEV_DIR)

TEMPLATES_DIR = os.path.join(DEV_DIR, "templates")
CONFIG_PATH = os.path.join(DEV_DIR, "mock_config.json")

# State for local simulation
proxy_robot_ip = None
simulated_logs = [
    "[BOOT] Kiwi System Booting (ESP32-C3 DevKitM-1)",
    "[OLED] SSD1306 128x64 display initialized OK",
    "[I2S] Audio engine ready (MAX98357A + INMP441)",
    "[NET] Connected to Wi-Fi. IP: 192.168.1.100 (RSSI: -52dBm)",
    "[NTP] Clock synced: 16:55:00",
    "[SYS] System ready. Eye skin: Default · Boredom interval: 5m",
    "[CMD] Web portal listening on port 80"
]
log_counter = 0

DEV_BAR_HTML = """
<!-- KIWI UI DEV TOOLBAR -->
<div id="kiwiDevToolbar" style="
  position:fixed;bottom:14px;right:14px;z-index:999999;
  background:rgba(28,28,30,0.95);backdrop-filter:blur(16px);
  border:1px solid rgba(255,255,255,0.18);border-radius:12px;
  padding:10px 14px;box-shadow:0 8px 32px rgba(0,0,0,0.45);
  font-family:-apple-system,BlinkMacSystemFont,'SF Pro Text','Segoe UI',sans-serif;
  color:#f2f2f7;font-size:12px;display:flex;align-items:center;gap:10px;
  transition:all 200ms ease;
">
  <div style="display:flex;align-items:center;gap:6px">
    <span id="devModeDot" style="width:8px;height:8px;border-radius:50%;background:#34c759;display:inline-block"></span>
    <span id="devModeLabel" style="font-weight:600">Dev Server</span>
  </div>
  <div style="height:16px;width:1px;background:rgba(255,255,255,0.15)"></div>
  <div style="display:flex;align-items:center;gap:6px">
    <input type="text" id="devRobotIpIn" placeholder="Robot IP (e.g. 192.168.1.50)" style="
      background:#2c2c2e;border:1px solid rgba(255,255,255,0.2);color:#fff;
      border-radius:6px;padding:4px 8px;font-size:11px;width:140px;outline:none;
    ">
    <button type="button" id="devBtnConnect" style="
      background:#0071e3;border:0;color:#fff;border-radius:6px;padding:4px 10px;
      font-size:11px;font-weight:600;cursor:pointer;
    ">Proxy</button>
  </div>
  <div style="height:16px;width:1px;background:rgba(255,255,255,0.15)"></div>
  <button type="button" id="devBtnSyncCpp" title="Export HTML changes back to C++ firmware source" style="
    background:rgba(255,255,255,0.1);border:1px solid rgba(255,255,255,0.2);color:#38bdf8;
    border-radius:6px;padding:4px 10px;font-size:11px;font-weight:600;cursor:pointer;
  ">💾 Sync to C++</button>
  <button type="button" onclick="location.reload()" title="Refresh Page" style="
    background:transparent;border:0;color:#aeaeb2;cursor:pointer;font-size:14px;padding:0 4px;
  ">🔄</button>
</div>
<script>
(function(){
  function updateDevStatus(){
    fetch('/api/dev/status').then(r=>r.json()).then(data=>{
      var dot = document.getElementById('devModeDot');
      var lbl = document.getElementById('devModeLabel');
      var inIp = document.getElementById('devRobotIpIn');
      var btn = document.getElementById('devBtnConnect');
      if(data.proxy_robot_ip){
        dot.style.background = '#0071e3';
        lbl.innerText = 'Proxy: ' + data.proxy_robot_ip;
        inIp.value = data.proxy_robot_ip;
        btn.innerText = 'Disconnect';
        btn.style.background = '#ff3b30';
      } else {
        dot.style.background = '#34c759';
        lbl.innerText = 'Local Mock Mode';
        btn.innerText = 'Connect';
        btn.style.background = '#0071e3';
      }
    }).catch(function(){});
  }
  document.getElementById('devBtnConnect').onclick = function(){
    var inIp = document.getElementById('devRobotIpIn').value.trim();
    var curBtn = document.getElementById('devBtnConnect').innerText;
    var target = (curBtn === 'Disconnect') ? '' : inIp;
    fetch('/api/dev/set_robot', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify({ip: target})
    }).then(r=>r.json()).then(function(){
      updateDevStatus();
      location.reload();
    });
  };
  document.getElementById('devBtnSyncCpp').onclick = function(){
    var btn = this;
    btn.innerText = '⏳ Syncing...';
    fetch('/api/dev/sync', {method: 'POST'}).then(r=>r.json()).then(res=>{
      if(res.ok){
        btn.innerText = '✅ Synced to C++!';
        setTimeout(function(){ btn.innerText = '💾 Sync to C++'; }, 2200);
      } else {
        alert('Sync error: ' + (res.error || 'Unknown'));
        btn.innerText = '💾 Sync to C++';
      }
    }).catch(function(e){
      alert('Sync failed: ' + e);
      btn.innerText = '💾 Sync to C++';
    });
  };
  updateDevStatus();
})();
</script>
"""

def load_config():
    if os.path.exists(CONFIG_PATH):
        try:
            with open(CONFIG_PATH, "r", encoding="utf-8") as f:
                return json.load(f)
        except Exception:
            pass
    return {}

def save_config(cfg):
    with open(CONFIG_PATH, "w", encoding="utf-8") as f:
        json.dump(cfg, f, indent=2)

def render_dashboard():
    cfg = load_config()
    with open(os.path.join(TEMPLATES_DIR, "dashboard.html"), "r", encoding="utf-8") as f:
        html = f.read()

    # Placeholders replacement
    stt_lang = cfg.get("sttLang", "auto")
    lang_name = "Hindi" if stt_lang == "hi" else ("English" if stt_lang == "en" else "Auto (HI+EN)")
    llm_prov = cfg.get("llmProvider", "gemini")
    active_model = cfg.get("groqModel", "openai/gpt-oss-20b") if llm_prov == "groq" else cfg.get("llmModel", "gemini-2.0-flash-lite")

    replacements = {
        "%AP%": cfg.get("apSSID", "MyRobot-DEV"),
        "%LLMPROV%": llm_prov,
        "%ACTIVEMODEL%": active_model,
        "%STTPROV%": cfg.get("sttProvider", "sarvam"),
        "%STTLANGNAME%": lang_name,
        "%TTSPROV%": cfg.get("ttsProvider", "sarvam"),
        "%SSID%": cfg.get("wifiSSID", "MyWiFi"),
        "%PASS%": cfg.get("wifiPassword", ""),
        "%STT_SARVAM_SEL%": "selected" if cfg.get("sttProvider") == "sarvam" else "",
        "%STT_GROQ_SEL%": "selected" if cfg.get("sttProvider") == "groq" else "",
        "%STT_OPENAI_SEL%": "selected" if cfg.get("sttProvider") == "openai" else "",
        "%LANG_AUTO_SEL%": "selected" if stt_lang == "auto" else "",
        "%LANG_HI_SEL%": "selected" if stt_lang == "hi" else "",
        "%LANG_EN_SEL%": "selected" if stt_lang == "en" else "",
        "%STTURL%": cfg.get("sttEndpoint", ""),
        "%STTKEY%": cfg.get("sttKey", ""),
        "%STT_SAARAS_SEL%": "selected" if cfg.get("sttModel") == "saaras:v3" else "",
        "%STT_SAARIKA_SEL%": "selected" if cfg.get("sttModel") == "saarika:v2" else "",
        "%GROQSTTURL%": cfg.get("groqSttEndpoint", ""),
        "%GROQSTTKEY%": cfg.get("groqSttKey", ""),
        "%GW3T_SEL%": "selected" if cfg.get("groqSttModel") == "whisper-large-v3-turbo" else "",
        "%GW3_SEL%": "selected" if cfg.get("groqSttModel") == "whisper-large-v3" else "",
        "%GDW_SEL%": "selected" if cfg.get("groqSttModel") == "distil-whisper-large-v3-en" else "",
        "%OPENAIURL%": cfg.get("openaiSttEndpoint", ""),
        "%OPENAIKEY%": cfg.get("openaiSttKey", ""),
        "%OW1_SEL%": "selected" if cfg.get("openaiSttModel") == "whisper-1" else "",
        "%OG4M_SEL%": "selected" if cfg.get("openaiSttModel") == "gpt-4o-mini-transcribe" else "",
        "%OG4_SEL%": "selected" if cfg.get("openaiSttModel") == "gpt-4o-transcribe" else "",
        "%GROQ_SEL%": "selected" if llm_prov == "groq" else "",
        "%GEMINI_SEL%": "selected" if llm_prov == "gemini" else "",
        "%OPENAI_SEL%": "selected" if llm_prov == "openai" else "",
        "%G20B_SEL%": "selected" if cfg.get("groqModel") == "openai/gpt-oss-20b" else "",
        "%G120B_SEL%": "selected" if cfg.get("groqModel") == "openai/gpt-oss-120b" else "",
        "%GQ32B_SEL%": "selected" if cfg.get("groqModel") == "qwen/qwen3-32b" else "",
        "%GL33_SEL%": "selected" if cfg.get("groqModel") == "llama-3.3-70b-versatile" else "",
        "%GE20L_SEL%": "selected" if "gemini-2.0-flash-lite" in cfg.get("llmModel", "") else "",
        "%GE25_SEL%": "selected" if cfg.get("llmModel") == "gemini-2.5-flash" else "",
        "%GE25L_SEL%": "selected" if cfg.get("llmModel") == "gemini-2.5-flash-lite" else "",
        "%GPT4OM_SEL%": "selected" if cfg.get("llmModel") == "gpt-4o-mini" else "",
        "%GPT41M_SEL%": "selected" if cfg.get("llmModel") == "gpt-4.1-mini" else "",
        "%GPT41N_SEL%": "selected" if cfg.get("llmModel") == "gpt-4.1-nano" else "",
        "%GPT4O_SEL%": "selected" if cfg.get("llmModel") == "gpt-4o" else "",
        "%LLMURL%": cfg.get("llmEndpoint", ""),
        "%LLMKEY%": cfg.get("llmKey", ""),
        "%OPENAI_LLM_URL%": cfg.get("llmEndpoint", ""),
        "%OPENAI_LLM_KEY%": cfg.get("llmKey", ""),
        "%GROQURL%": cfg.get("groqEndpoint", ""),
        "%GROQKEY%": cfg.get("groqKey", ""),
        "%TTS_SARVAM_SEL%": "selected" if cfg.get("ttsProvider") == "sarvam" else "",
        "%TTS_OPENAI_SEL%": "selected" if cfg.get("ttsProvider") == "openai" else "",
        "%TTS_ELEVEN_SEL%": "selected" if cfg.get("ttsProvider") == "elevenlabs" else "",
        "%TTS_FISH_SEL%": "selected" if cfg.get("ttsProvider") in ["fish", "fishaudio"] else "",
        "%TTSURL%": cfg.get("ttsEndpoint", ""),
        "%TTSKEY%": cfg.get("ttsKey", ""),
        "%SARVAM_V4F_SEL%": "selected" if cfg.get("ttsModel") == "bulbul:v4-flash" else "",
        "%SARVAM_V3_SEL%": "selected" if cfg.get("ttsModel") == "bulbul:v3" else "",
        "%SARVAM_V2_SEL%": "selected" if cfg.get("ttsModel") == "bulbul:v2" else "",
        "%OPENAITTSURL%": cfg.get("openaiTtsEndpoint", ""),
        "%OPENAITTSKEY%": cfg.get("openaiTtsKey", ""),
        "%OPENAITTSMODEL%": cfg.get("openaiTtsModel", "gpt-4o-mini-tts"),
        "%OPENAITTSVOICE%": cfg.get("openaiTtsVoice", "alloy"),
        "%OA_ALLOY_SEL%": "selected" if cfg.get("openaiTtsVoice") == "alloy" else "",
        "%OA_ECHO_SEL%": "selected" if cfg.get("openaiTtsVoice") == "echo" else "",
        "%OA_FABLE_SEL%": "selected" if cfg.get("openaiTtsVoice") == "fable" else "",
        "%OA_ONYX_SEL%": "selected" if cfg.get("openaiTtsVoice") == "onyx" else "",
        "%OA_NOVA_SEL%": "selected" if cfg.get("openaiTtsVoice") == "nova" else "",
        "%OA_SHIMMER_SEL%": "selected" if cfg.get("openaiTtsVoice") == "shimmer" else "",
        "%ELEVURL%": cfg.get("elevenEndpoint", ""),
        "%ELEVKEY%": cfg.get("elevenKey", ""),
        "%ELEVMODEL%": cfg.get("elevenModel", ""),
        "%ELEVVOICE%": cfg.get("elevenVoice", ""),
        "%FISHURL%": cfg.get("fishEndpoint", ""),
        "%FISHKEY%": cfg.get("fishKey", ""),
        "%FISHMODEL%": cfg.get("fishModel", ""),
        "%FISHVOICE%": cfg.get("fishVoice", ""),
        "%VOL%": str(cfg.get("speakerVolume", 85)),
        "%REPLANG%": cfg.get("replyLang", "auto"),
        "%BRIGHT%": str(cfg.get("displayBrightness", 90)),
        "%NIGHT_CHECK_0%": "checked" if cfg.get("nightMode") == 0 else "",
        "%NIGHT_CHECK_1%": "checked" if cfg.get("nightMode") == 1 else "",
        "%NIGHT_CHECK_2%": "checked" if cfg.get("nightMode") == 2 else "",
        "%NIGHT_SEL_0%": "selected" if cfg.get("nightMode") == 0 else "",
        "%NIGHT_SEL_1%": "selected" if cfg.get("nightMode") == 1 else "",
        "%NIGHT_SEL_2%": "selected" if cfg.get("nightMode") == 2 else "",
        "%NIGHT_BRIGHT%": str(cfg.get("nightBrightness", 15)),
        "%NIGHT_MODE%": str(cfg.get("nightMode", 1)),
        "%NIGHT_SLIDER_STYLE%": "display:none;" if cfg.get("nightMode") == 0 else "",
        "%CIRC_EN_CHECKED%": "checked" if cfg.get("circadianSleepEnabled", True) else "",
        "%CIRC_EN_VAL%": "1" if cfg.get("circadianSleepEnabled", True) else "0",
        "%GREET_EN_CHECKED%": "checked" if cfg.get("greetingsEnabled", False) else "",
        "%GREET_EN_VAL%": "1" if cfg.get("greetingsEnabled", False) else "0",
        "%CIRC_START%": str(cfg.get("circadianSleepStart", 23)),
        "%CIRC_END%": str(cfg.get("circadianSleepEnd", 7)),
        "%BOREDOM_CHECKED%": "checked" if cfg.get("boredomEnabled") else "",
        "%BOREDOM_VAL%": "1" if cfg.get("boredomEnabled") else "0",
        "%BM_1_SEL%": "selected" if cfg.get("boredomMinutes") == 1 else "",
        "%BM_2_SEL%": "selected" if cfg.get("boredomMinutes") == 2 else "",
        "%BM_3_SEL%": "selected" if cfg.get("boredomMinutes") == 3 else "",
        "%BM_5_SEL%": "selected" if cfg.get("boredomMinutes") == 5 else "",
        "%BM_10_SEL%": "selected" if cfg.get("boredomMinutes") == 10 else "",
        "%BM_15_SEL%": "selected" if cfg.get("boredomMinutes") == 15 else "",
        "%BM_30_SEL%": "selected" if cfg.get("boredomMinutes") == 30 else "",
        "%WTHR_CITY%": cfg.get("weatherCity", ""),
        "%WTHR_LAT%": cfg.get("weatherLat", ""),
        "%WTHR_LON%": cfg.get("weatherLon", ""),
        "%PROMPT%": cfg.get("systemPrompt", ""),
        "%MEM_NAME%": cfg.get("ownerName", "Rohil"),
        "%MEM_FRIENDS%": cfg.get("friendsFamily", ""),
        "%TCH_DEB%": "50",
        "%TCH_SND_CHECKED%": "checked",
        "%ACT_TOP_TAP%": "listen",
        "%ACT_TOP_DBL%": "skin",
        "%ACT_LT_TAP%": "pet",
        "%ACT_LT_DBL%": "tickle",
        "%ACT_LT_HLD%": "vol_dn",
        "%ACT_RT_TAP%": "clock",
        "%ACT_RT_DBL%": "weather",
        "%ACT_RT_HLD%": "vol_up",
    }

    # Generate Sarvam voices options (all 29 official voices)
    sarvam_voices = [
        ("amit", "Amit (Male · Clear & Expressive)"),
        ("priya", "Priya (Female · Default)"),
        ("aditya", "Aditya (Male · Dynamic)"),
        ("ritu", "Ritu (Female · Natural)"),
        ("ashutosh", "Ashutosh (Male · Deep Voice)"),
        ("neha", "Neha (Female · Warm)"),
        ("rahul", "Rahul (Male · Casual)"),
        ("pooja", "Pooja (Female · Friendly)"),
        ("rohan", "Rohan (Male · Young)"),
        ("simran", "Simran (Female · Cheerful)"),
        ("kabir", "Kabir (Male · Authoritative)"),
        ("shreya", "Shreya (Female · Sweet)"),
        ("varun", "Varun (Male · Energetic)"),
        ("ishita", "Ishita (Female · Professional)"),
        ("kavya", "Kavya (Female · Soft)"),
        ("dev", "Dev (Male · Modern)"),
        ("ratan", "Ratan (Male · Classic)"),
        ("manan", "Manan (Male)"),
        ("sumit", "Sumit (Male)"),
        ("roopa", "Roopa (Female)"),
        ("aayan", "Aayan (Male)"),
        ("shubh", "Shubh (Male)"),
        ("advait", "Advait (Male)"),
        ("anand", "Anand (Male)"),
        ("tanya", "Tanya (Female)"),
        ("tarun", "Tarun (Male)"),
        ("sunny", "Sunny (Male)"),
        ("mani", "Mani (Male)"),
        ("gokul", "Gokul (Male)")
    ]
    cur_voice = cfg.get("ttsVoice", "priya")
    voice_opts = ""
    for vid, vlabel in sarvam_voices:
        sel = "selected" if cur_voice == vid else ""
        voice_opts += f'<option value="{vid}" {sel}>{vlabel}</option>\n'
    replacements["%SARVAM_VOICE_OPTIONS%"] = voice_opts

    # Hour options
    def make_hours(sel_h):
        out = ""
        for h in range(24):
            h12 = 12 if (h % 12 == 0) else (h % 12)
            ampm = "PM" if h >= 12 else "AM"
            sel = "selected" if h == sel_h else ""
            out += f'<option value="{h}" {sel}>{h12:02d}:00 {ampm}</option>\n'
        return out
    replacements["%CIRC_START_OPTIONS%"] = make_hours(cfg.get("circadianSleepStart", 23))
    replacements["%CIRC_END_OPTIONS%"] = make_hours(cfg.get("circadianSleepEnd", 7))

    # Memory list HTML
    mem_html = ""
    for idx, fact in enumerate(cfg.get("memories", [])):
        mem_html += f'''
        <div style="display:flex;align-items:center;justify-content:space-between;background:var(--bg);border:1px solid var(--border);border-radius:8px;padding:8px 12px;margin-bottom:6px">
          <span style="font-size:13px;color:var(--text)">{fact}</span>
          <button type="button" class="qbtn" style="padding:2px 8px;font-size:11px;color:var(--red);border-color:rgba(255,59,48,0.2)" onclick="this.parentElement.remove()">Delete</button>
        </div>'''
    replacements["%MEM_LIST_HTML%"] = mem_html

    for k, v in replacements.items():
        html = html.replace(k, str(v))

    return html

class DevPortalHandler(BaseHTTPRequestHandler):
    def proxy_to_robot(self, path, method="GET", body=None, headers=None):
        global proxy_robot_ip
        if not proxy_robot_ip:
            return None
        url = f"http://{proxy_robot_ip}{path}"
        try:
            req = urllib.request.Request(url, data=body, method=method)
            if headers:
                for k, v in headers.items():
                    if k.lower() not in ["host", "content-length"]:
                        req.add_header(k, v)
            with urllib.request.urlopen(req, timeout=3.5) as resp:
                resp_data = resp.read()
                return resp.status, resp.headers.get("Content-Type", "application/json"), resp_data
        except Exception as e:
            err = json.dumps({"error": f"Proxy request to {url} failed: {e}", "proxy_ip": proxy_robot_ip})
            return 502, "application/json", err.encode("utf-8")

    def do_GET(self):
        global proxy_robot_ip, simulated_logs, log_counter
        parsed = urllib.parse.urlparse(self.path)
        path = parsed.path
        query = urllib.parse.parse_qs(parsed.query)

                # Apple Touch Icon & Favicon
        if path in ["/apple-touch-icon.png", "/apple-touch-icon-precomposed.png", "/favicon.ico"]:
            icon_path = os.path.join(os.path.dirname(__file__), "apple-touch-icon.png")
            if not os.path.exists(icon_path):
                icon_path = os.path.join(WORKSPACE, "apple-touch-icon.png")
            if os.path.exists(icon_path):
                with open(icon_path, "rb") as f:
                    icon_data = f.read()
                self.send_response(200)
                self.send_header("Content-Type", "image/png")
                self.send_header("Cache-Control", "public, max-age=86400")
                self.end_headers()
                self.wfile.write(icon_data)
                return

        # Dev endpoints
        if path == "/api/dev/status":
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(json.dumps({
                "mode": "proxy" if proxy_robot_ip else "mock",
                "proxy_robot_ip": proxy_robot_ip
            }).encode("utf-8"))
            return

        # If proxying active, forward status, config, scan, animations, and do to robot
        if proxy_robot_ip and path in ["/status", "/config", "/scan", "/animations", "/do"]:
            res = self.proxy_to_robot(self.path, method="GET")
            if res:
                status, ctype, data = res
                self.send_response(status)
                self.send_header("Content-Type", ctype)
                self.send_header("Access-Control-Allow-Origin", "*")
                self.end_headers()
                self.wfile.write(data)
                return

        # Web pages
        if path == "/" or path == "/index.html":
            html = render_dashboard()
            html = html.replace("</body>", DEV_BAR_HTML + "</body>")
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.end_headers()
            self.wfile.write(html.encode("utf-8"))
            return

        if path == "/faces":
            with open(os.path.join(TEMPLATES_DIR, "faces.html"), "r", encoding="utf-8") as f:
                html = f.read()
            html = html.replace("</body>", DEV_BAR_HTML + "</body>")
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.end_headers()
            self.wfile.write(html.encode("utf-8"))
            return

        if path == "/wiring":
            self.send_response(302)
            self.send_header("Location", "/")
            self.end_headers()
            return

        if path == "/update":
            with open(os.path.join(TEMPLATES_DIR, "update.html"), "r", encoding="utf-8") as f:
                html = f.read()
            html = html.replace("</body>", DEV_BAR_HTML + "</body>")
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.end_headers()
            self.wfile.write(html.encode("utf-8"))
            return

        # Mock API: /status
        if path == "/status":
            emotions = ["idle", "happy", "curious", "wink", "excited", "listening"]
            cfg_curr = load_config()
            sk = cfg_curr.get("currentEyeSkin", "kiwi")
            show_mouth = cfg_curr.get("showMouth", True)
            log_counter += 1
            if log_counter % 3 == 0:
                simulated_logs.append(f"[HEARTBEAT] Free heap: 142850 B · RSSI: -51 dBm · CPU: 160MHz")
            if len(simulated_logs) > 30:
                simulated_logs.pop(0)

            data = {
                "emotion": ("sleeping" if cfg_curr.get("sleeping", False) else emotions[(int(time.time()) // 4) % len(emotions)]),
                "busy": False,
                "sleeping": cfg_curr.get("sleeping", False),
                "history": 4,
                "boredom": True,
                "boredom_mins": 5,
                "skin": sk,
                "mouth": show_mouth,
                "circadian": {
                    "enabled": cfg_curr.get("circadianSleepEnabled", True),
                    "start": cfg_curr.get("circadianSleepStart", 23),
                    "end": cfg_curr.get("circadianSleepEnd", 7)
                },
                "greetings": {
                    "enabled": cfg_curr.get("greetingsEnabled", False)
                },
                "state": ("Sleeping" if cfg_curr.get("sleeping", False) else "Idle & Happy"),
                "question": "What's up Kiwi?",
                "answer": "Surviving on 3.3V and pure attitude! What are we building today?",
                "version": FIRMWARE_VERSION,
                "logs": simulated_logs
            }
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Access-Control-Allow-Origin", "*")
            self.end_headers()
            self.wfile.write(json.dumps(data).encode("utf-8"))
            return

        # Mock API: /ota/status
        if path == "/ota/status":
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Access-Control-Allow-Origin", "*")
            self.end_headers()
            self.wfile.write(json.dumps({
                "state": "idle",
                "progress": 0,
                "bytes_read": 0,
                "bytes_total": 0,
                "running": False,
                "error": ""
            }).encode("utf-8"))
            return

        # Mock API: /config
        if path == "/config":
            cfg = load_config()
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(json.dumps(cfg).encode("utf-8"))
            return

        # Mock API: /scan
        if path == "/scan":
            nets = [
                {"ssid": "MyHome_WiFi_5G", "rssi": -48, "secure": True},
                {"ssid": "LivingRoom_2.4G", "rssi": -62, "secure": True},
                {"ssid": "Neighbor_IoT", "rssi": -81, "secure": True},
                {"ssid": "Open_Guest_WiFi", "rssi": -88, "secure": False}
            ]
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(json.dumps(nets).encode("utf-8"))
            return

        # Mock API: /do
        if path == "/do":
            action_desc = "action"
            cfg = load_config()
            cfg_changed = False
            for k in query:
                action_desc = f"{k}={query[k][0]}"
                val = query[k][0]
                if k in ["vol", "volume"]:
                    cfg["speakerVolume"] = int(val); cfg_changed = True
                elif k == "brightness":
                    cfg["displayBrightness"] = int(val); cfg_changed = True
                elif k == "night_bright":
                    cfg["nightBrightness"] = int(val); cfg_changed = True
                elif k == "night_mode":
                    cfg["nightMode"] = int(val); cfg_changed = True
                elif k == "boredom":
                    cfg["boredomEnabled"] = (val == "1" or val == "true"); cfg_changed = True
                elif k == "boredom_mins":
                    cfg["boredomMinutes"] = int(val); cfg_changed = True
                elif k == "mouth":
                    cfg["showMouth"] = (val == "1" or val == "true"); cfg_changed = True
                elif k == "skin":
                    cfg["currentEyeSkin"] = val; cfg_changed = True
                elif k == "circ_en":
                    cfg["circadianSleepEnabled"] = (val == "1" or val == "true"); cfg_changed = True
                elif k == "greet_en":
                    cfg["greetingsEnabled"] = (val == "1" or val == "true" or val == "on"); cfg_changed = True
                elif k == "circ_start":
                    cfg["circadianSleepStart"] = int(val); cfg_changed = True
                elif k == "circ_end":
                    cfg["circadianSleepEnd"] = int(val); cfg_changed = True
                elif k == "sleep":
                    if val == "toggle":
                        cfg["sleeping"] = not cfg.get("sleeping", False)
                    else:
                        cfg["sleeping"] = (val == "1" or val == "true")
                    cfg_changed = True
            if cfg_changed:
                save_config(cfg)
            simulated_logs.append(f"[ACTION] Live parameter update: {action_desc}")
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(json.dumps({"ok": True, "action": action_desc}).encode("utf-8"))
            return

        # Mock API: /animations
        if path == "/animations":
            anims = [
                {"id": "blink", "name": "Blink", "frames": 12},
                {"id": "wink", "name": "Wink", "frames": 16},
                {"id": "happy", "name": "Happy", "frames": 24},
                {"id": "hearts", "name": "Love Hearts", "frames": 20}
            ]
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(json.dumps(anims).encode("utf-8"))
            return

        self.send_response(404)
        self.end_headers()
        self.wfile.write(b"404 Not Found")

    def do_POST(self):
        global proxy_robot_ip
        parsed = urllib.parse.urlparse(self.path)
        path = parsed.path
        length = int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(length) if length > 0 else b""

        # Dev set robot IP
        if path == "/api/dev/set_robot":
            try:
                data = json.loads(body.decode("utf-8"))
                ip = data.get("ip", "").strip()
                proxy_robot_ip = ip if ip else None
                print(f"[DEV] Proxy target set to: {proxy_robot_ip or 'None (Mock Mode)'}")
                self.send_response(200)
                self.send_header("Content-Type", "application/json")
                self.end_headers()
                self.wfile.write(json.dumps({"ok": True, "proxy_robot_ip": proxy_robot_ip}).encode("utf-8"))
            except Exception as e:
                self.send_response(400)
                self.end_headers()
                self.wfile.write(json.dumps({"ok": False, "error": str(e)}).encode("utf-8"))
            return

        # Dev sync to C++
        if path == "/api/dev/sync":
            try:
                import sync_to_cpp
                sync_to_cpp.sync_dashboard()
                sync_to_cpp.sync_faces()
                sync_to_cpp.sync_wiring()
                sync_to_cpp.sync_ota()
                self.send_response(200)
                self.send_header("Content-Type", "application/json")
                self.end_headers()
                self.wfile.write(json.dumps({"ok": True}).encode("utf-8"))
            except Exception as e:
                self.send_response(500)
                self.send_header("Content-Type", "application/json")
                self.end_headers()
                self.wfile.write(json.dumps({"ok": False, "error": str(e)}).encode("utf-8"))
            return

        # Forward POST to robot if in proxy mode
        if proxy_robot_ip and path in ["/save", "/update", "/animation-upload", "/ota/install"]:
            res = self.proxy_to_robot(self.path, method="POST", body=body, headers=dict(self.headers))
            if res:
                status, ctype, data = res
                self.send_response(status)
                self.send_header("Content-Type", ctype)
                self.end_headers()
                self.wfile.write(data)
                return

        # Mock /ota/install handler
        if path == "/ota/install":
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Access-Control-Allow-Origin", "*")
            self.end_headers()
            self.wfile.write(json.dumps({"ok": True, "message": "OTA update initiated (mock mode)"}).encode("utf-8"))
            return

        # Mock /save form handler
        if path == "/save":
            try:
                params = urllib.parse.parse_qs(body.decode("utf-8"))
                cfg = load_config()
                for k, v in params.items():
                    val = v[0]
                    if k == "vol": cfg["speakerVolume"] = int(val)
                    elif k == "bright": cfg["displayBrightness"] = int(val)
                    elif k == "night_bright": cfg["nightBrightness"] = int(val)
                    elif k == "night_mode": cfg["nightMode"] = int(val)
                    elif k == "prompt": cfg["systemPrompt"] = val
                    elif k == "owner": cfg["ownerName"] = val
                    elif k == "ssid": cfg["wifiSSID"] = val
                    elif k == "pass": cfg["wifiPassword"] = val
                    elif k == "stt_prov": cfg["sttProvider"] = val
                    elif k == "stt_lang": cfg["sttLang"] = val
                    elif k == "llm_prov": cfg["llmProvider"] = val
                    elif k == "tts_prov" or k == "ttsprovider": cfg["ttsProvider"] = val
                    elif k == "sttprovider": cfg["sttProvider"] = val
                    elif k == "llmprovider": cfg["llmProvider"] = val
                    elif k == "sttlang": cfg["sttLang"] = val
                    elif k == "llmkey": cfg["llmKey"] = val
                    elif k == "groqkey":
                        cfg["groqKey"] = val
                        cfg["groqSttKey"] = val
                    elif k == "ttskey":
                        cfg["ttsKey"] = val
                        cfg["sttKey"] = val
                    elif k == "openaikey":
                        cfg["openaiKey"] = val
                        cfg["openaiSttKey"] = val
                        cfg["openaiTtsKey"] = val
                    elif k == "elevkey": cfg["elevenKey"] = val
                    elif k == "fishkey": cfg["fishKey"] = val
                save_config(cfg)
            except Exception as e:
                print(f"[SAVE] Error parsing form: {e}")

            # Return the exact saved confirmation screen from WebPortalApi.cpp
            resp_html = """<!doctype html><html><head>
<meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>
<meta http-equiv='refresh' content='4;url=/'>
<title>Saved · Kiwi</title>
<style>
body{background:#0b0f19;color:#f8fafc;font-family:-apple-system,BlinkMacSystemFont,Segoe UI,Roboto,sans-serif;
display:flex;align-items:center;justify-content:center;height:100vh;margin:0;padding:20px;box-sizing:border-box;text-align:center;}
.card{background:rgba(255,255,255,0.05);border:1px solid rgba(255,255,255,0.1);border-radius:24px;padding:36px;max-width:380px;box-shadow:0 20px 40px rgba(0,0,0,0.5);}
.spin{display:inline-block;width:44px;height:44px;border:4px solid rgba(255,255,255,0.15);border-top-color:#38bdf8;border-radius:50%;animation:sp 0.8s linear infinite;margin-bottom:18px;}
@keyframes sp{to{transform:rotate(360deg);}}
h2{margin:0 0 10px;font-size:22px;color:#38bdf8;} p{color:#94a3b8;font-size:14px;margin:0 0 18px;line-height:1.5;}
#sec{font-weight:700;color:#f8fafc;}
</style></head><body>
<div class='card'>
<div class='spin'></div>
<h2>Settings Saved!</h2>
<p>Kiwi Dev Server: Changes applied!<br>Redirecting to dashboard in <span id='sec'>4</span>s</p>
<script>var c=4;var iv=setInterval(function(){c--;var el=document.getElementById('sec');if(el)el.innerText=c;if(c<=0){clearInterval(iv);location.href='/';}},1000);</script>
</div></body></html>"""
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.end_headers()
            self.wfile.write(resp_html.encode("utf-8"))
            return

        self.send_response(404)
        self.end_headers()

def main():
    global proxy_robot_ip
    parser = argparse.ArgumentParser(description="Kiwi Robot UI Local Dev Server")
    parser.add_argument("--port", type=int, default=8080, help="Port to serve UI (default: 8080)")
    parser.add_argument("--robot", type=str, default="", help="Robot IP to proxy API requests to")
    parser.add_argument("--no-open", action="store_true", help="Do not open browser automatically")
    args = parser.parse_args()

    if args.robot:
        proxy_robot_ip = args.robot.strip()
        print(f"🤖 Proxying API calls to live ESP32 at: http://{proxy_robot_ip}")
    else:
        print("🟢 Running in Local Mock Mode (no hardware needed)")

    server_address = ("127.0.0.1", args.port)
    httpd = HTTPServer(server_address, DevPortalHandler)
    url = f"http://localhost:{args.port}"
    print(f"\n=======================================================")
    print(f"   KIWI ROBOT UI DEV SERVER IS RUNNING!")
    print(f"   URL: {url}")
    print(f"   - Edit files in: tools/web_dev/templates/")
    print(f"   - Refresh your browser anytime (instant hot reload)")
    print(f"   - Click '💾 Sync to C++' or run sync_to_cpp.py to export")
    print(f"=======================================================\n")

    if not args.no_open:
        try:
            webbrowser.open(url)
        except Exception:
            pass

    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nStopping dev server...")
        httpd.server_close()

if __name__ == "__main__":
    main()
