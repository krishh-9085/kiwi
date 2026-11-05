#include "hardware/TouchController.h"
#include "config/AppConfig.h"
#include "config/ConfigManager.h"
#include "animation/AnimationEngine.h"
#include "hardware/AudioEngine.h"
#include "face/FaceEngine.h"
#include "services/AiService.h"
#include "services/WeatherService.h"
#include "face/FaceDisplay.h"
#include "behavior/ActionExecutor.h"
#include <WiFi.h>
#include <esp_sleep.h>

TouchSensor touchL = {PIN_TOUCH_LEFT, false, false, 0, 0, false, false, false, 0, false};
TouchSensor touchR = {PIN_TOUCH_RIGHT, false, false, 0, 0, false, false, false, 0, false};
TouchSensor touchT = {PIN_TOUCH_TOP, false, false, 0, 0, false, false, false, 0, false};

void triggerPetReaction();
bool syncTimeFromNTP();

// ---------- Tickle / Gudgudi Tracking ----------
static uint32_t lastCheekTouchAt = 0;
static uint8_t  cheekTapCount = 0;
static uint32_t lastTickleTriggeredAt = 0;

void triggerTickleMode() {
  if (busyProcessing)
    return;
  busyProcessing = true;
  lastTickleTriggeredAt = millis();
  lastUserInteractionAt = millis();
  cheekTapCount = 0;
  Serial.println("[TICKLE] Cheek tickle detected! Triggering Gudgudi mode!");
  setEmotion(EMOTION_LAUGHING);
  soundPetGiggle();

  static const char *const tickleReplies[] = {
      "Arey arey gudgudi ho rahi hai, chhodo mujhe!",
      "Haha bas karo bhai, bohot ticklish hoon main!",
      "Hehe haha! Cheeks mat chhedo, gudgudi lagti hai!"
  };
  int idx = random(0, 3);
  speakText(tickleReplies[idx]);
  setEmotion(EMOTION_HAPPY);
  returnIdleAt = millis() + 1500;
  resyncTouch();
  busyProcessing = false;
}


void initTouch() {
  pinMode(PIN_TOUCH_LEFT, INPUT_PULLDOWN);
  pinMode(PIN_TOUCH_RIGHT, INPUT_PULLDOWN);
  pinMode(PIN_TOUCH_TOP, INPUT_PULLDOWN);
  esp_deep_sleep_enable_gpio_wakeup(1ULL << PIN_TOUCH_TOP, ESP_GPIO_WAKEUP_GPIO_HIGH);
  touchL.armed = true;
  touchR.armed = true;
  touchT.armed = true;
}

void resyncTouch() {
  touchL.state = false;
  touchL.lastRaw = digitalRead(PIN_TOUCH_LEFT);
  touchL.holdHandled = false;
  touchL.sleepHandled = false;
  touchL.startedAt = 0;
  touchL.lastReleaseAt = 0;
  touchL.pendingSingleTap = false;

  touchR.state = false;
  touchR.lastRaw = digitalRead(PIN_TOUCH_RIGHT);
  touchR.holdHandled = false;
  touchR.sleepHandled = false;
  touchR.startedAt = 0;
  touchR.lastReleaseAt = 0;
  touchR.pendingSingleTap = false;

  touchT.state = false;
  touchT.lastRaw = digitalRead(PIN_TOUCH_TOP);
  touchT.holdHandled = false;
  touchT.sleepHandled = false;
  touchT.startedAt = 0;
  touchT.lastReleaseAt = 0;
  touchT.pendingSingleTap = false;
}

// ---------- Touch Gesture Action Dispatcher ----------
constexpr uint32_t DOUBLE_TAP_WINDOW_MS = 240;

TouchAction getSingleTapAction(uint8_t pin) {
  if (pin == PIN_TOUCH_TOP) return touchActTopTap;
  if (pin == PIN_TOUCH_LEFT) return touchActLeftTap;
  if (pin == PIN_TOUCH_RIGHT) return touchActRightTap;
  return TOUCH_ACT_NONE;
}

