#include "face/FaceRenderer.h"
#include "config/AppConfig.h"
#include "face/BootWakeAnim.h"
#include "face/FaceDisplay.h"
#include "face/FaceExpressions.h"
#include "face/FaceIdle.h"
#include "face/FaceSkins.h"
#include "hardware/AudioEngine.h"
#include "hardware/SoundLevel.h"
#include "hardware/TouchController.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include <math.h>

// ---------------------------------------------------------------------------
// Small math helpers
// ---------------------------------------------------------------------------
static const float TWO_PI_F = 6.2831853f;
static const float PI_F = 3.1415927f;
static const float HALF_PI_F = 1.5707963f;

static inline float clampf(float v, float lo, float hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}
static inline int iround(float v) { return (int)lroundf(v); }
static inline int minI(int a, int b) { return a < b ? a : b; }
static inline int maxI(int a, int b) { return a > b ? a : b; }

// Frame-rate independent exponential smoothing.
static inline float approach(float cur, float tgt, float rate, float dt) {
  return cur + (tgt - cur) * (1.0f - expf(-rate * dt));
}

// sin()/cos() of (t / div) that stay precise after days of uptime: the phase is
// reduced with integer arithmetic before it ever reaches a float.
static inline uint32_t periodOf(float div) {
  uint32_t p = (uint32_t)(div * TWO_PI_F + 0.5f);
  return p ? p : 1;
}
static inline float swing(uint32_t t, float div) {
  uint32_t p = periodOf(div);
  return sinf(TWO_PI_F * (float)(t % p) / (float)p);
}
static inline float cwing(uint32_t t, float div) {
  uint32_t p = periodOf(div);
  return cosf(TWO_PI_F * (float)(t % p) / (float)p);
}

// Damped spring (semi-implicit Euler, sub-stepped so it is always stable).
// Gives sizes a lively, slightly elastic "squash & stretch" settle.
static void springStep(float &x, float &v, float tgt, float omega, float zeta,
                       float dt) {
  float left = dt;
  while (left > 0.0001f) {
    float h = left > 0.008f ? 0.008f : left;
    float a = omega * omega * (tgt - x) - 2.0f * zeta * omega * v;
    v += a * h;
    x += v * h;
    left -= h;
  }
}

// ---------------------------------------------------------------------------
// Face state integration
// ---------------------------------------------------------------------------
static uint32_t lastRenderAt = 0;
static float velW = 0, velH = 0, velGap = 0, velY = 0;
static float velGazeX = 0, velGazeY = 0;
static float speakW = 16, speakH = 3;
static uint32_t speakNextSaccadeAt = 0;
static float speakGazeTgtX = 0.0f;
static float speakGazeTgtY = -1.2f;
static bool speakLookingAtUser = true;

static void stepFace(float dt) {
  float omega = 24.0f;
  float zeta = 0.62f;
  if (currentEmotion == EMOTION_SURPRISED ||
      currentEmotion == EMOTION_SHOCKED || currentEmotion == EMOTION_SCARED ||
      currentEmotion == EMOTION_EXCITED || currentEmotion == EMOTION_PARTY) {
    omega = 29.0f;
    zeta = 0.52f;
  } else if (currentEmotion == EMOTION_BORED || currentEmotion == EMOTION_SAD ||
             currentEmotion == EMOTION_CRYING ||
             currentEmotion == EMOTION_SLEEPING ||
             currentEmotion == EMOTION_RELIEVED) {
    omega = 16.0f;
    zeta = 0.84f;
  }

  springStep(faceNow.w, velW, faceTgt.w, omega, zeta, dt);
  springStep(faceNow.h, velH, faceTgt.h, omega, zeta, dt);
  springStep(faceNow.gap, velGap, faceTgt.gap, omega, zeta, dt);
  springStep(faceNow.y, velY, faceTgt.y, omega, zeta, dt);
  if (faceNow.w < 2.0f)
    faceNow.w = 2.0f;
  if (faceNow.h < 0.0f)
    faceNow.h = 0.0f;

  const float R = 11.5f;
  faceNow.r = approach(faceNow.r, faceTgt.r, R, dt);
  faceNow.lidIn = approach(faceNow.lidIn, faceTgt.lidIn, R, dt);
  faceNow.lidOut = approach(faceNow.lidOut, faceTgt.lidOut, R, dt);
  faceNow.lidInR = approach(faceNow.lidInR, faceTgt.lidInR, R, dt);
  faceNow.lidOutR = approach(faceNow.lidOutR, faceTgt.lidOutR, R, dt);
  faceNow.cheek = approach(faceNow.cheek, faceTgt.cheek, R, dt);
  faceNow.cheekR = approach(faceNow.cheekR, faceTgt.cheekR, R, dt);
  faceNow.hR = approach(faceNow.hR, faceTgt.hR, R, dt);
  faceNow.wR = approach(faceNow.wR, faceTgt.wR, R, dt);
  faceNow.headTilt = approach(faceNow.headTilt, faceTgt.headTilt, 10.0f, dt);
  faceNow.glint = approach(faceNow.glint, faceTgt.glint, 13.0f, dt);
  faceNow.blush = approach(faceNow.blush, faceTgt.blush, 5.5f, dt);
  faceNow.mouthW = approach(faceNow.mouthW, faceTgt.mouthW, R, dt);
  faceNow.mouthH = approach(faceNow.mouthH, faceTgt.mouthH, R, dt);
  faceNow.mouthCurve = approach(faceNow.mouthCurve, faceTgt.mouthCurve, R, dt);
  faceNow.mouthY = approach(faceNow.mouthY, faceTgt.mouthY, R, dt);
  faceNow.mouthStyle = faceTgt.mouthStyle;
  faceNow.pupilScale =
      approach(faceNow.pupilScale, faceTgt.pupilScale, 9.0f, dt);
}

// ---------------------------------------------------------------------------
// Small drawing pieces
// ---------------------------------------------------------------------------
static void drawBlushMarks(int cx, int cy, float amt) {
  if (amt < 0.2f)
    return;
  int n = (amt > 0.7f) ? 3 : 2;
  for (int i = 0; i < n; i++)
    display.drawFastHLine(cx - 6 + (i & 1) * 3, cy + i * 3, 9, SSD1306_WHITE);
}

