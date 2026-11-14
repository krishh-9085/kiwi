#include "behavior/ActionExecutor.h"
#include "behavior/CommandHandler.h"
#include "behavior/InteractionManager.h"
#include "config/AppConfig.h"
#include "config/ConfigManager.h"
#include "core/CoreTypes.h"
#include "core/Timing.h"
#include "face/FaceDisplay.h"
#include "face/FaceEngine.h"
#include "face/FaceExpressions.h"
#include "face/FaceScreens.h"
#include "face/SleepyWakeAnim.h"
#include "hardware/AudioEngine.h"
#include "services/AiService.h"
#include "services/HyperHdrService.h"
#include "services/WledService.h"
#include <WiFi.h>

// ============================================================
// COUNTDOWN TIMER STATE
// ============================================================
bool pendingSleepAction = false;
static bool robotTimerActive = false;
static bool robotTimerRinging = false;
static uint32_t robotTimerEndMs = 0;
static uint32_t robotTimerTotalSec = 0;
static String robotTimerLabel = "Timer";
static uint32_t robotTimerRingingStartMs = 0;
static uint32_t robotTimerLastAlertMs = 0;
static uint16_t robotTimerAlertCount = 0;

bool isRobotTimerActive() { return robotTimerActive; }

bool isRobotTimerRinging() { return robotTimerRinging; }

uint32_t getRobotTimerRingingDurationMs() {
  if (!robotTimerRinging)
    return 0;
  return millis() - robotTimerRingingStartMs;
}

uint32_t getRobotTimerRemainingSec() {
  if (!robotTimerActive)
    return 0;
  uint32_t now = millis();
  if (now >= robotTimerEndMs)
    return 0;
  return (robotTimerEndMs - now) / 1000;
}

void stopRobotTimerAlarm() {
  if (robotTimerRinging) {
    robotTimerRinging = false;
    cancelRequest = true;
    spkEndSpeech();
    Serial.println("[TIMER] Alarm stopped by touch / command");
    addTerminalLog("[TIMER] Alarm stopped");
    soundPetSparkle();
    setEmotion(EMOTION_IDLE);
    returnIdleAt = 0;
  }
}

void cancelRobotTimer() {
  if (robotTimerActive || robotTimerRinging) {
    robotTimerActive = false;
    robotTimerRinging = false;
    cancelRequest = true;
    spkEndSpeech();
    Serial.println("[TIMER] Cancelled");
    addTerminalLog("[TIMER] Timer cancelled");
    soundPetSparkle();
    setEmotion(EMOTION_IDLE);
    returnIdleAt = 0;
  }
}

void startRobotTimer(uint32_t seconds, const String &label) {
  if (seconds == 0)
    return;
  if (robotTimerRinging) {
    stopRobotTimerAlarm();
  }
  robotTimerActive = true;
  robotTimerRinging = false;
  robotTimerEndMs = millis() + (seconds * 1000UL);
  robotTimerTotalSec = seconds;
  robotTimerLabel = (label.length() > 0) ? label : "Timer";

  Serial.printf("[TIMER] Started: %u seconds (%s)\n", (unsigned)seconds,
                robotTimerLabel.c_str());
  soundPetSparkle();

  // Natural response without interrupting OLED face or error-beeping
  setEmotion(EMOTION_IDLE);
  returnIdleAt = 0;
}

