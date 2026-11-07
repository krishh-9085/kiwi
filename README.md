# MyRobot (Kiwi) - ESP32-C3 AI Desk Companion

Kiwi is a desk companion robot built on an **ESP32-C3 Super Mini**. It has a 128x64 OLED face, an I2S microphone, an I2S amplifier and speaker, and three capacitive touch pads. It listens through a cloud speech-to-text service, thinks with a cloud LLM, and talks back with a cloud text-to-speech service. Its eyes, sounds and behaviour respond to touch, time of day, voice commands and LLM-generated actions.

Firmware version: **v1.2** (`FIRMWARE_VERSION` in `src/config/AppConfig.h`).

---

## Status and Known Limitations

Read this section first. These items are either not implemented yet or behave differently from what the code's names suggest.

- **Battery percentage is not implemented.** `batteryPercent()` in `src/hardware/Battery.cpp` always returns `-1`. The battery gauge and battery icon have no data source yet. `showBatteryIcon` defaults to `false`.
- **Wi-Fi modem sleep is not active.** `finalizeWifiConnected()` in `src/web/WebPortal.cpp` calls `WiFi.setSleep(false)`, so the radio stays at full power. Power saving currently comes from display dimming, the 80 MHz CPU state, and the sleep modes described below.
- **The `/wiring` page redirects to `/`.** The wiring blueprint is not currently served.
- **Arduino IDE is not a supported build path.** The firmware is split into `src/` subfolders, which the Arduino IDE does not load. Use PlatformIO (see Build below).
- **`build_opt.h` contains a machine-specific path** (`-ID:/PROJECTS/...`). `platformio.ini` does not reference it, so it can be deleted or ignored for PlatformIO builds.
- **Sleep and power features need hardware validation.** The light-sleep and deep-sleep paths, touch wake and timer wake have been written but should be tested on your board with a current meter before you rely on them.

---

## Features

### Face and Expressions
- **34 emotion states**, including Idle, Listening, Thinking, Speaking, Happy, Excited, Sad, Confused, Angry, Sleeping, Error, Wink, Smug, Curious, Bored, Laughing, Scared, Suspicious, Relieved, Proud, Dizzy, Lovestruck, Evil, Crying, Hearts, Dead, Cool, Shocked, Singing, Party, Nervous and Cute.
- **10 pet reactions** (Love, Giggle, Shy, Sparkle, Kiss, Cuddle, Playful, Sleepy, Proud, Surprised), each with matching sound.
- **21 eye skins**, switchable live from the web portal or by voice: Default (Kiwi), Cyberpunk, Cat, Retro 8-Bit, Anime, Hearts, Steampunk, Demon, Matrix, Cyborg, Mecha, Sunglasses, Puppy, Hypno, Alien, Ninja, Round, Desk Buddy Kiwi, Squircle Solid, Squircle Outline and Squircle Glint.
- **Idle behaviour:** natural blinking, gaze drift and flourishes (hearts, sparkles, stars).
- **Boot wake animation:** a one-time animation of about 3.6 seconds on power-on. It runs while Wi-Fi connects. A 5-second watchdog guarantees the normal face appears.
- **Sleepy fall-asleep and wake animations** for manual and circadian sleep.
- **OLED marquee/banner:** scrolling custom text, dismissed by any touch.
- **Clock and weather screens** on the OLED.
- **Sound-reactive eyes** (amplitude only, no speech recognition). Off by default. Pauses while Kiwi is speaking.

### Voice Interaction
- **Speech-to-text providers:** Sarvam `saaras:v3` (default, language `hi`), Groq Whisper (`whisper-large-v3`), and OpenAI Whisper (`whisper-1`).
- **LLM providers:** Groq `llama-3.3-70b-versatile` (default), Google Gemini (default model `gemini-2.5-flash-lite`; the dashboard also offers `gemini-2.5-flash`, `gemini-3.1-flash-lite`, `gemini-3.5-flash-lite` and `gemini-3.8-flash`), and OpenAI chat models.
- **Text-to-speech providers:** Sarvam `bulbul:v3` (default voice `priya`), OpenAI `gpt-4o-mini-tts`, ElevenLabs `eleven_multilingual_v2`, and Fish Audio.
- **Streaming speech (on by default, `LLM_STREAM_TTS 1`):** Kiwi starts speaking the first sentence while the LLM is still generating. Later sentences are batched (about 120 characters per TTS request). This requires enough free heap at the start of a reply (`STREAM_TTS_MIN_MAX_ALLOC`, default 60000 bytes). If heap is too low, the reply is spoken after generation completes.
- **Multi-turn chat:** the last few turns are kept in RAM (`chat_hist`, default 4, capacity 8). Say "new topic", "reset chat", "bhool jao" or "forget everything" to clear it.
- **Cancel:** tap the top pad during a reply to stop recording or speech.
- **Persistent memory:** up to 8 facts (for example your name, goals or upcoming events) are stored in flash and added to the LLM context. Kiwi can also ask about things you said earlier.
- **Offline fallback:** without Wi-Fi, Kiwi does not answer questions. It still reacts to touch, pets, tickles, idle chatter and the offline buddy reactions.

