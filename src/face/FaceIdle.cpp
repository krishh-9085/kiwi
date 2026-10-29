#include "face/FaceIdle.h"
#include "face/FaceExpressions.h"
#include "config/AppConfig.h"

int eyeOpenPct = 100;
bool eyeBlinking = false;
static uint32_t eyeBlinkT0 = 0;
static uint32_t eyeNextBlink = 0;
static uint32_t eyeNextGaze = 0;
uint32_t petMoodT0 = 0;
uint32_t flourishT0 = 0;
uint32_t nextIdleLookAt = 0;
uint32_t idleExpressionEnd = 0;
int idleExpression = 0;

void eyesInit(uint32_t now) {
  gazeX = gazeY = gazeTgtX = gazeTgtY = 0;
  eyeOpenPct = 100;
  eyeBlinking = false;
  eyeNextBlink = now + 1500 + random(0, 2500);
  eyeNextGaze = now + 1000 + random(0, 1500);
}

static uint32_t activeNextWink = 0;
uint32_t activeWinkEnd = 0;
bool activeWinkLeft = false;

void triggerTouchBlink(uint32_t now) {
  eyeBlinking = true;
  eyeBlinkT0 = now;
}

void eyesUpdateAnim(uint32_t now) {
  if (!eyeBlinking && now >= eyeNextBlink) {
    eyeBlinking = true;
    eyeBlinkT0 = now;
  }
  if (eyeBlinking) {
    uint32_t t = now - eyeBlinkT0;
    const uint32_t HALF = 70;
    if (t < HALF)
      eyeOpenPct = 100 - (int)(100UL * t / HALF);
    else if (t < 2 * HALF)
      eyeOpenPct = (int)(100UL * (t - HALF) / HALF);
    else {
      eyeOpenPct = 100;
      eyeBlinking = false;
      eyeNextBlink = now + 1800 + random(0, 3500);
    }
  }

  // Active emotion micro-wink (every 6-11 seconds during non-idle emotion)
  if (currentEmotion != EMOTION_IDLE && currentEmotion != EMOTION_SLEEPING &&
      !busyProcessing) {
    if (activeNextWink == 0)
      activeNextWink = now + 5000 + random(0, 4000);
    if (now >= activeNextWink) {
      activeWinkLeft = (random(0, 2) == 0);
      activeWinkEnd = now + 260;
      activeNextWink = now + 6500 + random(0, 5000);
    }
  } else {
    activeNextWink = 0;
    activeWinkEnd = 0;
  }

  if (now >= eyeNextGaze) {
    if (currentEmotion != EMOTION_IDLE && currentEmotion != EMOTION_SLEEPING) {
      if (gazeLockUntil > now) {
        gazeTgtX = (gazeTgtX > 0) ? 8.0f : -8.0f;
        gazeTgtY = 0;
      } else {
        eyeNextGaze = now + 1800 + random(0, 2500);
        int gMode = random(0, 6);
        if (gMode == 0) {
          gazeTgtX = 0;
          gazeTgtY = 0;
        } else if (gMode == 1) {
          gazeTgtX = -6.0f;
          gazeTgtY = 0;
        } else if (gMode == 2) {
          gazeTgtX = 6.0f;
          gazeTgtY = 0;
        } else if (gMode == 3) {
          gazeTgtX = 0;
          gazeTgtY = -3.0f;
        } else if (gMode == 4) {
          gazeTgtX = -4.0f;
          gazeTgtY = 2.0f;
        } else {
          gazeTgtX = 4.0f;
          gazeTgtY = 2.0f;
        }
      }
    }
  }
}

static void resetIdleFace() {
  faceTgt.hR = 1.0f;
  faceTgt.lidIn = 0;
  faceTgt.lidOut = 0;
  faceTgt.cheekR = 0;
  faceTgt.cheek = 0;
  faceTgt.glint = 0;
  faceTgt.gazeLock = false;
  faceTgt.mouthCurve = 0.4f;
  faceTgt.mouthW = 20;
  faceTgt.mouthH = 2;
  faceTgt.mouthY = 0;
}