void checkRobotTimer() {
  if (!robotTimerActive && !robotTimerRinging)
    return;

  uint32_t now = millis();

  // 1. Transition from countdown to ringing alarm
  if (robotTimerActive && now >= robotTimerEndMs) {
    robotTimerActive = false;
    robotTimerRinging = true;
    robotTimerRingingStartMs = now;
    robotTimerLastAlertMs = 0; // Trigger first ring immediately
    robotTimerAlertCount = 0;
    Serial.printf("[TIMER] Expired! Ringing alarm on loop for '%s'...\n",
                  robotTimerLabel.c_str());
    addTerminalLog("[TIMER] Alarm ringing: " + robotTimerLabel);
  }

  // 2. Active alarm ringing loop (repeats until user touches ANY sensor)
  if (robotTimerRinging) {
    // Safety auto-dismiss after 5 minutes (300,000 ms) of unhandled ringing
    if (now - robotTimerRingingStartMs > 300000UL) {
      Serial.println("[TIMER] Alarm auto-dismissed after 5 min timeout.");
      stopRobotTimerAlarm();
      return;
    }

    // Cooldown interval between alert iterations (2500ms after previous
    // speech/chime)
    if (now - robotTimerLastAlertMs >= 2500UL) {
      robotTimerAlertCount++;

      // Visually alert on OLED (alternate party and surprised faces)
      if (robotTimerAlertCount % 2 == 1) {
        setEmotion(EMOTION_PARTY);
      } else {
        setEmotion(EMOTION_SURPRISED);
      }

      // Play fanfare / alert chime
      soundFanfare();

      // Wait briefly for chime while pumping UI so touch can interrupt
      // instantly
      uint32_t chimeStart = millis();
      while (isTonePlaying() && (millis() - chimeStart < 700)) {
        serviceUi();
        if (!robotTimerRinging)
          return;
        yield();
      }

      if (!robotTimerRinging)
        return;

      // Build natural alert speech based on language
      String alertMsg;
      bool isCustom =
          (robotTimerLabel != "Timer" && robotTimerLabel != "Focus Timer" &&
           robotTimerLabel.length() > 0);

      if (sttLang == "hi") {
        if (robotTimerAlertCount % 2 == 1) {
          alertMsg =
              isCustom
                  ? "Samay samapt! " + robotTimerLabel +
                        " poora ho gaya! Band karne ke liye mujhe touch karo!"
                  : "Samay samapt ho gaya! Band karne ke liye mujhe touch "
                    "karo!";
        } else {
          alertMsg =
              isCustom
                  ? "Time up! " + robotTimerLabel +
                        " khatam! Band karne ke liye sensor touch karo!"
                  : "Time up ho gaya! Band karne ke liye mujhe touch karo!";
        }
      } else {
        if (robotTimerAlertCount % 2 == 1) {
          alertMsg = isCustom
                         ? "Time is up! Your timer for " + robotTimerLabel +
                               " is done! Tap me to stop!"
                         : "Time is up! Your timer is done! Tap me to stop!";
        } else {
          alertMsg = isCustom
                         ? "Time is up for " + robotTimerLabel +
                               "! Tap any sensor to dismiss!"
                         : "Time is up! Tap any sensor to dismiss the alarm!";
        }
      }

      if (WiFi.status() == WL_CONNECTED) {
        cancelRequest = false;
        speakText(alertMsg);
        cancelRequest = false;
      } else {
        soundPetPlayful();
      }

      robotTimerLastAlertMs = millis();
    }
  }
}

// ============================================================
// SKIN AND SOUND RESOLVERS
// ============================================================
bool setEyeSkinByName(const String &nameIn) {
  String n = nameIn;
  n.toLowerCase();
  n.trim();

  // 1. Kiwi (Default / #1)
  if (n == "kiwi" || n == "desk buddy" || n == "deskbuddy" || n == "buddy" ||
      n == "default" || n == "cute" || n == "कीवी") {
    currentEyeSkin = SKIN_KIWI;
    return true;
  }
  // 2. Vector Classic (#2)
  if (n == "vector" || n == "vector classic" || n == "classic vector" ||
      n == "anki" || n == "classic" || n == "glow" || n == "वेक्टर") {
    currentEyeSkin = SKIN_VECTOR;
    return true;
  }
  // 3. Neon Capsule (from image)
  if (n == "capsule" || n == "neon capsule" || n == "pill" || n == "stadium" ||
      n == "neon" || n == "कैप्सूल") {
    currentEyeSkin = SKIN_CAPSULE;
    return true;
  }
  // 4. Pixel 8-Bit
  if (n == "pixel" || n == "pixel 8-bit" || n == "arcade" || n == "retro" ||
      n == "8bit" || n == "8-bit" || n == "पिक्सेल") {
    currentEyeSkin = SKIN_PIXEL;
    return true;
  }
  // 5. Squircle Bold
  if (n == "squircle bold" || n == "squircle solid" || n == "bold" ||
      n == "squircle" || n == "स्क्वार्कल") {
    currentEyeSkin = SKIN_SQUIRCLE_BOLD;
    return true;
  }
  // 6. Neon Frame
  if (n == "neon frame" || n == "squircle outline" || n == "frame" ||
      n == "outline") {
    currentEyeSkin = SKIN_NEON_FRAME;
    return true;
  }
  // 7. Lumina Spark
  if (n == "lumina spark" || n == "squircle glint" || n == "spark" ||
      n == "lumina" || n == "glint") {
    currentEyeSkin = SKIN_LUMINA_SPARK;
    return true;
  }

  // Gracefully fallback to Kiwi for any unrecognized or retired theme name
  currentEyeSkin = SKIN_KIWI;
  return true;
}

