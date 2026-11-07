#include "behavior/CircadianEngine.h"
#include "config/AppConfig.h"
#include "face/FaceEngine.h"
#include "face/FaceDisplay.h"
#include "face/SleepyWakeAnim.h"
#include "hardware/AudioEngine.h"
#include "behavior/ActionExecutor.h"

void checkCircadianSleep() {
  if (manualSleep) {
    if (currentEmotion != EMOTION_SLEEPING && returnIdleAt == 0 &&
        emotionAutoReturnAt == 0 && !showClockScreen && !showWeatherScreen &&
        !sleepyFallAsleepIsActive() && !sleepyWakeIsActive()) {
      setEmotion(EMOTION_SLEEPING);
      updateDisplayBrightnessMode();
    }
    return;
  }
  if (!circadianSleepEnabled || !timeSynced)
    return;
  if (busyProcessing || isRobotTimerActive() || isRobotTimerRinging())
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
      return; // Temporarily awake due to user touch interaction (stays at full brightness)
    circadianTempWakeUntil = 0;
    updateDisplayBrightnessMode(); // Temporary wake timeout reached, restore night dimming
  }

  if (isSleepTime) {
    if (!inCircadianSleep) {
      if (sleepyWakeIsActive()) {
        return; // Do NOT cancel an active wake-up animation! Let Kiwi finish waking up!
      }
      inCircadianSleep = true;
      sleepyWakeCancel();
      sleepyFallAsleepStart();
      Serial.printf("[CIRCADIAN] Bedtime reached (%02d:%02d). Entering night sleep mode.\n",
                    curHour, timeinfo.tm_min);
    } else {
      if (currentEmotion != EMOTION_SLEEPING && returnIdleAt == 0 &&
          emotionAutoReturnAt == 0 && !showClockScreen && !showWeatherScreen &&
          !sleepyFallAsleepIsActive() && !sleepyWakeIsActive()) {
        setEmotion(EMOTION_SLEEPING);
        updateDisplayBrightnessMode();
      }
    }
  } else {
    if (inCircadianSleep) {
      inCircadianSleep = false;
      sleepyFallAsleepCancel();
      updateDisplayBrightnessMode();
      sleepyWakeStart();
      Serial.printf("[CIRCADIAN] Good morning! (%02d:%02d). Waking up.\n",
                    curHour, timeinfo.tm_min);
    }
  }
}
