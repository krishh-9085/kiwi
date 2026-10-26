#pragma once
#include <Arduino.h>
#include "core/CoreTypes.h"

class Adafruit_SSD1306;

// ---------- Hardware & Initialization ----------
extern Adafruit_SSD1306 display;
extern bool oledOK;
bool initDisplay();
void setOledDim(bool dim);

// ---------- Face State & Animation ----------
extern FaceState faceNow, faceTgt;
void eyesInit(uint32_t now);
void eyesUpdateAnim(uint32_t now);
void setEmotion(Emotion e);
void faceTargetsForEmotion(Emotion e, PetMood pm);
void updateIdleVariety();
void maybeChangeEmotion();
void tickFace();
void drawFace();

// ---------- Specialized Screens & Dialogs ----------
void drawClockScreen();
void drawWeatherScreen();
void drawBannerScreen();
void drawMessage(const char *l1, const char *l2 = "");
void drawConnectingFrame(const char *targetSSID, int frame);
void drawConnectedNotice(const char *title, uint32_t holdMs = 800);
void showStatus(const char *l1, const char *l2, uint32_t ms);
void showBootInfo(uint32_t holdMs);
void showErrorFace(uint32_t ms);

// ---------- Emotion Action Helpers ----------
bool triggerEmotionByName(const String &nameIn, uint32_t holdMs, bool withSound);
void holdFace(uint32_t ms);
void showEmotionFor(Emotion e, uint32_t ms = 4000);
void showPetMood(PetMood m, uint32_t ms = 4000);