// Mouth shapes supporting expressive primitives + emotion-colored open shapes
static void drawMouthShape(int cx, int cy, int w, int h, float curve,
                           uint8_t style) {
  if (w < 4)
    return;
  int halfW = w / 2;
  if (halfW < 2)
    halfW = 2;
  float ac = fabsf(curve);

  if (style == MOUTH_KISS) {
    int r = maxI(2, minI(halfW, maxI(2, h / 2)));
    display.drawCircle(cx, cy, r, SSD1306_WHITE);
    if (r >= 3) {
      display.drawPixel(cx - 1, cy - 1, SSD1306_BLACK);
    }
    return;
  }

  if (style == MOUTH_WAVY) {
    int amp = clampf((float)maxI(1, h / 2), 1.0f, 3.0f);
    for (int x = -halfW; x <= halfW; x++) {
      float t = (float)x / (float)halfW;
      int y = cy + iround(sinf(t * TWO_PI_F) * (float)amp);
      int nx = (x < 0) ? x + 1 : (x > 0 ? x - 1 : 0);
      float tn = (float)nx / (float)halfW;
      int yn = cy + iround(sinf(tn * TWO_PI_F) * (float)amp);
      int lo = minI(y, yn);
      int hi = maxI(y, yn);
      display.drawFastVLine(cx + x, lo, (hi - lo) + 2, SSD1306_WHITE);
    }
    return;
  }

  if (style == MOUTH_CAT) {
    int amp = maxI(2, minI(4, h));
    int lobeW = maxI(1, halfW / 2);
    auto catY = [&](int x) -> int {
      float u = (x < 0) ? ((float)(x + lobeW) / (float)lobeW)
                        : ((float)(x - lobeW) / (float)lobeW);
      u = clampf(u, -1.0f, 1.0f);
      return cy + iround((1.0f - u * u) * (float)amp) - 1;
    };
    for (int x = -halfW; x <= halfW; x++) {
      int y = catY(x);
      int nx = (x < 0) ? x + 1 : (x > 0 ? x - 1 : 0);
      int yn = catY(nx);
      int lo = minI(y, yn);
      int hi = maxI(y, yn);
      display.drawFastVLine(cx + x, lo, (hi - lo) + 2, SSD1306_WHITE);
    }
    return;
  }

  if (style == MOUTH_SMIRK) {
    int amp = maxI(2, minI(4, h + 1));
    for (int x = -halfW; x <= halfW; x++) {
      float t = ((float)x / (float)halfW + 1.0f) * 0.5f; // 0..1 left->right
      int y = cy - iround(curve * amp * (t * t));
      display.drawFastVLine(cx + x, y, 2, SSD1306_WHITE);
    }
    return;
  }

  if (style == MOUTH_TONGUE && h >= 4) {
    int top = cy - h / 4;
    for (int x = -halfW; x <= halfW; x++) {
      float t = (float)x / (float)halfW;
      int lift = iround(curve * 2.0f * t * t);
      int depth = iround(h * sqrtf(fmaxf(0.0f, 1.0f - t * t)));
      if (depth < 1)
        depth = 1;
      display.drawFastVLine(cx + x, top - lift, depth, SSD1306_WHITE);
    }
    // Cute rounded tongue peeking out of bottom center
    int tw = minI(10, maxI(6, w / 3));
    int th = 4;
    int tx = cx - tw / 2;
    int ty = cy + h / 2 - 1;
    display.fillRoundRect(tx, ty, tw, th, 2, SSD1306_BLACK);
    display.drawRoundRect(tx, ty, tw, th, 2, SSD1306_WHITE);
    display.drawFastVLine(cx, ty, 2, SSD1306_WHITE);
    return;
  }

  // Smiling open D-mouth
  if (h >= 5 && curve > 0.45f) {
    int top = cy - h / 4;
    for (int x = -halfW; x <= halfW; x++) {
      float t = (float)x / (float)halfW;
      int lift = iround(curve * 2.0f * t * t); // corners curl upward
      int depth = iround(h * sqrtf(fmaxf(0.0f, 1.0f - t * t)));
      if (depth < 1)
        depth = 1;
      display.drawFastVLine(cx + x, top - lift, depth, SSD1306_WHITE);
    }
    if (h >= 7 && w >= 14) {
      int tw = maxI(4, w / 4);
      display.fillRoundRect(cx - tw / 2, top + h - 3, tw, 2, 1, SSD1306_BLACK);
    }
    return;
  }

  // Frowning / angry open mouth (upturned arch with downturned corners)
  if (h >= 5 && curve < -0.45f) {
    int bot = cy + h / 4;
    for (int x = -halfW; x <= halfW; x++) {
      float t = (float)x / (float)halfW;
      int drop = iround(-curve * 2.0f * t * t);
      int depth = iround(h * sqrtf(fmaxf(0.0f, 1.0f - t * t)));
      if (depth < 1)
        depth = 1;
      display.drawFastVLine(cx + x, bot + drop - depth, depth, SSD1306_WHITE);
    }
    return;
  }

  if (h >= 4 && ac <= 0.45f) {
    int rad = minI(w, h) / 2;
    int rx = cx - halfW;
    int ry = cy - h / 2;
    int rw = halfW * 2;
    // Elegant hollow round outline with bold 2-pixel lip border
    display.fillRoundRect(rx, ry, rw, h, rad, SSD1306_BLACK);
    display.drawRoundRect(rx, ry, rw, h, rad, SSD1306_WHITE);
    if (h >= 6 && rw >= 8) {
      display.drawRoundRect(rx + 1, ry + 1, rw - 2, h - 2, maxI(1, rad - 1),
                            SSD1306_WHITE);
    }
    if (h >= 9 && rw >= 12) {
      int tw = maxI(4, rw / 3);
      display.drawFastHLine(cx - tw / 2, ry + h - 3, tw, SSD1306_WHITE);
    }
    return;
  }

  int thick = (h < 1) ? 1 : h;
  if (thick > 3)
    thick = 3;
  int amp = thick * 2;
  if (amp < 2)
    amp = 2;
  for (int x = -halfW; x <= halfW; x++) {
    float t = (float)x / (float)halfW;
    int y = cy + iround(curve * amp * (1.0f - t * t));
    int nx = (x < 0) ? x + 1 : (x > 0 ? x - 1 : 0); // neighbour toward centre
    float tn = (float)nx / (float)halfW;
    int yn = cy + iround(curve * amp * (1.0f - tn * tn));
    int lo = y < yn ? y : yn;
    int hi = y > yn ? y : yn;
    display.drawFastVLine(cx + x, lo, (hi - lo) + thick, SSD1306_WHITE);
  }
}

