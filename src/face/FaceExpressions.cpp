#include "face/FaceExpressions.h"
#include "behavior/InteractionManager.h"
#include "config/AppConfig.h"
#include "face/FaceDisplay.h"
#include "face/FaceIdle.h"
#include "face/FaceScreens.h"
#include "hardware/AudioEngine.h"

FaceState faceNow, faceTgt;
uint32_t emotionStartedAt = 0;
static uint32_t nextRandomEmotionAt = 0;
extern volatile int8_t pendingSound;

void setFace(float w, float h, float gap, float y, float r, float lidIn,
             float lidOut, float cheek, float blush) {
  faceTgt.w = w;
  faceTgt.h = h;
  faceTgt.gap = gap;
  faceTgt.y = y;
  faceTgt.r = r;
  faceTgt.lidIn = lidIn;
  faceTgt.lidOut = lidOut;
  faceTgt.cheek = cheek;
  faceTgt.cheekR = cheek;
  faceTgt.blush = blush;
  faceTgt.hR = 1.0f;
  faceTgt.glint = 0;
  faceTgt.gazeLock = false;
  faceTgt.lockX = 0;
  faceTgt.lockY = 0;
  faceTgt.mouthW = 20;
  faceTgt.mouthH = 2;
  faceTgt.mouthCurve = 0.4f;
  faceTgt.mouthY = 0;
}

// Emotions whose entrance should "pop" instead of being hidden by a blink
static bool emotionSkipsTransitionBlink(Emotion e) {
  return e == EMOTION_SLEEPING || e == EMOTION_ERROR ||
         e == EMOTION_SURPRISED || e == EMOTION_SHOCKED ||
         e == EMOTION_SCARED || e == EMOTION_DEAD;
}

void setEmotion(Emotion e) {
  Emotion prev = currentEmotion;
  uint32_t now = millis();
  currentEmotion = e;
  emotionStartedAt = now;
  faceTargetsForEmotion(e, petMood);
  // A quick blink hides the face morph, exactly like a person changing
  // expression.
  if (e != prev && prev != EMOTION_SLEEPING &&
      !emotionSkipsTransitionBlink(e) && !eyeBlinking)
    triggerTouchBlink(now);
  if (e == EMOTION_IDLE) {
    emotionAutoReturnAt = 0;
    nextIdleLookAt = now + 300 + random(0, 500);
    idleExpression = 0;
    idleExpressionEnd = 0;
  }
}

