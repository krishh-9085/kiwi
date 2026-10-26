#include "behavior/ActionExecutor.h"
#include "config/AppConfig.h"
#include "config/ConfigManager.h"
#include "core/CoreTypes.h"
#include "face/FaceEngine.h"
#include "face/FaceExpressions.h"
#include "face/FaceScreens.h"
#include "hardware/AudioEngine.h"
#include "services/AiService.h"
#include "behavior/CommandHandler.h"
#include "behavior/InteractionManager.h"
#include <WiFi.h>

// ============================================================
// COUNTDOWN TIMER STATE
// ============================================================
static bool     robotTimerActive = false;
static uint32_t robotTimerEndMs = 0;
static uint32_t robotTimerTotalSec = 0;
static String   robotTimerLabel = "Timer";

bool isRobotTimerActive() {
  return robotTimerActive;
}

uint32_t getRobotTimerRemainingSec() {
  if (!robotTimerActive) return 0;
  uint32_t now = millis();
  if (now >= robotTimerEndMs) return 0;
  return (robotTimerEndMs - now) / 1000;
}

void cancelRobotTimer() {
  if (robotTimerActive) {
    robotTimerActive = false;
    Serial.println("[TIMER] Cancelled");
    addTerminalLog("[TIMER] Timer cancelled");
  }
}

void startRobotTimer(uint32_t seconds, const String &label) {
  if (seconds == 0) return;
  robotTimerActive = true;
  robotTimerEndMs = millis() + (seconds * 1000UL);
  robotTimerTotalSec = seconds;
  robotTimerLabel = (label.length() > 0) ? label : "Timer";

  Serial.printf("[TIMER] Started: %u seconds (%s)\n", (unsigned)seconds, robotTimerLabel.c_str());
  soundPetSparkle();

  String desc;
  if (seconds >= 60) {
    desc = String(seconds / 60) + "m " + String(seconds % 60) + "s";
  } else {
    desc = String(seconds) + " seconds";
  }
  showStatus("TIMER STARTED!", desc.c_str(), 1800);
}

void checkRobotTimer() {
  if (!robotTimerActive)
    return;

  if (millis() >= robotTimerEndMs) {
    robotTimerActive = false;
    Serial.printf("[TIMER] Done! Label='%s'\n", robotTimerLabel.c_str());
    addTerminalLog("[TIMER] Done: " + robotTimerLabel);

    setEmotion(EMOTION_PARTY);
    soundPetPlayful();
    showStatus("TIME IS UP!", robotTimerLabel.c_str(), 3500);

    if (WiFi.status() == WL_CONNECTED) {
      speakText("Time is up! Your timer for " + robotTimerLabel + " is done!");
    } else {
      soundPetPlayful();
    }
    setEmotion(EMOTION_HAPPY);
    returnIdleAt = millis() + 2500;
  }
}

