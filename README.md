# MyRobot (Kiwi) — ESP32-S3 Modular AI Desk Companion

**Kiwi** is an expressive, autonomous AI desk companion robot built on the **ESP32-S3** dual-core processor. It features a 128x64 OLED vector face, an I2S digital microphone, an I2S power amplifier and speaker, an onboard WS2812 RGB LED, and three capacitive touch pads.

Kiwi is powered by local offline neural wake word detection, dual-core multithreaded rendering, an interactive Tamagotchi-style pet hunger system, cloud speech-to-text, multiple LLM reasoning engines (Groq, Google Gemini), and expressive streaming text-to-speech. Its eyes, sounds, RGB LED, and behaviors dynamically react to physical touch, vocal emotions, time of day, room lighting (HyperHDR / WLED), and LLM-driven actions.

- **Firmware Version:** `v1.2` (`FIRMWARE_VERSION` in [AppConfig.h](file:///d:/PROJECTS/MyRobot_Modular/src/config/AppConfig.h#L8))
- **Hardware Platform:** ESP32-S3 (Dual-Core Xtensa LX7 @ 240 MHz, 4 MB Flash)
- **Web UI Platform:** Modern Single-Page App (React 19 + Vite + TailwindCSS + Framer Motion) embedded in PROGMEM Gzip
- **Build System:** PlatformIO (`default_envs = esp32s3`)

---

## Status and Known Limitations

Please review these items before configuring or flashing:

1. **Microcontroller Platform:** The active firmware is built for **ESP32-S3** (`esp32-s3-devkitc-1` / ESP32-S3 Super Mini), **not** the older ESP32-C3. Pin assignments, I2S clock routing, and dual-core tasks reflect the ESP32-S3 pinout.
2. **Flash Chip Size:** 4 MB Flash partition scheme with dual 1.93 MB OTA app partitions (`partitions.csv`).
3. **Battery Sensing:** Battery monitoring is a placeholder stub (`batteryPercent()` in [Battery.cpp](file:///d:/PROJECTS/MyRobot_Modular/src/hardware/Battery.cpp#L4) returns `-1`). `showBatteryIcon` defaults to `false`.
4. **Wiring Blueprint Route:** The `/wiring` endpoint currently redirects (HTTP 302) to the root dashboard `/`.
5. **Build Path:** PlatformIO is required. The Arduino IDE is **not supported** because the codebase is structured into modular subfolders under `src/`.
6. **Wi-Fi Modem Sleep:** Wi-Fi modem sleep is explicitly deactivated while connected (`WiFi.setSleep(false)`) to maintain instantaneous Web Portal responsiveness and low network latency. Power saving is managed via display dimming, CPU frequency scaling (down to 80 MHz), Light Sleep, and Deep Sleep.

---

## Hardware Pinout & Wiring (ESP32-S3)

Configured in [AppConfig.h](file:///d:/PROJECTS/MyRobot_Modular/src/config/AppConfig.h#L27-L39):

| Peripheral | Signal | ESP32-S3 Pin | Notes |
| :--- | :--- | :--- | :--- |
| **OLED Display** (SSD1306, 0x3C) | `SDA` | `GPIO2` | 800 kHz I2C clock during writes |
| **OLED Display** (SSD1306, 0x3C) | `SCL` | `GPIO3` | 800 kHz I2C clock during writes |
| **Touch Sensor (Left Cheek)** | `SIG` | `GPIO4` | Digital capacitive touch input |
| **Touch Sensor (Right Cheek)** | `SIG` | `GPIO5` | Digital capacitive touch input |
| **Touch Sensor (Top Head)** | `SIG` | `GPIO12` | RTC GPIO (wakes ESP32-S3 from deep sleep) |
| **INMP441 Microphone** | `SD` | `GPIO13` | I2S Serial Data input |
| **MAX98357A Amplifier** | `DIN` | `GPIO1` | I2S Serial Data output |
| **I2S Shared Bus** | `BCLK` | `GPIO43` | I2S Bit Clock (shared between mic & amp) |
| **I2S Shared Bus** | `WS` | `GPIO44` | I2S Word Select / LRCLK (shared) |
| **Onboard RGB LED** | `DATA` | `GPIO48` | Built-in WS2812 addressable NeoPixel |

### Audio Specifications
- **Sample Rate:** `16,000 Hz` (16 kHz mono WAV / 32-bit Philips I2S stereo frames) — matches MicroWakeWord model training.
- **Recording Buffer:** 6-second maximum capture window (`RECORD_SECONDS 6`).
- **Microphone Digital Gain:** `4.5x` (`MIC_GAIN 4.5f`) with auto left/right channel detection and NVS flash caching.
- **Speaker Max Digital Gain:** `2.6x` (+8.3 dB, `SPEAKER_MAX_GAIN 2.6f`) with hyperbolic soft-clipping protection.
- **Default Volume:** 70% (0–100% scale).

---

## Key Features

### 1. Dual-Core FreeRTOS Multithreading
- **Core 0 — Display & Lighting Engine:** A dedicated FreeRTOS task ([FaceEngine.cpp](file:///d:/PROJECTS/MyRobot_Modular/src/face/FaceEngine.cpp#L150-L162)) runs at a locked 40 FPS, driving display I2C rendering and WS2812 RGB LED color blending. It offloads I2C bus traffic completely from Core 1.
- **Core 0 — Wake Word Background Worker:** Runs TFLite Micro audio feature extraction and inference in parallel without stalling audio or web requests.
- **Core 1 — Robot Logic & Networking:** Handles main loop execution, Wi-Fi maintenance, captive SoftAP, REST APIs, TLS client communication, cloud AI pipelines, and touch gesture state machines.

### 2. Local Neural Wake Word Engine (MicroWakeWord)
- **Offline Hotword Detection:** Runs on-device via TensorFlow Lite Micro and ESPMicroSpeechFeatures frontend.
- **Optimized Single Active Model:** Dedicated **`Alexa`** model (probability cutoff: `0.90`, sliding window: 5).
- **RAM Optimization:** TensorFlow arena size reduced from 36 KB to **20 KB** (`TENSOR_ARENA_SIZE = 20000`), freeing ~16 KB of static internal SRAM for TLS networking and streaming audio buffers.
- **Barge-In Interruption:** Say the wake word while Kiwi is speaking to immediately cut off speech playback and begin listening for your new command (with configurable barge-in sensitivity bump `0.08`).
- **Serial Console CLI:** Tune sensitivity, gains, and parameters on the fly via the USB serial monitor without reflashing (e.g. `ww cutoff 0.90`, `ww gain 8.0`, `ww spike 0`).

### 3. Onboard WS2812 RGB LED (GPIO 48)
- **Automatic Mood & State Syncing:**
  - *Listening Mode:* Glowing Cyan pulse
  - *Thinking Mode:* Mystical Purple breathing
  - *Speaking / Answering:* Vibrant Emerald Green pulse
  - *Happy / Smiles:* Warm Amber / Gold
  - *Sad / Crying:* Deep Blue
  - *Angry:* Fiery Crimson Red
  - *Sleeping:* Ultra-dim Indigo breathing
- **Live Web Color Override:** Choose any custom static color via the Web Console color picker (`/do?rgb_color=#RRGGBB`) or clear the override to resume autonomous emotion syncing (`/do?rgb_clear=1`).
- Configurable brightness (0–100%, default 63% / 160) and enable toggle in the web dashboard.

### 4. Interactive Tamagotchi Pet Hunger System
- **Meal Schedule:**
  - **Breakfast:** 8:30 AM – 9:30 AM
  - **Lunch:** 1:00 PM – 2:00 PM (13:00 – 14:00)
  - **Dinner:** 8:00 PM – 9:00 PM (20:00 – 21:00)
- **Night Quiet Hours:** 10:00 PM – 7:00 AM (Kiwi never demands food or makes noise at night).
- **Hunger Countdown:** When hungry, Kiwi adopts a pleading cute face, plays a sad violin chime, and starts a 2-minute deadline countdown (`HUNGER_DEADLINE_MS = 120s`) with 45-second reminder whimpers.
- **How to Feed Kiwi:**
  - Dual-cheek hold: Press and hold both Left & Right cheeks simultaneously for ≥ 300 ms.
  - Web Portal: Click the **Feed Snack** button on the dashboard.
  - Voice Command: Say *"feed kiwi"*, *"khana khao"*, or *"kuch khao"*.
- **The "Angry" Neglect State:** If not fed within 2 minutes, Kiwi enters `HUNGER_ANGRY`. In this state, Kiwi pouts, refuses to chat, and rejects top head taps (*"Nahi! Main tumse baat nahi karunga! Bohot gussa hoon!"*).
- **Reconciliation (Manaao):**
  - Alternating cheek tickle combo: Tap **Left → Right → Left → Right** (or R-L-R-L) within 4.5 seconds to tickle Kiwi and restore its happiness.
  - Web Portal: Click the **Apologize** button.
  - Voice Command: Say *"sorry kiwi"*, *"maaf kardo"*, or *"maan jao"*.
- **Persistence:** Fed meal status is recorded in NVS flash (`kiwi_hunger`) so reboots or firmware uploads don't re-trigger past meals.

### 5. OLED Vector Face & Eye Skins
- **34 Emotion States:** Idle, Listening, Thinking, Speaking, Happy, Excited, Sad, Confused, Angry, Sleeping, Error, Petted, Surprised, Wink, Smug, Curious, Bored, Laughing, Scared, Suspicious, Relieved, Proud, Dizzy, Lovestruck, Evil, Crying, Hearts, Dead, Cool, Shocked, Singing, Party, Nervous, Cute.
- **10 Pet Touch Reactions:** Love, Giggle, Shy, Sparkle, Kiss, Cuddle, Playful, Sleepy, Proud, Surprised.
- **6 Curated Procedural Eye Skins:**
  1. `Kiwi` (*Default Flagship*): Flat top, rounded lower base, U-shaped pupil with 3D parallax shift, and upper specular glint.
  2. `Vector Classic`: Clean round vector eyes with pupil catchlight.
  3. `Pixel 8-Bit`: Retro stepped arcade pixels.
  4. `Squircle Bold`: Mathematical superellipse ($n=4$) solid fill with square pupil.
  5. `Lumina Spark`: Solid squircle with dual sparkling glints.
  6. `Mecha`: Futuristic robotic circtangle eyes with technical visor aperture.
- **Physics Simulation:** Damped harmonic spring dynamics ($f_0 = 24\text{ rad/s}, \zeta = 0.62$) with squash-and-stretch settling, procedural eye blinking, and directional gaze tracking.
- **Auxiliary Screens:** Live Clock screen, Open-Meteo Weather screen, and Marquee text banner.

### 6. Cloud AI & Voice Interaction Pipeline
- **Speech-to-Text (STT):**
  - Sarvam AI `saaras:v4` (default, language `hi`, `en`, or `auto`)
  - Groq Whisper `whisper-large-v3`
- **Large Language Model (LLM):**
  - Groq Cloud `openai/gpt-oss-120b` (default)
  - Google Gemini (`gemini-2.5-flash-lite`, `gemini-2.5-flash`, etc.)
  - Max token limit: 800 tokens (`LLM_MAX_TOKENS`).
- **Text-to-Speech (TTS):**
  - Sarvam AI `bulbul:v1-flash` (default)
  - Fish Audio `s2.1-pro-free` (detects vocal tags like `[laugh]`, `[whisper]`, `[crying]`, `[angry]` and syncs facial expressions in real time).
- **Streaming Audio (`LLM_STREAM_TTS 1`):** Kiwi streams speech as sentence chunks arrive from the LLM, achieving low time-to-first-audio latency.
- **Persistent Memory:** Stores up to **16 persistent facts** in flash (owner name, family details, user goals, and proactive follow-ups).
- **Chat History Buffer:** Maintains the last 1–8 conversation turns (default 4). Reset anytime by saying *"new topic"*, *"reset chat"*, or *"bhool jao"*.

### 7. LLM Tool Actions (`[ACTION: ...]`)
When the LLM outputs command tags, Kiwi executes the command locally and strips the tag from speech output:

| Action Command | Example Tag | Description |
| :--- | :--- | :--- |
| `volume` | `[ACTION: volume 80]` | Adjusts speaker volume (0–100%) |
| `skin` | `[ACTION: skin mecha]` | Switches eye skin (`kiwi`, `vector`, `pixel`, `squircle_solid`, `squircle_glint`, `mecha`) |
| `emotion` | `[ACTION: emotion evil]` | Displays any expressive emotion face |
| `timer` | `[ACTION: timer 300 Tea]` | Sets countdown timer in seconds with optional label |
| `marquee` | `[ACTION: marquee Hello!]` | Scrolls custom text banner across OLED screen |
| `sleep` / `wake` | `[ACTION: sleep]` | Triggers smooth fall-asleep or wake-up animation |
| `weather` | `[ACTION: weather]` | Displays live Open-Meteo weather screen |
| `time` | `[ACTION: time]` | Displays live NTP clock screen |
| `light` | `[ACTION: light red]` | Controls HyperHDR / WLED smart lighting |
| `sound` | `[ACTION: sound fanfare]` | Plays named sound effect or soundboard chime |

### 8. Smart Lighting Integration (HyperHDR & WLED)
- **Coordinated Light Control:** Unified executor (`executeUnifiedLightAction`) coordinates both HyperHDR and WLED devices.
- **WLED Integration:** Auto-discovered via mDNS (`_wled._tcp`) or manual IP. Controls power, brightness, colors (Hindi and English color names), and 160+ effects.
- **HyperHDR Ambilight:** Controls HyperHDR JSON-RPC on port 8090. Supports screen ambilight synchronization (`screensync`, `sync`, `ambilight`, `screen`), HDR toggle, priority channel overrides, and brightness.

### 9. Touch Gestures & Default Actions

| Sensor | Gesture | Default Action | Behavior |
| :--- | :--- | :--- | :--- |
| **Top Head (GPIO12)** | Single Tap | `TOUCH_ACT_LISTEN` | Starts voice recording; cancels active speech or interrupts via barge-in |
| **Top Head** | Double Tap | `TOUCH_ACT_NONE` | Unassigned (customizable) |
| **Top Head** | Hold 3 s | `TOUCH_ACT_CYCLE_SKIN` | Cycles to next eye skin theme |
| **Top Head** | Hold 7 s (2 s if asleep) | Sleep Toggle | Toggles manual sleep mode |
| **Left Cheek (GPIO4)** | Single Tap | `TOUCH_ACT_DISMISS` | Dismisses clock/weather/banner; winks left eye & blips sound |
| **Left Cheek** | Double Tap | `TOUCH_ACT_CLOCK` | Displays live digital clock screen |
| **Left Cheek** | Hold 1.5 s | `TOUCH_ACT_PET` | Triggers random pet reaction |
| **Right Cheek (GPIO5)**| Single Tap | `TOUCH_ACT_DISMISS` | Dismisses clock/weather/banner; winks right eye & blips sound |
| **Right Cheek** | Double Tap | `TOUCH_ACT_WEATHER` | Displays live weather forecast screen |
| **Right Cheek** | Hold 1.5 s | `TOUCH_ACT_PET` | Triggers random pet reaction |
| **Both Cheeks** | Simultaneous Hold (300 ms) | Pet Feed / Tickle | Feeds Kiwi if hungry; triggers tickle giggle mode if satisfied |
| **Both Cheeks** | Alternating Combo (4.5 s) | Apology / Reconcile | Tap L-R-L-R to reconcile Kiwi when angry |
| **Any Sensor** | Tap during alarm | Dismiss Alarm | Silences and dismisses active timer alarm immediately |

*All single, double, and hold gestures can be remapped in the Web Portal.*

### 10. Sleep & Power Management
- **Circadian Sleep (23:00 to 07:00 default):** Deep sleep with timer wakeup until the morning schedule ends. Wall-clock time is restored from the RTC without requiring immediate NTP re-sync (`POWER_RTC_MAGIC`).
- **Manual Sleep:** Enters 15 minutes of Light Sleep (`MANUAL_LIGHT_SLEEP_MAX_MIN = 15`) with 60-second periodic timer wakeups, then switches to Deep Sleep (touch wake only).
- **Idle Display Dimming:** OLED dims after 60 seconds of inactivity.
- **CPU Frequency Scaling:** Drops to 80 MHz during idle periods when display is off/dimmed and wake word is inactive; returns to 160/240 MHz on activity.

### 11. Offline Fallback Brain
When Wi-Fi is unavailable or drops:
- Activates **Desk Buddy Offline Mode**.
- Tapping the head triggers 6 responsive offline reactions (winks, pet kisses, puppy eyes, giggles, hearts, and party chimes) instead of throwing network error prompts.

---

## Modern Web Portal (React 19 SPA)

Kiwi serves an ultra-modern, zero-latency Web Portal built with **React 19, Vite, TailwindCSS, Lucide Icons, and Motion**. The entire frontend is pre-built, inlined, and compressed into a single Gzip byte array in ESP32 flash memory (`dashboard_html_gz.h`, ~163 KB PROGMEM).

Access Kiwi via:
- `http://kiwi.local` (Local network mDNS)
- `http://192.168.4.1` (SoftAP setup mode)

### Portal Architecture & Features
- **Live Console View:**
  - **Live Hardware Vitals:** Real-time on-chip CPU Temperature, SRAM Heap Usage with Free KB & Total KB, and Flash Storage Telemetry (`getSketchSize` vs `getFreeSketchSpace`, app partition vs 4 MB physical chip).
  - **Link Status Pill:** Dynamic real-time connection badge (`ONLINE` in Emerald Green vs `OFFLINE` in Crimson Red) with tap-to-refresh.
  - **Tactile Segmented Sliders:** Hardware-matched VU-meter sliders for Speaker Volume (Acid Lime), OLED Brightness (Electric Blue matching blue OLED hardware), and NeoPixel LED (Emerald Green).
  - **Instant Mic Trigger & Chat:** Quick text prompt input and one-click voice recording trigger.
  - **Pet Mood & Hunger Deck:** Direct Feed Snack, Make Hungry, Apologize, and Head Pet controls.
  - **Audio Soundboard:** 1-click sound effects (Wake Chime, Pet Giggle, Sparkle, Joy Laugh, Fanfare, etc.).
- **Faces & Studio View:**
  - **Interactive Skin Matrix:** Live SVG silhouettes for all 6 procedural eye skins.
  - **42 Expression Triggers:** Visual gallery of all facial emotion states.
  - **Screen Modes:** Interactive 1-click toggles for Clock, Weather, and OLED Marquee text banner.
- **Settings & AI Configuration:**
  - **AI Engines:** Select providers and models for STT (Sarvam, Groq Whisper), LLM (Groq, Gemini), and TTS (Sarvam, Fish Audio).
  - **Hardware & Sensing:** Wake word sensitivity, barge-in bump, and 9-gesture capacitive touch remapping.
  - **Personal Life Diary:** Edit up to 16 persistent memory facts.
- **Wireless OTA Update View:**
  - **Local Binary Flasher:** Drag and drop `.bin` files with live progress bar and auto-restart.
  - **Cloud GitHub OTA:** 1-click install from the latest releases on GitHub.

### UI Design System
- **Desktop Layout:** Full-page canvas with precise `0.5cm` border padding and adaptive 3-column grid.
- **Mobile Experience:** 100% responsive Bento mobile layout with bottom navigation dock and touch gestures.
- **Toast Notifications:** Apple Dynamic Island / Sonner style frosted glass floating HUD with spring physics.
- **Zero-Heap Log Footprint:** Terminal string buffer removed from firmware heap, eliminating RAM fragmentation and maximizing ESP32 stability.

### Local Frontend Development & Hot Reload

Develop and style the React UI directly on your computer with instantaneous Hot Module Replacement (HMR):

```bash
# 1. Start the React development server:
start_react_ui.bat
# (Or: cd react_ui && npm run dev)

# 2. Compile and sync React build to ESP32 firmware headers:
sync_react_to_cpp.bat
# (Or: python tools/web_dev/sync_react_to_cpp.py)
```

- **Mock Python Backend:** Run `python tools/web_dev/server.py` to simulate all REST endpoints, status responses, and hardware telemetry on `http://localhost:8080`.

---

## Build and Flash Guide

The project is configured for **PlatformIO**.

### Requirements
- VS Code with the **PlatformIO IDE** extension, or **PlatformIO Core (CLI)**.
- USB-C cable connected to the ESP32-S3 USB port.
- On Windows, verify long paths are enabled (`LongPathsEnabled = 1`).

### PlatformIO Configuration (`platformio.ini`)
```ini
[platformio]
default_envs = esp32s3
src_dir = .
include_dir = src

[env:esp32s3]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
board_build.partitions = partitions.csv
board_upload.flash_size = 4MB
board_build.f_cpu = 240000000L
board_build.f_flash = 80000000L
board_build.flash_mode = qio
build_flags = 
    -Isrc
    -DARDUINO_USB_MODE=1
    -DARDUINO_USB_CDC_ON_BOOT=1
    -O2
lib_deps = 
    adafruit/Adafruit SSD1306@^2.5.9
    adafruit/Adafruit GFX Library@^1.11.9
    adafruit/Adafruit BusIO@^1.15.0
    nickjgniklu/ESP_TF@^2.1.2
    https://github.com/kahrendt/ESPMicroSpeechFeatures.git
```

### Partition Scheme (`partitions.csv` - 4 MB Flash)
```csv
# Name,   Type, SubType, Offset,   Size,     Flags
nvs,      data, nvs,     0x9000,   0x5000,
otadata,  data, ota,     0xe000,   0x2000,
app0,     app,  ota_0,   0x10000,  0x1F0000,
app1,     app,  ota_1,   0x200000, 0x1F0000,
coredump, data, coredump,0x3F0000, 0x10000,
```

### Build Commands

```bash
# Build firmware binary
pio run -e esp32s3

# Upload firmware to ESP32-S3 via USB
pio run -e esp32s3 -t upload

# Open Serial Monitor (115200 baud)
pio device monitor -b 115200
```

---

## First-Time Setup & Provisioning

1. Flash the firmware and power Kiwi via USB-C (5V).
2. If Wi-Fi is not yet configured, Kiwi broadcasts an open access point:
   - **SSID:** `MyRobot-XXXX` (last 4 hex digits of MAC address)
   - **Default AP Password:** `25691128` (configurable)
   - **IP:** `192.168.4.1`
3. Connect your phone or laptop to `MyRobot-XXXX` and navigate to `http://192.168.4.1`.
4. Enter your home Wi-Fi SSID & password, along with your STT, LLM, and TTS API keys.
5. Save settings. Kiwi connects to your home network and registers on mDNS as:
   ```
   http://kiwi.local
   ```

---

## Configuration Reference (Defaults in `AppConfig.cpp`)

Saved NVS flash settings take precedence over code defaults.

| Setting | NVS Key | Default Value | Range / Options |
| :--- | :--- | :--- | :--- |
| Speaker Volume | `volume` | `70` | 0 – 100% |
| Daytime Brightness | `disp_bright` | `40` | 0 – 100% |
| Night Brightness | `night_bright` | `10` | 0 – 100% |
| Night Mode Style | `night_mode` | `1` (Dim) | `0` = Same as day, `1` = Dim, `2` = Screen off |
| Circadian Sleep | `circ_en` | `true` | `true` / `false` |
| Circadian Schedule | `circ_start`, `circ_end`| `23`, `7` | 23:00 (11 PM) to 07:00 (7 AM) |
| Tamagotchi Hunger | `hunger_en` | `true` | `true` / `false` |
| Wake Word Enabled | `ww_en` | `true` | `true` / `false` |
| Wake Word Model | `ww_model` | `0` (Alexa) | `0` = Alexa |
| Wake Word Cutoff | `ww_cutoff` | `0.90` | 0.10 to 0.99 |
| Wake Word Gain | `ww_gain` | `8.0f` | 0.5 to 20.0 |
| WS2812 RGB LED | `rgb_en` | `true` | `true` / `false` |
| RGB LED Brightness | `rgb_bright` | `160` (~63%) | 0 – 255 |
| Sound-Reactive Eyes | `snd_react` | `false` | `true` / `false` |
| Idle Banter (Boredom)| `boredom`, `boredom_mins`| `false`, `2` | 1 – 60 minutes |
| Default Eye Skin | `eyeskin` | `0` (`SKIN_KIWI`) | 0 – 5 (6 curated skins) |
| Chat History Turns | `chat_hist` | `4` | 1 – 8 turns |
| STT Provider | `sttprov` | `sarvam` | `sarvam`, `groq` |
| STT Language | `sttlang` | `hi` | `hi`, `en`, `auto` |
| LLM Provider | `llmprov` | `groq` | `groq`, `gemini` |
| TTS Provider | `ttsprov` | `sarvam` | `sarvam`, `fish` |
| Weather City | `wthr_city` | `New Delhi` | City name |
| Weather Coordinates | `wthr_lat`, `wthr_lon` | `28.6139`, `77.2090` | Lat / Lon decimals |
| mDNS Hostname | `hostname` | `kiwi` | Resolves to `kiwi.local` |

---

## Serial Console Commands (Wake Word CLI)

Type these commands directly into the serial monitor (`115200` baud) for live tuning:

```text
ww status          # Print current model, cutoff sensitivity, gain, and detection state
ww gain 8.0        # Set pre-amplifier software gain (default: 8.0)
ww cutoff 0.90     # Set probability trigger cutoff (0.10 to 0.99)
ww model 0         # Active model: 0 = Alexa
ww spike 0         # Toggle secondary voice spike trigger (0 = off, 1 = on)
ww bump 0.08       # Set barge-in sensitivity bump applied during speech playback
ww mic 0           # Set I2S microphone slot (0 = Left, 1 = Right)
ww lock 1          # Lock microphone slot to prevent automatic channel scan on boot
```

---

## Architecture & Codebase Map

```
MyRobot_Modular.ino          Main sketch: setup(), loop(), boot watchdog, boot greeting
platformio.ini               PlatformIO configuration (ESP32-S3, 240 MHz, partitions, libs)
partitions.csv               4MB Dual OTA layout (app0 1.9MB, app1 1.9MB, NVS, coredump)
react_ui/                    Modern React 19 + Vite + Tailwind frontend single-page application
  src/
    components/              LiveConsoleView, FacesStudioView, SettingsView, OtaUpdateView, etc.
    data/                    faceData.js (SVG definitions, skins, emotion matrices)
start_react_ui.bat           1-click React UI development server launcher (Hot Reload)
sync_react_to_cpp.bat        1-click React bundle builder & C++ Gzip PROGMEM sync script
src/
  core/
    CoreTypes.h              Emotion, EyeSkin (6 skins), PetMood, TouchAction, Tone enums
    Timing.h                 serviceUi() and waitMs() non-blocking pump helpers
  config/
    AppConfig.h / .cpp       Hardware pinout, constants, defaults, and runtime globals
    ConfigManager.h / .cpp   NVS flash storage manager (Preferences) with debounced commit
  hardware/
    AudioEngine.h / .cpp     I2S standard driver, procedural tone synthesizer, soft-clipper
    Battery.h / .cpp         Battery sensing stub (returns -1)
    RgbLed.h / .cpp          WS2812 NeoPixel (GPIO 48) state, emotion & custom color sync
    SoundLevel.h / .cpp      Microphone amplitude detector for sound-reactive eyes
    TouchController.h / .cpp 3-pad touch debounce, gesture recognition, tickle & apology combo
    WakeWordEngine.h / .cpp  MicroWakeWord (20 KB arena) Alexa task on Core 0 with barge-in
    alexa_model.h            Official MicroWakeWord Alexa neural network model weights
  face/
    FaceEngine.h / .cpp      OLED refresh loop & Core 0 dedicated 40 FPS task (uiDisplayTask)
    FaceRenderer.h / .cpp    Eye, eyelid, mouth, blush, and flourish rendering with spring physics
    FaceExpressions.h / .cpp 34 facial emotion state definitions and tween easing
    FaceIdle.h / .cpp        Procedural blinking, gaze drift, and idle fidgets
    FaceSkins.h / .cpp       6 procedural eye skins (Kiwi, Vector, Pixel, Squircle, Lumina, Mecha)
    FaceScreens.h / .cpp     Clock, weather forecast, status banner, and toast message renderers
    FaceDisplay.h / .cpp     SSD1306 display hardware driver, contrast, and power control
    BootWakeAnim.h / .cpp    Power-on wake animation routine
    SleepyWakeAnim.h / .cpp  Bedtime fall-asleep and morning wake-up animations
  animation/
    AnimationEngine.h / .cpp Custom 128x64 OLED frame animation player
  behavior/
    InteractionManager.h/.cpp Voice interaction pipeline, streaming TTS pump, vocal emotion tags
    ActionExecutor.h / .cpp  Execution of [ACTION: ...] tags & countdown timer alert loop
    CommandHandler.h / .cpp  Fast-path local voice commands (time, weather, skins, lights, volume)
    HungerEngine.h / .cpp    Tamagotchi hunger scheduling, feeding, anger, and apology combo
    CircadianEngine.h / .cpp Circadian night sleep schedule and manual sleep logic
    GreetingEngine.h / .cpp  Time-of-day greetings
    BoredomBehavior.h / .cpp Autonomous idle remarks and chatter
    OfflineBrain.h / .cpp    Desk Buddy offline fallback behavior when Wi-Fi is disconnected
  power/
    PowerManager.h / .cpp    Light & Deep sleep, touch wake, RTC epoch sync, 80/160/240 MHz scaling
  services/
    AiService.h / .cpp       Facade header uniting AI modules
    AiTransport.h / .cpp     HTTP / TLS stream transport
    AiTextTools.h / .cpp     JSON parsers, string sanitizers, and Devanagari helpers
    AiChatHistory.h / .cpp   Multi-turn conversation history buffer
    AiLlmService.h / .cpp    Groq and Google Gemini client with SSE streaming
    AiSttService.h / .cpp    I2S audio recorder & multipart STT client (Sarvam, Whisper)
    AiTtsService.h / .cpp    Sarvam and Fish Audio PCM stream decoder
    MemoryService.h / .cpp   16-fact persistent memory storage, owner name, and follow-ups
    HyperHdrService.h / .cpp HyperHDR JSON-RPC client (screen sync, ambilight, HDR, colors)
    WledService.h / .cpp     WLED mDNS discovery and HTTP control
    WeatherService.h / .cpp  Open-Meteo REST API live weather client
  web/
    WebPortal.h / .cpp       SoftAP captive portal, mDNS, HTTP routing
    WebPortalApi.cpp         REST API handlers (/status, /config, /do, /save, /scan, telemetry)
    WebPortalDashboard.cpp   Single-page React app delivery handler from PROGMEM Gzip
    WebPortalOtaEngine.cpp   FreeRTOS background OTA downloader from GitHub or custom URL
    WebPortalWiring.cpp      Redirect handler for /wiring
    dashboard_html_gz.h      Compiled Gzip PROGMEM byte array of the unified React SPA
    icon.h                   Embedded Web Portal apple-touch icon
tools/web_dev/               Local Python web dev server, mock simulator, and sync scripts
```

---

## Troubleshooting

- **Compilation error on Windows ("file name too long"):** Enable long paths in Windows (`New-ItemProperty -Path "HKLM:\SYSTEM\CurrentControlSet\Control\FileSystem" -Name "LongPathsEnabled" -Value 1 -PropertyType DWORD -Force`).
- **Wake word not triggering:** Run `ww status` in the serial monitor to verify the engine is listening. Ensure mic slot is detected correctly (`ww mic 0` vs `ww mic 1`). Default probability cutoff for Alexa is `0.90`.
- **Kiwi refuses to chat and displays an angry face:** Kiwi was not fed within 2 minutes of mealtime! Tap **Left → Right → Left → Right** on the cheeks to tickle and reconcile, click **Apologize** in the web portal, or say *"sorry kiwi"*.
- **No audio from speaker:** Check MAX98357A `DIN` (GPIO 1), `BCLK` (GPIO 43), `WS` (GPIO 44), 5V power, and GND. Test audio output by triggering a sound effect from the Web Portal.
- **Web Portal unreachable during sleep:** Normal behavior. Kiwi powers down the Wi-Fi radio during deep sleep. Tap the top head pad (GPIO 12) to wake Kiwi.
- **OTA update failure:** Ensure Kiwi has a stable Wi-Fi connection with internet access. If flashing from a local `.bin`, verify that the binary was built for `esp32s3` with `partitions.csv` (1.93 MB partition limit).

---

## License

All rights reserved by the repository owner. Contact the author before redistributing or reusing this code for commercial applications.
