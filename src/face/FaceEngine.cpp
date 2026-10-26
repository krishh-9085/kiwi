#include "face/FaceEngine.h"
#include "config/AppConfig.h"
#include "animation/AnimationEngine.h"

void tickFace() {
  if (!oledOK)
    return;
  if (bootInfoUntil) {
    if (millis() < bootInfoUntil)
      return;
    bootInfoUntil = 0;
  }
  if (animationIsActive()) {
    uint32_t now = millis();
    tickAnimation(now);
    return;
  }
  if (bannerActive) {
    uint32_t now = millis();
    if (now - lastFaceDrawAt < 35)
      return;
    lastFaceDrawAt = now;
    drawBannerScreen();
    return;
  }
  if (showClockScreen) {
    uint32_t now = millis();
    if (now - lastFaceDrawAt < 200)
      return;
    lastFaceDrawAt = now;
    drawClockScreen();
    return;
  }
  if (showWeatherScreen) {
    uint32_t now = millis();
    if (now - lastFaceDrawAt < 200)
      return;
    lastFaceDrawAt = now;
    drawWeatherScreen();
    return;
  }
  uint32_t now = millis();
  if (now - lastFaceDrawAt < gFaceFrameMs)
    return;
  lastFaceDrawAt = now;
  eyesUpdateAnim(now);
  drawFace();
}

