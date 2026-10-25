# MyRobot (Kiwi) - Modular Architecture (v4.6)

A clean, modular C++ architecture for the **ESP32-C3 Super Mini AI Desktop Companion Robot**.

---

## 📁 Architecture Overview

```
MyRobot_Modular/
├── MyRobot_Modular.ino      # Main sketch: setup(), loop(), voice command parsing, pipeline
├── Config.h / .cpp         # Pin definitions, constants, shared global variables
├── Types.h                 # Data structs (HttpStream, Tone, FaceState, etc.) & Enums
├── Battery.h / .cpp        # Accurate piecewise Li-Ion/LiPo battery percentage estimator
├── ConfigManager.h / .cpp  # Preferences (NVS) read, write, and clear operations
├── AudioEngine.h / .cpp    # I2S driver, Tone player, WAV header, soft-clip volume scaler
├── FaceEngine.h / .cpp     # OLED vector eye renderer, facial animation easing, screens
├── WeatherService.h / .cpp # Open-Meteo REST API live weather client (HTTP port 80)
├── CircadianEngine.h / .cpp# Nighttime sleep schedule, auto-dim, morning wake cycle
├── AiService.h / .cpp      # STT multipart client, LLM (Gemini/Groq), streaming TTS decoders
├── TouchController.h / .cpp# Debouncing, tap vs hold, pet reactions, sleep wake-up
├── MemoryService.h / .cpp  # Long-term life diary, auto fact extraction, next-day follow-ups
└── WebPortal.h / .cpp      # SoftAP captive portal, settings web UI, Web OTA updates
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
13. **Custom OLED Eye Skins:** Live switch between 4 unique facial aesthetics from the Web Portal without rebooting:
    - **Default:** Clean Round Vector Eyes.
    - **Cyberpunk:** Sci-Fi holographic visor with scanlines, tech HUD corner targeting brackets, and core reticle.
    - **Kawaii Cat:** Neko almond eyes with vertical slit pupils, double anime sparkle glints, and animated twitching cat ears on OLED!
    - **Retro 8-Bit:** Classic pixelated arcade/Tamagotchi stepped pixel art blocks.

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
