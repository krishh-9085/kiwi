#include "TouchController.h"
#include "Config.h"
#include "AnimationEngine.h"
#include "AudioEngine.h"
#include "FaceEngine.h"
#include "AiService.h"
#include <WiFi.h>
#include <esp_sleep.h>

TouchSensor touchL = {PIN_TOUCH_LEFT, false, false, 0, 0, false, false, false};
TouchSensor touchR = {PIN_TOUCH_RIGHT, false, false, 0, 0, false, false, false};
TouchSensor touchT = {PIN_TOUCH_TOP, false, false, 0, 0, false, false, false};

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

  touchR.state = false;
  touchR.lastRaw = digitalRead(PIN_TOUCH_RIGHT);
  touchR.holdHandled = false;
  touchR.sleepHandled = false;
  touchR.startedAt = 0;

  touchT.state = false;
  touchT.lastRaw = digitalRead(PIN_TOUCH_TOP);
  touchT.holdHandled = false;
  touchT.sleepHandled = false;
  touchT.startedAt = 0;
}

static void updateOneTouch(TouchSensor &ts, uint32_t now) {
  if (now < 2500)
    return;
  bool raw = digitalRead(ts.pin);
  if (raw != ts.lastRaw) {
    ts.lastRaw = raw;
    ts.changedAt = now;
  }
  if (now - ts.changedAt >= TOUCH_DEBOUNCE && raw != ts.state) {
    ts.state = raw;
    if (ts.state) {
      ts.startedAt = now;
      ts.holdHandled = false;
      ts.sleepHandled = false;
      returnIdleAt = 0;
      Serial.printf("[TOUCH] Pin %d PRESSED\n", ts.pin);
    } else {
      uint32_t held = now - ts.startedAt;
      Serial.printf("[TOUCH] Pin %d RELEASED after %lu ms\n", ts.pin, (unsigned long)held);
      uint32_t tapMax = (ts.pin == PIN_TOUCH_TOP) ? TOUCH_CLOCK_MS : TOUCH_TAP_MAX_MS;
      if (!ts.holdHandled && !ts.sleepHandled && held < tapMax) {
        lastUserInteractionAt = now;
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
        if (ts.pin == PIN_TOUCH_LEFT || ts.pin == PIN_TOUCH_RIGHT) {
          // Cheek tap sequence detection for Tickle Mode
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

          if (showClockScreen) {
            showClockScreen = false;
          } else if (showWeatherScreen) {
            showWeatherScreen = false;
          } else if (inCircadianSleep || manualSleep) {
            manualSleep = false;
            inCircadianSleep = false;
            circadianTempWakeUntil = millis() + CIRCADIAN_TEMP_WAKE_MS;
            setOledDim(false);
            setEmotion(EMOTION_HAPPY);
            soundPetSparkle();
            Serial.println("[TOUCH] Left/Right tap wake-up from Sleep Mode");
          } else if (currentEmotion != EMOTION_IDLE && !busyProcessing) {
            // Dismiss active emotion back to IDLE immediately on left/right tap
            setEmotion(EMOTION_IDLE);
            emotionAutoReturnAt = 0;
            returnIdleAt = 0;
            soundPetSparkle();
            Serial.println("[TOUCH] Left/Right tap dismissed active emotion -> IDLE");
          } else if (!busyProcessing) {
            gazeTgtX = (ts.pin == PIN_TOUCH_LEFT) ? -8.0f : 8.0f;
            gazeTgtY = 0;
            gazeLockUntil = millis() + 2500;
            faceTgt.gazeLock = false;
          }
        } else if (ts.pin == PIN_TOUCH_TOP) {
          if (showClockScreen)
            showClockScreen = false;
          else if (showWeatherScreen)
            showWeatherScreen = false;
          else if (inCircadianSleep || manualSleep) {
            manualSleep = false;
            inCircadianSleep = false;
            circadianTempWakeUntil = millis() + CIRCADIAN_TEMP_WAKE_MS;
            setOledDim(false);
            setEmotion(EMOTION_HAPPY);
            soundPetSparkle();
            Serial.println("[TOUCH] Tap wake-up from Sleep Mode");
          } else if (busyProcessing) {
            Serial.println("[TOUCH] Top tap -> Cancel active processing");
            cancelRequest = true;
            soundPetSparkle();
          } else {
            Serial.println("[TOUCH] Top tap -> Enter Listening Mode!");
            setEmotion(EMOTION_LISTENING);
            soundListening();
            startRecording = true;
          }
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

    if ((ts.pin == PIN_TOUCH_LEFT || ts.pin == PIN_TOUCH_RIGHT) &&
        held >= TOUCH_HOLD_MS && !ts.holdHandled && !busyProcessing) {
      ts.holdHandled = true;
      triggerPetReaction();
    }
    // Hold 3s -> Clock
    if (ts.pin == PIN_TOUCH_TOP && held >= TOUCH_CLOCK_MS && !ts.holdHandled && !busyProcessing) {
      ts.holdHandled = true;
      if (!showClockScreen && !inCircadianSleep && !manualSleep) {
        soundClock();
        showClockScreen = true;
        if (!timeSynced && WiFi.status() == WL_CONNECTED)
          syncTimeFromNTP();
      }
    }
    // Hold 7-10s -> Sleep Mode Toggle!
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
}