### LLM-Driven Actions
The LLM can emit tags such as `[ACTION: ...]`. Kiwi executes them and strips them from the spoken text. Supported actions:

| Action | Examples |
| :--- | :--- |
| `volume` | `[ACTION: volume 80]` |
| `skin` | `[ACTION: skin cat]`, `[ACTION: skin default]` |
| `emotion` | `[ACTION: emotion happy]` |
| `timer` | `[ACTION: timer 300 tea]` (seconds, then optional label), `[ACTION: timer cancel]` |
| `marquee` | `[ACTION: marquee <text>]` |
| `sleep` / `wake` | `[ACTION: sleep]`, `[ACTION: wake]` |
| `weather` | `[ACTION: weather]` |
| `time` | `[ACTION: time]` |
| `light` (WLED) | `[ACTION: light ...]` |
| `sound` | `[ACTION: sound chime]` |

Actions run after the full reply is generated, so an action can take effect slightly after the first sentence has started playing.

### Voice Commands (handled locally, no LLM call)
- **Timers and alarms:** "set a 5 minute timer", "timer lagao", "cancel timer".
- **Time:** "what time is it", "samay kya hai".
- **Weather:** "how's the weather", "mausam kaisa hai".
- **Skins:** "show cat eyes", "matrix skin", "default skin".
- **Emotions:** "be happy", "sad ho jao", "sleep", and many more keywords.
- **Volume:** "volume 60", "aawaz badhao".
- **WLED lights:** light control commands when a WLED device is configured.

### Touch Gestures (defaults, all remappable in the web portal)

| Pad | Gesture | Default action |
| :--- | :--- | :--- |
| Top (GPIO3) | Tap | Start voice recording (tap again to cancel) |
| Top | Double tap | None |
| Top | Hold 3 s | Cycle eye skin |
| Top | Hold 7 s (2 s while already asleep) | Toggle manual sleep |
| Left (GPIO10) | Tap | Dismiss banner or screen |
| Left | Double tap | Clock screen |
| Left | Hold 1.5 s | Pet reaction |
| Right (GPIO20) | Tap | Dismiss banner or screen |
| Right | Double tap | Weather screen |
| Right | Hold 1.5 s | Pet reaction |
| Both cheeks | Hold 0.6 s, or triple tap within 1.8 s | Tickle mode (gudgudi) |

Available actions for remapping: listen, clock, weather, skin, pet, tickle, vol_up, vol_dn, dismiss and none.

### Sleep and Power
- **Manual sleep** (top hold 7 s): light sleep with 60-second timer wake-ups. Touch wakes Kiwi. After 15 minutes (`MANUAL_LIGHT_SLEEP_MAX_MIN`), Kiwi switches to deep sleep, which wakes only on touch.
- **Circadian sleep** (enabled by default, 23:00 to 07:00, needs NTP time sync): deep sleep until the end time, with a timer wake-up. Without time sync, Kiwi uses light sleep with 60-second checks.
- **Night display mode:** `nightMode` can be Same as day, Dim (default) or Screen off, with `nightBrightness` default 10%.
- **Temporary wake:** a touch during night sleep wakes the display for about 30 seconds (the touch handler sets `circadianTempWakeUntil`; a wake from deep sleep uses 10 seconds).
- **Idle dimming:** the OLED dims after 60 seconds without activity.
- **CPU scaling:** drops to 80 MHz after about 10 seconds of idle with the display dimmed or off. Returns to 160 MHz on any activity.
- **Frame rate:** face updates run at 33 ms (about 30 fps) when active and 100 ms (about 10 fps) when idle.
- **Brightness:** daytime default 40%.
- **Web portal during deep sleep:** unreachable until the device wakes.

