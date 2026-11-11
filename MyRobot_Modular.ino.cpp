# 1 "C:\\Users\\rohil\\AppData\\Local\\Temp\\tmp5th32czx"
#include <Arduino.h>
# 1 "D:/PROJECTS/MyRobot_Modular/MyRobot_Modular.ino"
# 29 "D:/PROJECTS/MyRobot_Modular/MyRobot_Modular.ino"
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
#include "face/SleepyWakeAnim.h"
#include "hardware/AudioEngine.h"
#include "hardware/SoundLevel.h"
#include "hardware/TouchController.h"
#include "power/PowerManager.h"
#include "services/AiService.h"
#include "services/MemoryService.h"
#include "services/WeatherService.h"
#include "services/WledService.h"
#include "web/WebPortal.h"
#include <Arduino.h>
#include <ESPmDNS.h>
#include <WiFi.h>





static uint32_t bootStartAt = 0;
static bool bootWifiFinalizedEarly = false;
void setup();
static void updateBootFlow();
static String pickGreetingTemplate(const char *const *pool, int count,
                                   int &lastIdx);
static String buildBootGreeting(int hour, const String &ownerName);
static void checkBootGreeting();
void loop();
#line 67 "D:/PROJECTS/MyRobot_Modular/MyRobot_Modular.ino"
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

  randomSeed(esp_random());
  initBase64Lookup();
  loadConfig();
  powerInit();

  if (!initDisplay()) {
    Serial.println("[OLED] FAILED to initialize");
    bootCompleted = true;
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
#if BOOT_TIMING_LOG
  Serial.printf("[BOOT_T] after_startAP_or_connect t=%lu ms\n",
                (unsigned long)(millis() - bootSetupEnterAt));
#endif

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




static void updateBootFlow() {
  if (bootCompleted)
    return;

  uint32_t now = millis();
  uint32_t elapsed = now - bootStartAt;


  if (elapsed >= 5000UL) {
    Serial.println("[BOOT] Watchdog reached -> transition to normal face");
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
    return;
  }


  if (bootWakeTick()) {
    if (WiFi.status() == WL_CONNECTED && !bootWifiFinalizedEarly) {
      bootWifiFinalizedEarly = true;
      finalizeWifiConnected();
    }
    bootAnimPump();
    return;
  }


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


static String pickGreetingTemplate(const char *const *pool, int count,
                                   int &lastIdx) {
  int idx = random(count);
  if (count > 1 && idx == lastIdx) {
    idx = (idx + 1 + random(count - 1)) % count;
  }
  lastIdx = idx;
  return String(pool[idx]);
}


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

  String nameTok = ownerName.length() > 0 ? ", " + ownerName : String("");
  tpl.replace("{n}", nameTok);
  return tpl;
}




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
  if (busyProcessing || inCircadianSleep || manualSleep)
    return;


  if (isTonePlaying())
    return;



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

  String owner = getOwnerName();
  owner.trim();
  String speech = buildBootGreeting(hour, owner);

  Serial.printf("[BOOT] Greeting: %s (hour=%d)\n", speech.c_str(), hour);
  setEmotion(EMOTION_HAPPY);
  flourish = FL_STAR;
  flourishT0 = millis();

  if (WiFi.status() == WL_CONNECTED &&
      (ttsKey.length() || sttKey.length() || llmKey.length())) {
    speakText(speech);
  }
  setEmotion(EMOTION_IDLE);
}




void loop() {
  powerUpdate();
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
  soundLevelTick();
  tickFace();
  maybeChangeEmotion();
  updateTonePlayer();
  processPendingSound();

  if (pendingSleepAction && !busyProcessing && !kiwiIsOutputting()) {
    triggerPendingSleepAction();
  }

  if (webQueuedQuestion.length() > 0 && !busyProcessing) {
    powerNotifyActivity();
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
      pendingSleepAction = false;
      returnIdleAt = 0;
      setEmotion(EMOTION_IDLE);
      robotStateDesc = "Ready";
      Serial.println("[ROBOT] Web request cancelled");
    } else if (pendingSleepAction) {
      triggerPendingSleepAction();
    } else if (returnIdleAt == 0 && !manualSleep && !inCircadianSleep &&
               !sleepyFallAsleepIsActive()) {
      setEmotion(EMOTION_IDLE);
    }
    resyncTouch();
    spkEndSpeech();
    busyProcessing = false;
  }

  if (startRecording && !busyProcessing) {
    if (showClockScreen || showWeatherScreen) {
      showClockScreen = false;
      showWeatherScreen = false;
      startRecording = false;
      setEmotion(EMOTION_IDLE);
      returnIdleAt = 0;
      soundPetSparkle();
      resyncTouch();
      return;
    }
    powerNotifyActivity();
    startRecording = false;
    busyProcessing = true;
    lastProgressAt = millis();
    cancelRequest = false;
    robotStateDesc = "Listening...";
    addTerminalLog("[REC] Listening for up to " + String(RECORD_SECONDS) +
                   " seconds; stops after speech ends...");

    setEmotion(EMOTION_LISTENING);

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
      pendingSleepAction = false;
      returnIdleAt = 0;
      setEmotion(EMOTION_IDLE);
      robotStateDesc = "Ready";
      Serial.println("[ROBOT] Cancelled - returning to Ready");
    } else if (pendingSleepAction) {
      triggerPendingSleepAction();
    } else if (returnIdleAt == 0 && !manualSleep && !inCircadianSleep &&
               !sleepyFallAsleepIsActive()) {
      setEmotion(EMOTION_IDLE);
    }
    resyncTouch();
    spkEndSpeech();
    busyProcessing = false;
  }


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