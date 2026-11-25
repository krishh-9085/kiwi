#include "hardware/TouchController.h"
#include "animation/AnimationEngine.h"
#include "behavior/ActionExecutor.h"
#include "behavior/HungerEngine.h"
#include "config/AppConfig.h"
#include "config/ConfigManager.h"
#include "face/BootWakeAnim.h"
#include "face/FaceDisplay.h"
#include "face/FaceEngine.h"
#include "face/SleepyWakeAnim.h"
#include "hardware/AudioEngine.h"
#include "hardware/WakeWordEngine.h"
#include "power/PowerManager.h"
#include "services/AiService.h"
#include "services/WeatherService.h"
#include <WiFi.h>
#include <esp_sleep.h>

TouchSensor touchL = {PIN_TOUCH_LEFT, false, false, 0, 0,
                      false,          false, false, 0, false};
TouchSensor touchR = {PIN_TOUCH_RIGHT, false, false, 0, 0,
                      false,           false, false, 0, false};
TouchSensor touchT = {PIN_TOUCH_TOP, false, false, 0, 0,
                      false,         false, false, 0, false};

void triggerPetReaction();
bool syncTimeFromNTP();

// ---------- Tickle / Gudgudi Tracking ----------
static uint32_t lastCheekTouchAt = 0;
static uint8_t cheekTapCount = 0;
uint32_t lastTickleTriggeredAt = 0;
static volatile bool s_inTickleSpeech = false;

bool isTickleActive() {
  return s_inTickleSpeech;
}

void triggerTickleMode() {
  if (busyProcessing || s_inTickleSpeech)
    return;
  busyProcessing = true;
  s_inTickleSpeech = true;
  cancelRequest = false;
  startRecording = false;
  lastTickleTriggeredAt = millis();
  lastUserInteractionAt = millis();
  cheekTapCount = 0;
  Serial.println("[TICKLE] Cheek tickle detected! Triggering Gudgudi mode!");
  setEmotion(EMOTION_LAUGHING);
  soundPetGiggle();

  static const char *const tickleReplies[] = {
      "Arey arey gudgudi ho rahi hai, chhodo mujhe!",
      "Haha bas karo bhai, bohot ticklish hoon main!",
      "Hehe haha! Cheeks mat chhedo, gudgudi lagti hai!"};
  int idx = random(0, 3);
  speakText(tickleReplies[idx]);
  setEmotion(EMOTION_IDLE);
  returnIdleAt = 0;
  resyncTouch();
  s_inTickleSpeech = false;
  busyProcessing = false;
}

void initTouch() {
  pinMode(PIN_TOUCH_LEFT, INPUT_PULLDOWN);
  pinMode(PIN_TOUCH_RIGHT, INPUT_PULLDOWN);
  pinMode(PIN_TOUCH_TOP, INPUT_PULLDOWN);
#if defined(ESP_GPIO_WAKEUP_GPIO_HIGH)
  esp_deep_sleep_enable_gpio_wakeup(1ULL << PIN_TOUCH_TOP,
                                    ESP_GPIO_WAKEUP_GPIO_HIGH);
#else
  esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_TOUCH_TOP, 1);
#endif
  touchL.armed = true;
  touchR.armed = true;
  touchT.armed = true;
}

void resyncTouch() {
  touchL.state = false;
  touchL.lastRaw = digitalRead(PIN_TOUCH_LEFT);
  touchL.holdHandled = (touchL.lastRaw != 0);
  touchL.sleepHandled = (touchL.lastRaw != 0);
  touchL.startedAt = 0;
  touchL.lastReleaseAt = 0;
  touchL.pendingSingleTap = false;

  touchR.state = false;
  touchR.lastRaw = digitalRead(PIN_TOUCH_RIGHT);
  touchR.holdHandled = (touchR.lastRaw != 0);
  touchR.sleepHandled = (touchR.lastRaw != 0);
  touchR.startedAt = 0;
  touchR.lastReleaseAt = 0;
  touchR.pendingSingleTap = false;

  touchT.state = false;
  touchT.lastRaw = digitalRead(PIN_TOUCH_TOP);
  touchT.holdHandled = (touchT.lastRaw != 0);
  touchT.sleepHandled = (touchT.lastRaw != 0);
  touchT.startedAt = 0;
  touchT.lastReleaseAt = 0;
  touchT.pendingSingleTap = false;
}

