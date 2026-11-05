/*
 ==============================================================================
  MyRobot (Kiwi) - Modular v1.0
  ESP32-C3 Super Mini / PlatformIO / Arduino Core 3.x
  Tools -> USB CDC On Boot = Enabled

  Architecture & Modules:
  - config/AppConfig.h / .cpp      : Hardware pinouts, constants, shared globals
  - core/CoreTypes.h               : Data structures, emotion & mood enums
  - hardware/Battery.h / .cpp      : Non-linear LiPo battery sensing curve
  - config/ConfigManager.h / .cpp  : NVS Preferences persistent storage
  - hardware/AudioEngine.h / .cpp  : I2S driver, Tone player, DSP &
 soft-clipping
  - face/FaceEngine.h / .cpp       : OLED procedural vector face engine &
 screens
  - services/WeatherService.h/.cpp : Open-Meteo REST API live weather client
  - behavior/CircadianEngine.h/.cpp: Circadian night sleep mode & auto-dimming
  - behavior/BoredomBehavior.h/.cpp: Autonomous idle remarks & desk banter
  - behavior/CommandHandler.h/.cpp : Voice command parsing & dispatch
  - behavior/InteractionManager.h/.: User interaction pipeline & UI pump
  - services/AiService.h / .cpp    : STT, LLM (Gemini/Groq), streaming TTS
 clients
  - hardware/TouchController.h/.cpp: Capacitive & digital touch sensor gestures
  - web/WebPortal.h / .cpp         : Captive SoftAP, settings UI, Web OTA
 updates
 ==============================================================================
*/

#include "animation/AnimationEngine.h"
#include "behavior/ActionExecutor.h"
#include "behavior/BoredomBehavior.h"
#include "behavior/CircadianEngine.h"
#include "behavior/CommandHandler.h"
#include "behavior/GreetingEngine.h"
#include "behavior/InteractionManager.h"
#include "behavior/OfflineBrain.h"
#include "config/AppConfig.h"
#include "config/ConfigManager.h"
#include "core/CoreTypes.h"
#include "core/Timing.h"
#include "face/BootWakeAnim.h"
#include "face/FaceEngine.h"
#include "hardware/AudioEngine.h"
#include "hardware/TouchController.h"
#include "services/AiService.h"
#include "services/MemoryService.h"
#include "services/WeatherService.h"
#include "services/WledService.h"
#include "web/WebPortal.h"
#include <Arduino.h>
#include <ESPmDNS.h>
#include <WiFi.h>

// ============================================================
// BOOT WAKE & CONNECTION FLOW STATE
// ============================================================
static uint32_t bootStartAt = 0;
static uint32_t lastBootDrawAt = 0;
static bool bootWifiFinalizedEarly = false;

// ============================================================
// MAIN SETUP
// ============================================================
void setup() {
  bootStartAt = millis();
  Serial.begin(115200);
#if ARDUINO_USB_CDC_ON_BOOT
  uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 2000) {
    delay(10);
  }
#else
  delay(100);
#endif
  Serial.println();
  Serial.println("==========================================");
  Serial.println("  MYROBOT (KIWI) - MODULAR ARCHITECTURE   ");
  Serial.println("==========================================");

  randomSeed(esp_random());
  initBase64Lookup();
  loadConfig();

  if (!initDisplay()) {
    Serial.println("[OLED] FAILED to initialize");
    bootCompleted = true;
  } else {
    Serial.println("[OLED] Display ready");
  }
  initAnimations();

  initTouch();
  resetBoredomInterval();
  initMemory();

  Serial.printf("[LLM] %s / %s\n", llmProvider.c_str(),
                llmProvider == "groq" ? groqModel.c_str() : llmModel.c_str());
  Serial.printf("[STT] %s / %s\n", sttProvider.c_str(), sttLang.c_str());
  Serial.printf("[TTS] %s\n", ttsProvider.c_str());
  Serial.printf("[VOL] %d%%\n", speakerVolume);

  eyesInit(millis());
  setEmotion(EMOTION_IDLE);

  if (wifiSSID.length()) {
    startConnectHomeAsync();
  } else {
    startAP();
    initOfflineBrain();
  }

  discoverWled();

  startWeb();
  Serial.printf("[NET] Heap: %u\n", (unsigned)ESP.getFreeHeap());

  if (!startAudio()) {
    setEmotion(EMOTION_ERROR);
    Serial.println("[I2S] Audio initialization FAILED");
  } else {
    detectMicSlot();
    playBootChime();
    Serial.printf("[I2S] Audio OK, heap=%u\n", (unsigned)ESP.getFreeHeap());
  }

  lastUserInteractionAt = millis();

  if (oledOK && !bootCompleted) {
    bootStartAt = millis();
    bootWakeStart();
  }
}

