#include "hardware/AudioEngine.h"
#include "config/AppConfig.h"
#include <Preferences.h>
#include <math.h>
#include <string.h>

void pumpUI();

// ---- Tunables (override in AppConfig.h or build flags) ----------------------

// Tone peak level at maximum volume, as a fraction of full scale.
// Lower this if tones clip or sound harsh; raise it if they are too quiet.
#ifndef TONE_PEAK_FRACTION
#define TONE_PEAK_FRACTION 0.70f
#endif

// Samples generated per tone-synth block (stereo frames).
#ifndef TONE_BLOCK_FRAMES
#define TONE_BLOCK_FRAMES 256
#endif

// Speech/transmit frame buffer size (stereo frames).
#ifndef TX_FRAMES
#define TX_FRAMES 256
#endif

// ---- Shared handles ---------------------------------------------------------

i2s_chan_handle_t rxHandle = nullptr;
i2s_chan_handle_t txHandle = nullptr;

// ---- Speaker ownership ------------------------------------------------------
// Only one source may drive the I2S TX channel at a time. Tones and speech
// are never interleaved, which would produce garbled audio.

enum SpkOwner : uint8_t { SPK_NONE, SPK_TONE, SPK_SPEECH };
static SpkOwner spkOwner = SPK_NONE;

// ---- Sample helpers ---------------------------------------------------------

// Soft-clip a sample given in int16 units and return a left-justified 32-bit
// I2S frame value. The cubic knee avoids the harsh edges of hard clipping.
static inline int32_t toFrame32(float x) {
  float n = x * (1.0f / 32768.0f);
  if (n > 1.0f)
    n = 1.0f;
  else if (n < -1.0f)
    n = -1.0f;
  n = 1.5f * n - 0.5f * n * n * n; // f(+-1) = +-1, smooth at the edges
  int32_t s16 = (int32_t)(n * 32767.0f);
  return s16 * 65536; // equivalent to << 16 without shifting a negative value
}

// Perceptual volume curve: speakerVolume 0..100 -> gain 0..1.
static inline float volumeGain() {
  float v = speakerVolume / 100.0f;
  if (v < 0.0f)
    v = 0.0f;
  else if (v > 1.0f)
    v = 1.0f;
  return v * v;
}

// ============================================================
// TONE SYNTHESIZER STATE
// ============================================================
static Tone toneQueue[8];
static int toneCount = 0;
static int toneIndex = 0;
static uint32_t toneStartTime = 0;
static bool tonePlaying = false;
static float tonePhase = 0.0f;

void playToneQueue(Tone *tones, int count) {
  if (spkOwner == SPK_SPEECH) {
    if (!busyProcessing) {
      // Auto-recover if speech finished but speaker lock was left held
      spkOwner = SPK_NONE;
    } else {
      return;
    }
  }
  if (count <= 0 || !tones)
    return;
  if (count > 8)
    count = 8;
  for (int i = 0; i < count; i++)
    toneQueue[i] = tones[i];
  toneCount = count;
  toneIndex = 0;
  toneStartTime = millis();
  tonePhase = 0.0f;
  tonePlaying = true;
  spkOwner = SPK_TONE;
}

bool isTonePlaying() {
  return tonePlaying;
}