static const uint8_t PROGMEM heartBmp[] = {0x6C, 0xFE, 0xFE, 0x7C, 0x38, 0x10};

static void drawOneFlourishGlyph(Flourish fl, int x, int y, bool smallOnly) {
  switch (fl) {
  case FL_HEART:
    if (smallOnly) {
      display.drawPixel(x - 1, y + 1, SSD1306_WHITE);
      display.drawPixel(x + 1, y + 1, SSD1306_WHITE);
      display.drawFastHLine(x - 1, y + 2, 3, SSD1306_WHITE);
      display.drawPixel(x, y + 3, SSD1306_WHITE);
    } else {
      display.drawBitmap(x - 3, y, heartBmp, 7, 6, SSD1306_WHITE);
    }
    break;
  case FL_SPARKLE:
    if (smallOnly) {
      display.drawFastHLine(x - 1, y + 2, 3, SSD1306_WHITE);
      display.drawFastVLine(x, y + 1, 3, SSD1306_WHITE);
    } else {
      display.drawFastHLine(x - 2, y + 3, 5, SSD1306_WHITE);
      display.drawFastVLine(x, y + 1, 5, SSD1306_WHITE);
    }
    break;
  case FL_NOTE:
    display.fillCircle(x - 2, y + 4, smallOnly ? 1 : 2, SSD1306_WHITE);
    display.drawFastVLine(x, y, smallOnly ? 4 : 5, SSD1306_WHITE);
    if (!smallOnly)
      display.drawFastHLine(x, y, 3, SSD1306_WHITE);
    break;
  case FL_MOON:
    display.drawCircle(x, y + 3, 3, SSD1306_WHITE);
    display.fillCircle(x + 1, y + 2, 2, SSD1306_BLACK);
    break;
  case FL_STAR:
    display.drawPixel(x, y, SSD1306_WHITE);
    display.drawPixel(x, y + 2, SSD1306_WHITE);
    display.drawPixel(x, y + 4, SSD1306_WHITE);
    display.drawPixel(x - 2, y + 2, SSD1306_WHITE);
    display.drawPixel(x + 2, y + 2, SSD1306_WHITE);
    break;
  default:
    break;
  }
}

static void drawFlourishAt(uint32_t now) {
  if (flourish == FL_NONE)
    return;
  uint32_t dt = now - flourishT0;
  if (dt > 1300) {
    flourish = FL_NONE;
    return;
  }
  if (currentEmotion == EMOTION_SLEEPING)
    return;
  float p = dt / 1300.0f;
  if (p > 0.85f)
    return;
  // Ease-out rise with a little side-to-side float
  float e = 1.0f - (1.0f - p) * (1.0f - p);
  int y = 50 - iround(e * 26.0f);
  int dx = iround(swing(dt, 120.0f) * 3.0f * p);
  // Flicker out near the end instead of vanishing abruptly
  if (!(p > 0.7f && ((now / 60) & 1))) {
    drawOneFlourishGlyph(flourish, 64 + dx, y, false);
  }
  // Secondary trailing mini-particle for extra sparkle
  if (dt > 180) {
    float p2 = (float)(dt - 180) / 1120.0f;
    if (p2 < 0.80f && !(p2 > 0.65f && (((now + 30) / 60) & 1))) {
      float e2 = 1.0f - (1.0f - p2) * (1.0f - p2);
      int y2 = 52 - iround(e2 * 22.0f);
      int dx2 = -iround(swing(dt, 140.0f) * 4.0f * p2);
      drawOneFlourishGlyph(flourish, 64 + 12 + dx2, y2, true);
    }
  }
}

// Custom clean vector Z renderer (crisp 1px lines on 128x64 OLED)
void drawCrispZ(int x, int y, int size) {
  if (x < 0 || x + size > 128 || y < 0 || y + size > 64)
    return;
  display.drawFastHLine(x, y, size, SSD1306_WHITE);
  display.drawFastHLine(x, y + size - 1, size, SSD1306_WHITE);
  display.drawLine(x + size - 1, y, x, y + size - 1, SSD1306_WHITE);
  if (size >= 8) {
    display.drawLine(x + size - 2, y, x - 1, y + size - 1, SSD1306_WHITE);
  }
}

static void drawSleepingFace(uint32_t now) {
  display.clearDisplay();

  // 1. Organic, peaceful respiratory rhythm (~3.8s period = ~16 breaths/min)
  // Asymmetric cycle: gentle inhale lift (38%), relaxed exhale settle (62%)
  uint32_t breathTime = now % 3800;
  float phase = (float)breathTime / 3800.0f;
  float breath;
  if (phase < 0.38f) {
    breath = sinf((phase / 0.38f) * HALF_PI_F); // 0.0 -> 1.0 inhale
  } else {
    breath = cosf(((phase - 0.38f) / 0.62f) * HALF_PI_F); // 1.0 -> 0.0 exhale
  }

  // Smooth chest/face elevation (0 to 2 pixels)
  int by = iround(breath * 2.0f);

  // 2. Rare REM dream flutter: every 10 seconds, a subtle 180ms micro-flutter
  int flutter = 0;
  if ((now % 10000 < 180) && ((now / 45) & 1)) {
    flutter = 1;
  }

  int sleepY = 32 + by - flutter;
  const int gapVal = 28;
  const int cL = 64 - gapVal; // 36
  const int cR = 64 + gapVal; // 92

  // 3. Flat sleeping eye bars ("asleep": { "w": 30, "h": 3, "rt": 1, "rb": 1 })
  const int SLEEP_W = 30;
  const int SLEEP_H = 3;
  const int SLEEP_R = 1;
  display.fillRoundRect(cL - SLEEP_W / 2, sleepY - 1, SLEEP_W, SLEEP_H, SLEEP_R,
                        SSD1306_WHITE);
  display.fillRoundRect(cR - SLEEP_W / 2, sleepY - 1, SLEEP_W, SLEEP_H, SLEEP_R,
                        SSD1306_WHITE);

  // 4. Two floating Z snore glyphs rising from right eye corner
  for (int i = 0; i < 2; i++) {
    uint32_t zTime = (now + (uint32_t)i * 1500) % 3000;
    float zp = (float)zTime / 3000.0f; // 0.0 to 1.0

    // Drift upward and to the right from the right eye corner
    float sway = sinf(zp * PI_F) * 2.0f;
    int zx = 106 + iround(zp * 10.0f + sway);
    int zy = 24 - iround(zp * 20.0f);

    if (zy >= 2 && zy <= 30 && zx <= 120) {
      int zSize = (zp < 0.5f) ? 5 : 7;
      bool visible = (zp <= 0.85f) || (((now / 50) & 1) != 0);
      if (visible) {
        drawCrispZ(zx, zy, zSize);
      }
    }
  }

  display.setTextSize(1);
  presentDisplay();
}