TouchAction getDoubleTapAction(uint8_t pin) {
  if (pin == PIN_TOUCH_TOP) return touchActTopDbl;
  if (pin == PIN_TOUCH_LEFT) return touchActLeftDbl;
  if (pin == PIN_TOUCH_RIGHT) return touchActRightDbl;
  return TOUCH_ACT_NONE;
}

void executeTouchAction(TouchAction act, uint8_t pin) {
  uint32_t now = millis();
  lastUserInteractionAt = now;

  if (isRobotTimerRinging()) {
    Serial.println("[TOUCH] Action -> Dismissing Timer Alarm");
    stopRobotTimerAlarm();
    return;
  }

  if (inCircadianSleep || manualSleep) {
    manualSleep = false;
    inCircadianSleep = false;
    circadianTempWakeUntil = now + CIRCADIAN_TEMP_WAKE_MS;
    setOledDim(false);
    updateDisplayBrightnessMode();
    setEmotion(EMOTION_HAPPY);
    soundPetSparkle();
    Serial.println("[TOUCH] Wake-up from Sleep Mode");
    return;
  }

  if (animationIsActive()) {
    stopAnimation();
    setEmotion(EMOTION_HAPPY);
    soundPetSparkle();
    return;
  }
  if (bannerActive) {
    bannerActive = false;
    setEmotion(EMOTION_HAPPY);
    soundPetSparkle();
    return;
  }

  // Cheek tickle rapid tap tracking
  if (pin == PIN_TOUCH_LEFT || pin == PIN_TOUCH_RIGHT) {
    if (now - lastCheekTouchAt < TICKLE_CHEEK_WINDOW_MS) {
      cheekTapCount++;
    } else {
      cheekTapCount = 1;
    }
    lastCheekTouchAt = now;
    if (cheekTapCount >= 3 && now - lastTickleTriggeredAt > TICKLE_COOLDOWN_MS &&
        !busyProcessing && !inCircadianSleep && !manualSleep) {
      triggerTickleMode();
      return;
    }
  }

  switch (act) {
    case TOUCH_ACT_LISTEN:
      if (busyProcessing || startRecording) {
        Serial.println("[TOUCH] Action LISTEN -> Cancel current request / listening");
        cancelRequest = true;
        startRecording = false;
        soundPetSparkle();
        setEmotion(EMOTION_IDLE);
      } else {
        Serial.println("[TOUCH] Action LISTEN -> Enter Listening Mode!");
        setEmotion(EMOTION_LISTENING);
        startRecording = true;
      }
      break;

    case TOUCH_ACT_CLOCK:
      showWeatherScreen = false;
      showClockScreen = !showClockScreen;
      if (showClockScreen) {
        soundClock();
        if (!timeSynced && WiFi.status() == WL_CONNECTED)
          syncTimeFromNTP();
      } else {
        setEmotion(EMOTION_HAPPY);
      }
      break;

    case TOUCH_ACT_WEATHER:
      showClockScreen = false;
      showWeatherScreen = !showWeatherScreen;
      if (showWeatherScreen) {
        soundPetSparkle();
        if (millis() - lastWeatherFetchAt > 600000UL)
          fetchWeather();
      } else {
        setEmotion(EMOTION_HAPPY);
      }
      break;

    case TOUCH_ACT_CYCLE_SKIN: {
      int nextSkin = ((int)currentEyeSkin + 1) % SKIN_COUNT;
      currentEyeSkin = (EyeSkin)nextSkin;
      soundCoin();
      saveConfig();
      Serial.printf("[TOUCH] Action CYCLE_SKIN -> Skin %d\n", nextSkin);
      break;
    }

    case TOUCH_ACT_PET:
      triggerPetReaction();
      break;

    case TOUCH_ACT_TICKLE:
      triggerTickleMode();
      break;

    case TOUCH_ACT_VOL_UP:
      speakerVolume = min(100, speakerVolume + 10);
      soundVolUp();
      saveConfig();
      Serial.printf("[TOUCH] Action VOL_UP -> %d%%\n", speakerVolume);
      break;

    case TOUCH_ACT_VOL_DOWN:
      speakerVolume = max(0, speakerVolume - 10);
      soundVolDown();
      saveConfig();
      Serial.printf("[TOUCH] Action VOL_DOWN -> %d%%\n", speakerVolume);
      break;

    case TOUCH_ACT_DISMISS:
      if (busyProcessing || startRecording) {
        Serial.println("[TOUCH] Action DISMISS -> Cancel current request / listening");
        cancelRequest = true;
        startRecording = false;
        soundPetSparkle();
        setEmotion(EMOTION_IDLE);
      } else if (showClockScreen) {
        showClockScreen = false;
      } else if (showWeatherScreen) {
        showWeatherScreen = false;
      } else if (currentEmotion != EMOTION_IDLE && !busyProcessing) {
        setEmotion(EMOTION_IDLE);
        emotionAutoReturnAt = 0;
        returnIdleAt = 0;
        soundPetSparkle();
      } else if (!busyProcessing) {
        gazeTgtX = (pin == PIN_TOUCH_LEFT) ? -8.0f : 8.0f;
        gazeTgtY = 0;
        gazeLockUntil = now + 2500;
        faceTgt.gazeLock = false;
      }
      break;

    case TOUCH_ACT_NONE:
    default:
      break;
  }
}

