#include "hardware/SoundLevel.h"
#include "hardware/AudioEngine.h"
#include "hardware/WakeWordEngine.h"
#include "config/AppConfig.h"
#include <driver/i2s_std.h>
#include <math.h>

static float s_smoothedLevel = 0.0f;
static float s_prevIn = 0.0f;
static float s_prevOut = 0.0f;
static uint32_t s_lastOutputAt = 0;

static volatile float s_listenLevel = 0.0f;
static volatile bool s_listenSpeechLocked = false;

void soundLevelResetListening() {
  s_listenLevel = 0.0f;
  s_listenSpeechLocked = false;
}

void soundLevelUpdateListening(float batchMeanAbs, bool speechLocked) {
  s_listenSpeechLocked = speechLocked;
  float net = batchMeanAbs - 140.0f;
  if (net < 0.0f)
    net = 0.0f;
  float raw = net / 5200.0f;
  if (raw > 1.0f)
    raw = 1.0f;

  float cur = s_listenLevel;
  if (raw > cur) {
    cur += 0.55f * (raw - cur);
  } else {
    cur += 0.18f * (raw - cur);
  }
  if (cur < 0.005f)
    cur = 0.0f;
  s_listenLevel = cur;
}

float soundLevelGetListening() {
  return s_listenLevel;
}

bool soundLevelIsSpeechLocked() {
  return s_listenSpeechLocked;
}

float soundLevelGet() {
  return s_smoothedLevel;
}

void soundLevelTick() {
  uint32_t now = millis();
  bool outputting = kiwiIsOutputting();
  if (outputting) {
    s_lastOutputAt = now;
  }
  bool gateClosed = outputting || (now - s_lastOutputAt < 400);

  if (!soundReactiveEnabled || gateClosed || !rxHandle || !micSlotLocked) {
    s_smoothedLevel = 0.0f;
    s_prevIn = 0.0f;
    s_prevOut = 0.0f;
    return;
  }

  // If wake-word engine or pre-wake bridge is actively reading I2S, don't compete for rxHandle
  if (wakeWordIsListening() || preWakeIsBridgeActive()) {
    return;
  }

  static int32_t buf[64 * 2];
  size_t br = 0;
  esp_err_t err = i2s_channel_read(rxHandle, buf, sizeof(buf), &br, 0);
  if (err != ESP_OK || br == 0) {
    return;
  }

  int frames = (int)(br / (2 * sizeof(int32_t)));
  if (frames <= 0) {
    return;
  }

  int slot = getMicSlot();
  float sumAbs = 0.0f;
  for (int i = 0; i < frames; i++) {
    int32_t s32 = buf[i * 2 + slot];
    float x = (float)(s32 >> 8) / 256.0f;
    float y = x - s_prevIn + 0.995f * s_prevOut;
    s_prevIn = x;
    s_prevOut = y;
    sumAbs += fabsf(y);
  }

  float meanAbs = sumAbs / (float)frames;
  float scaled = meanAbs * MIC_GAIN * SOUND_REACT_GAIN;

  // Subtract noise floor
  float net = scaled - (float)NOISE_GATE_THRESHOLD;
  if (net < 0.0f) {
    net = 0.0f;
  }

  // Normalize against fixed full-scale reference (16000.0f) and clamp 0.0..1.0
  float rawLevel = net / 16000.0f;
  if (rawLevel < 0.0f) {
    rawLevel = 0.0f;
  } else if (rawLevel > 1.0f) {
    rawLevel = 1.0f;
  }

  // Smooth with attack and release: rise quickly (alpha 0.5) and fall slowly (alpha 0.08)
  if (rawLevel > s_smoothedLevel) {
    s_smoothedLevel += 0.5f * (rawLevel - s_smoothedLevel);
  } else {
    s_smoothedLevel += 0.08f * (rawLevel - s_smoothedLevel);
  }

  if (s_smoothedLevel < 0.001f) {
    s_smoothedLevel = 0.0f;
  }
}

void soundLevelFeedSamples(const int16_t *pcm, int count) {
  if (!soundReactiveEnabled || count <= 0 || !pcm) {
    return;
  }
  uint32_t now = millis();
  bool outputting = kiwiIsOutputting();
  if (outputting) {
    s_lastOutputAt = now;
  }
  if (outputting || (now - s_lastOutputAt < 400)) {
    s_smoothedLevel = 0.0f;
    return;
  }

  float sumAbs = 0.0f;
  for (int i = 0; i < count; i++) {
    sumAbs += fabsf((float)pcm[i]);
  }
  float meanAbs = sumAbs / (float)count;
  float scaled = meanAbs * SOUND_REACT_GAIN;
  float net = scaled - (float)NOISE_GATE_THRESHOLD;
  if (net < 0.0f) net = 0.0f;
  float rawLevel = net / 16000.0f;
  if (rawLevel < 0.0f) rawLevel = 0.0f;
  else if (rawLevel > 1.0f) rawLevel = 1.0f;

  if (rawLevel > s_smoothedLevel) {
    s_smoothedLevel += 0.5f * (rawLevel - s_smoothedLevel);
  } else {
    s_smoothedLevel += 0.08f * (rawLevel - s_smoothedLevel);
  }
  if (s_smoothedLevel < 0.001f) {
    s_smoothedLevel = 0.0f;
  }
}