void updateTonePlayer() {
  if (!tonePlaying || !txHandle || spkOwner != SPK_TONE)
    return;

  uint32_t now = millis();

  // Advance past finished tones, keeping exact timing across boundaries.
  while (toneIndex < toneCount &&
         (now - toneStartTime) >= (uint32_t)toneQueue[toneIndex].ms) {
    toneStartTime += (uint32_t)toneQueue[toneIndex].ms;
    toneIndex++;
  }
  if (toneIndex >= toneCount) {
    tonePlaying = false;
    spkOwner = SPK_NONE;
    return;
  }

  const Tone &cur = toneQueue[toneIndex];
  const float duration = cur.ms / 1000.0f;
  const float attack = fminf(0.015f, duration * 0.25f);
  const float release = fminf(0.025f, duration * 0.35f);
  const float elapsed = (float)(now - toneStartTime) / 1000.0f;
  const float amp = 32767.0f * TONE_PEAK_FRACTION * volumeGain();
  const float phaseStep = 2.0f * (float)M_PI * cur.freq / (float)SAMPLE_RATE;

  static int32_t buf[TONE_BLOCK_FRAMES * 2];

  for (int i = 0; i < TONE_BLOCK_FRAMES; i++) {
    // Per-sample time keeps the envelope smooth within the block.
    float t = elapsed + (float)i / (float)SAMPLE_RATE;

    float env = 1.0f;
    if (attack > 0.001f && t < attack)
      env = t / attack;
    else if (release > 0.001f && t > duration - release)
      env = (duration - t) / release;
    if (env < 0.0f)
      env = 0.0f;
    else if (env > 1.0f)
      env = 1.0f;

    float s = sinf(tonePhase) * amp * env;
    tonePhase += phaseStep;
    if (tonePhase >= 2.0f * (float)M_PI)
      tonePhase -= 2.0f * (float)M_PI;

    int32_t v = toFrame32(s);
    buf[i * 2] = v;
    buf[i * 2 + 1] = v;
  }

  spkWriteAll(buf, sizeof(buf));
}

// ============================================================
// ROBOT SOUND PRESETS
// ============================================================
void soundPetLove() {
  Tone t[] = {{1046, 90}, {1318, 90}, {1568, 150}};
  playToneQueue(t, 3);
}
void soundPetGiggle() {
  Tone t[] = {{1318, 50}, {1568, 50}, {1318, 50}, {1568, 50}, {2093, 120}};
  playToneQueue(t, 5);
}
void soundPetShy() {
  Tone t[] = {{1568, 80}, {1318, 100}, {1046, 150}};
  playToneQueue(t, 3);
}
void soundPetSparkle() {
  Tone t[] = {{2093, 40}, {2349, 40}, {2637, 40}, {3136, 40}, {4186, 120}};
  playToneQueue(t, 5);
}
void soundPetKiss() {
  Tone t[] = {{1318, 60}, {2093, 100}};
  playToneQueue(t, 2);
}
void soundPetCuddle() {
  Tone t[] = {{659, 100}, {784, 100}, {880, 150}};
  playToneQueue(t, 3);
}
void soundPetPlayful() {
  Tone t[] = {{1046, 60}, {1568, 60}, {1046, 60}, {1568, 60}, {2093, 100}};
  playToneQueue(t, 5);
}
void soundPetSleepy() {
  Tone t[] = {{1318, 120}, {1175, 120}, {1046, 120}, {880, 200}};
  playToneQueue(t, 4);
}
void soundPetProud() {
  Tone t[] = {{1046, 80}, {1318, 80}, {1568, 100}, {2093, 180}};
  playToneQueue(t, 4);
}
void soundPetSurprised() {
  Tone t[] = {{1046, 60}, {2093, 60}, {1760, 100}};
  playToneQueue(t, 3);
}
void playBootChime() {
  Tone t[] = {{660, 100}, {990, 100}, {1320, 150}};
  playToneQueue(t, 3);
}
void soundError() {
  Tone t[] = {{660, 100}, {523, 150}};
  playToneQueue(t, 2);
}
void soundClock() {
  Tone t[] = {{880, 80}, {1175, 80}, {1568, 120}};
  playToneQueue(t, 3);
}
void soundListening() {
  Tone t[] = {{1046, 60}, {1568, 90}};
  playToneQueue(t, 2);
}
void soundTouchBlip() {
  Tone t[] = {{1318, 45}, {1760, 55}};
  playToneQueue(t, 2);
}
void soundVolUp() {
  Tone t[] = {{1200, 30}, {1800, 40}};
  playToneQueue(t, 2);
}
void soundVolDown() {
  Tone t[] = {{1800, 30}, {1200, 40}};
  playToneQueue(t, 2);
}

// ============================================================
// INTERACTIVE SOUNDBOARD PRESETS
// ============================================================
void soundFart() {
  Tone t[] = {{135, 60}, {95, 70}, {150, 40}, {85, 90}, {110, 80}, {65, 140}};
  playToneQueue(t, 6);
}

