#include "face/FaceIdle.h"
#include "config/AppConfig.h"
#include "face/FaceDisplay.h"
#include "face/FaceExpressions.h"
#include <math.h>
#include <time.h>


int eyeOpenPct = 100;
float eyeOpenAmt = 1.0f;
bool eyeBlinking = false;
static uint32_t eyeBlinkT0 = 0;
static uint32_t eyeNextBlink = 0;
static uint32_t eyeNextGaze = 0;
static bool eyeDoubleBlinkPending = false;
uint32_t petMoodT0 = 0;
uint32_t flourishT0 = 0;
uint32_t nextIdleLookAt = 0;

float gazeMicroX = 0.0f;
float gazeMicroY = 0.0f;
static uint32_t nextMicroAt = 0;

static uint32_t activeNextWink = 0;
uint32_t activeWinkEnd = 0;
bool activeWinkLeft = false;

// Asymmetric biological snap-blink timings (ms)
// Rapid downward eyelid snap (~45ms), hold at closure (~20ms), smooth organic ease-out (~160ms)
static const uint32_t BLINK_CLOSE = 45;
static const uint32_t BLINK_HOLD = 20;
static const uint32_t BLINK_OPEN = 160;

float idleSighY = 0.0f;
float idleSighLid = 0.0f;
static uint32_t nextDeskSighAt = 0;
static uint32_t deskSighStartAt = 0;
static const uint32_t SIGH_DURATION = 2600; // 2.6s complete sigh cycle

float idleYawnMouth = 0.0f;
float idleYawnLid = 0.0f;
float idleSleepyDroop = 0.0f;
static uint32_t nextIdleYawnAt = 0;
static uint32_t idleYawnStartAt = 0;
static const uint32_t YAWN_DURATION = 2400; // 2.4s cute idle yawn

static bool emotionSuppressesBlink(Emotion e) {
  return e == EMOTION_SLEEPING || e == EMOTION_DEAD || e == EMOTION_COOL ||
         e == EMOTION_LAUGHING || e == EMOTION_HEARTS || e == EMOTION_WINK ||
         e == EMOTION_CRYING;
}

static void scheduleNextBlink(uint32_t now) {
  if (eyeDoubleBlinkPending) {
    eyeDoubleBlinkPending = false;
    eyeNextBlink = now + 110 + random(0, 80);
    return;
  }
  eyeNextBlink = now + 1800 + random(0, 3500);
  // ~1 in 7 blinks is a quick double blink
  if (random(0, 100) < 14)
    eyeDoubleBlinkPending = true;
}

void eyesInit(uint32_t now) {
  gazeX = gazeY = gazeTgtX = gazeTgtY = 0;
  gazeMicroX = gazeMicroY = 0;
  eyeOpenPct = 100;
  eyeOpenAmt = 1.0f;
  eyeBlinking = false;
  eyeDoubleBlinkPending = false;
  eyeNextBlink = now + 1500 + random(0, 2500);
  eyeNextGaze = now + 1000 + random(0, 1500);
  nextMicroAt = now + 400;
}

void triggerTouchBlink(uint32_t now) {
  eyeBlinking = true;
  eyeBlinkT0 = now;
}

