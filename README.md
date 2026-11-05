# MyRobot (Kiwi) - Modular Architecture (v1.0)

A clean, modular C++ architecture for the **ESP32-C3 Super Mini AI Desktop Companion Robot**.

---

## 📁 Architecture Overview

```
MyRobot_Modular/
├── platformio.ini              # PlatformIO build configuration
├── partitions.csv              # Custom flash partition table (Dual OTA + Preferences)
├── README.md                   # Project documentation & wiring guide
├── .gitignore                  # Git ignore rules (*.bin, build/, .pio/)
├── archive/
│   └── WebPortal.cpp.bak       # Preserved legacy single-file portal backup
├── tools/
│   └── scratch/scratch_dog.py  # Utility test frame scripts
└── src/
    ├── MyRobot_Modular.ino     # Main entry point (setup() and loop() only)
    ├── core/
    │   └── CoreTypes.h         # Core data structs, enums (Emotion, Skin, Tone)
    ├── config/
    │   ├── AppConfig.h / .cpp  # Hardware pinouts, constants, runtime configuration
    │   └── ConfigManager.h/.cpp# Preferences (NVS) read, write, and clear operations
    ├── hardware/
    │   ├── Battery.h / .cpp    # Piecewise Li-Ion/LiPo battery percentage estimator
    │   ├── TouchController.h/c # Debounced capacitive/digital multi-touch gestures
    │   └── AudioEngine.h / .cpp# I2S driver, Tone player, soft-clip volume scaler
    ├── behavior/
    │   ├── CircadianEngine.h/.c# Nighttime sleep schedule, auto-dim, morning wake cycle
    │   ├── BoredomBehavior.h/.c# Autonomous idle remarks, desk banter & quirks
    │   ├── CommandHandler.h/.c # Voice command parsing & localized triggers
    │   └── InteractionManager.h# Pet touch reactions, UI pump & question pipeline
    ├── animation/
    │   ├── AnimationEngine.h/.c# Full-screen procedural bitmap animation system
    │   └── DogAnimationFrames.h# Dog animation bitmap frame data
    ├── face/
    │   ├── FaceEngine.h / .cpp # Face engine coordinator & tickFace()
    │   ├── FaceDisplay.h / .cpp# I2C OLED hardware initialization & dimming
    │   ├── FaceExpressions.h/.c# Emotion states, facial target easing & action helpers
    │   ├── FaceIdle.h / .cpp   # Natural eye blinking, gaze jitter & idle animations
    │   ├── FaceSkins.h / .cpp  # Procedural vector eye skins (Kiwi, Cyberpunk, Cat, etc.)
    │   ├── FaceRenderer.h /.cpp# Eyes, mouth, blush, flourish & status compositing
    │   └── FaceScreens.h / .cpp# Clock, weather, banner, status & connecting screens
    ├── services/
    │   ├── AiService.h / .cpp  # AI service unified facade
    │   ├── AiTextTools.h / .cpp# JSON parser, UTF-8 utils & text-to-speech sanitizer
    │   ├── AiTransport.h / .cpp# Non-blocking TLS/HTTP client & socket stream reader
    │   ├── AiChatHistory.h/.cpp# Multi-turn conversation buffer with auto-expiry
    │   ├── AiLlmService.h /.cpp# Cloud LLM integration (Gemini, Groq, OpenAI)
    │   ├── AiSttService.h /.cpp# Voice capture & cloud STT (Whisper, Sarvam)
    │   ├── AiTtsService.h /.cpp# Streaming TTS audio playback (Sarvam, OpenAI, Eleven)
    │   ├── MemoryService.h/.cpp# Long-term life diary, fact memory & follow-ups
    │   └── WeatherService.h/.cp# Open-Meteo REST API live weather client
    └── web/
        ├── WebPortal.h / .cpp  # Captive SoftAP & HTTP request router
        ├── WebPortalApi.h/.cpp # REST API endpoints for live status & controls
        ├── WebPortalDashboard.c# Cyber glassmorphic configuration dashboard
        ├── WebPortalFaces.cpp  # Interactive 42-face emoji control deck
        ├── WebPortalOta.cpp    # Web browser over-the-air firmware updates
        └── WebPortalWiring.cpp # Embedded interactive hardware wiring blueprint
```