void faceTargetsForEmotion(Emotion e, PetMood pm) {
  switch (e) {
  case EMOTION_IDLE:
    setFace(34, 32, 30, 29, 6);
    faceTgt.mouthW = 20;
    faceTgt.mouthH = 2;
    faceTgt.mouthCurve = 0.4f;
    break;
  case EMOTION_LISTENING:
    setFace(32, 32, 30, 27, 6);
    faceTgt.glint = 1;
    faceTgt.blush = 0.0f;
    faceTgt.mouthW = 0;
    break;
  case EMOTION_THINKING:
    setFace(34, 30, 30, 28, 6, 6, 6);
    faceTgt.mouthW = 16;
    faceTgt.mouthH = 2;
    faceTgt.mouthCurve = 0.1f;
    faceTgt.mouthY = 2;
    break;
  case EMOTION_SPEAKING:
    setFace(34, 30, 30, 24, 6);
    faceTgt.glint = 1;
    faceTgt.mouthW = 0;
    break;
  case EMOTION_HAPPY:
    setFace(34, 30, 30, 28, 7, 0, 0, 14, 0.3f);
    faceTgt.mouthW = 26;
    faceTgt.mouthH = 3;
    faceTgt.mouthCurve = 0.9f;
    break;
  case EMOTION_EXCITED:
    setFace(38, 38, 31, 28, 7);
    faceTgt.glint = 1;
    faceTgt.mouthW = 24;
    faceTgt.mouthH = 5;
    faceTgt.mouthCurve = 0.8f;
    break;
  case EMOTION_SAD:
    setFace(34, 28, 30, 30, 6, 0, 10);
    faceTgt.gazeLock = true;
    faceTgt.lockY = 3;
    faceTgt.mouthW = 22;
    faceTgt.mouthH = 2;
    faceTgt.mouthCurve = -0.7f;
    break;
  case EMOTION_CONFUSED:
    setFace(34, 34, 30, 28, 6);
    faceTgt.hR = 0.65f;
    faceTgt.mouthW = 14;
    faceTgt.mouthH = 2;
    faceTgt.mouthCurve = -0.2f;
    break;
  case EMOTION_ANGRY:
    setFace(34, 30, 30, 30, 5, 12, 0);
    faceTgt.mouthW = 22;
    faceTgt.mouthH = 2;
    faceTgt.mouthCurve = -0.9f;
    break;
  case EMOTION_SLEEPING:
    setFace(30, 3, 30, 34, 1);
    faceTgt.gazeLock = true;
    faceTgt.mouthW = 0;
    faceTgt.mouthH = 0;
    faceTgt.mouthCurve = 0.0f;
    break;
  case EMOTION_ERROR:
    setFace(30, 26, 32, 30, 4, 0, 8);
    faceTgt.mouthW = 16;
    faceTgt.mouthH = 2;
    faceTgt.mouthCurve = -0.5f;
    break;
  case EMOTION_SURPRISED:
    setFace(40, 42, 32, 26, 8);
    faceTgt.glint = 1;
    faceTgt.mouthW = 10;
    faceTgt.mouthH = 8;
    faceTgt.mouthCurve = 0.0f;
    break;
  case EMOTION_WINK:
    setFace(34, 32, 30, 29, 6);
    faceTgt.cheekR = 16;
    faceTgt.mouthW = 22;
    faceTgt.mouthH = 2;
    faceTgt.mouthCurve = 0.6f;
    break;
  case EMOTION_SMUG:
    setFace(34, 28, 30, 30, 6, 8, 4);
    faceTgt.hR = 0.9f;
    faceTgt.mouthW = 22;
    faceTgt.mouthH = 2;
    faceTgt.mouthCurve = 0.5f;
    faceTgt.mouthY = 1;
    break;
  case EMOTION_CURIOUS:
    setFace(34, 34, 30, 28, 6);
    faceTgt.hR = 1.15f;
    faceTgt.mouthW = 16;
    faceTgt.mouthH = 2;
    faceTgt.mouthCurve = 0.3f;
    break;
  case EMOTION_BORED:
    setFace(34, 30, 30, 30, 6, 11, 11);
    faceTgt.mouthW = 20;
    faceTgt.mouthH = 1;
    faceTgt.mouthCurve = 0.0f;
    break;
  case EMOTION_LAUGHING:
    setFace(34, 30, 30, 28, 7, 0, 0, 18, 0.5f);
    faceTgt.mouthW = 28;
    faceTgt.mouthH = 7;
    faceTgt.mouthCurve = 0.9f;
    break;
  case EMOTION_SCARED:
    setFace(38, 42, 32, 27, 8);
    faceTgt.mouthW = 12;
    faceTgt.mouthH = 6;
    faceTgt.mouthCurve = -0.3f;
    break;
  case EMOTION_SUSPICIOUS:
    setFace(36, 14, 30, 30, 5, 4, 4);
    faceTgt.mouthW = 18;
    faceTgt.mouthH = 1;
    faceTgt.mouthCurve = 0.0f;
    break;
  case EMOTION_RELIEVED:
    setFace(34, 26, 30, 30, 6, 0, 0, 8, 0.2f);
    faceTgt.mouthW = 22;
    faceTgt.mouthH = 2;
    faceTgt.mouthCurve = 0.5f;
    break;
  case EMOTION_PROUD:
    setFace(34, 24, 30, 26, 6, 0, 0, 6);
    faceTgt.mouthW = 24;
    faceTgt.mouthH = 2;
    faceTgt.mouthCurve = 0.5f;
    break;
  case EMOTION_DIZZY:
    setFace(34, 32, 30, 29, 6);
    faceTgt.mouthW = 18;
    faceTgt.mouthH = 2;
    faceTgt.mouthCurve = -0.3f;
    break;
  case EMOTION_LOVESTRUCK:
    setFace(36, 36, 31, 28, 7);
    faceTgt.glint = 1;
    faceTgt.blush = 0.9f;
    faceTgt.mouthW = 22;
    faceTgt.mouthH = 3;
    faceTgt.mouthCurve = 0.8f;
    break;
  case EMOTION_EVIL:
    setFace(34, 28, 30, 29, 6, 12, 1);
    faceTgt.mouthW = 24;
    faceTgt.mouthH = 3;
    faceTgt.mouthCurve = 0.85f;
    faceTgt.mouthY = 1;
    break;
  case EMOTION_CRYING:
    setFace(34, 28, 30, 31, 6, 1, 10);
    faceTgt.gazeLock = true;
    faceTgt.lockY = 4;
    faceTgt.mouthW = 22;
    faceTgt.mouthH = 4;
    faceTgt.mouthCurve = -0.9f;
    break;
  case EMOTION_HEARTS:
    setFace(38, 38, 31, 28, 8);
    faceTgt.glint = 1;
    faceTgt.blush = 1.0f;
    faceTgt.mouthW = 26;
    faceTgt.mouthH = 4;
    faceTgt.mouthCurve = 0.9f;
    break;
  case EMOTION_DEAD:
    setFace(34, 18, 30, 30, 4);
    faceTgt.mouthW = 18;
    faceTgt.mouthH = 4;
    faceTgt.mouthCurve = 0.0f;
    faceTgt.mouthY = 2;
    break;
  case EMOTION_COOL:
    setFace(36, 26, 30, 29, 6, 5, 5);
    faceTgt.mouthW = 22;
    faceTgt.mouthH = 2;
    faceTgt.mouthCurve = 0.6f;
    faceTgt.mouthY = 1;
    break;
  case EMOTION_SHOCKED:
    setFace(42, 44, 32, 26, 8);
    faceTgt.glint = 1;
    faceTgt.mouthW = 12;
    faceTgt.mouthH = 10;
    faceTgt.mouthCurve = 0.0f;
    break;
  case EMOTION_SINGING:
    setFace(34, 30, 30, 28, 7, 0, 0, 16);
    faceTgt.mouthW = 14;
    faceTgt.mouthH = 8;
    faceTgt.mouthCurve = 0.4f;
    break;
  case EMOTION_PARTY:
    setFace(38, 36, 31, 27, 7, 0, 0, 12, 0.7f);
    faceTgt.glint = 1;
    faceTgt.mouthW = 28;
    faceTgt.mouthH = 5;
    faceTgt.mouthCurve = 0.85f;
    break;
  case EMOTION_NERVOUS:
    setFace(32, 28, 30, 29, 6, 8, 4);
    faceTgt.mouthW = 18;
    faceTgt.mouthH = 3;
    faceTgt.mouthCurve = -0.3f;
    break;
  case EMOTION_CUTE:
    setFace(40, 42, 31, 27, 8);
    faceTgt.glint = 1;
    faceTgt.blush = 0.9f;
    faceTgt.mouthW = 14;
    faceTgt.mouthH = 2;
    faceTgt.mouthCurve = 0.5f;
    break;
  case EMOTION_PETTED:
    switch (pm) {
    case PET_LOVE:
      setFace(34, 30, 30, 29, 7, 0, 0, 15, 0.9f);
      faceTgt.mouthW = 24;
      faceTgt.mouthH = 3;
      faceTgt.mouthCurve = 0.85f;
      break;
    case PET_GIGGLE:
      setFace(34, 30, 30, 29, 7, 0, 0, 13, 0.5f);
      faceTgt.mouthW = 26;
      faceTgt.mouthH = 5;
      faceTgt.mouthCurve = 0.7f;
      break;
    case PET_SHY:
      setFace(32, 26, 30, 32, 6, 0, 5, 0, 1.0f);
      faceTgt.gazeLock = true;
      faceTgt.lockX = -5;
      faceTgt.lockY = 4;
      faceTgt.mouthW = 12;
      faceTgt.mouthH = 2;
      faceTgt.mouthCurve = 0.3f;
      break;
    case PET_SPARKLE:
      setFace(38, 38, 32, 28, 7);
      faceTgt.glint = 1;
      faceTgt.mouthW = 24;
      faceTgt.mouthH = 6;
      faceTgt.mouthCurve = 0.85f;
      break;
    case PET_KISS:
      setFace(34, 32, 30, 29, 6, 0, 0, 0, 0.7f);
      faceTgt.cheekR = 14;
      faceTgt.mouthW = 14;
      faceTgt.mouthH = 4;
      faceTgt.mouthCurve = 0.5f;
      break;
    case PET_CUDDLE:
      setFace(34, 20, 30, 31, 6, 0, 0, 0, 0.6f);
      faceTgt.lidIn = 8;
      faceTgt.lidOut = 8;
      faceTgt.mouthW = 20;
      faceTgt.mouthH = 3;
      faceTgt.mouthCurve = 0.6f;
      break;
    case PET_PLAYFUL:
      setFace(36, 30, 30, 28, 7);
      faceTgt.hR = 0.75f;
      faceTgt.mouthW = 26;
      faceTgt.mouthH = 4;
      faceTgt.mouthCurve = 0.85f;
      break;
    case PET_SLEEPY:
      setFace(34, 18, 30, 31, 6);
      faceTgt.lidIn = 6;
      faceTgt.lidOut = 6;
      faceTgt.mouthW = 16;
      faceTgt.mouthH = 2;
      faceTgt.mouthCurve = 0.3f;
      faceTgt.gazeLock = true;
      faceTgt.lockY = 3;
      break;
    case PET_PROUD:
      setFace(34, 20, 30, 28, 6);
      faceTgt.lidIn = 5;
      faceTgt.lidOut = 5;
      faceTgt.mouthW = 22;
      faceTgt.mouthH = 2;
      faceTgt.mouthCurve = 0.4f;
      faceTgt.mouthY = -1;
      faceTgt.gazeLock = true;
      faceTgt.lockY = -2;
      break;
    case PET_SURPRISED:
      setFace(38, 40, 32, 27, 8);
      faceTgt.glint = 1;
      faceTgt.mouthW = 12;
      faceTgt.mouthH = 7;
      faceTgt.mouthCurve = 0.0f;
      break;
    default:
      break;
    }
    break;
  default:
    setFace(34, 32, 30, 29, 6);
    break;
  }
}

