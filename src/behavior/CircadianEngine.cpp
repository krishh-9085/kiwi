#include "behavior/CircadianEngine.h"
#include "config/AppConfig.h"
#include "face/FaceEngine.h"
#include "hardware/AudioEngine.h"

void checkCircadianSleep() {
  if (manualSleep) {
    if (currentEmotion != EMOTION_SLEEPING && returnIdleAt == 0 &&
        emotionAutoReturnAt == 0 && !showClockScreen && !showWeatherScreen) {
      setEmotion(EMOTION_SLEEPING);
    }
    return;
  }
  if (!circadianSleepEnabled || !timeSynced)
    return;
  if (busyProcessing)
    return;

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 10))
    return;

  int curHour = timeinfo.tm_hour;
  bool isSleepTime = false;
  if (circadianSleepStart > circadianSleepEnd) {
    isSleepTime = (curHour >= circadianSleepStart || curHour < circadianSleepEnd);
  } else {
    isSleepTime = (curHour >= circadianSleepStart && curHour < circadianSleepEnd);
  }

  uint32_t now = millis();
  if (circadianTempWakeUntil > 0) {
    if (now < circadianTempWakeUntil)
      return; // Temporarily awake due to user touch interaction
    circadianTempWakeUntil = 0;
  }

  if (isSleepTime) {
    if (!inCircadianSleep) {
      inCircadianSleep = true;
      setEmotion(EMOTION_SLEEPING);
      setOledDim(true);
      Serial.printf("[CIRCADIAN] Bedtime reached (%02d:%02d). Entering night sleep mode.\n",
                    curHour, timeinfo.tm_min);
    } else {
      if (currentEmotion != EMOTION_SLEEPING && returnIdleAt == 0 &&
          emotionAutoReturnAt == 0 && !showClockScreen && !showWeatherScreen) {
        setEmotion(EMOTION_SLEEPING);
      }
    }
  } else {
    if (inCircadianSleep) {
      inCircadianSleep = false;
      setOledDim(false);
      setEmotion(EMOTION_HAPPY);
      playBootChime();
      Serial.printf("[CIRCADIAN] Good morning! (%02d:%02d). Waking up.\n",
                    curHour, timeinfo.tm_min);
    }
  }
}
