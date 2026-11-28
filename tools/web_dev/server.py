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
import time
import urllib.request
import urllib.parse
import urllib.error
import webbrowser
from http.server import HTTPServer, BaseHTTPRequestHandler
import argparse

FIRMWARE_VERSION = "v1.0"

DEV_DIR = os.path.dirname(os.path.abspath(__file__))
if DEV_DIR not in sys.path:
    sys.path.insert(0, DEV_DIR)

WORKSPACE = os.path.abspath(os.path.join(DEV_DIR, "..", ".."))
REACT_DIR = os.path.join(WORKSPACE, "react_ui")
DIST_HTML = os.path.join(REACT_DIR, "dist", "index.html")
CONFIG_PATH = os.path.join(DEV_DIR, "mock_config.json")

# State for local simulation
proxy_robot_ip = None
simulated_logs = [
    "[BOOT] Kiwi System Booting (ESP32-S3 DevKit)",
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
  <button type="button" id="devBtnSyncCpp" title="Export React bundle to C++ firmware header" style="
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

def get_react_dashboard_html():
    if os.path.exists(DIST_HTML):
        try:
            with open(DIST_HTML, "r", encoding="utf-8") as f:
                return f.read()
        except Exception as e:
            return f"<h1>Error reading React bundle: {e}</h1>"

    return """<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Kiwi Robot Dev Server</title>
  <style>
    body { background: #0b0f19; color: #f8fafc; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; display: flex; align-items: center; justify-content: center; height: 100vh; margin: 0; padding: 20px; box-sizing: border-box; text-align: center; }
    .card { background: rgba(255,255,255,0.05); border: 1px solid rgba(255,255,255,0.1); border-radius: 20px; padding: 36px; max-width: 480px; box-shadow: 0 20px 40px rgba(0,0,0,0.5); }
    h1 { color: #38bdf8; font-size: 22px; margin: 0 0 12px; }
    p { color: #94a3b8; font-size: 14px; margin: 0 0 20px; line-height: 1.6; }
    code { background: rgba(255,255,255,0.1); padding: 2px 6px; border-radius: 4px; color: #f1f5f9; font-size: 13px; }
    .actions { display: flex; flex-direction: column; gap: 10px; }
    a.btn { display: block; background: #0284c7; color: white; padding: 12px 20px; border-radius: 10px; text-decoration: none; font-weight: 600; font-size: 14px; transition: background 0.2s; }
    a.btn:hover { background: #0369a1; }
  </style>
</head>
<body>
  <div class="card">
    <h1>Kiwi React UI Not Built Yet</h1>
    <p>The single-file React bundle at <code>react_ui/dist/index.html</code> was not found.<br>Run <code>npm run build</code> inside <code>react_ui</code>, or start the Vite live dev server:</p>
    <div class="actions">
      <a class="btn" href="http://localhost:5173" target="_blank">Open Vite Dev Server (Port 5173)</a>
    </div>
  </div>
</body>
</html>"""

class DevPortalHandler(BaseHTTPRequestHandler):
    def do_OPTIONS(self):
        self.send_response(204)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "*")
        self.end_headers()

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
            icon_path = os.path.join(DEV_DIR, "apple-touch-icon.png")
            if not os.path.exists(icon_path):
                icon_path = os.path.join(WORKSPACE, "apple-touch-icon.png")
            if os.path.exists(icon_path):
                with open(icon_path, "rb") as f:
                    icon_data = f.read()
                self.send_response(200)
                self.send_header("Content-Type", "image/png")
                self.send_header("Access-Control-Allow-Origin", "*")
                self.send_header("Cache-Control", "public, max-age=86400")
                self.end_headers()
                self.wfile.write(icon_data)
                return

        if path == "/manifest.json":
            self.send_response(200)
            self.send_header("Content-Type", "application/manifest+json")
            self.send_header("Access-Control-Allow-Origin", "*")
            self.send_header("Cache-Control", "no-cache")
            self.end_headers()
            manifest = {
                "name": "Kiwi Robot",
                "short_name": "Kiwi",
                "start_url": "/",
                "display": "standalone",
                "background_color": "#0e1015",
                "theme_color": "#0e1015",
                "icons": [{
                    "src": "/apple-touch-icon.png?v=white",
                    "sizes": "180x180",
                    "type": "image/png",
                    "purpose": "any maskable"
                }]
            }
            self.wfile.write(json.dumps(manifest).encode("utf-8"))
            return

        # Dev endpoints
        if path == "/api/dev/status":
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Access-Control-Allow-Origin", "*")
            self.end_headers()
            self.wfile.write(json.dumps({
                "mode": "proxy" if proxy_robot_ip else "mock",
                "proxy_robot_ip": proxy_robot_ip
            }).encode("utf-8"))
            return

        # If proxying active, forward status, logs, config, scan, animations, and do to robot
        if proxy_robot_ip and path in ["/status", "/logs", "/config", "/scan", "/animations", "/do", "/ota/status"]:
            res = self.proxy_to_robot(self.path, method="GET")
            if res:
                status, ctype, data = res
                self.send_response(status)
                self.send_header("Content-Type", ctype)
                self.send_header("Access-Control-Allow-Origin", "*")
                self.end_headers()
                self.wfile.write(data)
                return

        # Web pages (Unified React SPA)
        if path in ["/", "/index.html", "/faces", "/wiring", "/update"]:
            html = get_react_dashboard_html()
            if "</body>" in html:
                html = html.replace("</body>", DEV_BAR_HTML + "</body>")
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Access-Control-Allow-Origin", "*")
            self.end_headers()
            self.wfile.write(html.encode("utf-8"))
            return

        # Mock API: /status (Fast Telemetry)
        if path == "/status":
            emotions = ["idle", "happy", "curious", "wink", "excited", "listening"]
            cfg_curr = load_config()
            sk = cfg_curr.get("currentEyeSkin", cfg_curr.get("skin", "kiwi"))
            show_mouth = cfg_curr.get("showMouth", True)
            log_counter += 1
            if log_counter % 3 == 0:
                simulated_logs.append(f"[HEARTBEAT] Free heap: 142850 B · RSSI: -51 dBm · CPU: 160MHz")
            if len(simulated_logs) > 30:
                simulated_logs.pop(0)

            data = {
                "host": "kiwi.local",
                "emotion": ("sleeping" if cfg_curr.get("sleeping", False) else emotions[(int(time.time()) // 4) % len(emotions)]),
                "busy": cfg_curr.get("busy", False),
                "sleeping": cfg_curr.get("sleeping", False),
                "history": 4,
                "boredom": cfg_curr.get("boredomEnabled", True),
                "boredom_mins": cfg_curr.get("boredomMinutes", 5),
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
                "soundReactive": {
                    "enabled": cfg_curr.get("soundReactiveEnabled", False)
                },
                "rgbLed": {
                    "enabled": cfg_curr.get("rgbLedEnabled", True),
                    "brightness": cfg_curr.get("rgbLedBrightness", 63)
                },
                "wakeWord": {
                    "enabled": cfg_curr.get("wakeWordEnabled", True),
                    "model": int(cfg_curr.get("wakeWordModel", 1)),
                    "name": ("Alexa" if int(cfg_curr.get("wakeWordModel", 1)) == 0 else "Hey Kiwi"),
                    "cutoff": float(cfg_curr.get("wakeWordCutoff", 0.90 if int(cfg_curr.get("wakeWordModel", 1)) == 0 else 0.92)),
                    "listening": True
                },
                "timer_active": False,
                "timer_ringing": False,
                "timer_remaining": 0,
                "hunger": "satisfied",
                "hunger_sec": 3600,
                "meal": "Breakfast",
                "state": ("Sleeping" if cfg_curr.get("sleeping", False) else cfg_curr.get("state", "Ready")),
                "question": cfg_curr.get("question", "What's up Kiwi?"),
                "answer": cfg_curr.get("answer", "Surviving on 3.3V and pure attitude! What are we building today?"),
                "version": FIRMWARE_VERSION,
                "heap": {
                    "free": 178420,
                    "total": 327680,
                    "used": 149260,
                    "pct": 45
                },
                "psram": {
                    "free": 1951744,
                    "total": 2097152,
                    "used": 145408,
                    "pct": 7
                },
                "flash": {
                    "used": 1648200,
                    "free": 383416,
                    "total": 2031616,
                    "chip": 4194304,
                    "pct": 81
                },
                "temp": 48.5,
                "owner": cfg_curr.get("ownerName") or cfg_curr.get("memory", {}).get("name", "Rohil")
            }
            if "logs" in query or query.get("logs") == ["1"]:
                data["logs"] = simulated_logs

            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Access-Control-Allow-Origin", "*")
            self.end_headers()
            self.wfile.write(json.dumps(data).encode("utf-8"))
            return

        # Mock API: /logs (On-Demand Serial Logs)
        if path == "/logs":
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Access-Control-Allow-Origin", "*")
            self.end_headers()
            self.wfile.write(json.dumps(simulated_logs).encode("utf-8"))
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
            self.send_header("Access-Control-Allow-Origin", "*")
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
            self.send_header("Access-Control-Allow-Origin", "*")
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
                if k == "clearlog":
                    simulated_logs.clear()
                    action_desc = "Logs cleared"
                elif k == "record":
                    if val in ["0", "stop", "cancel"]:
                        cfg["busy"] = False
                        cfg["emotion"] = "idle"
                        cfg["state"] = "Ready"
                        action_desc = "Mic stopped/cancelled"
                    else:
                        cfg["busy"] = True
                        cfg["emotion"] = "listening"
                        cfg["state"] = "Listening..."
                        action_desc = "Mic listening started"
                    cfg_changed = True
                elif k == "pat":
                    cfg["emotion"] = "petted"
                    cfg["state"] = "Petted & Happy!"
                    cfg_changed = True
                    action_desc = "Kiwi head petted"
                elif k in ["vol", "volume"]:
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
                elif k == "snd_react":
                    cfg["soundReactiveEnabled"] = (val == "1" or val == "true" or val == "on"); cfg_changed = True
                elif k == "rgb_en":
                    cfg["rgbLedEnabled"] = (val == "1" or val == "true" or val == "on"); cfg_changed = True
                elif k == "rgb_bright":
                    try:
                        cfg["rgbLedBrightness"] = int(val)
                    except ValueError:
                        cfg["rgbLedBrightness"] = 63
                    cfg_changed = True
                elif k == "rgb_color":
                    clean_hex = str(val).replace("%23", "").replace("#", "").strip()
                    cfg["rgbLedColor"] = "#" + clean_hex
                    cfg["rgbLedOverride"] = True
                    cfg_changed = True
                    action_desc = f"RGB LED color override: #{clean_hex}"
                elif k == "rgb_clear":
                    cfg["rgbLedOverride"] = False
                    cfg_changed = True
                    action_desc = "RGB LED override cleared"
                elif k == "ww_en":
                    cfg["wakeWordEnabled"] = (val == "1" or val == "true" or val == "on"); cfg_changed = True
                elif k == "ww_model":
                    m_idx = 0 if str(val).strip().lower() in ("0", "alexa") else 1
                    cfg["wakeWordModel"] = m_idx
                    cfg["wakeWordCutoff"] = 0.90 if m_idx == 0 else 0.92
                    cfg_changed = True
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
            self.send_header("Access-Control-Allow-Origin", "*")
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
            self.send_header("Access-Control-Allow-Origin", "*")
            self.end_headers()
            self.wfile.write(json.dumps(anims).encode("utf-8"))
            return

        self.send_response(404)
        self.send_header("Access-Control-Allow-Origin", "*")
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
                self.send_header("Access-Control-Allow-Origin", "*")
                self.end_headers()
                self.wfile.write(json.dumps({"ok": True, "proxy_robot_ip": proxy_robot_ip}).encode("utf-8"))
            except Exception as e:
                self.send_response(400)
                self.send_header("Content-Type", "application/json")
                self.send_header("Access-Control-Allow-Origin", "*")
                self.end_headers()
                self.wfile.write(json.dumps({"ok": False, "error": str(e)}).encode("utf-8"))
            return

        # Dev sync to C++
        if path == "/api/dev/sync":
            try:
                import sync_react_to_cpp
                sync_react_to_cpp.sync_react_to_cpp()
                self.send_response(200)
                self.send_header("Content-Type", "application/json")
                self.send_header("Access-Control-Allow-Origin", "*")
                self.end_headers()
                self.wfile.write(json.dumps({"ok": True}).encode("utf-8"))
            except Exception as e:
                self.send_response(500)
                self.send_header("Content-Type", "application/json")
                self.send_header("Access-Control-Allow-Origin", "*")
                self.end_headers()
                self.wfile.write(json.dumps({"ok": False, "error": str(e)}).encode("utf-8"))
            return

        # Forward POST to robot if in proxy mode
        if proxy_robot_ip and path in ["/save", "/update", "/ota/install", "/do"]:
            res = self.proxy_to_robot(self.path, method="POST", body=body, headers=dict(self.headers))
            if res:
                status, ctype, data = res
                self.send_response(status)
                self.send_header("Content-Type", ctype)
                self.send_header("Access-Control-Allow-Origin", "*")
                self.end_headers()
                self.wfile.write(data)
                return

        # Mock /do POST handler (delegates to mock GET)
        if path == "/do":
            return self.do_GET()

        # Mock /ota/install handler
        if path == "/ota/install":
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Access-Control-Allow-Origin", "*")
            self.end_headers()
            self.wfile.write(json.dumps({"ok": True, "message": "OTA update initiated (mock mode)"}).encode("utf-8"))
            return

        # Mock /save form/JSON handler
        if path == "/save":
            try:
                cfg = load_config()
                content_type = self.headers.get("Content-Type", "")
                if "application/json" in content_type:
                    params = json.loads(body.decode("utf-8"))
                    for k, v in params.items():
                        cfg[k] = v
                else:
                    form_params = urllib.parse.parse_qs(body.decode("utf-8"))
                    for k, v in form_params.items():
                        val = v[0]
                        if k == "vol": cfg["speakerVolume"] = int(val)
                        elif k == "bright": cfg["displayBrightness"] = int(val)
                        elif k == "night_bright": cfg["nightBrightness"] = int(val)
                        elif k == "night_mode": cfg["nightMode"] = int(val)
                        elif k == "prompt":
                            cfg["systemPrompt"] = val
                            cfg["prompt"] = val
                        elif k in ["owner", "mem_name"]:
                            cfg["ownerName"] = val
                        elif k == "ssid": cfg["wifiSSID"] = val
                        elif k == "pass": cfg["wifiPassword"] = val
                        elif k in ["stt_prov", "sttprovider"]: cfg["sttProvider"] = val
                        elif k in ["stt_lang", "sttlang"]: cfg["sttLang"] = val
                        elif k in ["llm_prov", "llmprovider"]: cfg["llmProvider"] = val
                        elif k in ["tts_prov", "ttsprovider"]: cfg["ttsProvider"] = val
                        elif k == "llmkey": cfg["llmKey"] = val
                        elif k == "groqkey": cfg["groqKey"] = val; cfg["groqSttKey"] = val
                        elif k == "ttskey": cfg["ttsKey"] = val; cfg["sttKey"] = val
                        elif k == "fishkey": cfg["fishKey"] = val
                        elif k == "snd_react": cfg["soundReactiveEnabled"] = (val in ["1", "true", "on"])
                        elif k == "greet_en": cfg["greetingsEnabled"] = (val in ["1", "true", "on"])
                        elif k == "circ_en": cfg["circadianSleepEnabled"] = (val in ["1", "true", "on"])
                save_config(cfg)
                simulated_logs.append("[CONFIG] Settings updated and saved to mock_config.json")
            except Exception as e:
                print(f"[SAVE] Error parsing config form: {e}")

            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Access-Control-Allow-Origin", "*")
            self.end_headers()
            self.wfile.write(json.dumps({"ok": True, "reboot": False, "message": "Settings saved and applied live (mock mode)!"}).encode("utf-8"))
            return

        self.send_response(404)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers()

def main():
    global proxy_robot_ip
    parser = argparse.ArgumentParser(description="Kiwi Robot UI Local Dev Server")
    parser.add_argument("--port", type=int, default=8080, help="Port to serve UI & Mock API (default: 8080)")
    parser.add_argument("--robot", type=str, default="", help="Robot IP to proxy API requests to live ESP32")
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
    print(f"   KIWI ROBOT REACT UI MOCK DEV SERVER RUNNING!")
    print(f"   URL: {url}")
    print(f"   - Mock API & Proxy:   http://localhost:{args.port}")
    print(f"   - React HMR Dev:      http://localhost:5173 (npm run dev)")
    print(f"   - React Singlefile:   react_ui/dist/index.html")
    print(f"   - Click '💾 Sync to C++' to export bundle to firmware")
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