// ---------------------------------------------------------------------------
// Skin accents and decorations
// ---------------------------------------------------------------------------
void drawSkinAccents(uint32_t now, int cxL, int cxR, int cy, int w, int h,
                     int lidIn, int lidOut) {
  // Accents reserved for future themes
}

// ---------------------------------------------------------------------------
// Main face renderer
// ---------------------------------------------------------------------------
static EyeSkin lastEyeSkin = (EyeSkin)-1;

static void drawDizzySpiral(int cx, int cy, int maxR, uint32_t now,
                            bool reverse) {
  float spin = (float)(now % 600000UL) * (reverse ? -0.014f : 0.014f);
  int prevX = cx, prevY = cy;
  const int STEPS = 36;
  for (int i = 1; i <= STEPS; i++) {
    float frac = (float)i / (float)STEPS;
    float ang = frac * TWO_PI_F * 2.25f + spin;
    float rad = frac * (float)maxR;
    int px = cx + iround(cosf(ang) * rad);
    int py = cy + iround(sinf(ang) * rad);
    display.drawLine(prevX, prevY, px, py, SSD1306_WHITE);
    prevX = px;
    prevY = py;
  }
}

static void drawCoolSunglasses(int cxL, int cyL, int cxR, int cyR, int wL,
                               int wR, int hL, int hR, uint32_t now) {
  int lw = maxI(30, wL);
  int rw = maxI(30, wR);
  int lh = iround(clampf((float)maxI(18, hL), 18.0f, 26.0f));
  int rh = iround(clampf((float)maxI(18, hR), 18.0f, 26.0f));
  int lx0 = cxL - lw / 2;
  int ly0 = cyL - lh / 2;
  int rx0 = cxR - rw / 2;
  int ry0 = cyR - rh / 2;

  // Solid bright frame + dark tinted inner lens
  display.fillRoundRect(lx0, ly0, lw, lh, 5, SSD1306_WHITE);
  display.fillRoundRect(lx0 + 2, ly0 + 2, lw - 4, lh - 4, 3, SSD1306_BLACK);
  display.fillRoundRect(rx0, ry0, rw, rh, 5, SSD1306_WHITE);
  display.fillRoundRect(rx0 + 2, ry0 + 2, rw - 4, rh - 4, 3, SSD1306_BLACK);

  // Nose bridge & temple arms
  int bridgeX = lx0 + lw - 1;
  int bridgeW = (rx0 + 1) - bridgeX;
  if (bridgeW > 0) {
    display.fillRect(bridgeX, cyL - 5, bridgeW, 3, SSD1306_WHITE);
  }
  display.drawLine(lx0 - 5, cyL - 7, lx0 + 1, cyL - 3, SSD1306_WHITE);
  display.drawLine(lx0 - 5, cyL - 6, lx0 + 1, cyL - 2, SSD1306_WHITE);
  display.drawLine(rx0 + rw + 5, cyR - 7, rx0 + rw - 1, cyR - 3, SSD1306_WHITE);
  display.drawLine(rx0 + rw + 5, cyR - 6, rx0 + rw - 1, cyR - 2, SSD1306_WHITE);

  // Sliding diagonal lens shine sweep
  float phase = (float)(now % 2600UL) / 2600.0f;
  int sweep = (phase < 0.45f) ? iround((phase / 0.45f) * (lw - 10)) : -99;
  if (sweep >= 0) {
    int sxL = lx0 + 4 + sweep;
    int sxR = rx0 + 4 + sweep;
    if (sxL + 5 < lx0 + lw - 2) {
      display.drawLine(sxL, ly0 + lh - 4, sxL + 5, ly0 + 3, SSD1306_WHITE);
      display.drawLine(sxL + 1, ly0 + lh - 4, sxL + 6, ly0 + 3, SSD1306_WHITE);
    }
    if (sxR + 5 < rx0 + rw - 2) {
      display.drawLine(sxR, ry0 + rh - 4, sxR + 5, ry0 + 3, SSD1306_WHITE);
      display.drawLine(sxR + 1, ry0 + rh - 4, sxR + 6, ry0 + 3, SSD1306_WHITE);
    }
  } else {
    // Resting crisp lens reflection line
    display.drawLine(lx0 + 6, ly0 + lh - 5, lx0 + 11, ly0 + 4, SSD1306_WHITE);
    display.drawLine(rx0 + 6, ry0 + rh - 5, rx0 + 11, ry0 + 4, SSD1306_WHITE);
  }
}

