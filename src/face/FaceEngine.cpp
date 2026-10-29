#include "face/FaceEngine.h"
#include "animation/AnimationEngine.h"
#include "config/AppConfig.h"


void tickFace() {
  if (!oledOK)
    return;
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
  eyesUpdateAnim(now);
  drawFace();
}