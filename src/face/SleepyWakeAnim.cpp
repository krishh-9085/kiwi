#include "face/SleepyWakeAnim.h"
#include "config/AppConfig.h"
#include "core/CoreTypes.h"
#include "face/FaceDisplay.h"
#include "face/FaceRenderer.h"
#include "face/FaceSkins.h"
#include "face/FaceEngine.h"
#include "hardware/AudioEngine.h"
#include <math.h>

static bool s_sleepyWakeActive = false;
static uint32_t s_sleepyWakeStartAt = 0;

static bool s_sleepyFallAsleepActive = false;
static uint32_t s_sleepyFallAsleepStartAt = 0;
static bool s_dimmedThisTransition = false;

static const float PI_F = 3.1415927f;

// ---------------------------------------------------------------------------
// Sleepy Wake Animation (Waking up smoothly from Sleep Mode)
// ---------------------------------------------------------------------------

void sleepyWakeStart() {
  s_sleepyFallAsleepActive = false;
  s_sleepyWakeActive = true;
  s_sleepyWakeStartAt = millis();
  soundPetSleepy();
}

bool sleepyWakeIsActive() {
  return s_sleepyWakeActive;
}

void sleepyWakeCancel() {
  s_sleepyWakeActive = false;
}

void sleepyWakeDraw(Adafruit_SSD1306 &d, uint32_t now) {
  if (!oledOK || !s_sleepyWakeActive)
    return;

  uint32_t t = now - s_sleepyWakeStartAt;

  // Timeline:
  // 0 .. 300 ms: Asleep stirring (o = 0.0, h = 3, w = 30)
  // 300 .. 1600 ms: Slowly opening eyes to half-open (0.0 -> 0.45, easeInOutCubic)
  // 1600 .. 2200 ms: Drowsy squint / flutter against the light (0.45 -> 0.32 -> 0.45)
  // 2200 .. 6000 ms: Settled in adorable sleepy face (o = 0.45, h = 14, lazy gaze, heavy lids)
  // 6000 .. 7200 ms: Smooth transition stretching into normal awake idle (0.45 -> 1.0)
  // >= 7200 ms: Done! Hand off to EMOTION_IDLE.

  if (t >= 7200UL) {
    s_sleepyWakeActive = false;
    setEmotion(EMOTION_IDLE);
    return;
  }

  float o = 0.0f;
  float lidInF = 0.0f;
  float lidOutF = 0.0f;
  float mouthWF = 0.0f;
  float gazeX = 0.0f;
  float gazeY = 0.0f;
  int cy = 30;
  int w = 34;

  if (t < 300UL) {
    // Phase 1: Closed eyes stirring (o = 0.0)
    o = 0.0f;
    cy = 32;
    w = 30;
  } else if (t < 1600UL) {
    // Phase 2: Opening slowly to half-eyes (0.0 -> 0.45, easeInOutCubic)
    float prog = (float)(t - 300UL) / 1300.0f;
    float k = easeInOutCubic(prog);
    o = 0.45f * k;
    w = 30 + (int)lroundf(4.0f * k);
    cy = 32 - (int)lroundf(2.0f * k);
    lidInF = 4.0f * k;
    lidOutF = 6.0f * k;
    mouthWF = 14.0f * k;
  } else if (t < 2200UL) {
    // Phase 3: Drowsy squint / eyelid flutter (0.45 -> 0.32 -> 0.45)
    float prog = (float)(t - 1600UL) / 600.0f;
    float squint = sinf(prog * PI_F);
    o = 0.45f - 0.13f * squint;
    w = 34;
    cy = 30;
    lidInF = 4.0f + 3.0f * squint;
    lidOutF = 6.0f + 3.0f * squint;
    mouthWF = 14.0f;
  } else if (t < 6000UL) {
    // Phase 4: Settled cute sleepy face ("sleepy sleepy sa face")
    o = 0.45f;
    w = 34;
    cy = 30;
    lidInF = 4.0f;
    lidOutF = 5.0f;
    mouthWF = 14.0f;

    // Subtle drowsy gaze drift
    gazeX = sinf((float)(t - 2200UL) / 800.0f) * 2.5f;
    gazeY = 1.5f + cosf((float)(t - 2200UL) / 1000.0f) * 1.0f;

    // Slow, heavy sleepy blink halfway through (around 3800-4400ms)
    if (t >= 3800UL && t < 4400UL) {
      float bp = sinf((float)(t - 3800UL) / 600.0f * PI_F);
      o -= 0.35f * bp;
    }
  } else {
    // Phase 5: Smooth wake stretch into normal idle (0.45 -> 1.0)
    float prog = (float)(t - 6000UL) / 1200.0f;
    float k = easeInOutCubic(prog);
    o = 0.45f + 0.55f * k;
    w = 34;
    cy = 30 - (int)lroundf(1.0f * k);
    lidInF = 4.0f * (1.0f - k);
    lidOutF = 5.0f * (1.0f - k);
    mouthWF = 14.0f + 6.0f * k;
  }

  // Active neutral geometry (matching EMOTION_IDLE neutral parameters exactly)
  const int cxL = 34;
  const int cxR = 94;
  const int baseR = 6;

  // Scaled height:
  int h = (int)lroundf(32.0f * o);
  if (h < 3)
    h = 3;

  int r = baseR;
  if (r * 2 > h)
    r = h / 2;
  if (r < 1)
    r = 1;

  int lidIn = (int)lroundf(lidInF);
  int lidOut = (int)lroundf(lidOutF);

  d.clearDisplay();

  // Draw eyes with active skin
  int gx = (int)lroundf(gazeX);
  int gy = (int)lroundf(gazeY);
  drawEye(cxL + gx, cy + gy, w, h, r, lidIn, lidOut, 0, true, false);
  drawEye(cxR + gx, cy + gy, w, h, r, lidIn, lidOut, 0, false, false);

  // Accents / decorations (ears, horns, etc.) appear near end when wide awake
  if (o >= 0.85f) {
    drawSkinAccents(now, cxL, cxR, cy, w, h, lidIn, lidOut);
  }

  // Draw cute sleepy mouth line
  if (showMouth && mouthWF >= 4.0f) {
    int mw = (int)lroundf(mouthWF);
    d.drawFastHLine(64 - mw / 2, 53, mw, SSD1306_WHITE);
  }

  presentDisplay();
}