void checkPendingSingleTap(TouchSensor &ts, uint32_t now) {
  if (ts.pendingSingleTap && (now - ts.lastReleaseAt > DOUBLE_TAP_WINDOW_MS)) {
    ts.pendingSingleTap = false;
    Serial.printf("[TOUCH] Pin %d SINGLE TAP confirmed\n", ts.pin);
    executeTouchAction(getSingleTapAction(ts.pin), ts.pin);
  }
}

static void updateOneTouch(TouchSensor &ts, uint32_t now) {
  if (now < 2500)
    return;
  bool raw = digitalRead(ts.pin);
  if (raw != ts.lastRaw) {
    ts.lastRaw = raw;
    ts.changedAt = now;
  }
  uint32_t deb = (touchDebounceMs > 0) ? (uint32_t)touchDebounceMs : TOUCH_DEBOUNCE;
  if (now - ts.changedAt >= deb && raw != ts.state) {
    ts.state = raw;
    if (ts.state) {
      ts.startedAt = now;
      ts.holdHandled = false;
      ts.sleepHandled = false;
      returnIdleAt = 0;
      if (bootInfoUntil > 0) {
        bootInfoUntil = 0;
      }
      Serial.printf("[TOUCH] Pin %d PRESSED (feedback=%d, busy=%d)\n", ts.pin, touchFeedbackSound, busyProcessing);

      // Instant dismissal of repeating timer alarm on ANY sensor touch
      if (isRobotTimerRinging() && getRobotTimerRingingDurationMs() >= 200) {
        Serial.printf("[TOUCH] Pin %d touched -> Timer Alarm Dismissed!\n", ts.pin);
        stopRobotTimerAlarm();
        ts.holdHandled = true;
        ts.pendingSingleTap = false;
        return;
      }

      // Instant Audio & Visual Touch Response
      static uint32_t lastTouchBlipAt = 0;
      if (!inCircadianSleep && !manualSleep) {
        if (touchFeedbackSound && !busyProcessing && (now - lastTouchBlipAt >= 200)) {
          lastTouchBlipAt = now;
          soundTouchBlip();
          updateTonePlayer();
        }
        if (ts.pin == PIN_TOUCH_LEFT) {
          activeWinkLeft = true;
          activeWinkEnd = now + 180;
        } else if (ts.pin == PIN_TOUCH_RIGHT) {
          activeWinkLeft = false;
          activeWinkEnd = now + 180;
        } else if (ts.pin == PIN_TOUCH_TOP) {
          triggerTouchBlink(now);
        }
      }
    } else {
      uint32_t held = now - ts.startedAt;
      Serial.printf("[TOUCH] Pin %d RELEASED after %lu ms\n", ts.pin, (unsigned long)held);
      uint32_t tapMax = (ts.pin == PIN_TOUCH_TOP) ? TOUCH_CLOCK_MS : TOUCH_TAP_MAX_MS;
      if (!ts.holdHandled && !ts.sleepHandled && held < tapMax) {
        if (inCircadianSleep || manualSleep) {
          executeTouchAction(TOUCH_ACT_DISMISS, ts.pin);
          ts.pendingSingleTap = false;
          return;
        }

        TouchAction dblAct = getDoubleTapAction(ts.pin);
        if (ts.pendingSingleTap && (now - ts.lastReleaseAt <= DOUBLE_TAP_WINDOW_MS) && dblAct != TOUCH_ACT_NONE) {
          ts.pendingSingleTap = false;
          Serial.printf("[TOUCH] Pin %d DOUBLE TAP triggered!\n", ts.pin);
          executeTouchAction(dblAct, ts.pin);
        } else if (dblAct == TOUCH_ACT_NONE) {
          executeTouchAction(getSingleTapAction(ts.pin), ts.pin);
        } else {
          ts.pendingSingleTap = true;
          ts.lastReleaseAt = now;
        }
      }
    }
  }
  if (ts.state) {
    lastUserInteractionAt = now;
    uint32_t held = now - ts.startedAt;

    // Dual-cheek hold (both left & right cheeks pressed simultaneously -> Tickle Mode)
    if (touchL.state && touchR.state) {
      uint32_t heldL = now - touchL.startedAt;
      uint32_t heldR = now - touchR.startedAt;
      if (heldL >= DUAL_CHEEK_HOLD_MS && heldR >= DUAL_CHEEK_HOLD_MS && !touchL.holdHandled && !touchR.holdHandled &&
          !busyProcessing && now - lastTickleTriggeredAt > TICKLE_COOLDOWN_MS && !inCircadianSleep && !manualSleep) {
        touchL.holdHandled = true;
        touchR.holdHandled = true;
        triggerTickleMode();
        return;
      }
    }

    if (ts.pin == PIN_TOUCH_LEFT && held >= TOUCH_HOLD_MS && !ts.holdHandled && !busyProcessing) {
      ts.holdHandled = true;
      executeTouchAction(touchActLeftHold, ts.pin);
    }
    if (ts.pin == PIN_TOUCH_RIGHT && held >= TOUCH_HOLD_MS && !ts.holdHandled && !busyProcessing) {
      ts.holdHandled = true;
      executeTouchAction(touchActRightHold, ts.pin);
    }

    // Hold 3s on Top -> Change Face Skin (Cycle Face Skin)
    if (ts.pin == PIN_TOUCH_TOP && held >= TOUCH_CLOCK_MS && !ts.holdHandled && !busyProcessing && !inCircadianSleep && !manualSleep) {
      ts.holdHandled = true;
      Serial.println("[TOUCH] Top Held 3s -> Changing Face Skin");
      executeTouchAction(touchActTopHold, ts.pin);
    }
    // Hold 7-10s on Top -> Sleep Mode Toggle!
    if (ts.pin == PIN_TOUCH_TOP && held >= SLEEP_HOLD_MS && !ts.sleepHandled && !busyProcessing) {
      ts.sleepHandled = true;
      showClockScreen = false;
      showWeatherScreen = false;
      if (!inCircadianSleep && !manualSleep) {
        manualSleep = true;
        inCircadianSleep = true;
        setEmotion(EMOTION_SLEEPING);
        setOledDim(true);
        soundPetSleepy();
        Serial.println("[TOUCH] Held 7-10s -> Kiwi is now in Sleep Mode (Z Z)");
      } else {
        manualSleep = false;
        inCircadianSleep = false;
        setOledDim(false);
        setEmotion(EMOTION_HAPPY);
        soundPetSparkle();
        Serial.println("[TOUCH] Held 7-10s -> Kiwi Woke Up!");
      }
    }
  }
}

void updateTouch() {
  uint32_t now = millis();
  updateOneTouch(touchL, now);
  updateOneTouch(touchR, now);
  updateOneTouch(touchT, now);

  checkPendingSingleTap(touchL, now);
  checkPendingSingleTap(touchR, now);
  checkPendingSingleTap(touchT, now);
}