// ============================================================
// SKIN AND SOUND RESOLVERS
// ============================================================
bool setEyeSkinByName(const String &nameIn) {
  String n = nameIn;
  n.toLowerCase();
  n.trim();

  if (n == "kiwi" || n == "buddy" || n == "desk buddy" || n == "deskbuddy" || n == "कीवी") {
    currentEyeSkin = SKIN_KIWI; return true;
  }
  if (n == "cyberpunk" || n == "cyber" || n == "hud" || n == "sci-fi" || n == "साइबरपंक" || n == "साइबर") {
    currentEyeSkin = SKIN_CYBERPUNK; return true;
  }
  if (n == "cat" || n == "neko" || n == "kitty" || n == "billi" || n == "कैट" || n == "बिल्ली") {
    currentEyeSkin = SKIN_CAT; return true;
  }
  if (n == "retro8bit" || n == "retro" || n == "8bit" || n == "8-bit" || n == "pixel" || n == "रेट्रो") {
    currentEyeSkin = SKIN_RETRO8BIT; return true;
  }
  if (n == "anime" || n == "star" || n == "starry" || n == "shounen" || n == "एनिमे" || n == "एनीमे") {
    currentEyeSkin = SKIN_ANIME; return true;
  }
  if (n == "hearts" || n == "heart" || n == "dil" || n == "love" || n == "हार्ट" || n == "दिल") {
    currentEyeSkin = SKIN_HEARTS; return true;
  }
  if (n == "steampunk" || n == "goggles" || n == "aviator" || n == "स्टीमपंक") {
    currentEyeSkin = SKIN_STEAMPUNK; return true;
  }
  if (n == "demon" || n == "devil" || n == "horns" || n == "shaitan" || n == "डेमन" || n == "शैतान") {
    currentEyeSkin = SKIN_DEMON; return true;
  }
  if (n == "matrix" || n == "code" || n == "hacker" || n == "terminal" || n == "मैट्रिक्स") {
    currentEyeSkin = SKIN_MATRIX; return true;
  }
  if (n == "cyborg" || n == "t800" || n == "t-800" || n == "terminator" || n == "साइबोर्ग") {
    currentEyeSkin = SKIN_CYBORG; return true;
  }
  if (n == "mecha" || n == "gundam" || n == "v-fin" || n == "मेका") {
    currentEyeSkin = SKIN_MECHA; return true;
  }
  if (n == "sunglasses" || n == "shades" || n == "cool" || n == "sunglass" || n == "boss" || n == "चश्मा") {
    currentEyeSkin = SKIN_SUNGLASSES; return true;
  }
  if (n == "puppy" || n == "dog" || n == "doggy" || n == "kutta" || n == "पपी" || n == "कुत्ता") {
    currentEyeSkin = SKIN_PUPPY; return true;
  }
  if (n == "hypno" || n == "spiral" || n == "vortex" || n == "dizzy" || n == "हिप्नो") {
    currentEyeSkin = SKIN_HYPNO; return true;
  }
  if (n == "alien" || n == "ufo" || n == "cosmic" || n == "एलियन") {
    currentEyeSkin = SKIN_ALIEN; return true;
  }
  if (n == "ninja" || n == "shinobi" || n == "naruto" || n == "निंजा") {
    currentEyeSkin = SKIN_NINJA; return true;
  }
  if (n == "round" || n == "circle" || n == "gol" || n == "गोल") {
    currentEyeSkin = SKIN_ROUND; return true;
  }
  if (n == "default" || n == "normal" || n == "clean" || n == "reset" || n == "डिफ़ॉल्ट" || n == "डिफॉल्ट") {
    currentEyeSkin = SKIN_DEFAULT; return true;
  }
  return false;
}

void triggerSoundByName(const String &nameIn) {
  String s = nameIn;
  s.toLowerCase();
  s.trim();

  if (s == "chime" || s == "boot")           playBootChime();
  else if (s == "sparkle" || s == "magic")   soundPetSparkle();
  else if (s == "giggle" || s == "laugh")    soundPetGiggle();
  else if (s == "kiss" || s == "love")       soundPetKiss();
  else if (s == "playful" || s == "party")   soundPetPlayful();
  else if (s == "sleepy" || s == "snore")    soundPetSleepy();
  else if (s == "proud" || s == "fanfare")   soundPetProud();
  else if (s == "cuddle")                    soundPetCuddle();
  else if (s == "shy")                       soundPetShy();
  else if (s == "error" || s == "alert")     soundError();
  else                                       soundPetSparkle();
}

// ============================================================
// SINGLE ACTION EXECUTOR
// ============================================================
bool executeRobotAction(const String &actionCmd) {
  String cmd = actionCmd;
  cmd.trim();
  int spaceIdx = cmd.indexOf(' ');
  String actionType = (spaceIdx > 0) ? cmd.substring(0, spaceIdx) : cmd;
  String param = (spaceIdx > 0) ? cmd.substring(spaceIdx + 1) : "";
  actionType.toLowerCase();
  param.trim();

  Serial.printf("[ACTION] Dispatching: type='%s' param='%s'\n", actionType.c_str(), param.c_str());

  // 1. VOLUME CONTROL
  if (actionType == "volume" || actionType == "vol" || actionType == "soundlevel") {
    int v = param.toInt();
    if (v < 0) v = 0;
    if (v > 100) v = 100;
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
  if (actionType == "timer" || actionType == "countdown") {
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
      addTerminalLog("[ACTION] Timer set for " + String(sec) + "s (" + lbl + ")");
      return true;
    }
    return false;
  }

  // 5. MARQUEE / BANNER SCROLL
  if (actionType == "marquee" || actionType == "banner" || actionType == "text") {
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
    manualSleep = true;
    inCircadianSleep = true;
    setOledDim(true);
    soundPetSleepy();
    addTerminalLog("[ACTION] Entered Sleep Mode");
    return true;
  }

  // 7. WAKE UP
  if (actionType == "wake" || actionType == "morning") {
    manualSleep = false;
    inCircadianSleep = false;
    setOledDim(false);
    setEmotion(EMOTION_HAPPY);
    soundPetSparkle();
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
        while (k < (int)text.length() && text[k] == ' ') k++;
        if (k + 7 <= (int)text.length() && text.substring(k, k + 7).equalsIgnoreCase("action:")) {
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
