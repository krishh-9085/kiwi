#include "behavior/CircadianEngine.h"
#include "config/AppConfig.h"
#include "commands/ActionExecutor.h"
#include "face/FaceDisplay.h"
#include "face/FaceEngine.h"
#include "face/SleepyWakeAnim.h"
#include "hardware/AudioEngine.h"

static bool s_prevInWindow = false;
static bool s_prevInWindowValid = false;

static int startTotalMinutes() {
  return circadianSleepStart * 60 + circadianSleepStartMin;
}

static int endTotalMinutes() {
  return circadianSleepEnd * 60 + circadianSleepEndMin;
}

bool isCircadianTempWakeActive() {
  if (circadianTempWakeUntil == 0)
    return false;
  return (int32_t)(circadianTempWakeUntil - millis()) > 0;
}

bool isCircadianScheduleActiveNow() {
  if (!circadianSleepEnabled || !timeSynced)
    return false;
  int startM = startTotalMinutes();
  int endM = endTotalMinutes();
  if (startM == endM)
    return false;

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 10))
    return false;
  int curM = timeinfo.tm_hour * 60 + timeinfo.tm_min;
  if (startM > endM) {
    return (curM >= startM || curM < endM);
  }
  return (curM >= startM && curM < endM);
}

int computeSecondsUntilScheduleWake() {
  if (!circadianSleepEnabled || !timeSynced)
    return 0;
  int startM = startTotalMinutes();
  int endM = endTotalMinutes();
  if (startM == endM)
    return 0;

  struct tm ti;
  if (!getLocalTime(&ti, 50)) {
    return 60;
  }
  int curSec = ti.tm_hour * 3600 + ti.tm_min * 60 + ti.tm_sec;
  int endSec = endM * 60;
  int secUntilEnd = (endSec <= curSec) ? ((24 * 3600 - curSec) + endSec)
                                       : (endSec - curSec);
  if (secUntilEnd < 60)
    secUntilEnd = 60;
  if (secUntilEnd > 12 * 3600)
    secUntilEnd = 12 * 3600;
  return secUntilEnd;
}

void startManualSleep() {
  manualSleep = true;
  inCircadianSleep = false;
  circadianSkipTonight = false;
  circadianTempWakeUntil = 0;
  showClockScreen = false;
  showWeatherScreen = false;
  bannerActive = false;
  emotionAutoReturnAt = 0;
  returnIdleAt = 0;
  sleepyWakeCancel();
  sleepyFallAsleepStart();
}

void wakeKiwiFromSleep(bool skipScheduleTonight, uint32_t tempWakeMs,
                       bool playWakeAnim) {
  manualSleep = false;
  inCircadianSleep = false;
  if (skipScheduleTonight && isCircadianScheduleActiveNow()) {
    circadianSkipTonight = true;
  }
  if (tempWakeMs > 0) {
    uint32_t newUntil = millis() + tempWakeMs;
    if (newUntil == 0)
      newUntil = 1;
    circadianTempWakeUntil = newUntil;
  }
  sleepyFallAsleepCancel();
  setDisplayPower(true);
  setOledDim(false);
  updateDisplayBrightnessMode();
  setEmotion(EMOTION_IDLE);
  emotionAutoReturnAt = 0;
  returnIdleAt = 0;
  if (playWakeAnim) {
    sleepyWakeStart();
  }
}

void extendTempWakeIfActive(uint32_t extraMs) {
  if (circadianTempWakeUntil == 0)
    return;
  uint32_t now = millis();
  if ((int32_t)(circadianTempWakeUntil - now) > 0) {
    uint32_t newUntil = now + extraMs;
    if (newUntil == 0)
      newUntil = 1;
    if ((int32_t)(newUntil - circadianTempWakeUntil) > 0) {
      circadianTempWakeUntil = newUntil;
    }
  }
}

