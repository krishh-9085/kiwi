#pragma once
#include <math.h>
#include <stddef.h>
#include <stdint.h>

#if defined(ARDUINO) || defined(ESP32)
#include "core/CoreTypes.h"
#include "face/FaceDisplay.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>

extern FaceState faceNow;
extern FaceState faceTgt;
extern uint32_t emotionStartedAt;
extern uint32_t petMoodT0;
extern uint32_t flourishT0;
extern Flourish flourish;
extern PetMood petMood;
extern bool showMouth;

void setFace(float w, float h, float gap, float y, float r, float lidIn = 0,
             float lidOut = 0, float cheek = 0, float blush = 0);
void drawMouthShape(int mx, int my, int mw, int mh, float curve);

static const uint8_t PROGMEM heartBmp[] = {0x6C, 0xFE, 0xFE, 0x7C, 0x38, 0x10};

static inline int minI(int a, int b) { return a < b ? a : b; }
static inline int maxI(int a, int b) { return a > b ? a : b; }
static inline float clampf(float v, float mn, float mx) {
  return v < mn ? mn : (v > mx ? mx : v);
}
static inline int iround(float v) { return (int)lroundf(v); }
static inline float swing(uint32_t now, float periodMs) {
  return sinf((float)(now % 600000UL) / periodMs * 6.2831853f);
}
static inline float cwing(uint32_t now, float periodMs) {
  return cosf((float)(now % 600000UL) / periodMs * 6.2831853f);
}
static inline float approach(float cur, float tgt, float rate, float dt) {
  return cur + (tgt - cur) * (1.0f - expf(-rate * dt));
}
#else
enum PetMood {
  PET_LOVE = 0,
  PET_GIGGLE,
  PET_SHY,
  PET_SPARKLE,
  PET_KISS,
  PET_CUDDLE,
  PET_PLAYFUL,
  PET_SLEEPY,
  PET_PROUD,
  PET_SURPRISED
};
#endif

#include "face/generated/face_ids.h"

typedef void (*EmotionTargetsFn)(PetMood pm);
typedef void (*EmotionMotionFn)(uint32_t now, float &offX, float &offY,
                                float &sc);
typedef void (*EmotionDecorFn)(uint32_t now, int cxL, int cxR, int cy, int w,
                               int h);

struct EmotionCardDef {
  const char *category;
  const char *dataE;
  const char *label;
  const char *desc;
  const char *previewKey;
  const char *previewSvg;
};

struct FaceEmotionDef {
  uint8_t id;
  const char *key;
  const char *statusName;
  const char *const *execNames;
  size_t execNameCount;
  bool skipsTransitionBlink;
  EmotionTargetsFn applyTargets;
  EmotionMotionFn motion;
  EmotionDecorFn decorations;
  const EmotionCardDef *cards;
  size_t cardCount;
};

#define FACE_EMOTION(name) const FaceEmotionDef emotion_##name =