void eyesUpdateAnim(uint32_t now) {
  if (emotionSuppressesBlink(currentEmotion)) {
    eyeBlinking = false;
    eyeOpenAmt = 1.0f;
    eyeOpenPct = 100;
  } else {
    if (!eyeBlinking && now >= eyeNextBlink) {
      eyeBlinking = true;
      eyeBlinkT0 = now;
    }
    if (eyeBlinking) {
      uint32_t blinkOpenMs = (idleSleepyDroop > 0.5f) ? 230 : BLINK_OPEN;
      uint32_t t = now - eyeBlinkT0;
      float a;
      if (t < BLINK_CLOSE) {
        // Rapid downward eyelid snap (~45ms): cubic acceleration
        float p = (float)t / (float)BLINK_CLOSE;
        a = 1.0f - p * p * p;
      } else if (t < BLINK_CLOSE + BLINK_HOLD) {
        a = 0.0f;
      } else if (t < BLINK_CLOSE + BLINK_HOLD + blinkOpenMs) {
        // Smooth biological ease-out (~160ms) with gentle elastic overshoot
        float p = (float)(t - BLINK_CLOSE - BLINK_HOLD) / (float)blinkOpenMs;
        float q = 1.0f - p;
        a = 1.0f - q * q * q;
        // Gentle micro-bounce at 75-95% opening
        if (p > 0.75f && p < 0.96f) {
          a += 0.025f * sinf((p - 0.75f) / 0.21f * 3.14159f);
        }
        if (a > 1.0f) a = 1.0f;
      } else {
        a = 1.0f;
        eyeBlinking = false;
        scheduleNextBlink(now);
      }
      eyeOpenAmt = a;
      eyeOpenPct = (int)(a * 100.0f + 0.5f);
    } else {
      eyeOpenAmt = 1.0f;
      eyeOpenPct = 100;
    }
  }

  // Micro-saccades: tiny involuntary drifts that make the eyes feel alive
  if (now >= nextMicroAt) {
    gazeMicroX = (float)random(-10, 11) * 0.07f;
    gazeMicroY = (float)random(-10, 11) * 0.04f;
    nextMicroAt = now + 250 + random(0, 500);
  }

  // Active emotion micro-wink (every 6-11 seconds during non-idle emotion)
  if (currentEmotion != EMOTION_IDLE && !emotionSuppressesBlink(currentEmotion) &&
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
        eyeNextGaze = now + 200;
      } else {
        eyeNextGaze = now + 1800 + random(0, 2500);
        float oldX = gazeTgtX;
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
        // Big gaze shifts are often accompanied by a blink
        if (!eyeBlinking && fabsf(gazeTgtX - oldX) >= 8.0f &&
            random(0, 100) < 30)
          triggerTouchBlink(now);
      }
    }
  }
}

struct IdleMoodEntry {
  Emotion emotion;
  const char *name;
};

static const IdleMoodEntry IDLE_MOOD_POOL[] = {
    {EMOTION_CURIOUS, "curious"},
    {EMOTION_SMUG, "smug"},
    {EMOTION_SUSPICIOUS, "suspicious"},
    {EMOTION_BORED, "bored"},
    {EMOTION_WINK, "wink"},
    {EMOTION_HAPPY, "happy"},
    {EMOTION_RELIEVED, "relieved"},
    {EMOTION_PROUD, "proud"},
    {EMOTION_CUTE, "cute"},
};
static const int IDLE_MOOD_POOL_COUNT =
    (int)(sizeof(IDLE_MOOD_POOL) / sizeof(IDLE_MOOD_POOL[0]));

static uint32_t idleSqueezeEnd = 0;
static uint32_t nextIdleSqueezeAt = 0;
static uint32_t nextIdleMoodAt = 0;
static int lastIdleMoodIdx = -1;
static bool s_triggeringIdleMood = false;

bool isTriggeringIdleMood() { return s_triggeringIdleMood; }

void resetIdleMoodTimer() { nextIdleMoodAt = 0; }

