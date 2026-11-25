#pragma once

#include <Arduino.h>

void initAnimations();
bool animationIsActive();
void playAnimation(uint8_t slot);
void stopAnimation();
void tickAnimation(uint32_t now);
String animationListJson();
void beginCustomAnimationUpload(const String &filename, const String &displayName = "");
bool writeCustomAnimationUpload(const uint8_t *data, size_t length);
bool finishCustomAnimationUpload();