static void applyIdleEye(IdleEye e) {
  switch (e) {
  case IE_NORMAL:
    break;
  case IE_WIDE:
    faceTgt.hR = 1.15f;
    break;
  case IE_HALF:
    faceTgt.lidIn = 6;
    faceTgt.lidOut = 6;
    break;
  case IE_SLEEPY:
    faceTgt.lidIn = 9;
    faceTgt.lidOut = 9;
    break;
  case IE_WINKL:
    faceTgt.cheekR = 14;
    break;
  case IE_WINKR:
    faceTgt.cheek = 14;
    break;
  case IE_ANGRY:
    faceTgt.lidIn = 10;
    faceTgt.lidOut = 2;
    break;
  case IE_SAD:
    faceTgt.lidIn = 2;
    faceTgt.lidOut = 8;
    faceTgt.gazeLock = true;
    faceTgt.lockY = 3;
    break;
  case IE_CURIOUS:
    faceTgt.hR = 1.20f;
    break;
  case IE_STARRY:
    faceTgt.glint = 1;
    break;
  case IE_LOOK_UP:
    faceTgt.gazeLock = true;
    faceTgt.lockY = -3;
    break;
  case IE_LOOK_DOWN:
    faceTgt.gazeLock = true;
    faceTgt.lockY = 3;
    break;
  case IE_SQUINT:
    faceTgt.lidIn = 7;
    faceTgt.lidOut = 7;
    break;
  case IE_GLINT:
    faceTgt.glint = 1;
    break;
  case IE_SURPRISED:
    faceTgt.glint = 1;
    faceTgt.hR = 1.30f;
    break;
  case IE_UNEVEN:
    faceTgt.hR = 0.70f;
    break;
  case IE_HEARTS:
    faceTgt.glint = 1;
    faceTgt.blush = 0.8f;
    break;
  case IE_PUPPY:
    faceTgt.hR = 1.25f;
    faceTgt.glint = 1;
    faceTgt.blush = 0.5f;
    break;
  case IE_HAPPY_ARCH:
    faceTgt.cheek = 15;
    faceTgt.cheekR = 15;
    break;
  case IE_SUSPICIOUS:
    faceTgt.lidIn = 8;
    faceTgt.lidOut = 8;
    break;
  case IE_COOL_HALF:
    faceTgt.lidIn = 4;
    faceTgt.lidOut = 4;
    faceTgt.glint = 0;
    break;
  case IE_SHOCKED_TINY:
    faceTgt.hR = 0.65f;
    faceTgt.lidIn = 0;
    faceTgt.lidOut = 0;
    break;
  }
}

static void applyIdleMouth(IdleMouth m) {
  switch (m) {
  case IM_FLAT:
    faceTgt.mouthCurve = 0.0f;
    break;
  case IM_SMILE:
    faceTgt.mouthCurve = 0.7f;
    break;
  case IM_BIGSMILE:
    faceTgt.mouthCurve = 0.95f;
    faceTgt.mouthW = 26;
    faceTgt.mouthH = 3;
    break;
  case IM_FROWN:
    faceTgt.mouthCurve = -0.7f;
    break;
  case IM_OPEN_O:
    faceTgt.mouthW = 8;
    faceTgt.mouthH = 8;
    faceTgt.mouthCurve = 0.0f;
    break;
  case IM_OPEN_WIDE:
    faceTgt.mouthW = 24;
    faceTgt.mouthH = 6;
    faceTgt.mouthCurve = 0.5f;
    break;
  case IM_POUT:
    faceTgt.mouthW = 14;
    faceTgt.mouthH = 3;
    faceTgt.mouthCurve = -0.3f;
    break;
  case IM_SMIRK:
    faceTgt.mouthW = 22;
    faceTgt.mouthCurve = 0.5f;
    faceTgt.mouthY = 1;
    break;
  case IM_TONGUE:
    faceTgt.mouthW = 16;
    faceTgt.mouthH = 4;
    faceTgt.mouthCurve = 0.3f;
    break;
  case IM_YAWN:
    faceTgt.mouthW = 14;
    faceTgt.mouthH = 9;
    faceTgt.mouthCurve = 0.0f;
    break;
  case IM_WAVY:
    faceTgt.mouthW = 24;
    faceTgt.mouthCurve = 0.4f;
    break;
  case IM_TINY:
    faceTgt.mouthW = 10;
    faceTgt.mouthH = 2;
    faceTgt.mouthCurve = 0.3f;
    break;
  case IM_KITTY_3:
    faceTgt.mouthW = 18;
    faceTgt.mouthH = 3;
    faceTgt.mouthCurve = 0.6f;
    break;
  case IM_WHISTLE:
    faceTgt.mouthW = 6;
    faceTgt.mouthH = 6;
    faceTgt.mouthCurve = 0.0f;
    faceTgt.mouthY = -1;
    break;
  case IM_ZIGZAG:
    faceTgt.mouthW = 20;
    faceTgt.mouthH = 2;
    faceTgt.mouthCurve = -0.2f;
    break;
  case IM_CHUCKLE:
    faceTgt.mouthW = 24;
    faceTgt.mouthH = 4;
    faceTgt.mouthCurve = 0.8f;
    break;
  case IM_PUPPY_POUT:
    faceTgt.mouthW = 12;
    faceTgt.mouthH = 3;
    faceTgt.mouthCurve = -0.4f;
    faceTgt.mouthY = 1;
    break;
  case IM_DEVIL_SMIRK:
    faceTgt.mouthW = 22;
    faceTgt.mouthH = 2;
    faceTgt.mouthCurve = 0.7f;
    faceTgt.mouthY = 2;
    break;
  }
}

