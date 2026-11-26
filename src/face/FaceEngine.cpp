#include "face/FaceEngine.h"
#include "face/AnimationEngine.h"
#include "config/AppConfig.h"
#include "face/BootWakeAnim.h"
#include "face/SleepyWakeAnim.h"
#include "hardware/AudioEngine.h"
#include "hardware/SoundLevel.h"
#include "hardware/TouchController.h"
#include "web/WebPortal.h"

void updateFaceFrameRate() {
  uint32_t now = millis();
  bool active = false;

  // Touch activity in the last 5 s
  if (now - lastUserInteractionAt < 5000) {
    active = true;
  }
  // Speaking or tones
  else if (kiwiIsOutputting() || isTonePlaying()) {
    active = true;
  }
  // Tickle active
  else if (now - lastTickleTriggeredAt < TICKLE_COOLDOWN_MS) {
    active = true;
  }
  // Boot wake animation active
  else if (!bootCompleted || !bootWakeDone()) {
    active = true;
  }
  // Emotion changed in the last 2 s
  else if (now - lastEmotionChangeAt < 2000) {
    active = true;
  }
  // Sound-reactive enabled and above noise gate
  else if (soundReactiveEnabled && soundLevelGet() > NOISE_GATE_THRESHOLD) {
    active = true;
  }
  // Sleepy or wake animation active
  else if (sleepyWakeIsActive() || sleepyFallAsleepIsActive()) {
    active = true;
  }

  // Run lip-sync & expressions smoothly at 30 FPS during active speech/output
  if (currentEmotion == EMOTION_SPEAKING ||
      (kiwiIsOutputting() && !isTonePlaying())) {
    gFaceFrameMs = FACE_FRAME_MS_ACTIVE; // 33 ms (~30 FPS)
  } else if (active) {
    gFaceFrameMs = FACE_FRAME_MS_ACTIVE; // 33 ms
  } else {
    gFaceFrameMs = FACE_FRAME_MS_IDLE; // 100 ms
  }

#if BOOT_TIMING_LOG
  static uint32_t lastFpsLogAt = 0;
  static uint32_t frameCount = 0;
  frameCount++;
  if (now - lastFpsLogAt >= 5000) {
    Serial.printf("[FACE] Frame rate: %lu ms target, %lu frames in 5s\n",
                  (unsigned long)gFaceFrameMs, (unsigned long)frameCount);
    frameCount = 0;
    lastFpsLogAt = now;
  }
#endif
}

#include "hardware/RgbLed.h"
#include "power/PowerManager.h"

void tickFace() {
  if (!oledOK || otaInProgress)
    return;
  if (!displayLock(0))
    return;

  updateFaceFrameRate();
  if (!bootWakeDone() && bootWakeTick()) {
    displayUnlock();
    return;
  }
  uint32_t now = millis();
  if (bootInfoUntil) {
    if (now < bootInfoUntil) {
      displayUnlock();
      return;
    }
    bootInfoUntil = 0;
    markDisplayDirty();
  }
  if (animationIsActive()) {
    // The animation engine draws straight to the panel, so make sure the next
    // face/screen frame is always pushed afterwards.
    markDisplayDirty();
    tickAnimation(now);
    displayUnlock();
    return;
  }
  if (bannerActive) {
    if (now - lastFaceDrawAt < 33) {
      displayUnlock();
      return;
    }
    lastFaceDrawAt = now;
    drawBannerScreen();
    displayUnlock();
    return;
  }
  if (showClockScreen) {
    if (now - lastFaceDrawAt < 200) {
      displayUnlock();
      return;
    }
    lastFaceDrawAt = now;
    drawClockScreen();
    displayUnlock();
    return;
  }
  if (showWeatherScreen) {
    if (now - lastFaceDrawAt < 100) {
      displayUnlock();
      return;
    }
    lastFaceDrawAt = now;
    drawWeatherScreen();
    displayUnlock();
    return;
  }
  if (now - lastFaceDrawAt < gFaceFrameMs) {
    displayUnlock();
    return;
  }
  lastFaceDrawAt = now;
  if (sleepyWakeIsActive()) {
    sleepyWakeDraw(display, now);
    displayUnlock();
    return;
  }
  if (sleepyFallAsleepIsActive()) {
    sleepyFallAsleepDraw(display, now);
    displayUnlock();
    return;
  }
  float sndLevel = soundLevelGet();
  eyesUpdateAnim(now);
  drawFace(sndLevel);
  displayUnlock();
}

static TaskHandle_t s_uiTaskHandle = NULL;

static void uiDisplayTask(void *params) {
  Serial.println("[UI_TASK] Dedicated Display & NeoPixel worker running on Core " + String(xPortGetCoreID()));
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(25); // 40 FPS

  for (;;) {
    if (bootCompleted && !powerIsHardwareSleeping()) {
      rgbLedUpdate();
      tickFace();
    }
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}

void startUiDisplayTask() {
  if (s_uiTaskHandle != NULL)
    return;
  BaseType_t res = xTaskCreatePinnedToCore(
      uiDisplayTask,
      "ui_disp_task",
      4096, // 4KB stack
      NULL,
      1,    // Priority 1
      &s_uiTaskHandle,
      0     // Core 0 (offloads display I2C completely from Core 1!)
  );
  if (res == pdPASS) {
    uiTaskRunning = true;
    Serial.println("[UI_TASK] Offloaded Display & LED to Core 0 (40 FPS)!");
  } else {
    Serial.println("[UI_TASK] Failed to create UI task, fallback to loop()");
  }
}