### Autonomous Behaviour
- **Idle chatter (boredom):** when enabled (off by default), Kiwi makes spontaneous remarks after `boredomMinutes` (default 2) of inactivity. Phrases are picked without immediate repeats.
- **Tickle mode:** repeated cheek taps or a two-cheek hold makes Kiwi giggle and complain.
- **Greetings (off by default, `greet_en`):** the boot greeting picks a random line for the time of day (morning, afternoon, evening, night) and uses your stored name if known. Periodic greetings use the same toggle. Greetings are only spoken once Wi-Fi and the speech key are configured.
- **Live weather:** Open-Meteo (no API key). Default location is New Delhi (28.6139, 77.2090). Change it in the web portal. Weather refreshes when older than 10 minutes.

### Audio
- **Procedural tone synthesis:** sine, warm vocal, purr, chirp and warble voices, used for sound effects and boot chime.
- **Speech output:** PCM streamed to the MAX98357A at 24 kHz with a soft-clip volume curve.
- **Microphone:** INMP441 with automatic channel detection, stored after the first successful detection.

### Connectivity and Web Portal
- **Wi-Fi:** connects to your home network. If no credentials are saved, Kiwi starts its own access point.
- **Access point:** SSID `MyRobot-XXXX` (last two bytes of the MAC address), gateway `192.168.4.1`. The AP password is set in `src/config/AppConfig.cpp` (`apPasswordBuf`). Change it before you use the device.
- **mDNS:** `http://kiwi.local` (hostname configurable).
- **WLED integration:** auto-discovery over mDNS (`_wled._tcp`), or enter an IP address manually.

**Web portal pages and endpoints:**

| Path | Purpose |
| :--- | :--- |
| `/` | Dashboard with **Live Console** (status, logs, quick ask/record) and **Settings & AI** (providers, keys, prompt, volume, sleep, touch mapping, skins) |
| `/faces` | Face control deck (set any emotion or skin, sound FX toggle, live status) |
| `/update` | Firmware OTA: install from GitHub releases (default repo `krishh-9085/kiwi`), a custom URL, or an uploaded `.bin` |
| `/animations` | Upload and manage up to three custom 128x64 OLED animation frames |
| `/status`, `/config`, `/scan` | JSON status, configuration and Wi-Fi scan |
| `/do` | Action endpoint (record, ask, clear log, stop, timer) |
| `/save` | Save settings |
| `/wiring` | Redirects to `/` (see Known Limitations) |

### Firmware Updates
- **OTA over Wi-Fi:** `/update` installs a new build from GitHub releases, from a custom URL, or from an uploaded `.bin` file, without USB.
- **Partition table** (`partitions.csv`): dual OTA slots of about 1.9 MB each (`app0`, `app1`), NVS, OTA data, SPIFFS and coredump.

---

## Hardware

Pinout from `src/config/AppConfig.h`:

| Peripheral | Signal | ESP32-C3 Pin | Notes |
| :--- | :--- | :--- | :--- |
| OLED SSD1306 (0x3C) | SDA | `GPIO8` | I2C data, 800 kHz during display writes |
| OLED SSD1306 | SCL | `GPIO9` | I2C clock |
| INMP441 microphone | SD | `GPIO4` | I2S data in |
| I2S (mic and amp shared) | BCLK | `GPIO5` | Bit clock |
| I2S (mic and amp shared) | WS | `GPIO6` | Word select |
| MAX98357A amplifier | DIN | `GPIO7` | I2S data out |
| Touch, left | SIG | `GPIO10` | Digital capacitive touch |
| Touch, right | SIG | `GPIO20` | Digital capacitive touch |
| Touch, top | SIG | `GPIO3` | Wakes the device from sleep (HIGH level) |

Audio configuration: 24 kHz sample rate, 6-second maximum recording window, microphone gain 3.2, speaker gain cap 2.0 (`SPEAKER_MAX_GAIN`), default speaker volume 70%.

**Battery (not yet implemented):** a voltage divider (for example 100k/100k) from the LiPo to an ADC pin would work with the `Battery.cpp` stub once implemented. The pin is not defined in `AppConfig.h` yet. Check the board schematic before using `GPIO0`, because it may be shared with other functions on the C3.

---

## Build and Flash

The project uses **PlatformIO**. The Arduino IDE is not supported (see Known Limitations).