// ---------- Touch Gesture Action Dispatcher ----------
// Calibrated double-tap window: 480ms provides natural, reliable multi-tap detection
// on physical capacitive sensors without accidental single-tap misfires.
constexpr uint32_t DOUBLE_TAP_WINDOW_MS = 480;

TouchAction getSingleTapAction(uint8_t pin) {
  if (pin == PIN_TOUCH_TOP)
    return touchActTopTap;
  if (pin == PIN_TOUCH_LEFT)
    return touchActLeftTap;
  if (pin == PIN_TOUCH_RIGHT)
    return touchActRightTap;
  return TOUCH_ACT_NONE;
}

TouchAction getDoubleTapAction(uint8_t pin) {
  if (pin == PIN_TOUCH_TOP)
    return touchActTopDbl;
  if (pin == PIN_TOUCH_LEFT)
    return touchActLeftDbl;
  if (pin == PIN_TOUCH_RIGHT)
    return touchActRightDbl;
  return TOUCH_ACT_NONE;
}

void executeTouchAction(TouchAction act, uint8_t pin) {
  powerNotifyActivity();
  if (!bootWakeDone()) {
    return;
  }
  uint32_t now = millis();
  lastUserInteractionAt = now;

  // Protect ongoing speech and tickle mode from accidental cheek touches:
  // Cheek taps while Kiwi is speaking or tickling must NEVER cancel or interrupt speech!
  if (pin == PIN_TOUCH_LEFT || pin == PIN_TOUCH_RIGHT) {
    if (s_inTickleSpeech || isSpeechPlaying() || (now - lastTickleTriggeredAt < 4500 && busyProcessing)) {
      Serial.printf("[TOUCH] Cheek pin %d touched during active speech/tickle -> Absorbed! Speech protected.\n", pin);
      return;
    }
  }

  if (isRobotTimerRinging()) {
    Serial.println("[TOUCH] Action -> Dismissing Timer Alarm");
    stopRobotTimerAlarm();
    return;
  }

  if (sleepyWakeIsActive()) {
    sleepyWakeCancel();
    manualSleep = false;
    inCircadianSleep = false;
    circadianTempWakeUntil = now + 30000;
    setEmotion(EMOTION_IDLE);
    soundPetSparkle();
    Serial.println(
        "[TOUCH] Second tap during sleepy wake -> Full Wake-up (Happy)");
    return;
  }

  if (sleepyFallAsleepIsActive()) {
    sleepyFallAsleepCancel();
    manualSleep = false;
    inCircadianSleep = false;
    circadianTempWakeUntil = now + 30000;
    setDisplayPower(true);
    setOledDim(false);
    updateDisplayBrightnessMode();
    setEmotion(EMOTION_IDLE);
    emotionAutoReturnAt = 0;
    returnIdleAt = 0;
    sleepyWakeStart();
    Serial.println("[TOUCH] Tap during falling asleep -> Waking up with sleepy "
                   "wake animation!");
    return;
  }

  if (inCircadianSleep || manualSleep) {
    manualSleep = false;
    inCircadianSleep = false;
    circadianTempWakeUntil = now + 30000;
    setDisplayPower(true);
    setOledDim(false);
    updateDisplayBrightnessMode();
    setEmotion(EMOTION_IDLE);
    emotionAutoReturnAt = 0;
    returnIdleAt = 0;
    sleepyWakeStart();
    Serial.println(
        "[TOUCH] Tap during sleep -> Waking up with sleepy wake animation!");
    return;
  }

  if (animationIsActive()) {
    stopAnimation();
    setEmotion(EMOTION_IDLE);
    returnIdleAt = 0;
    soundPetSparkle();
    return;
  }
  if (bannerActive) {
    bannerActive = false;
    setEmotion(EMOTION_IDLE);
    returnIdleAt = 0;
    soundPetSparkle();
    return;
  }

  if (act != TOUCH_ACT_CLOCK && act != TOUCH_ACT_WEATHER) {
    if (showClockScreen || showWeatherScreen) {
      showClockScreen = false;
      showWeatherScreen = false;
      setEmotion(EMOTION_IDLE);
      returnIdleAt = 0;
      soundTouchBlip();
      Serial.println(
          "[TOUCH] Screen active (Clock/Weather) -> Dismissed by touch action!");
      return;
    }
  }

  // Cheek tickle rapid tap tracking (only for playful pet / idle touches, never for functional clock/weather commands)
  if ((pin == PIN_TOUCH_LEFT || pin == PIN_TOUCH_RIGHT) && (act == TOUCH_ACT_DISMISS || act == TOUCH_ACT_PET || act == TOUCH_ACT_NONE)) {
    if (isKiwiAngry()) {
      cheekTapCount = 0;
      return; // Never trigger tickle mode or single-tap actions while Kiwi is angry!
    }
    if (s_inTickleSpeech || isSpeechPlaying() || busyProcessing) {
      Serial.printf("[TOUCH] Cheek pin %d tap during busy/speech -> Absorbed!\n", pin);
      return;
    }
    if (now - lastCheekTouchAt < TICKLE_CHEEK_WINDOW_MS) {
      cheekTapCount++;
    } else {
      cheekTapCount = 1;
    }
    lastCheekTouchAt = now;
    if (cheekTapCount >= 4 &&
        now - lastTickleTriggeredAt > TICKLE_COOLDOWN_MS && !busyProcessing &&
        !inCircadianSleep && !manualSleep) {
      triggerTickleMode();
      return;
    }
  } else if (pin == PIN_TOUCH_LEFT || pin == PIN_TOUCH_RIGHT) {
    cheekTapCount = 0; // Clear tickle count on functional gestures (clock, weather, etc.)
  }

  switch (act) {
  case TOUCH_ACT_LISTEN: {
    if (isKiwiAngry()) {
      handleAngryRejection();
      return;
    }
    static uint32_t listenStartedAt = 0;
    if (showClockScreen || showWeatherScreen) {
      showClockScreen = false;
      showWeatherScreen = false;
      setEmotion(EMOTION_IDLE);
      returnIdleAt = 0;
      soundPetSparkle();
      Serial.println("[TOUCH] Action LISTEN intercepted -> Dismissed "
                     "Clock/Weather Screen!");
      return;
    }

    if (busyProcessing || startRecording || isSpeechPlaying() ||
        currentEmotion == EMOTION_SPEAKING) {
      if (pin == PIN_TOUCH_TOP) {
        // Debounce cancel: only cancel if listening was initiated > 600ms ago.
        // This prevents the user's initial touch release / bounce from
        // immediately cancelling listening!
        if (millis() - listenStartedAt > 600) {
          Serial.println(
              "[TOUCH] Action LISTEN (Top) -> Cancel current request / listening");
          cancelRequest = true;
          startRecording = false;
          toneStop();
          spkEndSpeech();
          setEmotion(EMOTION_IDLE);
        } else {
          Serial.println("[TOUCH] Action LISTEN -> Ignored cancel bounce (<600ms "
                         "since start)");
        }
      } else {
        Serial.printf("[TOUCH] Action LISTEN (pin %d) -> Ignored cancel during busyProcessing/startRecording\n", pin);
      }
      return;
    } else {
      Serial.println("[TOUCH] Action LISTEN -> Enter Listening Mode!");
      listenStartedAt = millis();
      setEmotion(EMOTION_LISTENING);
      preWakeReset();
      startRecording = true;
    }
    break;
  }

  case TOUCH_ACT_CLOCK:
    showWeatherScreen = false;
    showClockScreen = !showClockScreen;
    if (showClockScreen) {
      soundClock();
      if (!timeSynced && WiFi.status() == WL_CONNECTED)
        syncTimeFromNTP();
    } else {
      setEmotion(EMOTION_IDLE);
      returnIdleAt = 0;
    }
    break;

  case TOUCH_ACT_WEATHER:
    showClockScreen = false;
    showWeatherScreen = !showWeatherScreen;
    if (showWeatherScreen) {
      soundWeather();
      if (millis() - lastWeatherFetchAt > 600000UL)
        fetchWeather();
    } else {
      setEmotion(EMOTION_IDLE);
      returnIdleAt = 0;
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
    if (pin != PIN_TOUCH_TOP && (busyProcessing || isSpeechPlaying() || s_inTickleSpeech)) {
      Serial.printf("[TOUCH] Cheek pin %d touched during busy/speech -> Dismiss ignored to protect speech\n", pin);
      return;
    }
    if (busyProcessing || startRecording) {
      Serial.println(
          "[TOUCH] Action DISMISS -> Cancel current request / listening");
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
  // Only confirm single-tap if the sensor is NOT currently being touched/pressed (!ts.state)
  // and the double-tap window has elapsed since release.
  if (ts.pendingSingleTap && !ts.state && (now - ts.lastReleaseAt > DOUBLE_TAP_WINDOW_MS)) {
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
  uint32_t deb =
      (touchDebounceMs > 0) ? (uint32_t)touchDebounceMs : TOUCH_DEBOUNCE;
  if (now - ts.changedAt >= deb && raw != ts.state) {
    powerNotifyActivity();
    ts.state = raw;
    if (ts.state) {
      ts.startedAt = now;
      ts.holdHandled = false;
      ts.sleepHandled = false;
      returnIdleAt = 0;
      if (bootInfoUntil > 0) {
        bootInfoUntil = 0;
      }
      Serial.printf("[TOUCH] Pin %d PRESSED (feedback=%d, busy=%d)\n", ts.pin,
                    touchFeedbackSound, busyProcessing);

      // Instant dismissal of repeating timer alarm on ANY sensor touch
      if (isRobotTimerRinging() && getRobotTimerRingingDurationMs() >= 200) {
        Serial.printf("[TOUCH] Pin %d touched -> Timer Alarm Dismissed!\n",
                      ts.pin);
        stopRobotTimerAlarm();
        ts.holdHandled = true;
        ts.sleepHandled = true;
        ts.pendingSingleTap = false;
        return;
      }

      // Instant dismissal of Clock or Weather screen on ANY sensor touch
      if (showClockScreen || showWeatherScreen) {
        Serial.printf("[TOUCH] Pin %d touched -> Clock/Weather Dismissed!\n",
                      ts.pin);
        showClockScreen = false;
        showWeatherScreen = false;
        ts.holdHandled = true;
        ts.sleepHandled = true;
        ts.pendingSingleTap = false;
        setEmotion(EMOTION_IDLE);
        returnIdleAt = 0;
        soundPetSparkle();
        updateTonePlayer();
        return;
      }

      // Instant Audio & Visual Touch Response
      static uint32_t lastTouchBlipAt = 0;
      if (!inCircadianSleep && !manualSleep) {
        if (touchFeedbackSound && !busyProcessing &&
            (now - lastTouchBlipAt >= 200)) {
          lastTouchBlipAt = now;
          soundTouchBlip();
          updateTonePlayer();
        }
        if (ts.pin == PIN_TOUCH_LEFT) {
          activeWinkLeft = true;
          activeWinkEnd = now + 180;
          recordCheekTapForCombo(PIN_TOUCH_LEFT, now);
        } else if (ts.pin == PIN_TOUCH_RIGHT) {
          activeWinkLeft = false;
          activeWinkEnd = now + 180;
          recordCheekTapForCombo(PIN_TOUCH_RIGHT, now);
        } else if (ts.pin == PIN_TOUCH_TOP) {
          triggerTouchBlink(now);
        }

        // Left-Right alternating apology combo when angry
        if (isKiwiAngry() &&
            (ts.pin == PIN_TOUCH_LEFT || ts.pin == PIN_TOUCH_RIGHT)) {
          ts.holdHandled = true; // Mark handled so release never triggers
                                 // single-tap or tickle actions!
          ts.sleepHandled = true;
          ts.pendingSingleTap = false;
          cheekTapCount = 0; // Prevent tickle counter from accumulating
          if (checkCheekComboLRLR()) {
            Serial.println("[TOUCH] Alternating cheek combo triggered! "
                           "Reconciling angry Kiwi!");
            lastTickleTriggeredAt =
                now + 10000UL; // 10s cooldown for tickle mode
            reconcileKiwi();
          }
          return;
        }
      }
    } else {
      uint32_t held = now - ts.startedAt;
      if (!touchL.state && !touchR.state && !touchT.state) {
        if (currentEmotion == EMOTION_IDLE || currentEmotion == EMOTION_HAPPY) {
          gazeTgtX = 0.0f;
          gazeTgtY = 0.0f;
        }
      }
      Serial.printf("[TOUCH] Pin %d RELEASED after %lu ms\n", ts.pin,
                    (unsigned long)held);
      uint32_t tapMax =
          (ts.pin == PIN_TOUCH_TOP) ? TOUCH_CLOCK_MS : TOUCH_TAP_MAX_MS;
      if (!ts.holdHandled && !ts.sleepHandled && held < tapMax) {
        if (inCircadianSleep || manualSleep || sleepyFallAsleepIsActive()) {
          executeTouchAction(TOUCH_ACT_DISMISS, ts.pin);
          ts.pendingSingleTap = false;
          return;
        }

        TouchAction dblAct = getDoubleTapAction(ts.pin);
        bool isSecondTap = ts.pendingSingleTap &&
                           (dblAct != TOUCH_ACT_NONE) &&
                           ((ts.startedAt - ts.lastReleaseAt <= DOUBLE_TAP_WINDOW_MS) ||
                            (now - ts.lastReleaseAt <= DOUBLE_TAP_WINDOW_MS + 250));

        if (isSecondTap) {
          ts.pendingSingleTap = false;
          cheekTapCount = 0;
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

    // Directional Gazing: Eyes look towards the touched cheek or head
    if (!busyProcessing && !inCircadianSleep && !manualSleep) {
      if (touchL.state && !touchR.state) {
        gazeTgtX = -10.0f;
      } else if (touchR.state && !touchL.state) {
        gazeTgtX = 10.0f;
      } else if (touchT.state) {
        gazeTgtY = -6.0f;
      }
    }

    // Dual-cheek hold (both left & right cheeks pressed simultaneously)
    if (touchL.state && touchR.state) {
      uint32_t heldL = now - touchL.startedAt;
      uint32_t heldR = now - touchR.startedAt;
      if (heldL >= 300 && heldR >= 300 && !touchL.holdHandled &&
          !touchR.holdHandled) {
        touchL.holdHandled = true;
        touchR.holdHandled = true;
        if (isKiwiHungry()) {
          feedKiwi();
          return;
        } else if (isKiwiAngry()) {
          handleAngryFeedAttempt();
          return;
        } else if (!busyProcessing && !s_inTickleSpeech &&
                   now - lastTickleTriggeredAt > TICKLE_COOLDOWN_MS &&
                   !inCircadianSleep && !manualSleep) {
          triggerTickleMode();
          return;
        }
      }
    }

    if (ts.pin == PIN_TOUCH_LEFT && held >= TOUCH_HOLD_MS && !ts.holdHandled &&
        !busyProcessing) {
      ts.holdHandled = true;
      executeTouchAction(touchActLeftHold, ts.pin);
    }
    if (ts.pin == PIN_TOUCH_RIGHT && held >= TOUCH_HOLD_MS && !ts.holdHandled &&
        !busyProcessing) {
      ts.holdHandled = true;
      executeTouchAction(touchActRightHold, ts.pin);
    }

    // Hold 3s on Top -> Instantly Change Face Skin (Cycle Face Theme)
    if (ts.pin == PIN_TOUCH_TOP && held >= TOUCH_CLOCK_MS && !ts.holdHandled &&
        !ts.sleepHandled && !busyProcessing && !inCircadianSleep &&
        !manualSleep && !sleepyFallAsleepIsActive()) {
      ts.holdHandled = true;
      Serial.println("[TOUCH] Top Held 3s -> Instant Skin Cycle!");
      executeTouchAction(touchActTopHold, ts.pin);
    }
    // Hold 2s while asleep (or 7s while awake) on Top -> Sleep Mode Toggle!
    uint32_t sleepHoldRequired =
        (inCircadianSleep || manualSleep || sleepyFallAsleepIsActive())
            ? 2000
            : SLEEP_HOLD_MS;
    if (ts.pin == PIN_TOUCH_TOP && held >= sleepHoldRequired &&
        !ts.sleepHandled && !busyProcessing) {
      ts.sleepHandled = true;
      sleepyWakeCancel();
      sleepyFallAsleepCancel();
      showClockScreen = false;
      showWeatherScreen = false;
      if (!inCircadianSleep && !manualSleep && !sleepyFallAsleepIsActive()) {
        manualSleep = true;
        inCircadianSleep = false;
        sleepyFallAsleepStart();
        Serial.println("[TOUCH] Held to sleep -> Kiwi is falling asleep (Z Z)");
      } else {
        manualSleep = false;
        inCircadianSleep = false;
        circadianTempWakeUntil = now + 30000;
        setDisplayPower(true);
        setOledDim(false);
        updateDisplayBrightnessMode();
        setEmotion(EMOTION_IDLE);
        emotionAutoReturnAt = 0;
        returnIdleAt = 0;
        sleepyWakeStart();
        Serial.println("[TOUCH] Held to wake -> Kiwi Woke Up!");
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