void drawFace(float soundLevel) {
  if (!oledOK)
    return;
  if (bootInfoUntil && millis() < bootInfoUntil)
    return;
  uint32_t now = millis();

  float eyeOffset = 0.0f;
  float mouthPulse = 0.0f;
  if (soundReactiveEnabled && bootWakeDone() && soundLevel > SOUND_REACT_ACTIVE_LEVEL) {
    float norm = (soundLevel - SOUND_REACT_ACTIVE_LEVEL) / (1.0f - SOUND_REACT_ACTIVE_LEVEL);
    if (norm > 1.0f)
      norm = 1.0f;
    if (norm > 0.78f) {
      // Brief startled flinch on very loud claps/spikes
      eyeOffset = -(norm - 0.78f) * 14.0f;
    } else {
      // Attentive wide-eyed listening perk on ambient voices
      eyeOffset = norm * (float)SOUND_REACT_MAX_OPEN;
    }
    mouthPulse = norm * 2.0f;
  }

  bool skinChanged = false;
  if (lastEyeSkin != currentEyeSkin) {
    skinChanged = true;
    lastEyeSkin = currentEyeSkin;
  }

  float dt =
      (lastRenderAt == 0) ? 0.033f : (float)(now - lastRenderAt) / 1000.0f;
  lastRenderAt = now;
  dt = clampf(dt, 0.001f, 0.08f);

  if (faceTweenMs <= 0 || skinChanged) {
    faceNow = faceTgt;
    velW = velH = velGap = velY = 0.0f;
  } else {
    float speedScale = clampf(220.0f / (float)faceTweenMs, 0.35f, 2.5f);
    stepFace(dt * speedScale);
  }

  if (currentEmotion == EMOTION_SLEEPING) {
    drawSleepingFace(now);
    return;
  }

  // ---- Gaze -------------------------------------------------------------
  float tx = gazeTgtX, ty = gazeTgtY;
  bool speechGazeActive = (currentEmotion == EMOTION_SPEAKING) ||
                          (isSpeechPlaying() && !faceTgt.gazeLock);

  if (faceTgt.gazeLock) {
    tx = faceTgt.lockX;
    ty = faceTgt.lockY;
  } else if (currentEmotion != EMOTION_THINKING &&
             currentEmotion != EMOTION_LISTENING && !speechGazeActive) {
    tx += gazeMicroX; // involuntary micro-saccades
    ty += gazeMicroY;
  }
  if (currentEmotion == EMOTION_THINKING) {
    // Calm, pensive upward-corner gaze
    tx = 2.2f + swing(now, 2600.0f) * 0.4f;
    ty = -3.8f + swing(now, 2000.0f) * 0.3f;
  } else if (currentEmotion == EMOTION_LISTENING) {
    // Attentive listening gaze: looking slightly up towards user's face
    tx = swing(now, 500.0f) * 1.5f;
    ty = -4.5f;
  } else if (speechGazeActive) {
    // Dynamic conversational gaze: mostly direct eye contact with user,
    // punctuated by realistic thinking saccades and speech-reactive micro-nods.
    if (now >= speakNextSaccadeAt) {
      if (!speakLookingAtUser) {
        speakLookingAtUser = true;
        speakGazeTgtX = (float)random(-8, 9) * 0.1f;
        speakGazeTgtY = -1.2f + (float)random(-4, 5) * 0.1f;
        speakNextSaccadeAt = now + 1500 + random(0, 1500);
        if (!eyeBlinking && random(0, 100) < 35)
          triggerTouchBlink(now);
      } else {
        speakLookingAtUser = false;
        int r = random(0, 100);
        if (r < 45) {
          speakGazeTgtX = (random(0, 2) == 0) ? -4.5f : 4.5f;
          speakGazeTgtY = -3.2f;
          speakNextSaccadeAt = now + 500 + random(0, 500);
        } else if (r < 75) {
          speakGazeTgtX = (random(0, 2) == 0) ? -5.2f : 5.2f;
          speakGazeTgtY = -0.5f;
          speakNextSaccadeAt = now + 400 + random(0, 450);
        } else {
          speakGazeTgtX = (float)random(-15, 16) * 0.1f;
          speakGazeTgtY = 2.0f;
          speakNextSaccadeAt = now + 350 + random(0, 400);
        }
        if (!eyeBlinking && random(0, 100) < 25)
          triggerTouchBlink(now);
      }
    }

    float spk = getSpeechAmplitude();
    float driftX = swing(now, 750.0f) * 0.35f;
    float driftY = cwing(now, 820.0f) * 0.25f;
    tx = speakGazeTgtX + driftX;
    ty = speakGazeTgtY + driftY + (spk * 1.6f);
  }
  // Ballistic biological saccade spring (omega ~32 rad/s, zeta ~0.68)
  // Rapid acceleration, crisp ~5% micro-overshoot and elastic settle!
  const float SACCADE_OMEGA = 32.0f;
  const float SACCADE_ZETA = 0.68f;
  springStep(gazeX, velGazeX, tx, SACCADE_OMEGA, SACCADE_ZETA, dt);
  springStep(gazeY, velGazeY, ty, SACCADE_OMEGA, SACCADE_ZETA, dt);

  // ---- Per-emotion body motion -----------------------------------------
  float offX = 0, offY = 0, sc = 1.0f;
  switch (currentEmotion) {
  case EMOTION_IDLE: {
    // Continuous respiratory breath cycle (~3.2s period) + organic desk sigh
    // offset
    float breath = swing(now, 510.0f);
    offY = breath * 0.70f + idleSighY;
    sc = 1.0f + 0.02f * breath;
    break;
  }
  case EMOTION_LISTENING:
    sc = 1.0f + 0.035f * swing(now, 160.0f);
    break;
  case EMOTION_SPEAKING: {
    // Natural conversational speech cadence & head bounce tied to live speech
    // energy!
    float spk = getSpeechAmplitude();
    offY = (swing(now, 450.0f) * 0.5f + spk * 3.2f);
    sc = 1.0f + 0.015f * swing(now, 380.0f) + spk * 0.04f;
    break;
  }
  case EMOTION_EXCITED:
    offY = swing(now, 110.0f) * 2.0f;
    break;
  case EMOTION_ERROR: {
    uint32_t t = now - emotionStartedAt;
    if (t < 800) {
      float damp = 1.0f - (float)t / 800.0f; // shake decays smoothly
      offX = sinf((float)t * 0.11f) * 3.0f * damp;
    }
    break;
  }
  case EMOTION_PETTED: {
    uint32_t t = now - petMoodT0;
    switch (petMood) {
    case PET_LOVE:
      offX = swing(t, 230.0f) * 2.5f;
      break;
    case PET_GIGGLE:
      offY = swing(t, 65.0f) * 2.0f;
      break;
    case PET_SPARKLE:
      sc = 1.0f + 0.05f * swing(t, 90.0f);
      break;
    case PET_CUDDLE:
      sc = 1.0f + 0.02f * swing(t, 500.0f);
      break;
    case PET_PLAYFUL:
      offX = swing(t, 180.0f) * 3.0f;
      break;
    case PET_SLEEPY:
      offY = swing(t, 900.0f) * 1.2f;
      break;
    case PET_PROUD:
      offY = -1 + swing(t, 700.0f) * 0.5f;
      break;
    case PET_SURPRISED:
      sc = 1.0f + 0.03f * swing(t, 150.0f);
      break;
    default:
      break;
    }
    break;
  }
  case EMOTION_SURPRISED:
    sc = 1.0f + 0.03f * swing(now, 200.0f);
    break;
  case EMOTION_WINK:
    offX = -2;
    break;
  case EMOTION_SMUG:
    offY = -1;
    break;
  case EMOTION_CURIOUS:
    offX = swing(now, 500.0f) * 3.0f;
    break;
  case EMOTION_BORED:
    offY = swing(now, 1400.0f) * 0.8f;
    break;
  case EMOTION_LAUGHING:
    offY = swing(now, 130.0f) * 2.5f;
    break;
  case EMOTION_SCARED: {
    uint32_t t = now - emotionStartedAt;
    if (t < 500) {
      float damp = 1.0f - (float)t / 500.0f;
      offX = sinf((float)t * 0.14f) * 3.5f * damp;
    }
    break;
  }
  case EMOTION_SUSPICIOUS:
    offX = swing(now, 900.0f) * 2.0f;
    break;
  case EMOTION_RELIEVED:
    sc = 1.0f + 0.02f * swing(now, 800.0f);
    break;
  case EMOTION_PROUD:
    offY = -2;
    break;
  case EMOTION_DIZZY:
    offX = swing(now, 180.0f) * 4.0f;
    offY = cwing(now, 180.0f) * 3.0f;
    break;
  case EMOTION_LOVESTRUCK:
    sc = 1.0f + 0.04f * swing(now, 500.0f);
    break;
  case EMOTION_EVIL:
    offY = swing(now, 150.0f) * 1.5f;
    break;
  case EMOTION_CRYING:
    offY = swing(now, 90.0f) * 1.5f;
    break;
  case EMOTION_HEARTS:
    sc = 1.0f + 0.05f * swing(now, 350.0f);
    break;
  case EMOTION_SINGING:
    offX = swing(now, 250.0f) * 3.0f;
    offY = fabsf(cwing(now, 250.0f)) * 2.0f;
    break;
  case EMOTION_PARTY:
    offY = -fabsf(swing(now, 120.0f)) * 3.5f;
    break;
  case EMOTION_NERVOUS:
    offX = ((now / 40) & 1) ? 1.5f : -1.5f;
    break;
  case EMOTION_COOL:
    offY = -1;
    break;
  case EMOTION_CUTE:
    sc = 1.0f + 0.03f * swing(now, 300.0f);
    break;
  default:
    break;
  }

  bool hasLivelyBounce =
      (currentEmotion == EMOTION_HAPPY || currentEmotion == EMOTION_EXCITED ||
       currentEmotion == EMOTION_PETTED || currentEmotion == EMOTION_PARTY ||
       currentEmotion == EMOTION_SINGING ||
       currentEmotion == EMOTION_LAUGHING || currentEmotion == EMOTION_CUTE ||
       currentEmotion == EMOTION_HEARTS ||
       currentEmotion == EMOTION_LOVESTRUCK ||
       currentEmotion == EMOTION_PROUD || currentEmotion == EMOTION_RELIEVED ||
       currentEmotion == EMOTION_CURIOUS || currentEmotion == EMOTION_WINK ||
       currentEmotion == EMOTION_SMUG || currentEmotion == EMOTION_COOL ||
       currentEmotion == EMOTION_SURPRISED);
  if (hasLivelyBounce) {
    offY += swing(now, 480.0f) * 1.3f;
  }
  if (isSpeechPlaying() && currentEmotion != EMOTION_SPEAKING) {
    float spk = getSpeechAmplitude();
    offY += spk * 2.8f;
    sc += spk * 0.035f;
  }

  // ---- Eye geometry -----------------------------------------------------
  float open = clampf(eyeOpenAmt, 0.0f, 1.0f);
  // Elastic Squash & Stretch: width expands up to +7.5% at closure and springs
  // back!
  float squash = 1.0f + 0.075f * (1.0f - open) * (1.0f - open);
  int w = iround(faceNow.w * sc * squash);
  int wL = w;
  int wR = iround(w * faceNow.wR);

  // Eyelid-Gaze Coupling: Looking UP widens eye (+2.5px); Looking DOWN droops
  // upper lid (-2.5px)
  float couplingH = 0.0f;
  float couplingLid = 0.0f;
  if (gazeY < 0.0f) {
    // Looking up: attentive & curious opening
    couplingH = -gazeY * 0.38f;
    if (couplingH > 2.5f)
      couplingH = 2.5f;
  } else if (gazeY > 0.0f) {
    // Looking down: relaxed desk posture, upper eyelids follow gaze downward
    couplingH = -gazeY * 0.45f;
    if (couplingH < -2.5f)
      couplingH = -2.5f;
    couplingLid = gazeY * 0.35f;
  }

  float hBase = faceNow.h * sc;
  float hBaseReact = hBase + eyeOffset + couplingH;
  if (hBaseReact < 4.0f)
    hBaseReact = 4.0f;
  int r = iround(faceNow.r);
  int gap = iround(faceNow.gap);

  // 3D Spherical Parallax: Outer eye socket / sclera shifts at 0.45x gaze speed
  int scleraShiftX = iround((gazeX + offX) * 0.45f);
  int scleraShiftY = iround((gazeY + offY) * 0.45f);
  int tiltPx = iround(faceNow.headTilt);
  int cy = iround(faceNow.y + scleraShiftY);
  int cyL = cy - tiltPx;
  int cyR = cy + tiltPx;
  int cxL = 64 - gap + scleraShiftX;
  int cxR = 64 + gap + scleraShiftX;

  float hLf = hBaseReact * open;
  float hRf = hBaseReact * faceNow.hR * open;
  float speechCheekScale =
      (isSpeechPlaying() && currentEmotion == EMOTION_HAPPY) ? 0.45f : 1.0f;
  float cheekL = faceNow.cheek * speechCheekScale * open;
  float cheekR = faceNow.cheekR * speechCheekScale * open;
  if (eyeBlinking) {
    // Arch (happy) eyes never fully disappear while blinking
    if (faceNow.cheek >= 2 && hLf < 3)
      hLf = 3;
    if (faceNow.cheekR >= 2 && hRf < 3)
      hRf = 3;
  }

  // Active micro-wink during non-idle emotions: smooth close and re-open
  if (activeWinkEnd > now) {
    float rem = (float)(activeWinkEnd - now);
    float t = clampf(1.0f - rem / 260.0f, 0.0f, 1.0f);
    float wink = clampf(sinf(PI_F * t) * 1.6f, 0.0f, 1.0f);
    if (activeWinkLeft) {
      hLf += (3.0f - hLf) * wink;
      cheekL *= (1.0f - wink);
    } else {
      hRf += (3.0f - hRf) * wink;
    }
  }

  int h = iround(hBase);
  int hL = minI(iround(hLf), SCREEN_HEIGHT - 4);
  int hR = minI(iround(hRf), SCREEN_HEIGHT - 4);

  // Clamp eye centers cleanly inside the 128x64 OLED so upward bounces,
  // wide-eyed emotions, or gaze shifts never clip or overflow past row 0.
  int minCyL = hL / 2 + 1;
  int maxCyL = SCREEN_HEIGHT - 2 - hL / 2;
  if (cyL < minCyL)
    cyL = minCyL;
  else if (cyL > maxCyL)
    cyL = maxCyL;

  int minCyR = hR / 2 + 1;
  int maxCyR = SCREEN_HEIGHT - 2 - hR / 2;
  if (cyR < minCyR)
    cyR = minCyR;
  else if (cyR > maxCyR)
    cyR = maxCyR;

  cy = (cyL + cyR) / 2;

  float extraLid = couplingLid + idleSighLid + idleYawnLid + idleSleepyDroop;
  int lidInL = maxI(0, iround(faceNow.lidIn + extraLid));
  int lidOutL = maxI(0, iround(faceNow.lidOut + extraLid));
  int lidInR = maxI(0, iround(faceNow.lidInR + extraLid));
  int lidOutR = maxI(0, iround(faceNow.lidOutR + extraLid));

  display.clearDisplay();
  bool gl = faceNow.glint > 0.5f;
  if (currentEmotion == EMOTION_COOL) {
    drawCoolSunglasses(cxL, cyL, cxR, cyR, wL, wR, hL, hR, now);
  } else if (currentEmotion == EMOTION_DIZZY) {
    int maxR = maxI(8, minI(wL, hL) / 2 - 1);
    drawDizzySpiral(cxL, cyL, maxR, now, false);
    drawDizzySpiral(cxR, cyR, maxR, now, true);
  } else if (currentEmotion == EMOTION_DEAD) {
    for (int d = -1; d <= 1; d++) {
      display.drawLine(cxL - 8 + d, cyL - 8, cxL + 8 + d, cyL + 8,
                       SSD1306_WHITE);
      display.drawLine(cxL - 8 + d, cyL + 8, cxL + 8 + d, cyL - 8,
                       SSD1306_WHITE);
      display.drawLine(cxR - 8 + d, cyR - 8, cxR + 8 + d, cyR + 8,
                       SSD1306_WHITE);
      display.drawLine(cxR - 8 + d, cyR + 8, cxR + 8 + d, cyR - 8,
                       SSD1306_WHITE);
    }
  } else {
    drawEye(cxL, cyL, wL, hL, r, lidInL, lidOutL, iround(cheekL), true, gl);
    drawEye(cxR, cyR, wR, hR, r, lidInR, lidOutR, iround(cheekR), false, gl);
    drawSkinAccents(now, cxL, cxR, cy, w, h, lidInL, lidOutL);
  }

  if (faceNow.blush > 0.2f) {
    drawBlushMarks(cxL, cyL + h / 2 + 4, faceNow.blush);
    drawBlushMarks(cxR, cyR + h / 2 + 4, faceNow.blush);
  }

  // ---- Mouth (follows the head a little for a subtle 3D parallax) ------
  int mx = 64 + iround((gazeX + offX) * 0.35f);
  int myBase = 52 + iround(offY * 0.6f + gazeY * 0.25f);
  bool speechActive = isSpeechPlaying();
  float toneSpk =
      (!speechActive && isTonePlaying()) ? getToneMouthAmplitude() : 0.0f;
  bool mouthTalking =
      (currentEmotion == EMOTION_SPEAKING) || speechActive || (toneSpk > 0.02f);

  if (currentEmotion == EMOTION_IDLE && idleYawnMouth > 0.08f) {
    int mw = 8 + iround(idleYawnMouth * 6.0f);
    int mh = 3 + iround(idleYawnMouth * 8.0f);
    drawMouthShape(mx, myBase, mw, mh, 0.0f, MOUTH_NORMAL);
  } else if (mouthTalking) {
    float activeAmp = speechActive ? getSpeechAmplitude() : toneSpk;
    float bright = speechActive ? getSpeechBrightness()
                                : (1.0f - getTonePitchNorm() * 0.5f);
    if (activeAmp < 0.015f) {
      // Natural pause between words: smoothly settle into expression's resting
      // curve
      float restW = fmaxf(14.0f, faceNow.mouthW * 0.85f);
      speakW = approach(speakW, restW, 34.0f, dt);
      speakH = approach(speakH, 2.0f, 38.0f, dt);
      int mw = iround(speakW);
      drawMouthShape(mx, myBase + iround(faceNow.mouthY), mw, 2,
                     faceNow.mouthCurve, faceNow.mouthStyle);
    } else {
      // Real DMA-aligned audio-driven viseme lip sync!
      float linSpk = clampf((activeAmp - 0.015f) * 4.2f, 0.0f, 1.0f);
      float normSpk =
          powf(linSpk, 0.72f); // Gentle boost so normal speech opens clearly
      float t = (float)(now % 600000UL) / 1000.0f;
      float nuance = 0.90f + 0.10f * sinf(t * 14.5f);

      // Spectral shape: vowels -> taller rounder mouth, sibilants -> wider
      // tighter slit
      float wShape = 0.88f + 0.34f * bright;
      float hShape = 1.15f - 0.38f * bright;
      float tw = (13.0f + 8.5f * normSpk * nuance) * wShape;
      float th = (4.5f + 11.5f * normSpk) * hShape;
      speakW = approach(speakW, clampf(tw, 9.0f, 28.0f), 36.0f, dt);
      speakH = approach(speakH, clampf(th, 3.0f, 16.0f), 48.0f, dt);
      int mw = iround(speakW);
      int mh = iround(speakH);
      int myTalk = myBase + iround(faceNow.mouthY);

      if (mh <= 3) {
        drawMouthShape(mx, myTalk, mw, 2, faceNow.mouthCurve * 0.85f,
                       MOUTH_NORMAL);
      } else if (faceNow.mouthCurve > 0.40f) {
        drawMouthShape(mx, myTalk, mw, mh, faceNow.mouthCurve, MOUTH_NORMAL);
      } else if (faceNow.mouthCurve < -0.40f) {
        drawMouthShape(mx, myTalk, mw, mh, faceNow.mouthCurve, MOUTH_NORMAL);
      } else {
        drawMouthShape(mx, myTalk, mw, mh, 0.0f, MOUTH_NORMAL);
      }
    }
  } else if (showMouth && faceNow.mouthW > 3) {
    drawMouthShape(mx, myBase + iround(faceNow.mouthY), iround(faceNow.mouthW),
                   iround(faceNow.mouthH + mouthPulse), faceNow.mouthCurve,
                   faceNow.mouthStyle);
  }

  if (currentEmotion == EMOTION_LISTENING) {
    // -----------------------------------------------------------------------
    // MINIMAL & MODERN VOICE DOTS (Google / Siri / ChatGPT style):
    // 4 smooth rounded glowing capsules reacting to live microphone energy!
    // -----------------------------------------------------------------------
    const int centerY = 53;
    float liveMic = fmaxf(soundLevel, soundLevelGetListening());
    bool locked = soundLevelIsSpeechLocked();
    float t = (float)(now % 100000UL) / (locked ? 95.0f : 150.0f);
    float liveSnd = clampf(liveMic * 12.0f, locked ? 1.5f : 0.0f, 10.0f);

    const int dotX[4] = {50, 58, 66, 74};
    for (int i = 0; i < 4; i++) {
      float phase = (float)i * 0.85f;
      float wave = 0.5f + 0.5f * sinf(t * 3.6f - phase);
      int dotH = 4 + iround(wave * (3.0f + liveSnd));
      if (dotH > 16)
        dotH = 16;
      int rx = dotX[i];
      int ry = centerY - dotH / 2;
      display.fillRoundRect(rx, ry, 4, dotH, 2, SSD1306_WHITE);
    }
  }

  if (currentEmotion == EMOTION_ERROR && !eyeBlinking) {
    if (((now / 260) & 1) == 0 && hL > 12) {
      // Draw bold high-contrast '!' badge in center between eyes so it pops on
      // all 5 skins
      display.fillRoundRect(60, cy - 8, 8, 15, 2, SSD1306_BLACK);
      display.drawRoundRect(60, cy - 8, 8, 15, 2, SSD1306_WHITE);
      display.fillRect(63, cy - 5, 2, 5, SSD1306_WHITE);
      display.fillRect(63, cy + 2, 2, 2, SSD1306_WHITE);
    }
  }

  if (currentEmotion == EMOTION_THINKING) {
    float p = (float)(now % 1200UL) / 1200.0f;
    if (p > 0.15f)
      display.fillCircle(114, 15, 1, SSD1306_WHITE);
    if (p > 0.45f)
      display.fillCircle(118, 10, 1, SSD1306_WHITE);
    if (p > 0.72f)
      display.fillCircle(122, 5, 2, SSD1306_WHITE);
  }

  if (currentEmotion == EMOTION_EVIL) {
    display.fillTriangle(cxL - 10, cyL - h / 2 - 1, cxL - 16, cyL - h / 2 - 9,
                         cxL - 4, cyL - h / 2 - 1, SSD1306_WHITE);
    display.fillTriangle(cxR + 4, cyR - h / 2 - 1, cxR + 16, cyR - h / 2 - 9,
                         cxR + 10, cyR - h / 2 - 1, SSD1306_WHITE);
  }

  if (currentEmotion == EMOTION_CRYING) {
    int tearY = (int)((now / 40) % 18);
    int tearY2 = (tearY + 9) % 18;
    // Tears shrink as they fall so they seem to drip away
    display.fillCircle(cxL - w / 4, cyL + h / 2 + 2 + tearY, tearY > 12 ? 1 : 2,
                       SSD1306_WHITE);
    display.fillCircle(cxR + w / 4, cyR + h / 2 + 2 + tearY2,
                       tearY2 > 12 ? 1 : 2, SSD1306_WHITE);
  }

  if (currentEmotion == EMOTION_HEARTS) {
    int hY = 10 + iround(swing(now, 180.0f) * 3.0f);
    display.drawBitmap(12, hY, heartBmp, 7, 6, SSD1306_WHITE);
    display.drawBitmap(108, hY, heartBmp, 7, 6, SSD1306_WHITE);
  }

  if (currentEmotion == EMOTION_SINGING) {
    int noteY = 12 + iround(swing(now, 150.0f) * 4.0f);
    display.fillCircle(112, noteY + 4, 2, SSD1306_WHITE);
    display.drawFastVLine(114, noteY, 5, SSD1306_WHITE);
    display.drawFastHLine(114, noteY, 3, SSD1306_WHITE);
  }

  if (currentEmotion == EMOTION_NERVOUS) {
    int swY = 10 + iround(swing(now, 200.0f) * 2.0f);
    display.fillCircle(112, swY + 3, 2, SSD1306_WHITE);
    display.drawPixel(112, swY, SSD1306_WHITE);
  }

  if (currentEmotion == EMOTION_PARTY) {
    if (((now / 150) % 2) == 0) {
      display.drawPixel(16, 12, SSD1306_WHITE);
      display.drawPixel(112, 14, SSD1306_WHITE);
      display.drawPixel(20, 20, SSD1306_WHITE);
      display.drawPixel(108, 22, SSD1306_WHITE);
    } else {
      display.drawPixel(14, 16, SSD1306_WHITE);
      display.drawPixel(114, 10, SSD1306_WHITE);
      display.drawPixel(18, 24, SSD1306_WHITE);
      display.drawPixel(110, 18, SSD1306_WHITE);
    }
  }

  drawFlourishAt(now);

  float holdProg = getTopTouchHoldProgress();
  if (holdProg > 0.01f) {
    int bw = maxI(4, iround(60.0f * clampf(holdProg, 0.0f, 1.0f)));
    display.drawRoundRect(33, 60, 62, 4, 1, SSD1306_WHITE);
    display.fillRect(34, 61, bw, 2, SSD1306_WHITE);
  }

  presentDisplay(false);
}