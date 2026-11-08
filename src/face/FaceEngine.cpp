#include "face/FaceEngine.h"
#include "animation/AnimationEngine.h"
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

  // Keep existing speech-time gFaceFrameMs = 100 behavior working
  if (currentEmotion == EMOTION_SPEAKING ||
      (kiwiIsOutputting() && !isTonePlaying())) {
    gFaceFrameMs = FACE_FRAME_MS_IDLE; // 100 ms
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

void tickFace() {
    if (!oledOK || otaInProgress)
      return;
  updateFaceFrameRate();
  if (!bootWakeDone() && bootWakeTick()) {
    return;
  }
  uint32_t now = millis();
  if (bootInfoUntil) {
    if (now < bootInfoUntil)
      return;
    bootInfoUntil = 0;
    markDisplayDirty();
  }
  if (animationIsActive()) {
    // The animation engine draws straight to the panel, so make sure the next
    // face/screen frame is always pushed afterwards.
    markDisplayDirty();
    tickAnimation(now);
    return;
  }
  if (bannerActive) {
    if (now - lastFaceDrawAt < 33)
      return;
    lastFaceDrawAt = now;
    drawBannerScreen();
    return;
  }
  if (showClockScreen) {
    if (now - lastFaceDrawAt < 200)
      return;
    lastFaceDrawAt = now;
    drawClockScreen();
    return;
  }
  if (showWeatherScreen) {
    if (now - lastFaceDrawAt < 100)
      return;
    lastFaceDrawAt = now;
    drawWeatherScreen();
    return;
  }
  if (now - lastFaceDrawAt < gFaceFrameMs)
    return;
  lastFaceDrawAt = now;
  if (sleepyWakeIsActive()) {
    sleepyWakeDraw(display, now);
    return;
  }
  if (sleepyFallAsleepIsActive()) {
    sleepyFallAsleepDraw(display, now);
    return;
  }
  float sndLevel = soundLevelGet();
  eyesUpdateAnim(now);
  drawFace(sndLevel);
}