---

## 🛠️ Hardware Pinout (ESP32-C3 Super Mini)

| Peripheral | Signal | ESP32-C3 Pin | Notes |
| :--- | :--- | :--- | :--- |
| **OLED SSD1306** | SDA | `GPIO8` | I2C Data (Default wiring preserved) |
| **OLED SSD1306** | SCL | `GPIO9` | I2C Clock (Default wiring preserved) |
| **INMP441 Microphone** | SD | `GPIO4` | I2S RX Data |
| **Audio Clock** | BCLK | `GPIO5` | I2S Bit Clock (shared between Mic & Amp) |
| **Audio Word Select** | WS / LRCLK | `GPIO6` | I2S Word Select (shared between Mic & Amp) |
| **MAX98357A Amplifier** | DIN | `GPIO7` | I2S TX Data |
| **Touch Sensor (Left)** | SIG | `GPIO20` | Digital/Capacitive Touch |
| **Touch Sensor (Right)**| SIG | `GPIO21` | Digital/Capacitive Touch |
| **Touch Sensor (Top)**  | SIG | `GPIO3`  | Digital/Capacitive Touch (Deep Sleep Wakeup) |
| **Battery ADC** (opt)   | VBAT | `GPIO0`  | 100k / 100k Voltage Divider |

---

## 🌟 Key Features

1. **34+ Facial Expressions & Emotional Life:**
   - **Core & Expressive:** Idle, Happy, Excited, Sad, Angry, Confused, Thinking, Sleeping, Listening, Speaking, Error, Wink, Smug, Curious, Bored, Laughing, Scared, Suspicious, Relieved, Proud, Dizzy, Lovestruck.
   - **Wild & Vector Overlays:** Cool (sunglasses), Evil (devil horns), Crying (streaming tears), Hearts (floating hearts), Dead/KO (X X eyes), Shocked (tiny pupils), Singing (musical notes), Party (confetti dance), Nervous (sweat drop), Cute (puppy eyes).
   - **Pet Touch Reactions:** Love, Giggle, Shy, Sparkle, Kiss, Cuddle, Playful, Sleepy, Proud, Surprised.
2. **Top Touch Button Gestures:**
   - **Short Tap:** Start voice recording / Cancel active speech / Tap to wake from Sleep Mode.
   - **Hold 3 Seconds:** Show synced digital Clock screen (`soundClock()`).
   - **Hold 7-10 Seconds:** Toggle **Sleep Mode** on/off manually (screen dims, cute "Z Z" snoring animation). Tap again or hold to wake up!
3. **Cyber Glassmorphic Web UI & Emoji Deck:**
   - **Face Control Deck (`/faces`):** 42 interactive emoji cards categorized into All, Pet Moods, Core, Expressive, and Wild & Fun, with sound FX toggle, Surprise Me 🎲 button, and live status pill.
   - **Settings Portal (`/`):** Frosted glass styling with live battery gauge, active brain pills, and volume slider.
