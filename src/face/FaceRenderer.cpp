#include "face/FaceRenderer.h"
#include "config/AppConfig.h"
#include "face/FaceDisplay.h"
#include "face/FaceExpressions.h"
#include "face/FaceIdle.h"
#include "face/FaceSkins.h"
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
static float speakW = 16, speakH = 3;

static void stepFace(float dt) {
  const float OMEGA = 24.0f; // spring stiffness (rad/s)
  const float ZETA = 0.62f;  // damping ratio (<1 = slight overshoot)
  springStep(faceNow.w, velW, faceTgt.w, OMEGA, ZETA, dt);
  springStep(faceNow.h, velH, faceTgt.h, OMEGA, ZETA, dt);
  springStep(faceNow.gap, velGap, faceTgt.gap, OMEGA, ZETA, dt);
  springStep(faceNow.y, velY, faceTgt.y, OMEGA, ZETA, dt);
  if (faceNow.w < 2.0f)
    faceNow.w = 2.0f;
  if (faceNow.h < 0.0f)
    faceNow.h = 0.0f;

  const float R = 11.0f; // ~ the old 0.28/frame at ~30 fps
  faceNow.r = approach(faceNow.r, faceTgt.r, R, dt);
  faceNow.lidIn = approach(faceNow.lidIn, faceTgt.lidIn, R, dt);
  faceNow.lidOut = approach(faceNow.lidOut, faceTgt.lidOut, R, dt);
  faceNow.cheek = approach(faceNow.cheek, faceTgt.cheek, R, dt);
  faceNow.cheekR = approach(faceNow.cheekR, faceTgt.cheekR, R, dt);
  faceNow.hR = approach(faceNow.hR, faceTgt.hR, R, dt);
  faceNow.glint = approach(faceNow.glint, faceTgt.glint, 13.0f, dt);
  faceNow.blush = approach(faceNow.blush, faceTgt.blush, 5.5f, dt);
  faceNow.mouthW = approach(faceNow.mouthW, faceTgt.mouthW, R, dt);
  faceNow.mouthH = approach(faceNow.mouthH, faceTgt.mouthH, R, dt);
  faceNow.mouthCurve = approach(faceNow.mouthCurve, faceTgt.mouthCurve, R, dt);
  faceNow.mouthY = approach(faceNow.mouthY, faceTgt.mouthY, R, dt);
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

// Mouth shapes:
//  - open + smiling  -> D-shaped laugh
//  - open + neutral  -> oval
//  - otherwise       -> a connected, symmetric parabolic line
static void drawMouthShape(int cx, int cy, int w, int h, float curve) {
  if (w < 4)
    return;
  int halfW = w / 2;
  if (halfW < 2)
    halfW = 2;
  float ac = fabsf(curve);

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
    return;
  }
  if (h >= 4 && ac <= 0.45f) {
    int rad = minI(w, h) / 2;
    display.fillRoundRect(cx - halfW, cy - h / 2, halfW * 2, h, rad,
                          SSD1306_WHITE);
    return;
  }

  int thick = (h < 1) ? 1 : h;
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

static void drawFlourishAt(uint32_t now) {
  if (flourish == FL_NONE)
    return;
  uint32_t dt = now - flourishT0;
  if (dt > 1300) {
    flourish = FL_NONE;
    return;
  }
  if (currentEmotion != EMOTION_PETTED)
    return;
  float p = dt / 1300.0f;
  if (p > 0.85f)
    return;
  // Ease-out rise with a little side-to-side float
  float e = 1.0f - (1.0f - p) * (1.0f - p);
  int y = 50 - iround(e * 24.0f);
  int dx = iround(swing(dt, 120.0f) * 3.0f * p);
  // Flicker out near the end instead of vanishing abruptly
  if (p > 0.7f && ((now / 60) & 1))
    return;
  int x = 64 + dx;
  switch (flourish) {
  case FL_HEART:
    display.drawBitmap(x - 3, y, heartBmp, 7, 6, SSD1306_WHITE);
    break;
  case FL_SPARKLE:
    display.drawFastHLine(x - 2, y + 3, 5, SSD1306_WHITE);
    display.drawFastVLine(x, y + 1, 5, SSD1306_WHITE);
    break;
  case FL_NOTE:
    display.fillCircle(x - 2, y + 4, 2, SSD1306_WHITE);
    display.drawFastVLine(x, y, 5, SSD1306_WHITE);
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

// Custom clean vector Z renderer (crisp 1px lines on 128x64 OLED)
static void drawCrispZ(int x, int y, int size) {
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
  display.fillRoundRect(cL - SLEEP_W / 2, sleepY - 1, SLEEP_W, SLEEP_H, SLEEP_R, SSD1306_WHITE);
  display.fillRoundRect(cR - SLEEP_W / 2, sleepY - 1, SLEEP_W, SLEEP_H, SLEEP_R, SSD1306_WHITE);

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
// Main face renderer
// ---------------------------------------------------------------------------
void drawFace() {
  if (!oledOK)
    return;
  if (bootInfoUntil && millis() < bootInfoUntil)
    return;
  uint32_t now = millis();

  float dt =
      (lastRenderAt == 0) ? 0.033f : (float)(now - lastRenderAt) / 1000.0f;
  lastRenderAt = now;
  dt = clampf(dt, 0.001f, 0.05f);

  // Keep integrating even while asleep so waking up animates from a closed eye
  stepFace(dt);

  if (currentEmotion == EMOTION_SLEEPING) {
    drawSleepingFace(now);
    return;
  }

  // ---- Gaze -------------------------------------------------------------
  float tx = gazeTgtX, ty = gazeTgtY;
  if (faceTgt.gazeLock) {
    tx = faceTgt.lockX;
    ty = faceTgt.lockY;
  } else if (currentEmotion != EMOTION_THINKING &&
             currentEmotion != EMOTION_LISTENING &&
             currentEmotion != EMOTION_SPEAKING) {
    tx += gazeMicroX; // involuntary micro-saccades
    ty += gazeMicroY;
  }
  switch (currentEmotion) {
  case EMOTION_THINKING: {
    // Looking UP while pondering with a gentle left/right scan
    tx = swing(now, 750.0f) * 5.5f + cwing(now, 416.0f) * 1.6f;
    ty = -6.5f + swing(now, 577.0f) * 1.2f;
    break;
  }
  case EMOTION_LISTENING:
    tx = swing(now, 500.0f) * 2.0f;
    ty = -3.0f;
    break;
  case EMOTION_SPEAKING: {
    // Engaged with the user, fluidly glancing left/right while speaking
    tx = swing(now, 580.0f) * 6.2f + swing(now, 276.0f) * 2.0f;
    ty = swing(now, 414.0f) * 1.6f;
    break;
  }
  default:
    break;
  }
  // Fast, saccade-like gaze motion (tau ~70 ms)
  gazeX = approach(gazeX, tx, 14.0f, dt);
  gazeY = approach(gazeY, ty, 14.0f, dt);

  // ---- Per-emotion body motion -----------------------------------------
  float offX = 0, offY = 0, sc = 1.0f;
  switch (currentEmotion) {
  case EMOTION_IDLE:
    sc = 1.0f + 0.03f * swing(now, 900.0f);
    break;
  case EMOTION_LISTENING:
    sc = 1.0f + 0.035f * swing(now, 160.0f);
    break;
  case EMOTION_SPEAKING:
    // Subtle conversational speech cadence and head bounce
    offY = swing(now, 150.0f) * 1.2f;
    sc = 1.0f + 0.025f * swing(now, 210.0f);
    break;
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

  if (currentEmotion != EMOTION_IDLE) {
    offY += swing(now, 480.0f) * 1.5f;
  }

  // ---- Eye geometry -----------------------------------------------------
  float open = clampf(eyeOpenAmt, 0.0f, 1.0f);
  // Eyes bulge a hair wider as the lids close (squash & stretch)
  int w = iround(faceNow.w * sc * (1.0f + 0.05f * (1.0f - open)));
  float hBase = faceNow.h * sc;
  int r = iround(faceNow.r);
  int gap = iround(faceNow.gap);
  int cy = iround(faceNow.y + gazeY + offY);
  int cxL = 64 - gap + iround(gazeX + offX);
  int cxR = 64 + gap + iround(gazeX + offX);

  float hLf = hBase * open;
  float hRf = hBase * faceNow.hR * open;
  float cheekL = faceNow.cheek * open;
  float cheekR = faceNow.cheekR * open;
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
      cheekR *= (1.0f - wink);
    }
  }

  int h = iround(hBase);
  int hL = iround(hLf);
  int hR = iround(hRf);
  int lidIn = iround(faceNow.lidIn);
  int lidOut = iround(faceNow.lidOut);

  display.clearDisplay();
  bool gl = faceNow.glint > 0.5f;
  drawEye(cxL, cy, w, hL, r, lidIn, lidOut, iround(cheekL), true, gl);
  drawEye(cxR, cy, w, hR, r, lidIn, lidOut, iround(cheekR), false, gl);

  // ---- Custom skin accents (cat ears, HUD frames, horns ...) -----------
  if (currentEyeSkin == SKIN_CAT) {
    float earTwitch = swing(now, 900.0f);
    int twL = iround(clampf((earTwitch - 0.75f) / 0.25f, 0.0f, 1.0f) * 3.0f);
    int twR = iround(clampf((-earTwitch - 0.75f) / 0.25f, 0.0f, 1.0f) * 3.0f);
    int earBaseY = cy - h / 2;
    if (earBaseY > 15) {
      display.fillTriangle(cxL - w / 2 - 2, earBaseY, cxL - w / 4,
                           earBaseY - 12 - twL, cxL + 4, earBaseY,
                           SSD1306_WHITE);
      display.fillTriangle(cxL - w / 2, earBaseY, cxL - w / 4,
                           earBaseY - 9 - twL, cxL + 1, earBaseY,
                           SSD1306_BLACK);
      display.fillTriangle(cxR - 4, earBaseY, cxR + w / 4, earBaseY - 12 - twR,
                           cxR + w / 2 + 2, earBaseY, SSD1306_WHITE);
      display.fillTriangle(cxR - 1, earBaseY, cxR + w / 4, earBaseY - 9 - twR,
                           cxR + w / 2, earBaseY, SSD1306_BLACK);
    }
  } else if (currentEyeSkin == SKIN_CYBERPUNK) {
    display.drawFastHLine(3, 3, 7, SSD1306_WHITE);
    display.drawFastVLine(3, 3, 7, SSD1306_WHITE);
    display.drawFastHLine(118, 3, 7, SSD1306_WHITE);
    display.drawFastVLine(121, 3, 7, SSD1306_WHITE);
    display.drawFastHLine(3, 60, 7, SSD1306_WHITE);
    display.drawFastVLine(3, 54, 7, SSD1306_WHITE);
    display.drawFastHLine(118, 60, 7, SSD1306_WHITE);
    display.drawFastVLine(121, 54, 7, SSD1306_WHITE);
  } else if (currentEyeSkin == SKIN_DEMON) {
    int hornBaseY = cy - h / 2 - 1;
    if (hornBaseY > 14) {
      display.fillTriangle(cxL - 8, hornBaseY, cxL - 16, hornBaseY - 13,
                           cxL - 2, hornBaseY, SSD1306_WHITE);
      display.fillTriangle(cxR + 2, hornBaseY, cxR + 16, hornBaseY - 13,
                           cxR + 8, hornBaseY, SSD1306_WHITE);
    }
  } else if (currentEyeSkin == SKIN_STEAMPUNK) {
    int bridgeY = cy;
    int bw = (cxR - w / 2) - (cxL + w / 2);
    display.drawFastHLine(cxL + w / 2 + 1, bridgeY - 2, bw - 2, SSD1306_WHITE);
    display.drawFastHLine(cxL + w / 2 + 1, bridgeY + 2, bw - 2, SSD1306_WHITE);
    display.drawFastHLine(cxL + w / 2 + 3, bridgeY, bw - 6, SSD1306_WHITE);
  } else if (currentEyeSkin == SKIN_MATRIX) {
    display.drawFastHLine(36, 1, 56, SSD1306_WHITE);
    display.drawFastVLine(36, 1, 4, SSD1306_WHITE);
    display.drawFastVLine(91, 1, 4, SSD1306_WHITE);
    display.drawPixel(6, 4, SSD1306_WHITE);
    display.drawPixel(10, 2, SSD1306_WHITE);
    display.drawPixel(118, 4, SSD1306_WHITE);
    display.drawPixel(122, 2, SSD1306_WHITE);
  } else if (currentEyeSkin == SKIN_CYBORG) {
    display.drawCircle(118, 9, 6, SSD1306_WHITE);
    display.drawFastHLine(110, 9, 17, SSD1306_WHITE);
    display.drawFastVLine(118, 1, 17, SSD1306_WHITE);
    display.drawPixel(118, 9, SSD1306_WHITE);
    display.drawFastHLine(2, 62, 12, SSD1306_WHITE);
    display.drawFastVLine(2, 54, 8, SSD1306_WHITE);
  } else if (currentEyeSkin == SKIN_MECHA) {
    int vY = cy - h / 2 - 2;
    if (vY > 10) {
      display.fillTriangle(63, vY, 48, vY - 9, 60, vY - 5, SSD1306_WHITE);
      display.fillTriangle(65, vY, 80, vY - 9, 68, vY - 5, SSD1306_WHITE);
      display.fillTriangle(61, vY - 2, 64, vY - 8, 67, vY - 2, SSD1306_WHITE);
      display.fillTriangle(61, vY - 2, 64, vY + 2, 67, vY - 2, SSD1306_WHITE);
    }
  } else if (currentEyeSkin == SKIN_SUNGLASSES) {
    int bridgeY = cy - 2;
    display.fillRect(cxL + w / 2 - 1, bridgeY,
                     (cxR - w / 2) - (cxL + w / 2) + 2, 4, SSD1306_WHITE);
    display.drawFastHLine(cxL - w / 2, cy - h / 2 - 1,
                          (cxR + w / 2) - (cxL - w / 2) + 1, SSD1306_WHITE);
  } else if (currentEyeSkin == SKIN_PUPPY) {
    int sway = iround(swing(now, 800.0f) * 2.0f);
    display.fillRoundRect(2, cy - 8 + sway, 9, 22, 4, SSD1306_WHITE);
    display.fillRoundRect(3, cy - 5 + sway, 7, 16, 3, SSD1306_BLACK);
    display.fillRoundRect(117, cy - 8 - sway, 9, 22, 4, SSD1306_WHITE);
    display.fillRoundRect(118, cy - 5 - sway, 7, 16, 3, SSD1306_BLACK);
    display.fillTriangle(62, cy + h / 2 + 3, 66, cy + h / 2 + 3, 64,
                         cy + h / 2 + 6, SSD1306_WHITE);
  } else if (currentEyeSkin == SKIN_HYPNO) {
    float a = (float)(now % 1000) / 1000.0f * TWO_PI_F;
    int dX = iround(cosf(a) * 4);
    int dY = iround(sinf(a) * 2);
    display.drawPixel(20 + dX, 8 + dY, SSD1306_WHITE);
    display.drawPixel(108 - dX, 8 - dY, SSD1306_WHITE);
  } else if (currentEyeSkin == SKIN_ALIEN) {
    display.drawFastVLine(64, 1, 9, SSD1306_WHITE);
    display.drawCircle(64, 2, 2, SSD1306_WHITE);
    if ((now / 250) % 2 == 0) {
      display.fillCircle(64, 2, 1, SSD1306_WHITE);
      display.drawFastHLine(61, 2, 7, SSD1306_WHITE);
    }
  } else if (currentEyeSkin == SKIN_NINJA) {
    display.drawFastHLine(0, 6, 128, SSD1306_WHITE);
    display.drawFastHLine(0, 7, 128, SSD1306_WHITE);
    display.fillRoundRect(48, 2, 32, 9, 2, SSD1306_BLACK);
    display.drawRoundRect(48, 2, 32, 9, 2, SSD1306_WHITE);
    display.drawPixel(50, 4, SSD1306_WHITE);
    display.drawPixel(78, 4, SSD1306_WHITE);
    display.drawPixel(50, 8, SSD1306_WHITE);
    display.drawPixel(78, 8, SSD1306_WHITE);
    display.drawLine(60, 6, 68, 6, SSD1306_WHITE);
    display.drawLine(66, 4, 62, 8, SSD1306_WHITE);
  } else if (currentEyeSkin == SKIN_ROUND) {
    // Dynamic floating micro-brows that articulate with emotion
    int browY = cy - h / 2 - 3;
    if (browY >= 2 && h > 10) {
      int tiltL = iround((faceNow.lidIn - faceNow.lidOut) / 3.0f);
      int tiltR = iround((faceNow.lidOut - faceNow.lidIn) / 3.0f);
      display.drawLine(cxL - 7, browY + tiltL, cxL + 7, browY - tiltL,
                       SSD1306_WHITE);
      display.drawLine(cxR - 7, browY - tiltR, cxR + 7, browY + tiltR,
                       SSD1306_WHITE);
    }
  }

  if (faceNow.blush > 0.2f) {
    drawBlushMarks(cxL, cy + h / 2 + 4, faceNow.blush);
    drawBlushMarks(cxR, cy + h / 2 + 4, faceNow.blush);
  }

  // ---- Mouth (follows the head a little for a subtle 3D parallax) ------
  int mx = 64 + iround((gazeX + offX) * 0.35f);
  int myBase = 52 + iround(offY * 0.6f + gazeY * 0.25f);

  if (currentEmotion == EMOTION_SPEAKING) {
    // Syllable-like envelope from incommensurate sines, then smoothed so the
    // mouth flows instead of flickering.
    float t = (float)(now % 600000UL) / 1000.0f;
    float env =
        fabsf(sinf(t * 12.4f)) * (0.55f + 0.45f * sinf(t * 3.3f + 1.0f));
    float env2 = 0.5f + 0.5f * sinf(t * 19.1f + 0.7f);
    float o = clampf(env * 0.8f + env2 * 0.25f, 0.0f, 1.0f);
    float tw = 19.0f - 5.0f * o; // narrower when the mouth opens ("oo")
    float th = 3.0f + 7.0f * o;
    speakW = approach(speakW, tw, 28.0f, dt);
    speakH = approach(speakH, th, 28.0f, dt);
    int mw = iround(speakW);
    int mh = iround(speakH);
    display.fillRoundRect(mx - mw / 2, myBase - mh / 2, mw, mh,
                          mh > 3 ? minI(mw, mh) / 2 : 1, SSD1306_WHITE);
  } else if (showMouth && faceNow.mouthW > 3) {
    drawMouthShape(mx, myBase + iround(faceNow.mouthY), iround(faceNow.mouthW),
                   iround(faceNow.mouthH), faceNow.mouthCurve);
  }

  if (currentEmotion == EMOTION_LISTENING) {
    // Sleek, minimal voice spectrum pulse (5 micro-bars, undulating wave)
    for (int i = 0; i < 5; i++) {
      float phase = (i - 2) * 0.9f;
      float wave = fabsf(sinf((float)(now % 100000UL) / 140.0f + phase));
      int bh = 1 + iround(wave * (i == 2 ? 5 : (i == 1 || i == 3 ? 4 : 2)));
      int bx = 56 + i * 4; // x = 56, 60, 64, 68, 72
      display.fillRect(bx, 53 - bh, 2, bh * 2 + 1, SSD1306_WHITE);
    }
  }

  if (currentEmotion == EMOTION_DEAD) {
    for (int d = -1; d <= 1; d++) {
      display.drawLine(cxL - 7 + d, cy - 7, cxL + 7 + d, cy + 7, SSD1306_BLACK);
      display.drawLine(cxL - 7 + d, cy + 7, cxL + 7 + d, cy - 7, SSD1306_BLACK);
      display.drawLine(cxR - 7 + d, cy - 7, cxR + 7 + d, cy + 7, SSD1306_BLACK);
      display.drawLine(cxR - 7 + d, cy + 7, cxR + 7 + d, cy - 7, SSD1306_BLACK);
    }
  }

  if (currentEmotion == EMOTION_COOL) {
    display.fillRect(cxL + w / 2 - 2, cy - 5, (cxR - w / 2) - (cxL + w / 2) + 4,
                     3, SSD1306_WHITE);
    display.drawLine(cxL - w / 2 - 4, cy - 7, cxL - w / 2, cy - 4,
                     SSD1306_WHITE);
    display.drawLine(cxR + w / 2 + 4, cy - 7, cxR + w / 2, cy - 4,
                     SSD1306_WHITE);
  }

  if (currentEmotion == EMOTION_EVIL) {
    display.fillTriangle(cxL - 10, cy - h / 2 - 1, cxL - 16, cy - h / 2 - 9,
                         cxL - 4, cy - h / 2 - 1, SSD1306_WHITE);
    display.fillTriangle(cxR + 4, cy - h / 2 - 1, cxR + 16, cy - h / 2 - 9,
                         cxR + 10, cy - h / 2 - 1, SSD1306_WHITE);
  }

  if (currentEmotion == EMOTION_CRYING) {
    int tearY = (int)((now / 40) % 18);
    int tearY2 = (tearY + 9) % 18;
    // Tears shrink as they fall so they seem to drip away
    display.fillCircle(cxL - w / 4, cy + h / 2 + 2 + tearY, tearY > 12 ? 1 : 2,
                       SSD1306_WHITE);
    display.fillCircle(cxR + w / 4, cy + h / 2 + 2 + tearY2,
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
  presentDisplay();
}