static void applyClassicIdle(int expr) {
  switch (expr) {
  case 1:
    faceTgt.mouthCurve = 0.9f;
    faceTgt.mouthW = 26;
    faceTgt.mouthH = 3;
    break;
  case 2:
    faceTgt.cheekR = 10;
    faceTgt.mouthCurve = 0.6f;
    break;
  case 3:
    faceTgt.lidIn = 7;
    faceTgt.lidOut = 7;
    faceTgt.mouthCurve = 0.5f;
    break;
  case 4:
    faceTgt.hR = 1.25f;
    faceTgt.mouthCurve = 0.2f;
    break;
  case 5:
    faceTgt.mouthCurve = -0.4f;
    faceTgt.mouthW = 18;
    break;
  case 6:
    faceTgt.cheekR = 12;
    faceTgt.cheek = 12;
    faceTgt.mouthCurve = 0.7f;
    break;
  case 7:
    faceTgt.lidIn = 9;
    faceTgt.lidOut = 9;
    faceTgt.mouthW = 14;
    faceTgt.mouthH = 8;
    faceTgt.mouthCurve = 0.0f;
    break;
  case 8:
    faceTgt.mouthCurve = 0.0f;
    faceTgt.mouthW = 14;
    break;
  case 9:
    faceTgt.cheek = 14;
    faceTgt.cheekR = 14;
    faceTgt.mouthCurve = 0.3f;
    break;
  case 10:
    faceTgt.glint = 1;
    faceTgt.mouthW = 8;
    faceTgt.mouthH = 6;
    faceTgt.mouthCurve = 0.0f;
    break;
  case 11:
    faceTgt.lidIn = 5;
    faceTgt.lidOut = 5;
    faceTgt.mouthW = 16;
    faceTgt.mouthH = 1;
    faceTgt.mouthCurve = -0.1f;
    break;
  case 12:
    faceTgt.glint = 1;
    faceTgt.cheek = 10;
    faceTgt.cheekR = 10;
    faceTgt.mouthCurve = 0.6f;
    faceTgt.mouthW = 18;
    break;
  case 13:
    faceTgt.hR = 1.35f;
    faceTgt.mouthW = 24;
    faceTgt.mouthH = 5;
    faceTgt.mouthCurve = 0.5f;
    break;
  case 14: // Mischievous Wink
    faceTgt.cheekR = 14;
    faceTgt.mouthCurve = 0.7f;
    faceTgt.mouthW = 20;
    faceTgt.glint = 1;
    break;
  case 15: // Innocent Puppy
    faceTgt.glint = 1;
    faceTgt.blush = 0.6f;
    faceTgt.mouthCurve = 0.3f;
    faceTgt.mouthW = 12;
    break;
  case 16: // Whistling look away
    faceTgt.gazeLock = true;
    faceTgt.lockX = 8;
    faceTgt.lockY = -4;
    faceTgt.mouthW = 6;
    faceTgt.mouthH = 6;
    faceTgt.mouthCurve = 0.0f;
    break;
  case 17: // Cool cat smirk
    faceTgt.lidIn = 4;
    faceTgt.lidOut = 4;
    faceTgt.mouthCurve = 0.6f;
    faceTgt.mouthW = 18;
    break;
  case 18: // Daydreaming love
    faceTgt.glint = 1;
    faceTgt.blush = 0.8f;
    faceTgt.mouthCurve = 0.75f;
    faceTgt.mouthW = 22;
    break;
  case 19: // Skeptical eyebrow raise
    faceTgt.hR = 1.35f;
    faceTgt.lidIn = 6;
    faceTgt.mouthCurve = -0.2f;
    faceTgt.mouthW = 16;
    break;
  case 20: // Wide joyful grin
    faceTgt.cheek = 12;
    faceTgt.cheekR = 12;
    faceTgt.mouthCurve = 0.95f;
    faceTgt.mouthW = 26;
    faceTgt.mouthH = 4;
    break;
  }
}