void triggerSoundByName(const String &nameIn) {
  String s = nameIn;
  s.toLowerCase();
  s.trim();

  if (s == "chime" || s == "boot")
    playBootChime();
  else if (s == "sparkle" || s == "magic")
    soundPetSparkle();
  else if (s == "giggle" || s == "laugh")
    soundPetGiggle();
  else if (s == "kiss" || s == "love")
    soundPetKiss();
  else if (s == "playful" || s == "party")
    soundPetPlayful();
  else if (s == "sleepy" || s == "snore")
    soundPetSleepy();
  else if (s == "proud" || s == "fanfare")
    soundPetProud();
  else if (s == "cuddle")
    soundPetCuddle();
  else if (s == "shy")
    soundPetShy();
  else if (s == "error" || s == "alert")
    soundError();
  else
    soundPetSparkle();
}

// ============================================================
// SINGLE ACTION EXECUTOR
// ============================================================
void triggerPendingSleepAction() {
  pendingSleepAction = false;
  manualSleep = true;
  inCircadianSleep = false;
  showClockScreen = false;
  showWeatherScreen = false;
  setEmotion(EMOTION_IDLE);
  emotionAutoReturnAt = 0;
  returnIdleAt = 0;
  sleepyWakeCancel();
  sleepyFallAsleepStart();
  addTerminalLog("[ACTION] Falling asleep...");
  Serial.println("[ACTION] Sleep mode activated (manualSleep=1, fall-asleep "
                 "animation running)");
}