// ---------------------------------------------------------------------------
// Sleepy Fall Asleep Animation (Smooth transition from Normal to Sleep Mode)
// ---------------------------------------------------------------------------

void sleepyFallAsleepStart() {
  s_sleepyWakeActive = false;
  s_sleepyFallAsleepActive = true;
  s_sleepyFallAsleepStartAt = millis();
  s_dimmedThisTransition = false;
  soundPetSleepy();
}

bool sleepyFallAsleepIsActive() {
  return s_sleepyFallAsleepActive;
}

void sleepyFallAsleepCancel() {
  s_sleepyFallAsleepActive = false;
  s_dimmedThisTransition = false;
}

void sleepyFallAsleepDraw(Adafruit_SSD1306 &d, uint32_t now) {
  if (!oledOK || !s_sleepyFallAsleepActive)
    return;

  uint32_t t = now - s_sleepyFallAsleepStartAt;

  // Timeline:
  // 0 .. 1400 ms: Drowsiness sets in (o = 1.0 -> 0.48, heavy lids droop, mouth relaxes)
  // 1400 .. 2400 ms: Sleepy nod-off (head dips down, eyes drop nearly closed: 0.48 -> 0.12)
  // 2400 .. 3400 ms: Lazy drowsy recovery & sway (gaze drifts, eyelids flutter at half-mast)
  // 3400 .. 4800 ms: Final surrender into sleep (eyes gently glide shut: 0.44 -> 0.0, bars morph)
  // 4800 .. 5800 ms: Settled in deep sleep (breathing rise, first baby Z glyph, display dims)
  // >= 5800 ms: Seamless handoff to EMOTION_SLEEPING (drawSleepingFace).

  if (t >= 5800UL) {
    s_sleepyFallAsleepActive = false;
    setEmotion(EMOTION_SLEEPING);
    updateDisplayBrightnessMode();
    return;
  }

  float o = 1.0f;
  float lidInF = 0.0f;
  float lidOutF = 0.0f;
  float mouthWF = 20.0f;
  float gazeX = 0.0f;
  float gazeY = 0.0f;
  int cy = 29;
  int w = 34;
  int cxL = 34;
  int cxR = 94;

  if (t < 1400UL) {
    // Phase 1: Drowsiness sets in (1.0 -> 0.48)
    float prog = (float)t / 1400.0f;
    float k = easeInOutCubic(prog);
    o = 1.0f - 0.52f * k;
    w = 34;
    cy = 29 + (int)lroundf(1.0f * k);
    lidInF = 4.0f * k;
    lidOutF = 5.5f * k;
    mouthWF = 20.0f - 6.0f * k;
    gazeY = 1.0f * k;
  } else if (t < 3400UL) {
    // Phase 2: Drowsy nod-off & lazy sway (1400 .. 3400 ms)
    if (t < 2400UL) {
      // Sub-phase A: Nod-off drop (1400 .. 2400 ms)
      float np = (float)(t - 1400UL) / 1000.0f;
      float nod = sinf(np * PI_F);
      o = 0.48f - 0.36f * nod; // drops down to 0.12 at peak
      gazeY = 1.0f + 2.0f * nod;
      cy = 30 + (int)lroundf(1.5f * nod);
      lidInF = 4.0f + 2.0f * nod;
      lidOutF = 5.5f + 1.5f * nod;
      mouthWF = 14.0f - 3.0f * nod;
    } else {
      // Sub-phase B: Groggily recovers to half-mast with gentle sway (2400 .. 3400 ms)
      float sp = (float)(t - 2400UL) / 1000.0f;
      float sway = sinf(sp * PI_F);
      o = 0.48f - 0.06f * sway;
      gazeX = sinf(sp * PI_F * 2.0f) * 1.8f;
      gazeY = 1.2f + 0.4f * sway;
      cy = 30;
      lidInF = 4.0f + 0.5f * sway;
      lidOutF = 5.5f;
      mouthWF = 13.0f;
    }
    w = 34;
  } else if (t < 4800UL) {
    // Phase 3: Final surrender into sleep (3400 .. 4800 ms)
    // Eyes gently and smoothly glide shut into flat sleeping bars
    float prog = (float)(t - 3400UL) / 1400.0f;
    float k = easeInOutCubic(prog);
    o = 0.44f * (1.0f - k);
    w = 34 - (int)lroundf(4.0f * k);          // 34 -> 30
    cy = 30 + (int)lroundf(2.0f * k);         // 30 -> 32
    cxL = 34 + (int)lroundf(2.0f * k);        // 34 -> 36 (matches drawSleepingFace cL)
    cxR = 94 - (int)lroundf(2.0f * k);        // 94 -> 92 (matches drawSleepingFace cR)
    lidInF = 4.0f * (1.0f - k);
    lidOutF = 5.5f * (1.0f - k);
    mouthWF = 12.0f * (1.0f - k);
    gazeX = 0.0f;
    gazeY = 0.0f;
  } else {
    // Phase 4: Settled in deep sleep (4800 .. 5800 ms)
    // Closed sleeping eye bars matching drawSleepingFace exactly (w=30, h=3, r=1, cL=36, cR=92)
    o = 0.0f;
    w = 30;
    cy = 32;
    cxL = 36;
    cxR = 92;
    lidInF = 0.0f;
    lidOutF = 0.0f;
    mouthWF = 0.0f;
    gazeX = 0.0f;
    gazeY = 0.0f;

    // First gentle respiration lift (0 to 1 px)
    float bp = (float)(t - 4800UL) / 1000.0f;
    int by = (int)lroundf(sinf(bp * PI_F) * 1.5f);
    cy += by;

    // Dim display at 5000ms if night dimming is active
    if (!s_dimmedThisTransition && t >= 5000UL) {
      s_dimmedThisTransition = true;
      updateDisplayBrightnessMode();
    }
  }

  const int baseR = 6;
  int h = (int)lroundf(32.0f * o);
  if (h < 3)
    h = 3;

  int r = baseR;
  if (r * 2 > h)
    r = h / 2;
  if (r < 1)
    r = 1;

  int lidIn = (int)lroundf(lidInF);
  int lidOut = (int)lroundf(lidOutF);

  d.clearDisplay();

  // Draw eyes with active skin
  int gx = (int)lroundf(gazeX);
  int gy = (int)lroundf(gazeY);
  drawEye(cxL + gx, cy + gy, w, h, r, lidIn, lidOut, 0, true, false);
  drawEye(cxR + gx, cy + gy, w, h, r, lidIn, lidOut, 0, false, false);

  // Accents / decorations (ears, horns, etc.) stay visible while awake and tuck away as drowsiness sets in
  if (o >= 0.85f) {
    drawSkinAccents(now, cxL, cxR, cy, w, h, lidIn, lidOut);
  }

  // Draw cute sleepy mouth line while awake/drowsy
  if (showMouth && mouthWF >= 4.0f) {
    int mw = (int)lroundf(mouthWF);
    d.drawFastHLine(64 - mw / 2, 53, mw, SSD1306_WHITE);
  }

  // First gentle baby Z rises from right eye corner in the final moments
  if (t >= 5100UL) {
    float zp = (float)(t - 5100UL) / 700.0f;
    int zx = 106 + (int)lroundf(zp * 5.0f);
    int zy = 24 - (int)lroundf(zp * 8.0f);
    if (zy >= 2 && zy <= 30 && zx <= 120) {
      drawCrispZ(zx, zy, 5);
    }
  }

  presentDisplay();
}