Project settings (`platformio.ini`):
- Board: `esp32-c3-devkitm-1`, framework `arduino`
- CPU 160 MHz, flash 80 MHz, QIO flash mode
- Build flags: `-DARDUINO_USB_CDC_ON_BOOT=1`, `-Isrc`, `-O2`
- Libraries: Adafruit SSD1306 `^2.5.9`, Adafruit GFX `^1.11.9`, Adafruit BusIO `^1.15.0`
- Partition table: `partitions.csv`

Steps:

```bash
# Install PlatformIO Core or the PlatformIO IDE extension, then from the project root:
pio run -e esp32c3            # build
pio run -e esp32c3 -t upload  # flash over USB
pio device monitor -b 115200  # serial log
```

On Windows, enable long paths first (`LongPathsEnabled = 1`). Without it, PlatformIO can fail while it unpacks the ESP32 Arduino libraries.

Debug flags you can set in `AppConfig.h`:
- `BOOT_SERIAL_WAIT_MS` (default 0): waits up to this many milliseconds for USB serial at boot. Set to 2000 when debugging over USB.
- `BOOT_TIMING_LOG` (default 0): prints `[BOOT_T]` timing lines during boot.
- `LLM_STREAM_TTS` (default 1): set to 0 for the original non-streaming reply path.

---

## First-Time Setup

1. Flash the firmware and power the device.
2. If no Wi-Fi credentials are saved, Kiwi starts the access point `MyRobot-XXXX`. Connect to it from your phone or laptop and open `http://192.168.4.1`.
3. In **Settings & AI**, enter your Wi-Fi name and password, and your API keys for the speech-to-text, LLM and text-to-speech providers you want to use. Keys are stored in flash on the device.
4. Once connected to Wi-Fi, open `http://kiwi.local` on your home network.
5. Optional: enable **Greetings**, **Idle chatter** or **Sound-reactive eyes** in the dashboard.

---

## Configuration Reference (defaults from `AppConfig.cpp`)

Saved values in flash override these defaults. Changing a default in code does not change a device that already has a saved value.

| Setting | Default | Notes |
| :--- | :--- | :--- |
| Speaker volume | 70 | 0 to 100 |
| Daytime brightness | 40 | 0 to 100 (`disp_bright`) |
| Night mode | Dim | Same as day, Dim or Screen off |
| Night brightness | 10 | 0 to 100 (`night_bright`) |
| Circadian sleep | On, 23:00 to 07:00 | `circ_en`, `circ_start`, `circ_end` |
| Greetings | Off | `greet_en` |
| Sound-reactive eyes | Off | `snd_react` |
| Idle chatter | Off, 2 minutes | `boredom`, `boredom_mins` |
| Touch debounce | 35 ms | `tch_deb` |
| Chat history turns | 4 | `chat_hist` |
| Speech-to-text | Sarvam, language `hi` | Provider, key, model and endpoint all editable |
| LLM | Groq, `llama-3.3-70b-versatile` | Max tokens 300 (`LLM_MAX_TOKENS`) |
| Text-to-speech | Sarvam, `bulbul:v3`, voice `priya` | Max characters 600 (`TTS_MAX_CHARS`) |
| Weather location | New Delhi (28.6139, 77.2090) | `wthr_city`, `wthr_lat`, `wthr_lon` |
| Hostname | `kiwi` | `hostname`, used for `kiwi.local` |
| Face tween | 220 ms | `face_tween` |

---

## Architecture