void soundLaser() {
  Tone t[] = {{2800, 25}, {2200, 25}, {1700, 25},
              {1200, 25}, {750, 30},  {400, 35}};
  playToneQueue(t, 6);
}

void soundFanfare() {
  Tone t[] = {{523, 100}, {659, 100}, {784, 120}, {1046, 300}};
  playToneQueue(t, 4);
}

void soundCoin() {
  Tone t[] = {{988, 80}, {1319, 280}};
  playToneQueue(t, 2);
}

void soundSadViolin() {
  Tone t[] = {{784, 250}, {740, 250}, {659, 320}, {587, 450}};
  playToneQueue(t, 4);
}

// ============================================================
// I2S DRIVER INITIALIZATION
// ============================================================
bool startAudio() {
  i2s_chan_config_t chan =
      I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  chan.dma_desc_num = 6;
  chan.dma_frame_num = 256;
  chan.auto_clear = true;
  if (i2s_new_channel(&chan, &txHandle, &rxHandle) != ESP_OK)
    return false;

  // TX (speaker): dout = amp pin, din unused
  i2s_std_config_t txCfg = {.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
                            .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
                                I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO),
                            .gpio_cfg = {.mclk = I2S_GPIO_UNUSED,
                                         .bclk = (gpio_num_t)PIN_I2S_BCLK,
                                         .ws = (gpio_num_t)PIN_I2S_WS,
                                         .dout = (gpio_num_t)PIN_AMP_DIN,
                                         .din = I2S_GPIO_UNUSED}};
  // RX (microphone): din = mic pin, dout unused
  i2s_std_config_t rxCfg = {.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
                            .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
                                I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO),
                            .gpio_cfg = {.mclk = I2S_GPIO_UNUSED,
                                         .bclk = (gpio_num_t)PIN_I2S_BCLK,
                                         .ws = (gpio_num_t)PIN_I2S_WS,
                                         .dout = I2S_GPIO_UNUSED,
                                         .din = (gpio_num_t)PIN_MIC_SD}};
  if (i2s_channel_init_std_mode(txHandle, &txCfg) != ESP_OK)
    return false;
  if (i2s_channel_init_std_mode(rxHandle, &rxCfg) != ESP_OK)
    return false;
  if (i2s_channel_enable(txHandle) != ESP_OK ||
      i2s_channel_enable(rxHandle) != ESP_OK)
    return false;
  return true;
}

// ============================================================
// MICROPHONE
// ============================================================
void drainMic() {
  static int32_t tmp[64];
  size_t br = 0;
  int guard = 0;
  while (guard++ < 300 &&
         i2s_channel_read(rxHandle, tmp, sizeof(tmp), &br, 5) == ESP_OK &&
         br > 0) {
    if ((guard & 31) == 0)  // pump face every ~32 iterations
      pumpUI();
  }
}

static void saveMicSlot(int slot) {
  Preferences p;
  p.begin("myrobot", false);
  p.putInt("micslot", slot);
  p.end();
}

void detectMicSlot() {
  if (micSlotLocked || !rxHandle)
    return;

  static int32_t tmp[AUDIO_FRAMES * 2];
  float pL = 0, pR = 0, oL = 0, oR = 0;
  double sumL = 0, sumR = 0;
  uint32_t seen = 0, counted = 0;
  uint32_t t0 = millis();

  while (counted < SAMPLE_RATE / 4 && millis() - t0 < 900) {
    size_t br = 0;
    if (i2s_channel_read(rxHandle, tmp, sizeof(tmp), &br, 200) != ESP_OK ||
        br == 0)
      break;

    int frames = br / (2 * sizeof(int32_t));
    for (int i = 0; i < frames; i++) {
      // High-pass filter each channel to remove DC offset before measuring.
      float xl = (float)(tmp[i * 2] >> 16);
      float xr = (float)(tmp[i * 2 + 1] >> 16);
      float yl = xl - pL + 0.995f * oL;
      pL = xl;
      oL = yl;
      float yr = xr - pR + 0.995f * oR;
      pR = xr;
      oR = yr;

      if (++seen > SAMPLE_RATE / 10) {
        sumL += fabsf(yl);
        sumR += fabsf(yr);
        counted++;
      }
    }
    pumpUI();
  }

  if (counted < 1000)
    return;

  float aL = (float)(sumL / counted);
  float aR = (float)(sumR / counted);
  Serial.printf("[MIC] slot energy L=%.1f R=%.1f\n", aL, aR);

  if (aL > 1.2f && aL > 1.5f * aR) {
    micSlot = 0;
    micSlotLocked = true;
    saveMicSlot(0);
    Serial.println("[MIC] Using LEFT slot 0 (locked & saved to NVS)");
  } else if (aR > 5.0f && aR > 2.5f * aL) {
    micSlot = 1;
    micSlotLocked = true;
    saveMicSlot(1);
    Serial.println("[MIC] Using RIGHT slot 1 (locked & saved to NVS)");
  } else {
    // INMP441 standard is GND -> Left channel (slot 0)
    micSlot = 0;
    Serial.println("[MIC] Ambiguous slot, defaulting to LEFT slot 0 (standard INMP441 GND)");
  }
}