// ============================================================
// BOOT FLOW TICK (NON-BLOCKING WAKE ANIMATION + WIFI GUARD)
// ============================================================
static void updateBootFlow() {
  if (bootCompleted)
    return;

  uint32_t now = millis();
  uint32_t elapsed = now - bootStartAt;

  // Watchdog guard: reach normal face within 5000 ms of wake start
  if (elapsed >= 5000UL) {
    Serial.println("[BOOT] Watchdog reached -> transition to normal face");
    if (WiFi.status() == WL_CONNECTED && !bootWifiFinalizedEarly) {
      bootWifiFinalizedEarly = true;
      finalizeWifiConnected();
    } else if (WiFi.status() != WL_CONNECTED && !wifiConnecting) {
      finalizeWifiOffline();
    }
    bootCompleted = true;
    eyesInit(now);
    setEmotion(EMOTION_IDLE);
    markDisplayDirty();
    return;
  }

  // Stage 1: Wake animation is ticking (0 to 3600 ms)
  if (bootWakeTick()) {
    if (WiFi.status() == WL_CONNECTED && !bootWifiFinalizedEarly) {
      bootWifiFinalizedEarly = true;
      finalizeWifiConnected();
    }
    if (now - lastBootDrawAt >= 25) {
      lastBootDrawAt = now;
      bootWakeDraw(display);
      presentDisplay(true);
    }
    return;
  }

  // Stage 2: Animation finished smoothly! Seamlessly hand off to normal face
  if (WiFi.status() == WL_CONNECTED && !bootWifiFinalizedEarly) {
    bootWifiFinalizedEarly = true;
    finalizeWifiConnected();
  } else if (WiFi.status() != WL_CONNECTED && !wifiConnecting) {
    finalizeWifiOffline();
  }

  bootCompleted = true;
  eyesInit(now);
  setEmotion(EMOTION_IDLE);
  markDisplayDirty();
}

// ============================================================
// BOOT GREETING FOR KRISHNA
// ============================================================
static void checkBootGreeting() {
  static bool greetingTriggered = false;
  if (greetingTriggered)
    return;
  if (!bootCompleted)
    return;
  if (!greetingsEnabled) {
    greetingTriggered = true;
    return;
  }
  if (busyProcessing || inCircadianSleep || manualSleep)
    return;

  // Let the boot chime finish playing first
  if (isTonePlaying())
    return;

  // Give up to 4 seconds for WiFi NTP time sync so the greeting matches the
  // exact time of day
  if (!timeSynced && (millis() - bootStartAt) < 10000UL)
    return;

  greetingTriggered = true;

  int hour = -1;
  if (timeSynced) {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 50)) {
      hour = timeinfo.tm_hour;
    }
  }

  String timeGreeting = "good morning";
  if (hour >= 5 && hour < 12) {
    timeGreeting = "good morning";
  } else if (hour >= 12 && hour < 17) {
    timeGreeting = "good afternoon";
  } else if (hour >= 17 && hour < 21) {
    timeGreeting = "good evening";
  } else if (hour >= 21 || (hour >= 0 && hour < 5)) {
    timeGreeting = "good night";
  } else {
    timeGreeting = "good day";
  }

  String speech = "Hi Krishna, " + timeGreeting + "! What's up?";

  Serial.printf("[BOOT] Greeting Krishna: %s (hour=%d)\n", speech.c_str(),
                hour);
  setEmotion(EMOTION_HAPPY);
  flourish = FL_STAR;
  flourishT0 = millis();

  if (WiFi.status() == WL_CONNECTED &&
      (ttsKey.length() || sttKey.length() || llmKey.length())) {
    speakText(speech);
  }
  setEmotion(EMOTION_IDLE);
}