```
MyRobot_Modular.ino          Entry point: setup(), loop(), boot flow, boot greeting
platformio.ini               PlatformIO build configuration
partitions.csv               Dual OTA partition layout
build_opt.h                  Legacy build-flag file (not used by PlatformIO; contains a machine path)
src/
  core/
    CoreTypes.h              Enums (emotions, skins, touch actions), tone and face structs
    Timing.h                 serviceUi() and waitMs() helpers (keep UI responsive while waiting)
  config/
    AppConfig.h / .cpp       Pinout, constants, defaults and shared runtime variables
    ConfigManager.h / .cpp   Load and save settings to flash (Preferences)
  hardware/
    AudioEngine.h / .cpp     I2S speaker and microphone, tone player, volume curve
    Battery.h / .cpp         Battery percentage stub (returns -1)
    SoundLevel.h / .cpp      Microphone amplitude for sound-reactive eyes
    TouchController.h / .cpp Touch debouncing, gestures and tickle detection
  face/
    FaceEngine.h / .cpp      Face update loop (tickFace), frame rate control
    FaceRenderer.h / .cpp    Eyes, mouth, blush, flourish compositing
    FaceExpressions.h / .cpp Emotion setting and easing
    FaceIdle.h / .cpp        Blinking, gaze, idle animations
    FaceSkins.h / .cpp       Procedural eye skins
    FaceScreens.h / .cpp     Clock, weather, banner, status and connecting screens
    FaceDisplay.h / .cpp     SSD1306 driver, brightness, dimming, power
    FaceEmotion.h, FaceTheme.h  Shared math helpers and theme constants
    BootWakeAnim.h / .cpp   Boot wake animation (bootAnimPump)
    SleepyWakeAnim.h / .cpp Sleepy fall-asleep and wake animations
  animation/
    AnimationEngine.h / .cpp Custom OLED frame animations
    DogAnimationFrames.h     Built-in dog animation frames
  behavior/
    InteractionManager.h/.cpp Touch reactions, pumpUI(), question pipeline, streaming speech
    CommandHandler.h / .cpp  Local voice commands (timer, time, weather, skin, emotion, volume, light)
    ActionExecutor.h / .cpp  [ACTION: ...] tag execution and robot timer
    CircadianEngine.h / .cpp Night sleep schedule and manual sleep
    GreetingEngine.h / .cpp  Periodic time-of-day greetings
    BoredomBehavior.h / .cpp Idle chatter
    OfflineBrain.h / .cpp    Offline reactions when Wi-Fi is unavailable
  power/
    PowerManager.h / .cpp    Light and deep sleep, touch and timer wake, CPU frequency, idle dimming
  services/
    AiService.h / .cpp       Facade for the AI modules
    AiTransport.h / .cpp     HTTP and TLS client and stream reader
    AiTextTools.h / .cpp     JSON helpers, UTF-8 utilities, speech text cleanup
    AiChatHistory.h / .cpp   Multi-turn conversation buffer
    AiLlmService.h / .cpp    LLM providers (Gemini, Groq, OpenAI) and SSE streaming
    AiSttService.h / .cpp    Recording and speech-to-text (Sarvam, Whisper)
    AiTtsService.h / .cpp    Text-to-speech providers (Sarvam, OpenAI, ElevenLabs, Fish)
    MemoryService.h / .cpp   Persistent facts and follow-ups
    WeatherService.h / .cpp  Open-Meteo weather client
    WledService.h / .cpp     WLED discovery and control
  web/
    WebPortal.h / .cpp       Access point, Wi-Fi connection, routing, mDNS
    WebPortalApi.cpp         Status, config, action and save endpoints
    WebPortalDashboard.cpp   Dashboard HTML (Live Console and Settings)
    WebPortalFaces.cpp       Face control deck HTML
    WebPortalOta.cpp         OTA update page
    WebPortalOtaEngine.cpp   OTA download and install logic
    WebPortalWiring.cpp      Redirect for the removed /wiring page
    icon.h                   Embedded app icon
tools/web_dev/               Local UI development server and HTML-to-C++ sync scripts
start_ui_dev.bat             Run the local UI dev server (Windows)
sync_ui_to_cpp.bat           Sync edited UI templates back into the C++ source (Windows)
```

---

## Development: Editing the Web UI

The dashboard, faces and update pages are stored as C++ strings. To edit them comfortably:

1. Run `start_ui_dev.bat` (Windows) or `python tools/web_dev/server.py`. This serves the templates in `tools/web_dev/templates/` locally.
2. Edit the templates in `tools/web_dev/templates/`.
3. Run `sync_ui_to_cpp.bat` (or `tools/web_dev/sync_to_cpp.py`) to write the changes back into `src/web/*.cpp`.

Keep `tools/web_dev/templates/` and `src/web/` in sync. Edits made directly in `src/web/` are overwritten the next time the sync script runs.

---

## Troubleshooting

- **Build fails with "No such file or directory" or path errors on Windows:** enable long paths (`LongPathsEnabled = 1`) and reinstall the PlatformIO packages.
- **No greeting at boot:** check that the **Greetings** toggle is on and that the speech and LLM keys are set.
- **Web portal not reachable during sleep:** expected. Wake Kiwi with a top tap.
- **Speech never starts, or reply is spoken only at the end:** check the serial log for `[STREAM] maxAlloc=` (heap is below `STREAM_TTS_MIN_MAX_ALLOC`) and set `LLM_STREAM_TTS` to 0 if needed.
- **Battery shows no value:** expected until the battery reading is implemented.

---

## License

No license file is included in this repository. Contact the author before reusing the code.
