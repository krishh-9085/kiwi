/*
 ==============================================================================
  MyRobot (Kiwi) - Modular v1.2
  ESP32-S3 Super Mini / PlatformIO / Arduino Core 3.x
  Tools -> USB CDC On Boot = Enabled

  Architecture & Modules:
  - config/AppConfig.h / .cpp        : Hardware pinouts, constants, shared globals
  - core/CoreTypes.h                 : Data structures, emotion & mood enums
  - config/ConfigManager.h / .cpp    : NVS Preferences persistent storage
  - hardware/AudioEngine.h / .cpp    : I2S driver, Tone player, DSP & soft-clipping
  - face/FaceEngine.h / .cpp         : OLED procedural vector face, animations & screens
  - integrations/WeatherService.h/.cp: Open-Meteo REST API live weather client
  - integrations/Wled & HyperHdr     : WLED & HyperHDR ambilight control
  - memory/MemoryService.h / .cpp    : Persistent personal diary & family graph
  - behavior/                        : Circadian, Greeting, Hunger, Boredom, OfflineBrain
  - commands/                        : Voice command handlers & [ACTION:] executor
  - ai/                              : STT, LLM (Gemini/Groq), streaming TTS
  - net/WifiManager.h / .cpp         : STA/SoftAP Wi-Fi, mDNS, captive DNS & NTP sync
  - web/WebPortal.h / .cpp           : Embedded Preact UI server, REST API & Web OTA
 ==============================================================================
*/

#include "face/AnimationEngine.h"
#include "commands/ActionExecutor.h"
#include "behavior/BoredomBehavior.h"
#include "behavior/CircadianEngine.h"
#include "commands/CommandHandler.h"
#include "behavior/GreetingEngine.h"
#include "behavior/HungerEngine.h"
#include "behavior/InteractionManager.h"
#include "behavior/OfflineBrain.h"
#include "config/AppConfig.h"
#include "config/ConfigManager.h"
#include "core/CoreTypes.h"
#include "core/Timing.h"
#include "face/BootWakeAnim.h"
#include "face/FaceEngine.h"
#include "face/SleepyWakeAnim.h"
#include "hardware/AudioEngine.h"
#include "hardware/RgbLed.h"
#include "hardware/SoundLevel.h"
#include "hardware/TouchController.h"
#include "hardware/WakeWordEngine.h"
#include <ESP_TF.h>
#include "power/PowerManager.h"
#include "ai/AiService.h"
#include "integrations/HyperHdrService.h"
#include "memory/MemoryService.h"
#include "integrations/WeatherService.h"
#include "integrations/WledService.h"
#include "net/WifiManager.h"
#include "web/WebPortal.h"
#include <Arduino.h>
#include <ESPmDNS.h>
#include <WiFi.h>


// ============================================================
// BOOT WAKE & CONNECTION FLOW STATE
// ============================================================
static uint32_t bootStartAt = 0;
static bool bootWifiFinalizedEarly = false;

