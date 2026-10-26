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

