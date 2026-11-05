#pragma once
#include "core/CoreTypes.h"
#include <Arduino.h>
#include <math.h>

struct FaceParams {
  float cx, cy, w, h, r, lidIn, lidOut, cheek;
};

struct EyeTween {
  FaceParams from;
  FaceParams to;
  FaceParams cur;
  uint32_t t0;
  uint16_t durMs;
  bool active;
};

static inline float easeInOutCubic(float t) {
  return (t < 0.5f) ? (4.0f * t * t * t)
                    : (1.0f - powf(-2.0f * t + 2.0f, 3.0f) / 2.0f);
}

static inline float lerpf(float a, float b, float k) { return a + (b - a) * k; }

void drawSkinAccents(uint32_t now, int cxL, int cxR, int cy, int w, int h,
                     int lidIn = 0, int lidOut = 0);

void drawFace();