// ============================================================
// MAIN SETUP
// ============================================================
void setup() {
#if BOOT_TIMING_LOG
  bootSetupEnterAt = millis();
#endif
  bootStartAt = millis();
  Serial.begin(115200);
#if ARDUINO_USB_CDC_ON_BOOT
#if BOOT_SERIAL_WAIT_MS > 0
  uint32_t t0 = millis();
  while (!Serial && millis() - t0 < BOOT_SERIAL_WAIT_MS) {
    delay(10);
  }
#endif
#else
  delay(100);
#endif
#if BOOT_TIMING_LOG
  Serial.printf("[BOOT_T] setup_enter t=%lu ms\n",
                (unsigned long)(millis() - bootSetupEnterAt));
#endif
  Serial.println();
  Serial.println("==========================================");
  Serial.println("  MYROBOT (KIWI) - MODULAR ARCHITECTURE   ");
  Serial.println("==========================================");
  if (psramFound()) {
    Serial.printf("[PSRAM] Hardware 2MB PSRAM Initialized! Total: %u KB, Free: %u KB\n",
                  (unsigned)(ESP.getPsramSize() / 1024),
                  (unsigned)(ESP.getFreePsram() / 1024));
    initMbedTlsPsram();
    initTtsBuffers();
  } else {
    Serial.println("[PSRAM] Not initialized or not enabled.");
  }

  randomSeed(esp_random());
  initBase64Lookup();
  loadConfig();
  powerInit();

  if (!initDisplay()) {
    Serial.println("[OLED] FAILED to initialize");
    bootCompleted = true;
    bootWakeForceDone();
  } else {
    Serial.println("[OLED] Display ready");
  }
#if BOOT_TIMING_LOG
  Serial.printf("[BOOT_T] after_display_init t=%lu ms\n",
                (unsigned long)(millis() - bootSetupEnterAt));
#endif

  if (oledOK && !bootCompleted && !skipBootWakeAnim) {
    bootStartAt = millis();
    bootWakeStart();
  }

  initAnimations();

  initTouch();
  resetBoredomInterval();
  initMemory();
  initHungerEngine();

  Serial.printf("[LLM] %s / %s\n", llmProvider.c_str(),
                llmProvider == "groq" ? groqModel.c_str() : llmModel.c_str());
  Serial.printf("[STT] %s / %s\n", sttProvider.c_str(), sttLang.c_str());
  Serial.printf("[TTS] %s\n", ttsProvider.c_str());
  Serial.printf("[VOL] %d%%\n", speakerVolume);

  rgbLedInit();
  eyesInit(millis());
  setEmotion(EMOTION_IDLE);

  if (wifiSSID.length()) {
    startConnectHomeAsync();
  } else {
    startAP();
    initOfflineBrain();
  }
#if BOOT_TIMING_LOG
  Serial.printf("[BOOT_T] after_startAP_or_connect t=%lu ms\n",
                (unsigned long)(millis() - bootSetupEnterAt));
#endif

  startWeb();
  Serial.printf("[NET] Heap: %u\n", (unsigned)ESP.getFreeHeap());

  if (!startAudio()) {
    setEmotion(EMOTION_ERROR);
    Serial.println("[I2S] Audio initialization FAILED");
  } else {
    detectMicSlot();
    if (!wakeFromDeepSleepWakeAnim && !suppressBootGreeting) {
      playBootChime();
    }
    Serial.printf("[I2S] Audio OK, heap=%u\n", (unsigned)ESP.getFreeHeap());
    // P2: Always initialize wake word engine at boot so it can be enabled/disabled at runtime
    if (!wakeWordInit()) {
      Serial.println("[WAKEWORD] Engine initialization FAILED at boot!");
    }
  }
#if BOOT_TIMING_LOG
  Serial.printf("[BOOT_T] after_startAudio t=%lu ms\n",
                (unsigned long)(millis() - bootSetupEnterAt));
#endif

  lastUserInteractionAt = millis();
#if BOOT_TIMING_LOG
  Serial.printf("[BOOT_T] setup_exit t=%lu ms\n",
                (unsigned long)(millis() - bootSetupEnterAt));
#endif
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
    } else if (WiFi.status() != WL_CONNECTED && !isWifiConnecting()) {
      finalizeWifiOffline();
    }
    bootCompleted = true;
    bootWakeForceDone();
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
    bootAnimPump();
    return;
  }

  // Stage 2: Animation finished smoothly! Seamlessly hand off to normal face
  if (WiFi.status() == WL_CONNECTED && !bootWifiFinalizedEarly) {
    bootWifiFinalizedEarly = true;
    finalizeWifiConnected();
  } else if (WiFi.status() != WL_CONNECTED && !isWifiConnecting()) {
    finalizeWifiOffline();
  }

  bootCompleted = true;
  eyesInit(now);
  setEmotion(EMOTION_IDLE);
  markDisplayDirty();
}

