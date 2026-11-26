#include "hardware/TouchController.h"
#include "face/AnimationEngine.h"
#include "commands/ActionExecutor.h"
#include "behavior/HungerEngine.h"
#include "config/AppConfig.h"
#include "config/ConfigManager.h"
#include "face/BootWakeAnim.h"
#include "face/FaceDisplay.h"
#include "face/FaceEngine.h"
#include "face/SleepyWakeAnim.h"
#include "behavior/CircadianEngine.h"
#include "hardware/AudioEngine.h"
#include "hardware/WakeWordEngine.h"
#include "power/PowerManager.h"
#include "ai/AiService.h"
#include "integrations/WeatherService.h"
#include "net/WifiManager.h"
#include <WiFi.h>
#include <esp_sleep.h>

TouchSensor touchL = {PIN_TOUCH_LEFT, false, false, 0, 0,
                      false,          false, false, 0, false};
TouchSensor touchR = {PIN_TOUCH_RIGHT, false, false, 0, 0,
                      false,           false, false, 0, false};
TouchSensor touchT = {PIN_TOUCH_TOP, false, false, 0, 0,
                      false,         false, false, 0, false};

void triggerPetReaction();

// ---------- Tickle / Gudgudi Tracking ----------
static uint32_t lastCheekTouchAt = 0;
static uint8_t cheekTapCount = 0;
uint32_t lastTickleTriggeredAt = 0;
static uint32_t s_tickleSuppressUntil = 0;
static volatile bool s_inTickleSpeech = false;
static portMUX_TYPE s_touchMux = portMUX_INITIALIZER_UNLOCKED;

bool isTickleActive() {
  return s_inTickleSpeech;
}

