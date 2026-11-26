#include "hardware/RgbLed.h"
#include "config/AppConfig.h"
#include "hardware/AudioEngine.h"
#include "hardware/SoundLevel.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <math.h>

static SemaphoreHandle_t s_rgbMutex = NULL;

static inline bool rgbLock(uint32_t waitMs = 15) {
  if (!s_rgbMutex)
    return true;
  return xSemaphoreTake(s_rgbMutex, pdMS_TO_TICKS(waitMs)) == pdTRUE;
}

static inline void rgbUnlock() {
  if (s_rgbMutex)
    xSemaphoreGive(s_rgbMutex);
}

// Current smoothed RGB output (for buttery smooth transitions)
static float s_currR = 0.0f;
static float s_currG = 0.0f;
static float s_currB = 0.0f;

// Master brightness factor (0 - 255, default ~160 to avoid eye glare)
static uint8_t s_brightness = 160;

// Manual override state
static bool s_overrideActive = false;
static uint8_t s_overrideR = 0;
static uint8_t s_overrideG = 0;
static uint8_t s_overrideB = 0;

static uint32_t s_lastUpdate = 0;
static uint8_t s_lastSentR = 0;
static uint8_t s_lastSentG = 0;
static uint8_t s_lastSentB = 0;
static bool s_hasSentRgb = false;

static inline void rgbWriteIfChangedLocked(uint8_t r, uint8_t g, uint8_t b) {
  if (!s_hasSentRgb || r != s_lastSentR || g != s_lastSentG || b != s_lastSentB) {
    s_lastSentR = r;
    s_lastSentG = g;
    s_lastSentB = b;
    s_hasSentRgb = true;
    rgbLedWrite(PIN_RGB_LED, r, g, b);
  }
}

void rgbLedInit() {
  if (!s_rgbMutex) {
    s_rgbMutex = xSemaphoreCreateMutex();
  }
  pinMode(PIN_RGB_LED, OUTPUT);
  // Turn off initially
  rgbLedShow(0, 0, 0);
  Serial.printf("[RGB] Onboard WS2812 initialized on GPIO %d (enabled: %d)\n", PIN_RGB_LED, rgbLedEnabled);
}

void rgbLedSetEnabled(bool en) {
  rgbLedEnabled = en;
  if (!en) {
    rgbLedShow(0, 0, 0);
  }
}

bool rgbLedIsEnabled() {
  return rgbLedEnabled;
}

void rgbLedSetBrightness(uint8_t b) {
  if (rgbLock()) {
    s_brightness = b;
    rgbUnlock();
  } else {
    s_brightness = b;
  }
}

uint8_t rgbLedGetBrightness() {
  return s_brightness;
}

void rgbLedSetOverride(uint8_t r, uint8_t g, uint8_t b) {
  if (!rgbLock())
    return;
  s_overrideActive = true;
  s_overrideR = r;
  s_overrideG = g;
  s_overrideB = b;
  uint8_t sr = rgbLedEnabled ? (uint8_t)(((uint16_t)r * s_brightness) / 255) : 0;
  uint8_t sg = rgbLedEnabled ? (uint8_t)(((uint16_t)g * s_brightness) / 255) : 0;
  uint8_t sb = rgbLedEnabled ? (uint8_t)(((uint16_t)b * s_brightness) / 255) : 0;
  s_currR = (float)sr;
  s_currG = (float)sg;
  s_currB = (float)sb;
  rgbWriteIfChangedLocked(sr, sg, sb);
  rgbUnlock();
}

void rgbLedClearOverride() {
  if (rgbLock()) {
    s_overrideActive = false;
    rgbUnlock();
  } else {
    s_overrideActive = false;
  }
}

void rgbLedShow(uint8_t r, uint8_t g, uint8_t b) {
  if (!rgbLock())
    return;
  s_currR = (float)r;
  s_currG = (float)g;
  s_currB = (float)b;
  rgbWriteIfChangedLocked(r, g, b);
  rgbUnlock();
}