bool parseScheduleTimeString(const String &raw, int &outHour, int &outMin) {
  String v = raw;
  v.trim();
  if (v.length() == 0)
    return false;
  int colon = v.indexOf(':');
  int h = -1;
  int m = outMin;
  if (colon > 0) {
    h = v.substring(0, colon).toInt();
    m = v.substring(colon + 1).toInt();
  } else {
    h = v.toInt();
  }
  if (h < 0 || h > 23)
    return false;
  if (m < 0 || m > 59)
    m = 0;
  outHour = h;
  outMin = m;
  return true;
}

void onCircadianScheduleChanged() {
  circadianSkipTonight = false;
  circadianTempWakeUntil = 0;
  s_prevInWindowValid = false;

  if (!circadianSleepEnabled) {
    if (inCircadianSleep && !manualSleep) {
      wakeKiwiFromSleep(false, 0, true);
    } else {
      updateDisplayBrightnessMode();
    }
    return;
  }

  checkCircadianSleep();
  updateDisplayBrightnessMode();
}

void checkCircadianSleep() {
  uint32_t now = millis();
  if (circadianTempWakeUntil != 0 &&
      (int32_t)(circadianTempWakeUntil - now) <= 0) {
    circadianTempWakeUntil = 0;
    updateDisplayBrightnessMode();
  }

  bool isSleepTime = isCircadianScheduleActiveNow();
  if (s_prevInWindowValid && s_prevInWindow && !isSleepTime) {
    // Exited sleep window (Good morning edge): reset tonight-override and also
    // let an overnight manual sleep wake up cleanly on schedule.
    circadianSkipTonight = false;
    if (manualSleep) {
      Serial.println("[CIRCADIAN] Wake At time reached during manual sleep -> Morning wake!");
      wakeKiwiFromSleep(false, 0, true);
      s_prevInWindow = isSleepTime;
      return;
    }
  }
  if (circadianSleepEnabled && timeSynced) {
    s_prevInWindow = isSleepTime;
    s_prevInWindowValid = true;
  }

  if (manualSleep) {
    if (currentEmotion != EMOTION_SLEEPING && returnIdleAt == 0 &&
        emotionAutoReturnAt == 0 && !showClockScreen && !showWeatherScreen &&
        !sleepyFallAsleepIsActive() && !sleepyWakeIsActive()) {
      setEmotion(EMOTION_SLEEPING);
      updateDisplayBrightnessMode();
    }
    return;
  }

  if (!circadianSleepEnabled || !timeSynced) {
    if (inCircadianSleep) {
      wakeKiwiFromSleep(false, 0, true);
    }
    return;
  }

  if (busyProcessing || isRobotTimerActive() || isRobotTimerRinging())
    return;

  if (isCircadianTempWakeActive() || circadianSkipTonight) {
    return;
  }

  if (isSleepTime) {
    if (!inCircadianSleep) {
      if (sleepyWakeIsActive()) {
        return;
      }
      inCircadianSleep = true;
      showClockScreen = false;
      showWeatherScreen = false;
      bannerActive = false;
      sleepyWakeCancel();
      sleepyFallAsleepStart();
      struct tm timeinfo;
      if (getLocalTime(&timeinfo, 5)) {
        Serial.printf("[CIRCADIAN] Bedtime reached (%02d:%02d). Entering sleep mode.\n",
                      timeinfo.tm_hour, timeinfo.tm_min);
      }
    } else if (currentEmotion != EMOTION_SLEEPING && returnIdleAt == 0 &&
               emotionAutoReturnAt == 0 && !showClockScreen &&
               !showWeatherScreen && !sleepyFallAsleepIsActive() &&
               !sleepyWakeIsActive()) {
      setEmotion(EMOTION_SLEEPING);
      updateDisplayBrightnessMode();
    }
  } else {
    circadianSkipTonight = false;
    if (inCircadianSleep) {
      inCircadianSleep = false;
      sleepyFallAsleepCancel();
      updateDisplayBrightnessMode();
      sleepyWakeStart();
      struct tm timeinfo;
      if (getLocalTime(&timeinfo, 5)) {
        Serial.printf("[CIRCADIAN] Good morning! (%02d:%02d). Waking up.\n",
                      timeinfo.tm_hour, timeinfo.tm_min);
      }
    }
  }
}