void maybeChangeEmotion() {
  if (showClockScreen || showWeatherScreen || inCircadianSleep || bannerActive)
    return;
  uint32_t now = millis();
  if (errorFaceUntil && now < errorFaceUntil)
    return;
  if (errorFaceUntil && now >= errorFaceUntil) {
    errorFaceUntil = 0;
    setEmotion(EMOTION_IDLE);
    return;
  }
  if (bootInfoUntil && now < bootInfoUntil)
    return;
  if (gazeLockUntil > now)
    return;
  if (emotionAutoReturnAt && now >= emotionAutoReturnAt) {
    emotionAutoReturnAt = 0;
    setEmotion(EMOTION_IDLE);
    return;
  }
  if (returnIdleAt && now >= returnIdleAt) {
    returnIdleAt = 0;
    setEmotion(EMOTION_IDLE);
    return;
  }
  // Face stays steady in currentEmotion until manually changed via UI or touch!
}

static Flourish flourishForMood(int m) {
  switch (m) {
  case PET_LOVE:
    return FL_HEART;
  case PET_GIGGLE:
    return FL_NOTE;
  case PET_SHY:
    return FL_HEART;
  case PET_SPARKLE:
    return FL_SPARKLE;
  case PET_KISS:
    return FL_HEART;
  case PET_CUDDLE:
    return FL_MOON;
  case PET_PLAYFUL:
    return FL_NOTE;
  case PET_SLEEPY:
    return FL_MOON;
  case PET_PROUD:
    return FL_STAR;
  case PET_SURPRISED:
    return FL_SPARKLE;
  default:
    return FL_SPARKLE;
  }
}

