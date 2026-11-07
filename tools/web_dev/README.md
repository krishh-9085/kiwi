# Kiwi Robot · Local UI Development Server

Develop, preview, and test the Kiwi Robot Web UI on your computer **without flashing the ESP32** on every design change.

---

## 🚀 Quick Start (1-Click)

### On Windows:
Double-click:
```
start_ui_dev.bat
```
This automatically starts the server and opens `http://localhost:8080` in your default browser.

### Via Command Line:
```bash
python tools/web_dev/server.py
```

---

## 🎨 How to Develop UI Without Flashing

1. Open the templates in `tools/web_dev/templates/`:
   - [dashboard.html](templates/dashboard.html) — Main dashboard, status pills, live console & configuration deck
   - [faces.html](templates/faces.html) — Face and eye emotion studio
   - [wiring.html](templates/wiring.html) — Hardware pinout blueprint
   - [update.html](templates/update.html) — Firmware OTA upload page

2. Edit any HTML, CSS, or JS in your code editor.
3. Refresh your browser at `http://localhost:8080/` — changes appear **instantly**!

---

## 🤖 2 Modes of Operation

### 1. Local Mock Mode (Default)
- No physical ESP32 required.
- Simulated real-time serial monitor logs, changing emotions, Wi-Fi networks scan, and mock test triggers.
- Great for designing while away from hardware.

### 2. Live ESP32 Proxy Mode
- Want your local UI changes to control your actual physical robot?
- Enter your Robot's IP address (e.g. `192.168.1.50`) in the floating toolbar at the bottom right and click **"Proxy"**.
- Now all buttons (Mic Record, AI Prompt, Volume, Eye Skin, Sound Test) interact with your real robot over your home Wi-Fi while serving the UI straight from your PC disk!

---

## 💾 Exporting Changes Back to C++ Firmware

When you are happy with your new design and want to flash it to the ESP32:

1. Click the **"💾 Sync to C++"** button in the floating toolbar in your browser, **OR**
2. Run in terminal:
```bash
python tools/web_dev/sync_to_cpp.py
```

This will automatically format and update the C++ PROGMEM strings in:
- [WebPortalDashboard.cpp](../../src/web/WebPortalDashboard.cpp)
- [WebPortalFaces.cpp](../../src/web/WebPortalFaces.cpp)
- [WebPortalWiring.cpp](../../src/web/WebPortalWiring.cpp)
- [WebPortalOta.cpp](../../src/web/WebPortalOta.cpp)
