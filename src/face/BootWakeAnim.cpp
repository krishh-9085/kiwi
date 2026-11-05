#include "face/BootWakeAnim.h"
#include "config/AppConfig.h"
#include "core/CoreTypes.h"
#include "face/FaceDisplay.h"
#include "face/FaceRenderer.h"
#include "face/FaceSkins.h"
#include <math.h>

static BootWakeState bootState = BootWakeState::Idle;
static uint32_t bootStartMs = 0;
static EyeSkin wakeSkin = SKIN_KIWI;

static inline float easeOutCubic(float t) {
  if (t <= 0.0f) return 0.0f;
  if (t >= 1.0f) return 1.0f;
  float f = 1.0f - t;
  return 1.0f - f * f * f;
}

void bootWakeStart() {
  wakeSkin = currentEyeSkin;
  bootStartMs = millis();
  bootState = BootWakeState::Running;
}

bool bootWakeTick() {
  if (bootState != BootWakeState::Running) {
    return false;
  }
  uint32_t elapsed = millis() - bootStartMs;
  if (elapsed >= 6000UL) {
    bootState = BootWakeState::Done;
    return false;
  }
  return true;
}

bool bootWakeDone() {
  return (bootState == BootWakeState::Done);
}

void bootWakeDraw(Adafruit_SSD1306 &d) {
  if (!oledOK || bootState == BootWakeState::Idle)
    return;

  uint32_t now = millis();
  uint32_t t = now - bootStartMs;
  float o = 0.0f;

  if (t >= 6000UL) {
    bootState = BootWakeState::Done;
    o = 1.0f;
  } else if (t < 1200UL) {
    // 0 to 1200 ms: asleep (o = 0.0)
    o = 0.0f;
  } else if (t < 2900UL) {
    // 1200 to 2900 ms: opening (0.0 -> 0.45, easeInOutCubic)
    float prog = (float)(t - 1200UL) / 1700.0f;
    o = 0.45f * easeInOutCubic(prog);
  } else if (t < 3300UL) {
    // 2900 to 3300 ms: squint (0.45 -> 0.25, easeInOutCubic)
    float prog = (float)(t - 2900UL) / 400.0f;
    o = 0.45f - 0.20f * easeInOutCubic(prog);
  } else if (t < 3500UL) {
    // 3300 to 3500 ms: ease into yawn peak at 3500 ms (0.25 -> 0.05)
    float prog = (float)(t - 3300UL) / 200.0f;
    o = 0.25f - 0.20f * easeInOutCubic(prog);
  } else if (t < 4100UL) {
    // 3500 to 4100 ms: yawn out (0.05 -> 0.35, easeOutCubic)
    float prog = (float)(t - 3500UL) / 600.0f;
    o = 0.05f + 0.30f * easeOutCubic(prog);
  } else if (t < 4600UL) {
    // 4100 to 4600 ms: awake (0.35 -> 1.0, easeOutCubic)
    float prog = (float)(t - 4100UL) / 500.0f;
    o = 0.35f + 0.65f * easeOutCubic(prog);
  } else {
    // 4600 to 5600 ms: awake hold (o = 1.0)
    // 5600 to 6000 ms: settle (o stays 1.0)
    o = 1.0f;
  }

  // Active neutral geometry (matching EMOTION_IDLE neutral parameters exactly):
  // faceNow.w = 34, faceNow.h = 32, faceNow.r = 6, gap = 30, cy = 29
  // cxL = 64 - 30 = 34, cxR = 64 + 30 = 94
  const int cxL = 34;
  const int cxR = 94;
  const int cy = 29;
  const int baseR = 6;

  // Squash & stretch width scaling from normal renderer:
  // w = iround(faceNow.w * (1.0f + 0.05f * (1.0f - o)))
  int w = (int)lroundf(34.0f * (1.0f + 0.05f * (1.0f - o)));

  // Scaled height:
  int h = (int)lroundf(32.0f * o);
  if (h < 2) h = 2;

  // Corner radius clamped to eye bounds
  int r = baseR;
  if (r * 2 > h) r = h / 2;

  d.clearDisplay();

  // Temporarily set currentEyeSkin to wakeSkin so all draw paths strictly use
  // the skin captured at animation start. Any user skin changes mid-boot are
  // preserved and applied when animation completes.
  EyeSkin userSkin = currentEyeSkin;
  currentEyeSkin = wakeSkin;

  // Call active skin's eye drawer directly
  drawEye(cxL, cy, w, h, r, 0, 0, 0, true, false);
  drawEye(cxR, cy, w, h, r, 0, 0, 0, false, false);

  // Accents / decorations (ears, horns, HUD brackets, etc.) appear near end (o >= 0.85)
  if (o >= 0.85f) {
    drawSkinAccents(now, cxL, cxR, cy, w, h, 0, 0);
  }

  currentEyeSkin = userSkin;
}