static uint32_t idleSqueezeEnd = 0;
static uint32_t nextIdleSqueezeAt = 0;

void updateIdleVariety() {
  if (currentEmotion != EMOTION_IDLE)
    return;
  if (busyProcessing || inCircadianSleep || showWeatherScreen)
    return;
  if (showClockScreen)
    return;
  if (bootInfoUntil && millis() < bootInfoUntil)
    return;

  uint32_t now = millis();

  // 1. Natural eye squeeze / squint (soft organic eyelid contraction, NO mouth or face mutation)
  if (idleSqueezeEnd > 0 && now >= idleSqueezeEnd) {
    faceTgt.lidIn = 0;
    faceTgt.lidOut = 0;
    faceTgt.hR = 1.0f;
    idleSqueezeEnd = 0;
    nextIdleSqueezeAt = now + 4500 + random(0, 5000);
  }

  if (nextIdleSqueezeAt == 0) {
    nextIdleSqueezeAt = now + 3000 + random(0, 4000);
  } else if (now >= nextIdleSqueezeAt && idleSqueezeEnd == 0) {
    int sqType = random(0, 3);
    if (sqType == 0) {
      faceTgt.lidIn = 5.0f;
      faceTgt.lidOut = 5.0f;
    } else if (sqType == 1) {
      faceTgt.hR = 0.82f;
      faceTgt.lidIn = 3.0f;
      faceTgt.lidOut = 3.0f;
    } else {
      faceTgt.lidIn = 6.0f;
      faceTgt.lidOut = 2.0f;
    }
    uint32_t dur = 450 + random(0, 450);
    idleSqueezeEnd = now + dur;
    nextIdleSqueezeAt = idleSqueezeEnd + 5000 + random(0, 6000);
  }

  // 2. Natural eye gaze shifts: Center (look at user), Left, Right, Up, Down
  if (now >= nextIdleLookAt) {
    int gRoll = random(0, 100);
    if (gRoll < 42) {
      // Direct forward engagement / eye contact with user
      gazeTgtX = 0.0f;
      gazeTgtY = 0.0f;
      nextIdleLookAt = now + 1800 + random(0, 2400);
    } else if (gRoll < 62) {
      // Look Left
      gazeTgtX = -7.0f;
      gazeTgtY = (random(0, 3) == 0) ? -2.0f : 0.0f;
      nextIdleLookAt = now + 1400 + random(0, 1800);
    } else if (gRoll < 82) {
      // Look Right
      gazeTgtX = 7.0f;
      gazeTgtY = (random(0, 3) == 0) ? -2.0f : 0.0f;
      nextIdleLookAt = now + 1400 + random(0, 1800);
    } else if (gRoll < 91) {
      // Glance Up (pondering / curious glance)
      gazeTgtX = (float)random(-4, 5);
      gazeTgtY = -4.5f;
      nextIdleLookAt = now + 1200 + random(0, 1500);
    } else {
      // Glance Down (soft gentle glance down)
      gazeTgtX = (float)random(-2, 3);
      gazeTgtY = 3.5f;
      nextIdleLookAt = now + 1200 + random(0, 1500);
    }
  }
}