// ============================================================
// VARIED BOOT GREETING (time-of-day aware, no immediate repeats)
// ============================================================
static const char *GREET_MORNING[] = {
    "Good morning{n}! Chai pee li?",
    "Subah subah{n}! Aaj ka din kaisa rahega?",
    "Morning{n}! Aaj kuch naya karein?",
    "Rise and shine{n}! Ready for the day?",
    "Good morning{n}! Main already ready hoon, tum?",
};
static const char *GREET_AFTERNOON[] = {
    "Good afternoon{n}! Lunch ho gaya?",
    "Hey{n}, dopahar ho gayi! Kaisa chal raha hai?",
    "Afternoon{n}! Thodi energy chahiye? Main hoon na.",
    "Good afternoon{n}! Aaj kuch interesting hua?",
};
static const char *GREET_EVENING[] = {
    "Good evening{n}! Din kaisa gaya?",
    "Shaam ho gayi{n}! Chai ya coffee?",
    "Evening{n}! Thaka hua lag raha hai kya?",
    "Good evening{n}! Chalo, kuch fun karte hain.",
};
static const char *GREET_NIGHT[] = {
    "Itni raat ko{n}? Sab theek hai?",
    "Hey night owl{n}! Abhi tak jaag rahe ho?",
    "Late night{n}! Sone ka time ho gaya hai, kya?",
    "Shh, raat ho gayi{n}. Kya chal raha hai?",
};
static const char *GREET_ANY[] = {
    "Hi{n}! Kaise ho?",
    "Hello{n}! Kya chal raha hai?",
    "Hey{n}! Main ready hoon, batao.",
};

// Picks one template from a pool, never the same index twice in a row.
static String pickGreetingTemplate(const char *const *pool, int count,
                                   int &lastIdx) {
  int idx = random(count);
  if (count > 1 && idx == lastIdx) {
    idx = (idx + 1 + random(count - 1)) % count;
  }
  lastIdx = idx;
  return String(pool[idx]);
}

// hour: 0-23, or -1 if unknown. ownerName: trimmed, may be empty.
static String buildBootGreeting(int hour, const String &ownerName) {
  static int lastMorning = -1, lastAfternoon = -1, lastEvening = -1,
             lastNight = -1, lastAny = -1;
  String tpl;
  if (hour >= 5 && hour < 12) {
    tpl = pickGreetingTemplate(GREET_MORNING,
                               sizeof(GREET_MORNING) / sizeof(GREET_MORNING[0]),
                               lastMorning);
  } else if (hour >= 12 && hour < 17) {
    tpl = pickGreetingTemplate(
        GREET_AFTERNOON, sizeof(GREET_AFTERNOON) / sizeof(GREET_AFTERNOON[0]),
        lastAfternoon);
  } else if (hour >= 17 && hour < 21) {
    tpl = pickGreetingTemplate(GREET_EVENING,
                               sizeof(GREET_EVENING) / sizeof(GREET_EVENING[0]),
                               lastEvening);
  } else if (hour >= 21 || (hour >= 0 && hour < 5)) {
    tpl = pickGreetingTemplate(
        GREET_NIGHT, sizeof(GREET_NIGHT) / sizeof(GREET_NIGHT[0]), lastNight);
  } else {
    tpl = pickGreetingTemplate(
        GREET_ANY, sizeof(GREET_ANY) / sizeof(GREET_ANY[0]), lastAny);
  }
  // {n} becomes ", Name" if a name is known, otherwise nothing.
  String nameTok = ownerName.length() > 0 ? ", " + ownerName : String("");
  tpl.replace("{n}", nameTok);
  return tpl;
}

// ============================================================
// BOOT GREETING
// ============================================================
static void checkBootGreeting() {
  static bool greetingTriggered = false;
  if (suppressBootGreeting) {
    greetingTriggered = true;
    suppressBootGreeting = false;
  }
  if (greetingTriggered)
    return;
  if (!bootCompleted)
    return;
  if (!greetingsEnabled) {
    greetingTriggered = true;
    return;
  }
  if (busyProcessing || startRecording || inCircadianSleep || manualSleep ||
      sleepyWakeIsActive() || sleepyFallAsleepIsActive() ||
      animationIsActive() || showClockScreen || showWeatherScreen ||
      bannerActive || isRobotTimerActive() || isRobotTimerRinging())
    return;

  // Let the boot chime or any active audio finish playing first
  if (isTonePlaying() || isSpeechPlaying())
    return;

  // Give up to 10 seconds for WiFi NTP time sync so the greeting matches the
  // exact time of day
  if (!timeSynced && (millis() - bootStartAt) < 10000UL)
    return;

  greetingTriggered = true;

  int hour = -1, minute = 0, wday = -1;
  if (timeSynced) {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 50)) {
      hour = timeinfo.tm_hour;
      minute = timeinfo.tm_min;
      wday = timeinfo.tm_wday;
    }
  }

  String owner = getOwnerName();
  owner.trim();
  String fallback = buildBootGreeting(hour, owner);

  Serial.printf("[BOOT] Triggering greeting (hour=%d)\n", hour);
  deliverGreeting(hour, minute, wday, owner, fallback, "[BOOT]");
}