float getTopTouchHoldProgress() {
  portENTER_CRITICAL(&s_touchMux);
  bool tState = touchT.state;
  uint32_t tStarted = touchT.startedAt;
  bool tHold = touchT.holdHandled;
  bool tSleep = touchT.sleepHandled;
  portEXIT_CRITICAL(&s_touchMux);

  if (!tState || busyProcessing || inCircadianSleep || manualSleep ||
      sleepyFallAsleepIsActive() || sleepyWakeIsActive()) {
    return 0.0f;
  }
  uint32_t held = millis() - tStarted;
  if (held < 450 || tSleep)
    return 0.0f;
  if (!tHold && held < TOUCH_CLOCK_MS) {
    return (float)(held - 450) / (float)(TOUCH_CLOCK_MS - 450);
  }
  if (held >= TOUCH_CLOCK_MS && held < SLEEP_HOLD_MS) {
    return (float)(held - TOUCH_CLOCK_MS) / (float)(SLEEP_HOLD_MS - TOUCH_CLOCK_MS);
  }
  return 0.0f;
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
  bool wasCancelled = cancelRequest;
  bool bargeInListen = startRecording;
  cancelRequest = false;
  if (wasCancelled && bargeInListen) {
    toneStop();
    setEmotion(EMOTION_LISTENING);
    robotStateDesc = "Listening...";
  } else {
    setEmotion(EMOTION_IDLE);
  }
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

static void resyncOneTouch(TouchSensor &ts, uint32_t now) {
  bool raw = (digitalRead(ts.pin) != 0);
  portENTER_CRITICAL(&s_touchMux);
  ts.lastRaw = raw;
  ts.state = raw;
  ts.holdHandled = raw;
  ts.sleepHandled = raw;
  ts.changedAt = now;
  ts.startedAt = raw ? now : 0;
  ts.lastReleaseAt = 0;
  ts.pendingSingleTap = false;
  portEXIT_CRITICAL(&s_touchMux);
}

void resyncTouch() {
  uint32_t now = millis();
  resyncOneTouch(touchL, now);
  resyncOneTouch(touchR, now);
  resyncOneTouch(touchT, now);
}

// ---------- Touch Gesture Action Dispatcher ----------
// Calibrated double-tap window: 300ms for snappy single-tap dispatch while
// keeping reliable double-tap detection on capacitive sensors.
constexpr uint32_t DOUBLE_TAP_WINDOW_MS = 300;

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
    wakeKiwiFromSleep(false, CIRCADIAN_TEMP_WAKE_MS, false);
    soundPetSparkle();
    Serial.println(
        "[TOUCH] Second tap during sleepy wake -> Full Wake-up (Happy)");
    return;
  }

  if (sleepyFallAsleepIsActive() || inCircadianSleep || manualSleep) {
    wakeKiwiFromSleep(false, CIRCADIAN_TEMP_WAKE_MS, true);
    Serial.println("[TOUCH] Tap during sleep -> Waking up with sleepy "
                   "wake animation!");
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
      updateDisplayBrightnessMode();
      setEmotion(EMOTION_IDLE);
      returnIdleAt = 0;
      soundTouchBlip();
      Serial.println(
          "[TOUCH] Screen active (Clock/Weather) -> Dismissed by touch action!");
      return;
    }
  }

  // Cheek angry guard (rapid 4-tap tickle tracking is handled on press so mapped
  // tap/double-tap cheek actions do not swallow rapid tickle bursts)
  if ((pin == PIN_TOUCH_LEFT || pin == PIN_TOUCH_RIGHT) && isKiwiAngry() &&
      (act == TOUCH_ACT_DISMISS || act == TOUCH_ACT_PET || act == TOUCH_ACT_NONE)) {
    cheekTapCount = 0;
    return;
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
      updateDisplayBrightnessMode();
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
        // Single-tap barge-in during active TTS playback: smoothly ramp down
        // current speech and transition directly into listening mode.
        if ((isSpeechPlaying() || currentEmotion == EMOTION_SPEAKING) &&
            !startRecording) {
          Serial.println(
              "[TOUCH] Barge-in (Top) during TTS -> Abort speech & listen!");
          listenStartedAt = millis();
          cancelRequest = true;
          toneStop();
          spkAbortSpeech();
          preWakeReset();
          startRecording = true;
          setEmotion(EMOTION_LISTENING);
          robotStateDesc = "Listening...";
          return;
        }
        // Debounce cancel: only cancel if listening was initiated > 600ms ago.
        // This prevents the user's initial touch release / bounce from
        // immediately cancelling listening!
        if (millis() - listenStartedAt > 600) {
          Serial.println(
              "[TOUCH] Action LISTEN (Top) -> Cancel current request / listening");
          cancelRequest = true;
          startRecording = false;
          toneStop();
          spkAbortSpeech();
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
      updateDisplayBrightnessMode();
      setEmotion(EMOTION_IDLE);
      returnIdleAt = 0;
    }
    break;

  case TOUCH_ACT_WEATHER:
    showClockScreen = false;
    showWeatherScreen = !showWeatherScreen;
    if (showWeatherScreen) {
      soundWeather();
      if (millis() - lastWeatherFetchAt > 600000UL || weatherCode < 0)
        fetchWeatherAsync();
    } else {
      updateDisplayBrightnessMode();
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
    if (busyProcessing) {
      Serial.println(
          "[TOUCH] Action DISMISS -> Cancel current request / listening");
      cancelRequest = true;
      startRecording = false;
      setEmotion(EMOTION_IDLE);
    } else if (startRecording) {
      Serial.println("[TOUCH] Action DISMISS -> Cancel pending listen");
      startRecording = false;
      soundPetSparkle();
      setEmotion(EMOTION_IDLE);
    } else if (showClockScreen) {
      showClockScreen = false;
      updateDisplayBrightnessMode();
    } else if (showWeatherScreen) {
      showWeatherScreen = false;
      updateDisplayBrightnessMode();
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
    if (raw) {
      portENTER_CRITICAL(&s_touchMux);
      ts.state = true;
      ts.startedAt = now;
      ts.holdHandled = false;
      ts.sleepHandled = false;
      portEXIT_CRITICAL(&s_touchMux);
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
        updateDisplayBrightnessMode();
        ts.holdHandled = true;
        ts.sleepHandled = true;
        ts.pendingSingleTap = false;
        bool isCheek = (ts.pin == PIN_TOUCH_LEFT || ts.pin == PIN_TOUCH_RIGHT);
        if (isCheek && !isKiwiAngry() && !busyProcessing &&
            !inCircadianSleep && !manualSleep &&
            cheekTapCount > 0 &&
            (now - lastCheekTouchAt < TICKLE_CHEEK_WINDOW_MS)) {
          cheekTapCount++;
          lastCheekTouchAt = now;
        } else if (isCheek) {
          cheekTapCount = 0;
        }
        setEmotion(EMOTION_IDLE);
        returnIdleAt = 0;
        soundPetSparkle();
        updateTonePlayer();
        return;
      }

      // Instant Audio & Visual Touch Response
      static uint32_t lastTouchBlipAt = 0;
      if (!inCircadianSleep && !manualSleep) {
        if (touchFeedbackSound && !busyProcessing && !isSpeechPlaying() &&
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

        // Left-Right alternating apology combo when angry (leave holdHandled unset
        // until reconciled so simultaneous dual-cheek hold can trigger handleAngryFeedAttempt)
        if (isKiwiAngry() &&
            (ts.pin == PIN_TOUCH_LEFT || ts.pin == PIN_TOUCH_RIGHT)) {
          touchL.pendingSingleTap = false;
          touchR.pendingSingleTap = false;
          cheekTapCount = 0;
          if (checkCheekComboLRLR()) {
            Serial.println("[TOUCH] Alternating cheek combo triggered! "
                           "Reconciling angry Kiwi!");
            ts.holdHandled = true;
            ts.sleepHandled = true;
            lastTickleTriggeredAt = now;
            s_tickleSuppressUntil = now + 10000UL; // 10s cooldown for tickle mode
            reconcileKiwi();
          }
          return;
        }

        // Rapid 4-tap cheek tickle tracking independent of tap/double-tap mappings
        if ((ts.pin == PIN_TOUCH_LEFT || ts.pin == PIN_TOUCH_RIGHT) &&
            !busyProcessing && !s_inTickleSpeech && !isSpeechPlaying() &&
            !isKiwiHungry()) {
          if (now - lastCheekTouchAt < TICKLE_CHEEK_WINDOW_MS) {
            cheekTapCount++;
          } else {
            cheekTapCount = 1;
          }
          lastCheekTouchAt = now;
          if (cheekTapCount >= 4 &&
              (s_tickleSuppressUntil == 0 ||
               (int32_t)(now - s_tickleSuppressUntil) >= 0) &&
              now - lastTickleTriggeredAt > TICKLE_COOLDOWN_MS) {
            showClockScreen = false;
            showWeatherScreen = false;
            updateDisplayBrightnessMode();
            touchL.pendingSingleTap = false;
            touchR.pendingSingleTap = false;
            ts.holdHandled = true;
            ts.sleepHandled = true;
            triggerTickleMode();
            return;
          }
        }
      }
    } else {
      portENTER_CRITICAL(&s_touchMux);
      ts.state = false;
      uint32_t held = now - ts.startedAt;
      portEXIT_CRITICAL(&s_touchMux);
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
        if (isKiwiAngry() &&
            (ts.pin == PIN_TOUCH_LEFT || ts.pin == PIN_TOUCH_RIGHT)) {
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
    extendTempWakeIfActive(CIRCADIAN_TEMP_WAKE_MS);
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
                   (s_tickleSuppressUntil == 0 ||
                    (int32_t)(now - s_tickleSuppressUntil) >= 0) &&
                   now - lastTickleTriggeredAt > TICKLE_COOLDOWN_MS &&
                   !inCircadianSleep && !manualSleep) {
          triggerTickleMode();
          return;
        }
      }
    }

    if (ts.pin == PIN_TOUCH_LEFT && held >= TOUCH_HOLD_MS && !ts.holdHandled &&
        !busyProcessing && !isSpeechPlaying() && !isKiwiAngry()) {
      ts.holdHandled = true;
      executeTouchAction(touchActLeftHold, ts.pin);
    }
    if (ts.pin == PIN_TOUCH_RIGHT && held >= TOUCH_HOLD_MS && !ts.holdHandled &&
        !busyProcessing && !isSpeechPlaying() && !isKiwiAngry()) {
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
      updateDisplayBrightnessMode();
      if (!inCircadianSleep && !manualSleep && !sleepyFallAsleepIsActive()) {
        startManualSleep();
        Serial.println("[TOUCH] Held to sleep -> Kiwi is falling asleep (Z Z)");
      } else {
        wakeKiwiFromSleep(true, CIRCADIAN_TEMP_WAKE_MS, true);
        Serial.println("[TOUCH] Held to wake -> Kiwi Woke Up!");
      }
    }
  }
}

void updateTouch() {
  updateOneTouch(touchL, millis());
  updateOneTouch(touchR, millis());
  updateOneTouch(touchT, millis());

  uint32_t now = millis();
  checkPendingSingleTap(touchL, now);
  checkPendingSingleTap(touchR, now);
  checkPendingSingleTap(touchT, now);
}