4. **Circadian Sleep Mode:** Automatic scheduled nighttime sleep with gentle dimming and morning wake-up chime, plus manual hold toggle.
5. **Live Weather:** Ask Kiwi *"How's the weather?"* or *"Mausam kaisa hai"* to get real-time Open-Meteo forecast and temperature readouts on OLED with spoken audio.
6. **Web Over-The-Air (OTA) Updates:** Flash new `.bin` builds directly over Wi-Fi via `http://<ip>/update` without plugging in a USB cable.
7. **Multi-Model Support:** Gemini (3.8 Flash, 3.5 Flash-Lite, 2.5 Flash), Groq LLM, Sarvam AI STT/TTS, OpenAI Whisper/TTS, and ElevenLabs.
8. **Autonomous Desk Chatter & Quirks (Idle ~2 Min):** If Kiwi is left idle on the desk for ~2 minutes, he automatically makes spontaneous, lively remarks, tea/water breaks, and witty observations (32+ unique phrases with zero repetitions & owner name personalization) instead of repetitive robotic speech! Includes live toggle switches in both the Face Control Deck (`/faces`) and System Dashboard (`/`).
9. **Tickle Mode (Gudgudi):** Repeatedly tapping or rubbing left and right cheeks triggers laughing eyes, giggle sound FX, and hilarious voice complaints ("Arey arey gudgudi ho rahi hai, chhodo mujhe!").
10. **Named OLED Animations:** Upload up to three custom 128x64 OLED frames, choose a name for each, and keep them persistently stored in the robot across reboots.
11. **OLED Custom Marquee / Banner:** Type any custom message from the Web Portal (e.g. *"Rohil is Coding - Do Not Disturb"*), and Kiwi displays a smooth scrolling marquee across the OLED screen! Tap any touch button to dismiss.
12. **Persistent Long-Term Memory & Life Diary:** Kiwi permanently remembers your name, goals, upcoming tests, and personal facts in Flash memory across power-offs and reboots! Autonomously extracts facts from voice chats, injects them into LLM context, and checks in with next-day follow-up questions (*"Kal tumhara interview kaisa gaya?"*). View and edit memories in the Web Portal.
13. **Custom OLED Eye Skins:** Live switch between unique facial aesthetics from the Web Portal without rebooting (Default, Cyberpunk, Kawaii Cat, Retro 8-Bit, Desk Buddy Kiwi, Anime, Hearts, Matrix, etc.). Includes procedural squircle eye skins:
    - **Squircle Solid (`squircle_solid`):** Solid white squircle eyes with black square pupil and blink support.
    - **Squircle Outline (`squircle_outline`):** Outlined squircle eyes (thin white stroke, transparent inside) with solid white circular pupil that drifts with gaze.
    - **Squircle Glint (`squircle_glint`):** Solid white squircle eyes with black square pupil and small specular white square glint inside the pupil drifting with gaze.
14. **LLM Tool / Function Calling (Smart Action Execution):** The cloud LLMs (Gemini, Groq, OpenAI) have direct hardware actuation capabilities! Whenever the user asks to change volume (*"Aawaz 80 kardo"*), change skins (*"Show cat eyes"*), set timers (*"Set a 5 minute timer"*), scroll banners, or change moods, the LLM emits embedded `[ACTION: ...]` tags. Kiwi executes the hardware actions instantaneously and cleanly strips the technical tags so speech/TTS speaks only natural human dialogue!
15. **Offline Fallback Brain (Desk Buddy Cute Eye Theme):** If Wi-Fi is disconnected or unavailable, Kiwi does NOT become an inert or broken brick. Instead, the offline fallback brain immediately engages the **Desk Buddy Cute Eye Theme** (`SKIN_KIWI`) with adorable flat-top, round-bottom U-pupil catchlight eyes. Tapping the top sensor plays cute procedural audio chirps, winks, hearts, and puppy reactions rather than an error buzz, while offline cheek petting, tickle gudgudi mode, and idle desk yawns/singing work 100% locally!
16. **Non-Blocking Boot Wake-Up Animation:** On power-on, Kiwi plays a smooth, one-time 6-second wake-up animation (begins asleep with thin horizontal eye lines, slowly eases open, squints, yawns, and smoothly settles into the normal awake eyes) while Wi-Fi connects concurrently in the background. Fully non-blocking (zero `delay()`), runs once per boot, and is protected by an 8-second watchdog safety guard to ensure Kiwi always reaches the interactive face.

---

## 💻 Arduino IDE Setup

1. Open `MyRobot_Modular.ino` in Arduino IDE.
2. All module files (`.h` and `.cpp`) will automatically load as tabs across the top.
3. **Board:** `ESP32C3 Dev Module` (or `ESP32-C3 Super Mini`)
4. **Tools Settings:**
   - **USB CDC On Boot:** `Enabled`
   - **Flash Size:** `4MB`
   - **Partition Scheme:** `Minimal SPIFFS (Large APPS with OTA)`
5. Click **Upload** (or export `.bin` and flash via Web OTA at `/update`).