// ============================================================
// MAIN EVENT LOOP
// ============================================================
void loop() {
  if (!uiTaskRunning) {
    rgbLedUpdate();
  }
  serviceConfigManager();
  pumpWeb();
  maintainWiFi();

  if (!bootCompleted) {
    powerUpdate();
    updateBootFlow();
    updateTonePlayer();
    yield();
    return;
  }

  // Once boot is complete, offload Display rendering and NeoPixel LED to dedicated Core 0 task
  if (!uiTaskRunning) {
    startUiDisplayTask();
  }

  checkBootGreeting();

  checkCircadianSleep();
  powerUpdate();
  checkGreetings();
  updateTouch();
  checkRobotTimer();
  updateOfflineBrain();
  checkAutonomousBoredom();
  checkHungerBehavior();
  updateIdleVariety();
  soundLevelTick();
  if (!uiTaskRunning) {
    tickFace();
  }
  maybeChangeEmotion();
  updateTonePlayer();
  processPendingSound();

  if (pendingSleepAction && !busyProcessing && !kiwiIsOutputting()) {
    triggerPendingSleepAction();
  }

  if (webQueuedQuestion.length() > 0 && !busyProcessing) {
    powerNotifyActivity();
    extendTempWakeIfActive(CIRCADIAN_TEMP_WAKE_MS);
    String q = cleanTranscribedQuestion(webQueuedQuestion);
    webQueuedQuestion = "";
    busyProcessing = true;
    lastProgressAt = millis();
    cancelRequest = false;
    lastTranscribedQuestion = q;
    addTerminalLog("[WEB] Prompt received: \"" + q + "\"");
    setEmotion(EMOTION_THINKING);
    handleQuestion(q);
    bool wasCancelled = cancelRequest;
    bool bargeInListen = startRecording;
    cancelRequest = false;
    if (wasCancelled) {
      pendingSleepAction = false;
      returnIdleAt = 0;
      if (bargeInListen) {
        toneStop();
        setEmotion(EMOTION_LISTENING);
        robotStateDesc = "Listening...";
        Serial.println("[ROBOT] Web TTS barge-in -> Immediate listening");
      } else {
        setEmotion(EMOTION_IDLE);
        robotStateDesc = "Ready";
        Serial.println("[ROBOT] Web request cancelled");
      }
    } else if (pendingSleepAction) {
      triggerPendingSleepAction();
    } else if (returnIdleAt == 0 && emotionAutoReturnAt == 0 && !manualSleep && !inCircadianSleep &&
               !sleepyFallAsleepIsActive() && !isKiwiHungry() && !isKiwiAngry()) {
      setEmotion(EMOTION_IDLE);
    }
    resyncTouch();
    if (!bargeInListen) {
      spkEndSpeech();
    }
    busyProcessing = false;
  }

  if (startRecording && !busyProcessing) {
    if (showClockScreen || showWeatherScreen) {
      showClockScreen = false;
      showWeatherScreen = false;
      returnIdleAt = 0;
    }
    powerNotifyActivity();
    extendTempWakeIfActive(CIRCADIAN_TEMP_WAKE_MS);
    startRecording = false;
    busyProcessing = true;
    lastProgressAt = millis();
    cancelRequest = false;
    robotStateDesc = "Listening...";
    addTerminalLog("[REC] Listening for up to " + String(RECORD_SECONDS) +
                   " seconds; stops after speech ends...");

    setEmotion(EMOTION_LISTENING);
    pumpUI();

    if (cancelRequest) {
      Serial.println("[ROBOT] Cancelled before recording");
      preWakeReset();
      setEmotion(EMOTION_IDLE);
      robotStateDesc = "Ready";
      resyncTouch();
      busyProcessing = false;
    } else if (WiFi.status() != WL_CONNECTED) {
      preWakeReset();
      handleOfflineBuddyTap();
    } else {
      SttCfg cfg = resolveStt();
      if (!cfg.key.length()) {
        robotStateDesc = "No STT API Key";
        addTerminalLog("[REC] Error: No STT API Key configured");
        Serial.println("[ROBOT] Top tap: No STT API Key configured!");
        preWakeReset();
        setEmotion(EMOTION_CONFUSED);
        showStatus("No STT API Key", "Add in Web Portal", 2500);
        setEmotion(EMOTION_IDLE);
        resyncTouch();
        busyProcessing = false;
      } else {
        robotStateDesc = "Listening...";
        addTerminalLog("[STT] Listening with " + sttProvider +
                       " (" + cfg.model + ")...");
        String question = recordAndTranscribe(RECORD_SECONDS);
        if (!cancelRequest && question.length()) {
          question = cleanTranscribedQuestion(question);
          lastTranscribedQuestion = question;
          addTerminalLog("[STT] User: \"" + question + "\"");
          handleQuestion(question);
        } else if (!cancelRequest) {
          robotStateDesc = "STT: " + sttFail;
          addTerminalLog("[STT] Speech not recognized: " + sttFail);
          Serial.printf("[ROBOT] STT failed: %s\n", sttFail.c_str());
          if (sttFail == "quiet") {
            // Siri / Alexa style: user didn't speak within timeout window, smoothly return to idle
            setEmotion(EMOTION_IDLE);
            robotStateDesc = "Ready";
          } else if (sttFail == "empty") {
            setEmotion(EMOTION_CONFUSED);
            speakText("Maine kuch suna, par samajh nahi aaya. Firse bolo?");
            returnIdleAt = millis() + 2500;
          } else {
            setEmotion(EMOTION_CONFUSED);
            speakText("Awaaz theek se nahi aayi, ek baar firse try karo!");
            returnIdleAt = millis() + 2000;
          }
        }
        bool wasCancelled = cancelRequest;
        bool bargeInListen = startRecording;
        cancelRequest = false;
        if (wasCancelled) {
          pendingSleepAction = false;
          returnIdleAt = 0;
          wakeWordReset();
          if (bargeInListen) {
            toneStop();
            setEmotion(EMOTION_LISTENING);
            robotStateDesc = "Listening...";
            Serial.println("[ROBOT] Barge-in during speech -> Immediate listening");
          } else {
            setEmotion(EMOTION_IDLE);
            robotStateDesc = "Ready";
            toneStop();
            spkEndSpeech();
            Serial.println("[ROBOT] Cancelled - returning to Ready");
          }
        } else if (pendingSleepAction) {
          triggerPendingSleepAction();
        } else if (returnIdleAt == 0 && emotionAutoReturnAt == 0 && !manualSleep && !inCircadianSleep &&
                   !sleepyFallAsleepIsActive() && !isKiwiHungry() && !isKiwiAngry()) {
          setEmotion(EMOTION_IDLE);
        }
        resyncTouch();
        if (!bargeInListen) {
          spkEndSpeech();
        }
        busyProcessing = false;
      }
    }
  }

  // 30s Inactivity Watchdog (never interrupt an active firmware flash)
  if (busyProcessing && !otaInProgress) {
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

  if (returnIdleAt != 0 && (int32_t)(millis() - returnIdleAt) >= 0 && !busyProcessing) {
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

  // Serial console commands for live wake word tuning & diagnostics
  while (Serial.available()) {
    static String s_serialCmdBuf = "";
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      s_serialCmdBuf.trim();
      if (s_serialCmdBuf.length() > 0) {
        wakeWordProcessSerialCommand(s_serialCmdBuf);
        s_serialCmdBuf = "";
      }
    } else if (s_serialCmdBuf.length() < 128) {
      s_serialCmdBuf += c;
    }
  }

  if (!busyProcessing && !startRecording && !isSpeechPlaying() &&
      !otaInProgress) {
    vTaskDelay(pdMS_TO_TICKS(2));
  } else {
    yield();
  }
}
