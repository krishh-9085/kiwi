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

// Slanted eyelid mask (angry / sad / sleepy / thinking).
static inline void maskLids(int x0, int x1, int top, int lidIn, int lidOut,
                            bool isLeft) {
  if (lidIn <= 0 && lidOut <= 0)
    return;
  int lL = isLeft ? lidOut : lidIn;
  int lR = isLeft ? lidIn : lidOut;
  display.fillTriangle(x0 - 1, top - 1, x1 + 1, top - 1, x1 + 1, top + lR,
                       SSD1306_BLACK);
  display.fillTriangle(x0 - 1, top - 1, x1 + 1, top + lR, x0 - 1, top + lL,
                       SSD1306_BLACK);
}

// Cheek mask (happy / laughing arch).
static inline void maskCheek(int cx, int bot, int w, int cheek) {
  if (cheek > 1) {
    int R = w * 7 / 10;
    display.fillCircle(cx, bot - cheek + R, R, SSD1306_BLACK);
  }
}
#endif

#include "face/generated/face_ids.h"

typedef void (*ThemeDrawEyeFn)(int cx, int cy, int w, int h, int r, int lidIn,
                               int lidOut, int cheek, bool isLeft, bool glint);
typedef void (*ThemeDrawDecorFn)(uint32_t now, int cxL, int cxR, int cy, int w,
                                 int h);

struct FaceThemeDef {
  uint8_t id;
  const char *key;
  const char *statusName;
  int pickerOrder;
  int pageOrder;
  const char *dashboardLabel;
  const char *pageLabel;
  const char *const *execAliases;
  size_t execAliasCount;
  const char *const *webAliases;
  size_t webAliasCount;
  const char *previewSvg;
  ThemeDrawEyeFn drawEye;
  ThemeDrawDecorFn drawDecor;
};

#define FACE_THEME(name) const FaceThemeDef theme_##name =