// ============================================================
// SPEAKER WRITE & SPEECH PATH
// ============================================================
static int32_t txBuf[TX_FRAMES * 2];
static int txPos = 0;

bool spkWriteAll(const void *data, size_t bytes) {
  if (!txHandle)
    return false;

  const uint8_t *p = (const uint8_t *)data;
  size_t off = 0;
  int stalls = 0;

  while (off < bytes) {
    size_t w = 0;
    esp_err_t e = i2s_channel_write(txHandle, p + off, bytes - off, &w, 50);
    if (e != ESP_OK && e != ESP_ERR_TIMEOUT)
      return false;
    off += w;
    if (w == 0 && ++stalls > 8)
      return false;
  }
  return true;
}

void spkFlush() {
  if (txPos > 0) {
    spkWriteAll(txBuf, (size_t)txPos * 2 * sizeof(int32_t));
    txPos = 0;
  }
}

// Call once before a speech stream starts. Speech takes priority over tones.
void spkBeginSpeech() {
  if (spkOwner != SPK_SPEECH) {
    tonePlaying = false;
    spkOwner = SPK_SPEECH;
  }
}

// Call when a speech stream ends. Flushes remaining samples and releases
// the speaker so tones can play again.
void spkEndSpeech() {
  spkFlush();
  spkOwner = SPK_NONE;
}

void spkPushSample(int16_t s) {
  if (spkOwner != SPK_SPEECH)
    spkBeginSpeech();

  int32_t v = toFrame32((float)s * volumeGain());
  txBuf[txPos * 2] = v;
  txBuf[txPos * 2 + 1] = v;
  if (++txPos >= TX_FRAMES)
    spkFlush();
}

// ============================================================
// WAV HEADER
// ============================================================
void makeWavHeader(uint8_t *h, uint32_t dataSize) {
  memset(h, 0, 44);
  uint32_t fileSize = dataSize + 36;
  uint32_t byteRate = SAMPLE_RATE * 2;

  memcpy(h, "RIFF", 4);
  h[4] = fileSize;
  h[5] = fileSize >> 8;
  h[6] = fileSize >> 16;
  h[7] = fileSize >> 24;
  memcpy(h + 8, "WAVE", 4);
  memcpy(h + 12, "fmt ", 4);
  h[16] = 16; // PCM chunk size
  h[20] = 1;  // PCM format
  h[22] = 1;  // mono
  h[24] = (uint8_t)(SAMPLE_RATE & 0xFF);
  h[25] = SAMPLE_RATE >> 8;
  h[26] = SAMPLE_RATE >> 16;
  h[27] = SAMPLE_RATE >> 24;
  h[28] = (uint8_t)(byteRate & 0xFF);
  h[29] = byteRate >> 8;
  h[30] = byteRate >> 16;
  h[31] = byteRate >> 24;
  h[32] = 2;  // block align
  h[34] = 16; // bits per sample
  memcpy(h + 36, "data", 4);
  h[40] = dataSize;
  h[41] = dataSize >> 8;
  h[42] = dataSize >> 16;
  h[43] = dataSize >> 24;
}