// ============================================================
// MAIN EVENT LOOP
// ============================================================
void loop() {
  pumpWeb();
  maintainWiFi();

  if (!bootCompleted) {
    updateBootFlow();
    updateTonePlayer();
    yield();
    return;
  }

  checkBootGreeting();

  checkCircadianSleep();
  checkGreetings();
  updateTouch();
  checkRobotTimer();
  updateOfflineBrain();
  checkAutonomousBoredom();
  updateIdleVariety();
  tickFace();
  maybeChangeEmotion();
  updateTonePlayer();
  processPendingSound();

  if (webQueuedQuestion.length() > 0 && !busyProcessing) {
    String q = webQueuedQuestion;
    webQueuedQuestion = "";
    busyProcessing = true;
    lastProgressAt = millis();
    cancelRequest = false;
    lastTranscribedQuestion = q;
    addTerminalLog("[WEB] Prompt received: \"" + q + "\"");
    setEmotion(EMOTION_THINKING);
    handleQuestion(q);
    bool wasCancelled = cancelRequest;
    cancelRequest = false;
    if (wasCancelled) {
      returnIdleAt = 0;
      setEmotion(EMOTION_IDLE);
      robotStateDesc = "Ready";
      Serial.println("[ROBOT] Web request cancelled");
    } else if (returnIdleAt == 0) {
      setEmotion(EMOTION_IDLE);
    }
    resyncTouch();
    spkEndSpeech();
    busyProcessing = false;
  }

  if (startRecording && !busyProcessing) {
    startRecording = false;
    busyProcessing = true;
    lastProgressAt = millis();
    cancelRequest = false;
    robotStateDesc = "Listening...";
    addTerminalLog("[REC] Listening for up to " + String(RECORD_SECONDS) +
                   " seconds; stops after speech ends...");

    setEmotion(EMOTION_LISTENING);
    // Smoothly ease face into listening pose before blocking network STT setup
    waitMs(120);

    if (cancelRequest) {
      Serial.println("[ROBOT] Cancelled before recording");
      setEmotion(EMOTION_IDLE);
      robotStateDesc = "Ready";
      resyncTouch();
      busyProcessing = false;
      return;
    }

    if (WiFi.status() != WL_CONNECTED) {
      handleOfflineBuddyTap();
      return;
    }

    SttCfg cfg = resolveStt();
    if (!cfg.key.length()) {
      robotStateDesc = "No STT API Key";
      addTerminalLog("[REC] Error: No STT API Key configured");
      Serial.println("[ROBOT] Top tap: No STT API Key configured!");
      setEmotion(EMOTION_CONFUSED);
      showStatus("No STT API Key", "Add in Web Portal", 2500);
      setEmotion(EMOTION_IDLE);
      resyncTouch();
      busyProcessing = false;
      return;
    }

    robotStateDesc = "Transcribing...";
    addTerminalLog("[STT] Transcribing captured audio with " + sttProvider +
                   " (" + cfg.model + ")...");
    String question = recordAndTranscribe(RECORD_SECONDS);
    if (!cancelRequest && question.length()) {
      lastTranscribedQuestion = question;
      addTerminalLog("[STT] User: \"" + question + "\"");
      handleQuestion(question);
    } else if (!cancelRequest) {
      robotStateDesc = "STT: " + sttFail;
      addTerminalLog("[STT] Speech not recognized: " + sttFail);
      Serial.printf("[ROBOT] STT failed: %s\n", sttFail.c_str());
      setEmotion(EMOTION_CONFUSED);
      if (sttFail == "quiet" || sttFail == "empty" || sttFail == "") {
        static const char *const quietReplies[] = {
            "Kuch bola kya? Mujhe theek se sunai nahi diya!",
            "Did you say something? Sunai nahi diya mujhe!",
            "Arey kuch kaha kya? Ek baar firse bolo!"};
        speakText(quietReplies[random(0, 3)]);
      } else {
        speakText("Awaaz theek se nahi aayi, ek baar firse try karo!");
      }
      returnIdleAt = millis() + 2000;
    }
    bool wasCancelled = cancelRequest;
    cancelRequest = false;
    if (wasCancelled) {
      returnIdleAt = 0;
      setEmotion(EMOTION_IDLE);
      robotStateDesc = "Ready";
      Serial.println("[ROBOT] Cancelled - returning to Ready");
    } else if (returnIdleAt == 0) {
      setEmotion(EMOTION_IDLE);
    }
    resyncTouch();
    spkEndSpeech();
    busyProcessing = false;
  }

  // 30s Inactivity Watchdog
  if (busyProcessing) {
    if (millis() - lastProgressAt > 30000UL) {
      Serial.println(
          "[WATCHDOG] Inactive for >30s! Force recovering Kiwi to Ready.");
      addTerminalLog("[WATCHDOG] Request timed out (>30s inactivity). "
                     "Recovering to Ready.");
      spkFlush();
      spkEndSpeech();
      cancelRequest = true;
      busyProcessing = false;
      lastProgressAt = millis();
      robotStateDesc = "Ready";
      setEmotion(EMOTION_IDLE);
      resyncTouch();
    }
  }

  if (returnIdleAt && millis() >= returnIdleAt && !busyProcessing) {
    returnIdleAt = 0;
    robotStateDesc = "Ready";
    if (!showClockScreen && !showWeatherScreen)
      setEmotion(EMOTION_IDLE);
  }

  // Heap monitor: log every 10s, warn once if max alloc drops below 20KB
  static uint32_t lastHeapCheck = 0;
  static bool lowMemWarned = false;
  if (millis() - lastHeapCheck > 10000UL) {
    lastHeapCheck = millis();
    uint32_t freeH = ESP.getFreeHeap();
    uint32_t maxAlloc = ESP.getMaxAllocHeap();
    if (currentLogLevel >= LOG_LEVEL_DEBUG) {
      Serial.printf("[HEAP] Free: %u, MaxAlloc: %u\n", (unsigned)freeH,
                    (unsigned)maxAlloc);
    }
    if (maxAlloc < 20480UL) {
      if (!lowMemWarned) {
        Serial.printf("[HEAP] WARNING: Low memory! MaxAlloc below 20KB (%u "
                      "bytes, free %u bytes)\n",
                      (unsigned)maxAlloc, (unsigned)freeH);
        lowMemWarned = true;
      }
    } else {
      lowMemWarned = false;
    }
  }

  yield();
}