bool executeRobotAction(const String &actionCmd) {
  String cmd = actionCmd;
  cmd.trim();
  int spaceIdx = cmd.indexOf(' ');
  String actionType = (spaceIdx > 0) ? cmd.substring(0, spaceIdx) : cmd;
  String param = (spaceIdx > 0) ? cmd.substring(spaceIdx + 1) : "";
  actionType.toLowerCase();
  param.trim();

  Serial.printf("[ACTION] Dispatching: type='%s' param='%s'\n",
                actionType.c_str(), param.c_str());

  // 1. VOLUME CONTROL
  if (actionType == "volume" || actionType == "vol" ||
      actionType == "soundlevel") {
    int v = param.toInt();
    if (v < 0)
      v = 0;
    if (v > 100)
      v = 100;
    speakerVolume = v;
    saveConfig();
    soundPetSparkle();
    addTerminalLog("[ACTION] Volume set to " + String(v) + "%");
    return true;
  }

  // 2. EYE SKIN CHANGE
  if (actionType == "skin" || actionType == "eyes" || actionType == "theme") {
    if (setEyeSkinByName(param)) {
      saveConfig();
      soundPetSparkle();
      addTerminalLog("[ACTION] Eye skin switched to: " + param);
      return true;
    }
    return false;
  }

  // 3. FACIAL EMOTION CHANGE
  if (actionType == "emotion" || actionType == "face" || actionType == "mood") {
    if (triggerEmotionByName(param, 4000, true)) {
      addTerminalLog("[ACTION] Facial emotion set to: " + param);
      return true;
    }
    return false;
  }

  // 4. COUNTDOWN TIMER
  if (actionType == "timer" || actionType == "countdown" ||
      actionType == "alarm") {
    if (param == "cancel" || param == "stop" || param == "off" ||
        param == "dismiss") {
      cancelRobotTimer();
      stopRobotTimerAlarm();
      return true;
    }
    int pSpace = param.indexOf(' ');
    uint32_t sec = 0;
    String lbl = "Timer";
    if (pSpace > 0) {
      sec = (uint32_t)param.substring(0, pSpace).toInt();
      lbl = param.substring(pSpace + 1);
      lbl.trim();
    } else {
      sec = (uint32_t)param.toInt();
    }
    if (sec > 0) {
      startRobotTimer(sec, lbl);
      addTerminalLog("[ACTION] Timer set for " + String(sec) + "s (" + lbl +
                     ")");
      return true;
    }
    return false;
  }

  // 5. MARQUEE / BANNER SCROLL
  if (actionType == "marquee" || actionType == "banner" ||
      actionType == "text") {
    if (param.length() > 0) {
      bannerText = param;
      bannerScrollX = SCREEN_WIDTH + 10;
      bannerLoops = 0;
      bannerActive = true;
      soundPetSparkle();
      addTerminalLog("[ACTION] Marquee banner: \"" + param + "\"");
      return true;
    }
    return false;
  }

  // 6. SLEEP MODE
  if (actionType == "sleep" || actionType == "night") {
    if (kiwiIsOutputting() || busyProcessing) {
      pendingSleepAction = true;
      Serial.println("[ACTION] Speech in progress -> Queuing sleep action "
                     "until speech ends");
      return true;
    }
    triggerPendingSleepAction();
    return true;
  }

  // 7. WAKE UP
  if (actionType == "wake" || actionType == "morning") {
    manualSleep = false;
    inCircadianSleep = false;
    sleepyFallAsleepCancel();
    setOledDim(false);
    updateDisplayBrightnessMode();
    sleepyWakeStart();
    addTerminalLog("[ACTION] Woke up from Sleep Mode");
    return true;
  }

  // 8. WEATHER DISPLAY
  if (actionType == "weather" || actionType == "forecast") {
    handleWeatherCommand("weather");
    addTerminalLog("[ACTION] Displayed weather");
    return true;
  }

  // 9. TIME / CLOCK DISPLAY
  if (actionType == "time" || actionType == "clock") {
    handleTimeCommand("time");
    addTerminalLog("[ACTION] Displayed clock");
    return true;
  }

  // 11. WLED & HYPERHDR SMART LIGHT CONTROL
  if (actionType == "light" || actionType == "wled" || actionType == "lamp" ||
      actionType == "ambilight" || actionType == "hyperhdr" ||
      actionType == "screen_sync") {
    if (executeUnifiedLightAction(param)) {
      soundPetSparkle();
      addTerminalLog("[ACTION] Light control: " + param);
      return true;
    }
    return false;
  }

  // 10. SOUND FX
  if (actionType == "sound" || actionType == "sfx") {
    triggerSoundByName(param);
    addTerminalLog("[ACTION] Played sound: " + param);
    return true;
  }

  return false;
}

int lastActionsExecuted = 0;

// ============================================================
// SCAN, EXECUTE AND STRIP [ACTION: ...] TAGS FROM LLM
// ============================================================
int parseAndExecuteActions(String &text) {
  int actionCount = 0;
  while (true) {
    int start = -1;
    for (int i = 0; i < (int)text.length(); i++) {
      if (text[i] == '[') {
        int k = i + 1;
        while (k < (int)text.length() && text[k] == ' ')
          k++;
        if (k + 7 <= (int)text.length() &&
            text.substring(k, k + 7).equalsIgnoreCase("action:")) {
          start = i;
          break;
        }
      }
    }
    if (start < 0)
      break;
    int end = text.indexOf(']', start);
    if (end < 0)
      break;

    int colon = text.indexOf(':', start);
    if (colon > start && colon < end) {
      String actionCmd = text.substring(colon + 1, end);
      actionCmd.trim();
      if (actionCmd.length() > 0) {
        if (executeRobotAction(actionCmd)) {
          actionCount++;
        }
      }
    }
    text.remove(start, (end - start) + 1);
  }
  text.trim();
  lastActionsExecuted = actionCount;
  return actionCount;
}