void holdFace(uint32_t ms) {
  uint32_t t0 = millis();
  while (millis() - t0 < ms) {
    if (cancelRequest)
      break;
    pumpUI();
    delay(2);
  }
}

void showEmotionFor(Emotion e, uint32_t ms) {
  setEmotion(e);
  holdFace(ms);
}

void showPetMood(PetMood m, uint32_t ms) {
  petMood = m;
  petMoodT0 = millis();
  flourishT0 = millis();
  flourish = flourishForMood((int)m);
  setEmotion(EMOTION_PETTED);
  switch (m) {
  case PET_LOVE:
    soundPetLove();
    break;
  case PET_GIGGLE:
    soundPetGiggle();
    break;
  case PET_SHY:
    soundPetShy();
    break;
  case PET_SPARKLE:
    soundPetSparkle();
    break;
  case PET_KISS:
    soundPetKiss();
    break;
  case PET_CUDDLE:
    soundPetCuddle();
    break;
  case PET_PLAYFUL:
    soundPetPlayful();
    break;
  case PET_SLEEPY:
    soundPetSleepy();
    break;
  case PET_PROUD:
    soundPetProud();
    break;
  case PET_SURPRISED:
    soundPetSurprised();
    break;
  }
  holdFace(ms);
}

bool triggerEmotionByName(const String &nameIn, uint32_t holdMs,
                          bool withSound) {
  String e = nameIn;
  e.toLowerCase();
  e.trim();
  static const struct EmoteName {
    const char *n;
    Emotion e;
  } emotes[] = {{"idle", EMOTION_IDLE},
                {"listening", EMOTION_LISTENING},
                {"thinking", EMOTION_THINKING},
                {"speaking", EMOTION_SPEAKING},
                {"happy", EMOTION_HAPPY},
                {"excited", EMOTION_EXCITED},
                {"sad", EMOTION_SAD},
                {"confused", EMOTION_CONFUSED},
                {"angry", EMOTION_ANGRY},
                {"sleeping", EMOTION_SLEEPING},
                {"error", EMOTION_ERROR},
                {"surprised", EMOTION_SURPRISED},
                {"wink", EMOTION_WINK},
                {"smug", EMOTION_SMUG},
                {"curious", EMOTION_CURIOUS},
                {"bored", EMOTION_BORED},
                {"laughing", EMOTION_LAUGHING},
                {"scared", EMOTION_SCARED},
                {"suspicious", EMOTION_SUSPICIOUS},
                {"relieved", EMOTION_RELIEVED},
                {"proud", EMOTION_PROUD},
                {"dizzy", EMOTION_DIZZY},
                {"lovestruck", EMOTION_LOVESTRUCK},
                {"evil", EMOTION_EVIL},
                {"devil", EMOTION_EVIL},
                {"crying", EMOTION_CRYING},
                {"cry", EMOTION_CRYING},
                {"hearts", EMOTION_HEARTS},
                {"heart", EMOTION_HEARTS},
                {"dead", EMOTION_DEAD},
                {"ko", EMOTION_DEAD},
                {"cool", EMOTION_COOL},
                {"shades", EMOTION_COOL},
                {"shocked", EMOTION_SHOCKED},
                {"singing", EMOTION_SINGING},
                {"music", EMOTION_SINGING},
                {"party", EMOTION_PARTY},
                {"celebrate", EMOTION_PARTY},
                {"nervous", EMOTION_NERVOUS},
                {"cute", EMOTION_CUTE},
                {"puppy", EMOTION_CUTE}};
  for (const auto &m : emotes) {
    if (e == m.n) {
      if (showClockScreen)
        showClockScreen = false;
      if (showWeatherScreen)
        showWeatherScreen = false;
      if (m.e == EMOTION_SLEEPING) {
        manualSleep = true;
        inCircadianSleep = true;
        setOledDim(false);
        if (withSound)
          soundPetSleepy();
      } else {
        manualSleep = false;
        inCircadianSleep = false;
        setOledDim(false);
      }
      setEmotion(m.e);
      if (m.e == EMOTION_IDLE)
        emotionAutoReturnAt = 0;
      else
        emotionAutoReturnAt = millis() + (holdMs > 0 ? holdMs : 60000);
      if (withSound && m.e != EMOTION_SLEEPING) {
        if (m.e == EMOTION_HAPPY || m.e == EMOTION_PARTY)
          soundPetPlayful();
        else if (m.e == EMOTION_EXCITED || m.e == EMOTION_SURPRISED)
          soundPetSparkle();
        else if (m.e == EMOTION_SAD || m.e == EMOTION_CRYING)
          soundPetShy();
        else if (m.e == EMOTION_DEAD || m.e == EMOTION_ERROR)
          soundError();
        else if (m.e == EMOTION_COOL || m.e == EMOTION_PROUD)
          soundPetProud();
        else if (m.e == EMOTION_SINGING || m.e == EMOTION_LAUGHING)
          soundPetGiggle();
        else if (m.e == EMOTION_HEARTS || m.e == EMOTION_LOVESTRUCK)
          soundPetLove();
        else if (m.e == EMOTION_CUTE)
          soundPetKiss();
      }
      return true;
    }
  }
  int mood = -1;
  if (e == "love")
    mood = PET_LOVE;
  else if (e == "giggle")
    mood = PET_GIGGLE;
  else if (e == "shy")
    mood = PET_SHY;
  else if (e == "sparkle")
    mood = PET_SPARKLE;
  else if (e == "kiss")
    mood = PET_KISS;
  else if (e == "cuddle")
    mood = PET_CUDDLE;
  else if (e == "playful")
    mood = PET_PLAYFUL;
  else if (e == "sleepy")
    mood = PET_SLEEPY;
  else if (e == "proud")
    mood = PET_PROUD;
  else if (e == "random")
    mood = random(0, PET_MOOD_COUNT);
  if (mood >= 0) {
    if (showClockScreen)
      showClockScreen = false;
    if (showWeatherScreen)
      showWeatherScreen = false;
    manualSleep = false;
    inCircadianSleep = false;
    setOledDim(false);
    petMood = (PetMood)mood;
    petMoodT0 = millis();
    flourishT0 = millis();
    flourish = flourishForMood(mood);
    setEmotion(EMOTION_PETTED);
    emotionAutoReturnAt = millis() + (holdMs > 0 ? holdMs : 60000);
    if (withSound)
      pendingSound = (int8_t)mood;
    return true;
  }
  return false;
}