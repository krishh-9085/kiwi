/*
 ==============================================================================
  MyRobot (Kiwi) - Modular v4.6
  ESP32-C3 Super Mini / PlatformIO / Arduino Core 3.x
  Tools -> USB CDC On Boot = Enabled

  Architecture & Modules:
  - config/AppConfig.h / .cpp      : Hardware pinouts, constants, shared globals
  - core/CoreTypes.h               : Data structures, emotion & mood enums
  - hardware/Battery.h / .cpp      : Non-linear LiPo battery sensing curve
  - config/ConfigManager.h / .cpp  : NVS Preferences persistent storage
  - hardware/AudioEngine.h / .cpp  : I2S driver, Tone player, DSP & soft-clipping
  - face/FaceEngine.h / .cpp       : OLED procedural vector face engine & screens
  - services/WeatherService.h/.cpp : Open-Meteo REST API live weather client
  - behavior/CircadianEngine.h/.cpp: Circadian night sleep mode & auto-dimming
  - behavior/BoredomBehavior.h/.cpp: Autonomous idle remarks & desk banter
  - behavior/CommandHandler.h/.cpp : Voice command parsing & dispatch
  - behavior/InteractionManager.h/.: User interaction pipeline & UI pump
  - services/AiService.h / .cpp    : STT, LLM (Gemini/Groq), streaming TTS clients
  - hardware/TouchController.h/.cpp: Capacitive & digital touch sensor gestures
  - web/WebPortal.h / .cpp         : Captive SoftAP, settings UI, Web OTA updates
 ==============================================================================
*/

#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include "config/AppConfig.h"
#include "core/CoreTypes.h"
#include "config/ConfigManager.h"
#include "hardware/AudioEngine.h"
#include "animation/AnimationEngine.h"
#include "face/FaceEngine.h"
#include "services/WeatherService.h"
#include "behavior/CircadianEngine.h"
#include "behavior/BoredomBehavior.h"
#include "behavior/CommandHandler.h"
#include "behavior/InteractionManager.h"
#include "services/AiService.h"
#include "hardware/TouchController.h"
#include "web/WebPortal.h"
#include "services/MemoryService.h"
#include "behavior/ActionExecutor.h"
#include "behavior/OfflineBrain.h"

// ============================================================
// MAIN SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
#if ARDUINO_USB_CDC_ON_BOOT
  uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 300) { delay(10); }
#else
  delay(50);
#endif
  Serial.println();
  Serial.println("==========================================");
  Serial.println("  MYROBOT (KIWI) - MODULAR ARCHITECTURE   ");
  Serial.println("==========================================");

  randomSeed(esp_random());
  initBase64Lookup();

  if (!initDisplay()) {
    Serial.println("[OLED] FAILED to initialize");
  } else {
    Serial.println("[OLED] Display ready");
  }
  initAnimations();

  initTouch();
  loadConfig();
  resetBoredomInterval();
  initMemory();

  Serial.printf("[LLM] %s / %s\n", llmProvider.c_str(),
                llmProvider == "groq" ? groqModel.c_str() : llmModel.c_str());
  Serial.printf("[STT] %s / %s\n", sttProvider.c_str(), sttLang.c_str());
  Serial.printf("[TTS] %s\n", ttsProvider.c_str());
  Serial.printf("[VOL] %d%%\n", speakerVolume);

  eyesInit(millis());
  setEmotion(EMOTION_IDLE);
  drawConnectingFrame(wifiSSID.c_str(), 0);

  bool connected = false;
  if (wifiSSID.length())
    connected = connectHome();
  if (connected) {
    startAPParallel();
    startNtpAsync();
    lastNtpTryAt = millis();
    lastWeatherFetchAt = 0; // Handled asynchronously in maintainWiFi() during main loop
  } else {
    startAP();
  }

  if (MDNS.begin("myrobot")) {
    MDNS.addService("http", "tcp", 80);
    Serial.println("[MDNS] http://myrobot.local");
  }

  initOfflineBrain();
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
}

// ============================================================
// MAIN EVENT LOOP
// ============================================================
void loop() {
  pumpWeb();
  maintainWiFi();

  checkCircadianSleep();
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
    cancelRequest = false;
    lastTranscribedQuestion = q;
    addTerminalLog("[WEB] Prompt received: \"" + q + "\"");
    setEmotion(EMOTION_THINKING);
    handleQuestion(q);
    bool wasCancelled = cancelRequest;
    cancelRequest = false;
    setEmotion(EMOTION_IDLE);
    resyncTouch();
    busyProcessing = false;
    if (wasCancelled) {
      startRecording = true;
    }
  }

  if (startRecording && !busyProcessing) {
    startRecording = false;
    busyProcessing = true;
    cancelRequest = false;
    robotStateDesc = "Listening...";
    addTerminalLog("[REC] Listening for up to " + String(RECORD_SECONDS) +
                   " seconds; stops after speech ends...");

    setEmotion(EMOTION_LISTENING);

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
    addTerminalLog("[STT] Transcribing captured audio with " + sttProvider + " (" + cfg.model + ")...");
    String question = recordAndTranscribe(RECORD_SECONDS);
    if (!cancelRequest && question.length()) {
      lastTranscribedQuestion = question;
      addTerminalLog("[STT] User: \"" + question + "\"");
      handleQuestion(question);
    } else if (!cancelRequest) {
      robotStateDesc = "STT: " + sttFail;
      addTerminalLog("[STT] Speech not recognized: " + sttFail);
      Serial.printf("[ROBOT] STT failed: %s\n", sttFail.c_str());
      if (sttFail == "quiet" || sttFail == "empty" || sttFail == "") {
        showStatus("Didn't hear you", "try again", 1300);
      } else {
        showStatus("STT error", sttFail.c_str(), 2500);
      }
    }
    bool wasCancelled = cancelRequest;
    cancelRequest = false;
    setEmotion(EMOTION_IDLE);
    resyncTouch();
    busyProcessing = false;
    if (wasCancelled) {
      Serial.println("[ROBOT] Cancelled - restarting listen");
      startRecording = true;
    }
  }

  if (returnIdleAt && millis() >= returnIdleAt && !busyProcessing) {
    returnIdleAt = 0;
    if (!showClockScreen && !showWeatherScreen)
      setEmotion(EMOTION_IDLE);
  }

  delay(1);
}