void rgbLedUpdate() {
  if (!rgbLock(0))
    return;
  if (!rgbLedEnabled) {
    if (s_currR > 0.0f || s_currG > 0.0f || s_currB > 0.0f || !s_hasSentRgb ||
        s_lastSentR != 0 || s_lastSentG != 0 || s_lastSentB != 0) {
      s_currR = 0.0f;
      s_currG = 0.0f;
      s_currB = 0.0f;
      rgbWriteIfChangedLocked(0, 0, 0);
    }
    rgbUnlock();
    return;
  }

  uint32_t now = millis();
  // 50Hz update rate (every 20ms) is ideal for butter-smooth animation without CPU load
  if (now - s_lastUpdate < 20) {
    rgbUnlock();
    return;
  }
  s_lastUpdate = now;

  float targetR = 0.0f;
  float targetG = 0.0f;
  float targetB = 0.0f;
  float breathScale = 1.0f;

  if (s_overrideActive) {
    targetR = (float)s_overrideR;
    targetG = (float)s_overrideG;
    targetB = (float)s_overrideB;
  } else if (inCircadianSleep || manualSleep || currentEmotion == EMOTION_SLEEPING) {
    // Night / Sleep mode: very faint warm ember or completely off
    targetR = 4.0f;
    targetG = 1.0f;
    targetB = 0.0f;
    breathScale = 0.5f + 0.5f * sinf(now * 0.0015f); // 4-second slow respiration
  } else {
    // Check speech/answering state first (even if emotion hasn't caught up)
    Emotion em = currentEmotion;
    if (isSpeechPlaying() && em != EMOTION_LISTENING) {
      em = EMOTION_SPEAKING;
    }

    switch (em) {
      case EMOTION_LISTENING: {
        // Glowing Siri / Alexa Cyan pulse with live user voice reaction
        targetR = 0.0f;
        targetG = 220.0f;
        targetB = 255.0f;
        float micLvl = fmaxf(soundLevelGet(), soundLevelGetListening());
        float dynamicBoost = (micLvl > 0.03f) ? (micLvl * 3.0f) : 0.0f;
        if (dynamicBoost > 0.6f) dynamicBoost = 0.6f;
        float speed = soundLevelIsSpeechLocked() ? 0.011f : 0.006f;
        breathScale = 0.48f + 0.25f * sinf(now * speed) + dynamicBoost;
        break;
      }

      case EMOTION_SPEAKING: {
        // Vibrant Emerald Green answering glow reacting in real-time to speech amplitude
        targetR = 15.0f;
        targetG = 255.0f;
        targetB = 70.0f;
        float spk = getSpeechAmplitude();
        float spkBoost = (spk > 0.02f) ? (spk * 2.8f) : 0.0f;
        if (spkBoost > 0.65f) spkBoost = 0.65f;
        breathScale = 0.45f + 0.15f * sinf(now * 0.005f) + spkBoost;
        break;
      }

      case EMOTION_THINKING:
        // Deep Mystical Violet / Purple breathing
        targetR = 180.0f;
        targetG = 25.0f;
        targetB = 255.0f;
        breathScale = 0.60f + 0.40f * sinf(now * 0.004f); // Contemplative slow wave
        break;

      case EMOTION_HAPPY:
      case EMOTION_EXCITED:
      case EMOTION_PARTY:
        // Sunny Warm Gold / Yellow
        targetR = 255.0f;
        targetG = 180.0f;
        targetB = 0.0f;
        breathScale = 0.75f + 0.25f * sinf(now * 0.005f);
        break;

      case EMOTION_PETTED:
      case EMOTION_HEARTS:
      case EMOTION_LOVESTRUCK:
      case EMOTION_CUTE:
        // Sweet Rose Pink
        targetR = 255.0f;
        targetG = 30.0f;
        targetB = 140.0f;
        breathScale = 0.70f + 0.30f * sinf(now * 0.0045f);
        break;

      case EMOTION_ANGRY:
      case EMOTION_EVIL:
        // Fiery Crimson Red
        targetR = 255.0f;
        targetG = 12.0f;
        targetB = 10.0f;
        breathScale = 0.75f + 0.25f * sinf(now * 0.006f);
        break;

      case EMOTION_SAD:
      case EMOTION_CRYING:
        // Deep Melancholy Ocean Blue
        targetR = 10.0f;
        targetG = 60.0f;
        targetB = 255.0f;
        breathScale = 0.60f + 0.40f * sinf(now * 0.0025f);
        break;

      case EMOTION_CONFUSED:
      case EMOTION_CURIOUS:
        // Warm Orange / Amber
        targetR = 255.0f;
        targetG = 110.0f;
        targetB = 5.0f;
        breathScale = 0.70f + 0.30f * sinf(now * 0.004f);
        break;

      case EMOTION_ERROR:
        // Quick Red Alert Strobe
        targetR = 255.0f;
        targetG = 0.0f;
        targetB = 0.0f;
        breathScale = ((now / 200) % 2 == 0) ? 1.0f : 0.1f;
        break;

      case EMOTION_IDLE:
      default:
        // Gentle soothing Teal / Warm Cyan ambient breath
        targetR = 15.0f;
        targetG = 130.0f;
        targetB = 110.0f;
        breathScale = 0.70f + 0.30f * sinf(now * 0.0025f); // Calm 2.5s breathing
        break;
    }
  }

  // Apply master brightness limit and breathing modulation
  float masterFactor = ((float)s_brightness / 255.0f) * breathScale;
  targetR *= masterFactor;
  targetG *= masterFactor;
  targetB *= masterFactor;

  // Butter-smooth ease / lerp towards target (18% step per 20ms)
  s_currR += (targetR - s_currR) * 0.18f;
  s_currG += (targetG - s_currG) * 0.18f;
  s_currB += (targetB - s_currB) * 0.18f;

  uint8_t outR = (uint8_t)fminf(fmaxf(s_currR, 0.0f), 255.0f);
  uint8_t outG = (uint8_t)fminf(fmaxf(s_currG, 0.0f), 255.0f);
  uint8_t outB = (uint8_t)fminf(fmaxf(s_currB, 0.0f), 255.0f);

  rgbWriteIfChangedLocked(outR, outG, outB);
  rgbUnlock();
}