void updateIdleVariety() {
  if (currentEmotion != EMOTION_IDLE) {
    // Forget any half-finished squeeze, sigh, or yawn so it can't leak into the next idle
    idleSqueezeEnd = 0;
    deskSighStartAt = 0;
    idleSighY = 0.0f;
    idleSighLid = 0.0f;
    idleYawnStartAt = 0;
    idleYawnMouth = 0.0f;
    idleYawnLid = 0.0f;
    idleSleepyDroop = 0.0f;
    return;
  }
  if (busyProcessing || inCircadianSleep || showWeatherScreen)
    return;
  if (showClockScreen)
    return;
  if (bootInfoUntil && millis() < bootInfoUntil)
    return;

  uint32_t now = millis();

  // Gentle late-night drowsiness droop when awake near/after bedtime
  float targetSleepyDroop = 0.0f;
  if (timeSynced) {
    struct tm ti;
    if (getLocalTime(&ti, 5)) {
      int h = ti.tm_hour;
      if (h >= 21 || h < 6 || (circadianSleepEnabled && h == (circadianSleepStart + 23) % 24)) {
        targetSleepyDroop = 2.5f;
      }
    }
  }
  idleSleepyDroop = targetSleepyDroop;

  // 1. Natural eye squeeze / squint (soft organic eyelid contraction, NO mouth
  // or face mutation)
  if (idleSqueezeEnd > 0 && now >= idleSqueezeEnd) {
    if (displayLock(15)) {
      faceTgt.lidIn = 0;
      faceTgt.lidOut = 0;
      faceTgt.lidInR = 0;
      faceTgt.lidOutR = 0;
      faceTgt.hR = 1.0f;
      faceTgt.headTilt = 0.0f;
      displayUnlock();
      idleSqueezeEnd = 0;
      nextIdleSqueezeAt = now + 4500 + random(0, 5000);
    }
  }

  if (nextIdleSqueezeAt == 0) {
    nextIdleSqueezeAt = now + 3000 + random(0, 4000);
  } else if (now >= nextIdleSqueezeAt && idleSqueezeEnd == 0 && idleYawnStartAt == 0) {
    if (displayLock(15)) {
      int sqType = random(0, 4);
      if (sqType == 0) {
        faceTgt.lidIn = 5.0f;
        faceTgt.lidOut = 5.0f;
        faceTgt.lidInR = 5.0f;
        faceTgt.lidOutR = 5.0f;
      } else if (sqType == 1) {
        faceTgt.hR = 0.82f;
        faceTgt.lidIn = 3.0f;
        faceTgt.lidOut = 3.0f;
        faceTgt.lidInR = 5.0f;
        faceTgt.lidOutR = 5.0f;
        faceTgt.headTilt = -1.5f;
      } else if (sqType == 2) {
        faceTgt.lidIn = 6.0f;
        faceTgt.lidOut = 2.0f;
        faceTgt.lidInR = 2.0f;
        faceTgt.lidOutR = 1.0f;
        faceTgt.headTilt = 1.5f;
      } else {
        faceTgt.headTilt = (random(0, 2) == 0) ? -2.0f : 2.0f;
      }
      displayUnlock();
      uint32_t dur = 450 + random(0, 450);
      idleSqueezeEnd = now + dur;
      nextIdleSqueezeAt = idleSqueezeEnd + 5000 + random(0, 6000);
    }
  }

  // 2. Periodic idle mood rotation (full emotion presets via emotionAutoReturnAt)
  if (nextIdleMoodAt == 0) {
    nextIdleMoodAt = now + 20000UL + (uint32_t)random(0, 15001);
  } else if ((int32_t)(now - nextIdleMoodAt) >= 0 && !eyeBlinking &&
             idleSqueezeEnd == 0 && deskSighStartAt == 0 && idleYawnStartAt == 0) {
    int idx = (int)random(0, IDLE_MOOD_POOL_COUNT);
    if (IDLE_MOOD_POOL_COUNT > 1 && idx == lastIdleMoodIdx) {
      idx = (idx + 1 + (int)random(0, IDLE_MOOD_POOL_COUNT - 1)) %
            IDLE_MOOD_POOL_COUNT;
    }
    lastIdleMoodIdx = idx;

    uint32_t dur = (uint32_t)random(3000, 5001);
    nextIdleMoodAt = now + 25000UL + (uint32_t)random(0, 20001);

    s_triggeringIdleMood = true;
    setEmotion(IDLE_MOOD_POOL[idx].emotion);
    s_triggeringIdleMood = false;
    emotionAutoReturnAt = now + dur;

    Serial.printf("[IDLE_MOOD] %s (%lu ms)\n", IDLE_MOOD_POOL[idx].name,
                  (unsigned long)dur);
    return;
  }

  // 3. Natural eye gaze shifts: Center (look at user), Left, Right, Up, Down
  if (now >= nextIdleLookAt) {
    float oldX = gazeTgtX;
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
    // Large gaze jumps are often masked by a quick blink (like a real eye)
    if (!eyeBlinking && fabsf(gazeTgtX - oldX) >= 9.0f && random(0, 100) < 35)
      triggerTouchBlink(now);
  }

  // 4. Organic Desk Sigh Cycle (every 22 - 32 seconds of idle desk companionship)
  if (nextDeskSighAt == 0) {
    nextDeskSighAt = now + 15000 + random(0, 10000);
  }

  if (deskSighStartAt == 0 && now >= nextDeskSighAt && !eyeBlinking && idleSqueezeEnd == 0) {
    deskSighStartAt = now;
    nextDeskSighAt = now + 22000 + random(0, 10000);
  }

  if (deskSighStartAt > 0) {
    uint32_t st = now - deskSighStartAt;
    if (st < SIGH_DURATION) {
      float sp = (float)st / (float)SIGH_DURATION;
      if (sp < 0.38f) {
        // Phase 1 (0..38%): Gentle deep inhale lift (+1.0px up, eyelids open slightly)
        float inP = sp / 0.38f;
        float easeIn = sinf(inP * 1.5707963f);
        idleSighY = -1.2f * easeIn;
        idleSighLid = -1.0f * easeIn;
      } else if (sp < 0.82f) {
        // Phase 2 (38..82%): Relaxed long exhale sink (face sinks down -1.4px, eyelids droop by ~18%)
        float exP = (sp - 0.38f) / 0.44f;
        float easeEx = 0.5f - 0.5f * cosf(exP * 3.1415927f);
        idleSighY = -1.2f + 2.6f * easeEx; // sinks down to +1.4px
        idleSighLid = -1.0f + 3.8f * easeEx; // droop up to +2.8px
      } else {
        // Phase 3 (82..100%): Gentle relaxed settle back to resting neutral
        float retP = (sp - 0.82f) / 0.18f;
        idleSighY = 1.4f * (1.0f - retP);
        idleSighLid = 2.8f * (1.0f - retP);
      }
    } else {
      deskSighStartAt = 0;
      idleSighY = 0.0f;
      idleSighLid = 0.0f;
    }
  } else {
    idleSighY = 0.0f;
    idleSighLid = 0.0f;
  }

  // 5. Cute Idle Yawn Cycle (every 48 - 75 seconds)
  if (nextIdleYawnAt == 0) {
    nextIdleYawnAt = now + 35000UL + (uint32_t)random(0, 25000);
  }
  if (idleYawnStartAt == 0 && now >= nextIdleYawnAt && !eyeBlinking &&
      idleSqueezeEnd == 0 && deskSighStartAt == 0) {
    idleYawnStartAt = now;
    nextIdleYawnAt = now + 48000UL + (uint32_t)random(0, 27000);
    gazeTgtX = 0.0f;
    gazeTgtY = -1.5f;
  }
  if (idleYawnStartAt > 0) {
    uint32_t yt = now - idleYawnStartAt;
    if (yt < YAWN_DURATION) {
      float yp = (float)yt / (float)YAWN_DURATION;
      if (yp < 0.30f) {
        float k = sinf((yp / 0.30f) * 1.5707963f);
        idleYawnMouth = k;
        idleYawnLid = 10.0f * k;
      } else if (yp < 0.68f) {
        idleYawnMouth = 1.0f;
        idleYawnLid = 10.0f;
      } else {
        float k = 1.0f - (yp - 0.68f) / 0.32f;
        idleYawnMouth = k;
        idleYawnLid = 10.0f * k;
      }
    } else {
      idleYawnStartAt = 0;
      idleYawnMouth = 0.0f;
      idleYawnLid = 0.0f;
    }
  } else {
    idleYawnMouth = 0.0f;
    idleYawnLid = 0.0f;
  }
}