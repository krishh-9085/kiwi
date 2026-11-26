#include "hardware/AudioEngine.h"
#include "config/AppConfig.h"
#include "power/PowerManager.h"
#include <Preferences.h>
#include <math.h>
#include <string.h>

void pumpUI();

// ---- Tunables (override in AppConfig.h or build flags) ----------------------

// Tone peak level at maximum volume, as a fraction of full scale.
// Calibrated for 4-ohm 3W / 8-ohm speakers: 0.35f provides a clear, warm acoustic presence
// (~4 dB below speech peaks), preventing piercing resonance and power rail brownouts on 5V.
#ifndef TONE_PEAK_FRACTION
#define TONE_PEAK_FRACTION 0.35f
#endif

// Samples generated per tone-synth block (stereo frames).
#ifndef TONE_BLOCK_FRAMES
#define TONE_BLOCK_FRAMES 256
#endif

// Speech/transmit frame buffer size (stereo frames).
#ifndef TX_FRAMES
#define TX_FRAMES 512
#endif

// ---- Shared handles ---------------------------------------------------------

i2s_chan_handle_t rxHandle = nullptr;
i2s_chan_handle_t txHandle = nullptr;

// ---- Speaker ownership ------------------------------------------------------
// Only one source may drive the I2S TX channel at a time. Tones and speech
// are never interleaved, which would produce garbled audio.

enum SpkOwner : uint8_t { SPK_NONE, SPK_TONE, SPK_SPEECH };
static volatile SpkOwner spkOwner = SPK_NONE;

// ---- Sample helpers ---------------------------------------------------------

// Soft-clip a sample given in int16 units and return a left-justified 32-bit
// I2S frame value. A linear response is maintained through normal speech (|n| <= 0.80),
// with a smooth rational soft-knee applied only above 0.80 to protect 4-ohm / 3W speakers
// and the MAX98357A amplifier from square-wave clipping without adding harmonic distortion.
static inline int32_t toFrame32(float x) {
  float n = x * (1.0f / 32768.0f);
  if (n > 0.80f) {
    float diff = n - 0.80f;
    n = 0.80f + 0.20f * (diff / (1.0f + diff));
  } else if (n < -0.80f) {
    float diff = -n - 0.80f;
    n = -(0.80f + 0.20f * (diff / (1.0f + diff)));
  }
  if (n > 1.0f)
    n = 1.0f;
  else if (n < -1.0f)
    n = -1.0f;
  int32_t s16 = (int32_t)(n * 32767.0f);
  return s16 * 65536; // equivalent to << 16 without shifting a negative value
}

// Perceptual volume curve: speakerVolume 0..100 -> gain 0..SPEAKER_MAX_GAIN.
// Uses a v^1.5 psychoacoustic curve for smooth, natural loudness progression,
// multiplied by SPEAKER_MAX_GAIN so 100% volume utilizes the full 3W capacity
// of 4-ohm desktop speakers with cloud TTS audio.
static inline float volumeGain() {
  float v = speakerVolume / 100.0f;
  if (v <= 0.0f)
    return 0.0f;
  if (v >= 1.0f)
    return SPEAKER_MAX_GAIN;
  return (v * sqrtf(v)) * SPEAKER_MAX_GAIN;
}

// ============================================================
// CUTE ROBOT VOCAL & TONE SYNTHESIZER (Wall-E / EMO Engine)
// ============================================================
static Tone toneQueue[32];
static int toneCount = 0;
static int toneIndex = 0;
static uint32_t toneStartTime = 0;
static uint32_t toneNoteSamplesWritten = 0;
static uint32_t toneNoteTotalSamples = 0;
static uint32_t toneTotalSamplesSent = 0;
static uint32_t toneTailUntilMs = 0;
static volatile bool tonePlaying = false;
static float tonePhase = 0.0f;
static float vibPhase = 0.0f;
static float tremPhase = 0.0f;

static inline uint32_t toneMsToSamples(int ms) {
  if (ms <= 0)
    return 0;
  return (uint32_t)(((uint64_t)ms * (uint64_t)SAMPLE_RATE + 500ULL) / 1000ULL);
}

void playToneQueue(Tone *tones, int count) {
  powerNotifyActivity();
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
  if (count > 32)
    count = 32;
  for (int i = 0; i < count; i++)
    toneQueue[i] = tones[i];
  toneCount = count;
  toneIndex = 0;
  toneStartTime = millis();
  toneNoteSamplesWritten = 0;
  toneNoteTotalSamples = toneMsToSamples(toneQueue[0].ms);
  toneTotalSamplesSent = 0;
  toneTailUntilMs = 0;
  tonePhase = 0.0f;
  vibPhase = 0.0f;
  tremPhase = 0.0f;
  tonePlaying = true;
  spkOwner = SPK_TONE;
}

bool isTonePlaying() {
  return tonePlaying;
}

bool isSpeechPlaying() {
  return spkOwner == SPK_SPEECH;
}

bool kiwiIsOutputting() {
  if (spkOwner == SPK_SPEECH)
    return true;
  if (tonePlaying || isTonePlaying())
    return true;
  if (startRecording || (busyProcessing && !cancelRequest))
    return true;
  return false;
}

// Live speech & vocal-tone envelope followers for real-time lip sync
static volatile float s_speechAmplitude = 0.0f;
static volatile float s_speechBrightness = 0.3f;
static volatile float s_toneMouthAmplitude = 0.0f;
static volatile float s_tonePitchNorm = 0.5f;

struct LipSyncFrame {
  float amp;
  float bright;
};
static LipSyncFrame s_lipRing[16];
static int s_lipWrite = 0;
static int s_lipCount = 0;
static int s_blkSamples = 0;
static float s_blkAbsSum = 0.0f;
static float s_blkPeak = 0.0f;
static float s_blkLowSum = 0.0f;
static float s_blkHighSum = 0.0f;
static float s_lpState = 0.0f;
static float s_envAmp = 0.0f;
static float s_envBright = 0.25f;

static void resetLipSyncState() {
  s_speechAmplitude = 0.0f;
  s_speechBrightness = 0.25f;
  s_lipWrite = 0;
  s_lipCount = 0;
  s_blkSamples = 0;
  s_blkAbsSum = 0.0f;
  s_blkPeak = 0.0f;
  s_blkLowSum = 0.0f;
  s_blkHighSum = 0.0f;
  s_lpState = 0.0f;
  s_envAmp = 0.0f;
  s_envBright = 0.25f;
}

void updateTonePlayer() {
  if (!tonePlaying) {
    s_toneMouthAmplitude = 0.0f;
    return;
  }

  // Auto-recover if speaker owner changed or hardware was detached
  if (!txHandle || spkOwner != SPK_TONE) {
    tonePlaying = false;
    toneCount = 0;
    toneIndex = 0;
    toneNoteSamplesWritten = 0;
    toneNoteTotalSamples = 0;
    toneTotalSamplesSent = 0;
    toneTailUntilMs = 0;
    s_toneMouthAmplitude = 0.0f;
    return;
  }

  uint32_t now = millis();

  // Wait cleanly for buffered DMA tail after final note samples were queued
  if (toneTailUntilMs != 0) {
    if ((int32_t)(now - toneTailUntilMs) >= 0) {
      tonePlaying = false;
      toneTailUntilMs = 0;
      spkOwner = SPK_NONE;
      s_toneMouthAmplitude = 0.0f;
    }
    return;
  }

  // Pace synthesis to at most ~48ms ahead of wall-clock audio consumption so
  // short chimes never dump all 256ms of DMA in 1ms or repeat t=0 envelopes.
  uint32_t elapsedMs = now - toneStartTime;
  uint32_t sentMs = (uint32_t)(((uint64_t)toneTotalSamplesSent * 1000ULL) /
                               (uint64_t)SAMPLE_RATE);
  if (sentMs > elapsedMs + 48) {
    return;
  }

  static int32_t buf[TONE_BLOCK_FRAMES * 2];
  const float amp = 32767.0f * TONE_PEAK_FRACTION * volumeGain();

  for (int blk = 0; blk < 2; blk++) {
    elapsedMs = millis() - toneStartTime;
    sentMs = (uint32_t)(((uint64_t)toneTotalSamplesSent * 1000ULL) /
                        (uint64_t)SAMPLE_RATE);
    if (sentMs > elapsedMs + 48) {
      break;
    }

    while (toneIndex < toneCount && toneNoteTotalSamples == 0) {
      toneIndex++;
      toneNoteSamplesWritten = 0;
      if (toneIndex < toneCount) {
        toneNoteTotalSamples = toneMsToSamples(toneQueue[toneIndex].ms);
      }
    }
    if (toneIndex >= toneCount) {
      uint32_t remainMs = (sentMs > elapsedMs) ? (sentMs - elapsedMs) : 0;
      if (remainMs == 0) {
        tonePlaying = false;
        spkOwner = SPK_NONE;
        s_toneMouthAmplitude = 0.0f;
      } else {
        toneTailUntilMs = millis() + remainMs;
      }
      return;
    }

    bool blockHasVocal = false;
    float blockEnvSum = 0.0f;
    float blockFreqMid = toneQueue[toneIndex].freq;
    int activeFrames = 0;

    for (int i = 0; i < TONE_BLOCK_FRAMES; i++) {
      while (toneIndex < toneCount &&
             toneNoteSamplesWritten >= toneNoteTotalSamples) {
        toneIndex++;
        toneNoteSamplesWritten = 0;
        tonePhase = 0.0f;
        vibPhase = 0.0f;
        if (toneIndex < toneCount) {
          toneNoteTotalSamples = toneMsToSamples(toneQueue[toneIndex].ms);
        }
      }
      if (toneIndex >= toneCount) {
        buf[i * 2] = 0;
        buf[i * 2 + 1] = 0;
        continue;
      }

      const Tone &cur = toneQueue[toneIndex];
      uint32_t sampleIdx = toneNoteSamplesWritten++;
      activeFrames++;

      // Silence / rest pause if freq <= 1.0f
      if (cur.freq <= 1.0f) {
        buf[i * 2] = 0;
        buf[i * 2 + 1] = 0;
        continue;
      }

      const float duration =
          (float)toneNoteTotalSamples / (float)SAMPLE_RATE;
      const float attack = fminf(0.015f, duration * 0.25f);
      const float release = fminf(0.025f, duration * 0.35f);
      const float t = (float)sampleIdx / (float)SAMPLE_RATE;

      // Smooth Hann-tapered envelope eliminates sharp digital click transients
      float env = 1.0f;
      if (cur.shape == SHAPE_BELL) {
        // Natural acoustic bell/chime percussive envelope
        const float bellAttack = fminf(0.004f, duration * 0.25f);
        if (bellAttack > 0.0001f && t < bellAttack) {
          env = 0.5f * (1.0f - cosf((float)M_PI * (t / bellAttack)));
        } else {
          float decayDur = duration - bellAttack;
          float decayProgress =
              (decayDur > 0.0001f) ? ((t - bellAttack) / decayDur) : 1.0f;
          if (decayProgress < 0.0f)
            decayProgress = 0.0f;
          if (decayProgress > 1.0f)
            decayProgress = 1.0f;
          float d = 1.0f - decayProgress;
          env = d * d * (0.85f * d + 0.15f);
        }
      } else {
        if (attack > 0.001f && t < attack) {
          env = 0.5f * (1.0f - cosf((float)M_PI * (t / attack)));
        } else if (release > 0.001f && t > duration - release) {
          float rProg = (t - (duration - release)) / release;
          if (rProg > 1.0f)
            rProg = 1.0f;
          env = 0.5f * (1.0f + cosf((float)M_PI * rProg));
        }
      }
      if (env < 0.0f)
        env = 0.0f;
      else if (env > 1.0f)
        env = 1.0f;

      // Smooth pitch interpolation (glissando / slide)
      float progress = (duration > 0.0001f) ? (t / duration) : 1.0f;
      if (progress < 0.0f)
        progress = 0.0f;
      else if (progress > 1.0f)
        progress = 1.0f;

      float instFreq = cur.freq;
      if (cur.endFreq > 1.0f) {
        instFreq = cur.freq + (cur.endFreq - cur.freq) * progress;
      }

      // Vibrato / FM modulation
      if (cur.vibDepth > 0.1f && cur.vibRate > 0.1f) {
        instFreq += sinf(vibPhase) * cur.vibDepth;
        vibPhase += 2.0f * (float)M_PI * cur.vibRate / (float)SAMPLE_RATE;
        if (vibPhase >= 2.0f * (float)M_PI)
          vibPhase -= 2.0f * (float)M_PI;
      }

      // Phase step accumulator
      float phaseStep = 2.0f * (float)M_PI * instFreq / (float)SAMPLE_RATE;
      tonePhase += phaseStep;
      if (tonePhase >= 2.0f * (float)M_PI)
        tonePhase -= 2.0f * (float)M_PI;

      // Harmonic & timbre shaping
      float raw = 0.0f;
      switch (cur.shape) {
      case SHAPE_WARM_VOCAL: {
        raw = sinf(tonePhase) * 0.70f +
              sinf(2.0f * tonePhase) * 0.22f +
              sinf(3.0f * tonePhase) * 0.08f;
        break;
      }
      case SHAPE_PURR: {
        tremPhase += 2.0f * (float)M_PI * 25.0f / (float)SAMPLE_RATE;
        if (tremPhase >= 2.0f * (float)M_PI)
          tremPhase -= 2.0f * (float)M_PI;
        float tremolo = 0.55f + 0.45f * sinf(tremPhase);
        raw = (sinf(tonePhase) * 0.75f + sinf(2.0f * tonePhase) * 0.25f) *
              tremolo;
        break;
      }
      case SHAPE_CHIRP: {
        raw = sinf(tonePhase) * 0.82f + sinf(2.0f * tonePhase) * 0.18f;
        break;
      }
      case SHAPE_WARBLE: {
        float fmMod = sinf(tonePhase * 1.5f) * 1.6f;
        raw = sinf(tonePhase + fmMod) * 0.85f;
        break;
      }
      case SHAPE_BELL: {
        raw = sinf(tonePhase) * 0.74f +
              sinf(2.0f * tonePhase) * 0.16f +
              sinf(2.756f * tonePhase) * 0.10f;
        break;
      }
      default: // SHAPE_SINE
        raw = sinf(tonePhase);
        break;
      }

      bool isVocalTone = (cur.shape == SHAPE_WARM_VOCAL ||
                          cur.shape == SHAPE_WARBLE ||
                          cur.shape == SHAPE_CHIRP ||
                          cur.shape == SHAPE_PURR);
      if (isVocalTone) {
        blockHasVocal = true;
        blockEnvSum += env;
        if (i == TONE_BLOCK_FRAMES / 2) {
          blockFreqMid = instFreq;
        }
      }

      float s = raw * amp * env;
      int32_t v = toFrame32(s);
      buf[i * 2] = v;
      buf[i * 2 + 1] = v;
    }

    if (blockHasVocal && activeFrames > 0) {
      float meanEnv = blockEnvSum / (float)activeFrames;
      float pNorm = (blockFreqMid - 200.0f) / 900.0f;
      if (pNorm < 0.0f)
        pNorm = 0.0f;
      if (pNorm > 1.0f)
        pNorm = 1.0f;
      s_toneMouthAmplitude = fminf(1.0f, meanEnv * 0.85f);
      s_tonePitchNorm = pNorm;
    } else {
      s_toneMouthAmplitude = 0.0f;
    }

    if (!spkWriteAll(buf, sizeof(buf))) {
      tonePlaying = false;
      toneTailUntilMs = 0;
      spkOwner = SPK_NONE;
      s_toneMouthAmplitude = 0.0f;
      return;
    }
    toneTotalSamplesSent += TONE_BLOCK_FRAMES;
  }
}

// ============================================================
// WALL-E / EMO ROBOT VOCAL PRESETS (Warm, Soft & Cute)
// ============================================================

// Affectionate Wall-E Coo ("Awww <3")
void soundPetLove() {
  Tone t[] = {
    {380, 140, 580, SHAPE_WARM_VOCAL, 0, 0},
    {0, 25, 0, SHAPE_SINE, 0, 0},
    {720, 220, 480, SHAPE_WARM_VOCAL, 25, 14}
  };
  playToneQueue(t, 3);
}

// Cute Robot Tickle & Giggle (warm, playful chuckles)
void soundPetGiggle() {
  Tone t[] = {
    {580, 35, 720, SHAPE_WARM_VOCAL, 0, 0},
    {0, 15, 0, SHAPE_SINE, 0, 0},
    {640, 35, 780, SHAPE_WARM_VOCAL, 0, 0},
    {0, 15, 0, SHAPE_SINE, 0, 0},
    {700, 38, 860, SHAPE_WARM_VOCAL, 0, 0},
    {0, 15, 0, SHAPE_SINE, 0, 0},
    {780, 42, 960, SHAPE_WARM_VOCAL, 0, 0},
    {0, 18, 0, SHAPE_SINE, 0, 0},
    {860, 70, 1050, SHAPE_WARM_VOCAL, 25, 16}
  };
  playToneQueue(t, 9);
}

// Tiny Bashful Squeak / Murmur
void soundPetShy() {
  Tone t[] = {
    {440, 95, 620, SHAPE_WARM_VOCAL, 0, 0},
    {620, 160, 400, SHAPE_WARM_VOCAL, 20, 14}
  };
  playToneQueue(t, 2);
}

// Gentle, magical glockenspiel chime (warm bell trill without piercing screech)
void soundPetSparkle() {
  Tone t[] = {
    {659, 42, 784, SHAPE_WARM_VOCAL, 0, 0},     // E5 -> G5
    {784, 42, 988, SHAPE_WARM_VOCAL, 0, 0},     // G5 -> B5
    {988, 48, 1175, SHAPE_WARM_VOCAL, 0, 0},    // B5 -> D6
    {1318, 110, 1318, SHAPE_WARM_VOCAL, 20, 12} // E6 warm bell ring
  };
  playToneQueue(t, 4);
}

// Combined non-blocking coin + sparkle chime when fed
void soundFeedHappy() {
  Tone t[] = {
    {523.3f, 65, 0, SHAPE_BELL, 0, 0},          // C5
    {784.0f, 115, 0, SHAPE_BELL, 10, 7},        // G5
    {659, 42, 784, SHAPE_WARM_VOCAL, 0, 0},     // E5 -> G5
    {784, 42, 988, SHAPE_WARM_VOCAL, 0, 0},     // G5 -> B5
    {988, 48, 1175, SHAPE_WARM_VOCAL, 0, 0},    // B5 -> D6
    {1318, 110, 1318, SHAPE_WARM_VOCAL, 20, 12} // E6
  };
  playToneQueue(t, 6);
}

// Cute "Mwah / Squeak-Pop"
void soundPetKiss() {
  Tone t[] = {
    {480, 45, 960, SHAPE_WARM_VOCAL, 0, 0},
    {960, 50, 580, SHAPE_WARM_VOCAL, 0, 0}
  };
  playToneQueue(t, 2);
}

// Warm Purring Robot Kitten (low motor rumble + coo)
void soundPetCuddle() {
  Tone t[] = {
    {105, 140, 115, SHAPE_PURR, 0, 0},
    {115, 160, 125, SHAPE_PURR, 0, 0},
    {520, 90, 880, SHAPE_WARM_VOCAL, 0, 0}
  };
  playToneQueue(t, 3);
}

// Wall-E / BB-8 Excited Warble ("Ta-da!")
void soundPetPlayful() {
  Tone t[] = {
    {520, 60, 680, SHAPE_WARM_VOCAL, 0, 0},
    {680, 85, 880, SHAPE_WARM_VOCAL, 25, 16},
    {880, 110, 1050, SHAPE_WARM_VOCAL, 18, 12}
  };
  playToneQueue(t, 3);
}

// Cute Robot Yawn & Sigh
void soundPetSleepy() {
  Tone t[] = {
    {340, 220, 580, SHAPE_WARM_VOCAL, 0, 0},
    {580, 360, 220, SHAPE_WARM_VOCAL, 16, 10}
  };
  playToneQueue(t, 2);
}

// Chest-Puffed Robot Fanfare
void soundPetProud() {
  Tone t[] = {
    {440, 70, 554, SHAPE_WARM_VOCAL, 0, 0},
    {554, 70, 659, SHAPE_WARM_VOCAL, 0, 0},
    {659, 90, 880, SHAPE_WARM_VOCAL, 0, 0},
    {880, 200, 1100, SHAPE_WARM_VOCAL, 25, 14}
  };
  playToneQueue(t, 4);
}

// Wall-E Iconic "Whoooa?!"
void soundPetSurprised() {
  Tone t[] = {
    {360, 100, 920, SHAPE_WARM_VOCAL, 0, 0},
    {920, 90, 820, SHAPE_WARM_VOCAL, 30, 16}
  };
  playToneQueue(t, 2);
}

// Luxurious Ambient Robot Wakeup Chime (Warm F-Major-9 acoustic marimba/celesta arpeggio)
void playBootChime() {
  Tone t[] = {
    {349.2f, 90, 0, SHAPE_BELL, 0, 0},    // F4 (warm foundational root note)
    {440.0f, 95, 0, SHAPE_BELL, 0, 0},    // A4 (major third warmth)
    {523.3f, 110, 0, SHAPE_BELL, 0, 0},   // C5 (pure fifth clarity)
    {659.3f, 280, 0, SHAPE_BELL, 8, 6}    // E5 (major seventh resolution with gentle acoustic ring-out)
  };
  playToneQueue(t, 4);
}

// Wall-E Disappointed "Uh-Oh / Sad Wah"
void soundError() {
  Tone t[] = {
    {580, 110, 380, SHAPE_WARM_VOCAL, 0, 0},
    {0, 25, 0, SHAPE_SINE, 0, 0},
    {420, 180, 240, SHAPE_WARM_VOCAL, 25, 12}
  };
  playToneQueue(t, 3);
}

// Elegant Westminster Clock Chime (Classic 3-note antique clock bell motif)
void soundClock() {
  Tone t[] = {
    {392.0f, 110, 0, SHAPE_BELL, 0, 0},   // G4 (deep bell toll)
    {659.3f, 130, 0, SHAPE_BELL, 0, 0},   // E5 (high melodic bell)
    {523.3f, 260, 0, SHAPE_BELL, 6, 5}    // C5 (warm home clock bell resolution with lingering ring-out)
  };
  playToneQueue(t, 3);
}

// Refreshing Morning Breeze & Wind-Chime (Atmospheric pentatonic weather chime)
void soundWeather() {
  Tone t[] = {
    {587.3f, 90, 0, SHAPE_BELL, 0, 0},    // D5 (gentle airy droplet)
    {784.0f, 110, 0, SHAPE_BELL, 0, 0},   // G5 (bright sunny sky)
    {880.0f, 240, 0, SHAPE_BELL, 14, 8}   // A5 (fluttering wind chime in a gentle breeze)
  };
  playToneQueue(t, 3);
}

// Inquisitive Wall-E "Hm?"
void soundListening() {
  Tone t[] = {
    {440, 80, 660, SHAPE_WARM_VOCAL, 0, 0}
  };
  playToneQueue(t, 1);
}

// Soft, tactile water-drop / bubble tap on touch (warm, pleasant, non-fatiguing)
void soundTouchBlip() {
  Tone t[] = {
    {520, 26, 380, SHAPE_WARM_VOCAL, 0, 0}
  };
  playToneQueue(t, 1);
}

// Smooth Marimba Volume Cues
void soundVolUp() {
  Tone t[] = {
    {440, 35, 440, SHAPE_WARM_VOCAL, 0, 0}, // A4
    {554, 45, 554, SHAPE_WARM_VOCAL, 0, 0}  // C#5
  };
  playToneQueue(t, 2);
}

void soundVolDown() {
  Tone t[] = {
    {554, 35, 554, SHAPE_WARM_VOCAL, 0, 0}, // C#5
    {440, 45, 440, SHAPE_WARM_VOCAL, 0, 0}  // A4
  };
  playToneQueue(t, 2);
}

// ============================================================
// INTERACTIVE SOUNDBOARD PRESETS
// ============================================================
void soundFart() {
  Tone t[] = {
    {135, 60, 95, SHAPE_PURR, 30, 15},
    {95, 70, 75, SHAPE_PURR, 25, 12},
    {150, 40, 85, SHAPE_PURR, 20, 10},
    {85, 90, 65, SHAPE_PURR, 35, 14},
    {110, 80, 55, SHAPE_PURR, 25, 10},
    {65, 140, 45, SHAPE_PURR, 15, 8}
  };
  playToneQueue(t, 6);
}

void soundLaser() {
  Tone t[] = {
    {1800, 65, 260, SHAPE_CHIRP, 0, 0}
  };
  playToneQueue(t, 1);
}

void soundFanfare() {
  Tone t[] = {
    {440, 90, 440, SHAPE_WARM_VOCAL, 0, 0},
    {554, 90, 554, SHAPE_WARM_VOCAL, 0, 0},
    {659, 100, 659, SHAPE_WARM_VOCAL, 0, 0},
    {880, 260, 880, SHAPE_WARM_VOCAL, 25, 14}
  };
  playToneQueue(t, 4);
}

// Soft, musical acoustic marimba chime for skin and face change
void soundCoin() {
  Tone t[] = {
    {523.3f, 65, 0, SHAPE_BELL, 0, 0},    // C5
    {784.0f, 160, 0, SHAPE_BELL, 10, 7}   // G5 resonant chime
  };
  playToneQueue(t, 2);
}

void soundSadViolin() {
  Tone t[] = {
    {659, 250, 620, SHAPE_WARM_VOCAL, 18, 8},
    {620, 250, 554, SHAPE_WARM_VOCAL, 18, 8},
    {554, 320, 494, SHAPE_WARM_VOCAL, 20, 8},
    {494, 450, 440, SHAPE_WARM_VOCAL, 25, 6}
  };
  playToneQueue(t, 4);
}

// Infant / Baby Crying Vocalization (sobbing wails & sniffles)
void soundCrying() {
  Tone t[] = {
    // Phase 1: Breath intake & initial wail ("Ah-Waaaah...")
    {480, 80, 680, SHAPE_WARM_VOCAL, 0, 0},
    {720, 360, 560, SHAPE_WARM_VOCAL, 35, 13},
    {0, 35, 0, SHAPE_SINE, 0, 0},

    // Phase 2: Desperate high infant wail ("WAAAAAHH!!")
    {780, 440, 580, SHAPE_WARM_VOCAL, 45, 14},
    {0, 40, 0, SHAPE_SINE, 0, 0},

    // Phase 3: Trembling sob ("Uwaaah...")
    {680, 320, 480, SHAPE_WARM_VOCAL, 32, 12},
    {0, 40, 0, SHAPE_SINE, 0, 0},

    // Phase 4: Sniffling hiccups ("uh... uh... uh...")
    {540, 110, 420, SHAPE_WARM_VOCAL, 20, 10},
    {0, 40, 0, SHAPE_SINE, 0, 0},
    {480, 120, 380, SHAPE_WARM_VOCAL, 18, 9},
    {0, 40, 0, SHAPE_SINE, 0, 0},
    {420, 160, 320, SHAPE_WARM_VOCAL, 15, 8},
    {0, 60, 0, SHAPE_SINE, 0, 0},

    // Phase 5: Second crying wave ("Waaah... waah...")
    {750, 380, 520, SHAPE_WARM_VOCAL, 40, 13},
    {0, 40, 0, SHAPE_SINE, 0, 0},
    {620, 280, 440, SHAPE_WARM_VOCAL, 28, 11},
    {0, 35, 0, SHAPE_SINE, 0, 0},
    {460, 180, 330, SHAPE_WARM_VOCAL, 20, 9}
  };
  playToneQueue(t, 18);
}

// Joyful Baby / Robot Giggle & Belly Laughter ("Hehe-haha-hahahaha-heee!")
void soundLaughing() {
  Tone t[] = {
    // Phase 1: Rising staccato giggles ("He-he-he-he!")
    {600, 35, 750, SHAPE_WARM_VOCAL, 0, 0},
    {0, 15, 0, SHAPE_SINE, 0, 0},
    {680, 38, 840, SHAPE_WARM_VOCAL, 0, 0},
    {0, 15, 0, SHAPE_SINE, 0, 0},
    {760, 42, 940, SHAPE_WARM_VOCAL, 0, 0},
    {0, 15, 0, SHAPE_SINE, 0, 0},
    {860, 45, 1060, SHAPE_WARM_VOCAL, 0, 0},
    {0, 18, 0, SHAPE_SINE, 0, 0},

    // Phase 2: Big melodic laugh burst ("Haaahaahaa!")
    {1120, 220, 920, SHAPE_WARM_VOCAL, 32, 16},
    {0, 25, 0, SHAPE_SINE, 0, 0},

    // Phase 3: Second chuckle roll ("He-he-he-hee!")
    {820, 40, 980, SHAPE_WARM_VOCAL, 0, 0},
    {0, 15, 0, SHAPE_SINE, 0, 0},
    {940, 42, 1120, SHAPE_WARM_VOCAL, 0, 0},
    {0, 15, 0, SHAPE_SINE, 0, 0},
    {1080, 50, 1280, SHAPE_WARM_VOCAL, 0, 0},
    {0, 20, 0, SHAPE_SINE, 0, 0},

    // Phase 4: Big joyful squeal & chuckles ("Heeeee-chuckle!")
    {1220, 240, 860, SHAPE_WARM_VOCAL, 35, 15},
    {0, 25, 0, SHAPE_SINE, 0, 0},
    {740, 60, 880, SHAPE_WARM_VOCAL, 0, 0},
    {0, 20, 0, SHAPE_SINE, 0, 0},
    {840, 85, 960, SHAPE_WARM_VOCAL, 20, 12}
  };
  playToneQueue(t, 21);
}

// ============================================================
// I2S DRIVER INITIALIZATION
// ============================================================
bool startAudio() {
  i2s_chan_config_t chan =
      I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  chan.dma_desc_num = psramFound() ? 8 : 6;
  chan.dma_frame_num = psramFound() ? 512 : 256;
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
  auto failInit = [&](bool txEnabled, bool rxEnabled) -> bool {
    if (txEnabled && txHandle)
      i2s_channel_disable(txHandle);
    if (rxEnabled && rxHandle)
      i2s_channel_disable(rxHandle);
    if (txHandle) {
      i2s_del_channel(txHandle);
      txHandle = nullptr;
    }
    if (rxHandle) {
      i2s_del_channel(rxHandle);
      rxHandle = nullptr;
    }
    return false;
  };
  if (i2s_channel_init_std_mode(txHandle, &txCfg) != ESP_OK)
    return failInit(false, false);
  if (i2s_channel_init_std_mode(rxHandle, &rxCfg) != ESP_OK)
    return failInit(false, false);
  if (i2s_channel_enable(txHandle) != ESP_OK)
    return failInit(false, false);
  if (i2s_channel_enable(rxHandle) != ESP_OK)
    return failInit(true, false);
  return true;
}

// ============================================================
// MICROPHONE
// ============================================================
void drainMic() {
  if (!rxHandle)
    return;
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

static portMUX_TYPE s_micMux = portMUX_INITIALIZER_UNLOCKED;

static void saveMicSlot(int slot) {
  Preferences p;
  p.begin("myrobot", false);
  p.putInt("micslot", slot);
  p.putBool("micslot_lock", true);
  p.end();
}

bool setMicSlot(int slot, bool persist, bool force) {
  if (slot != 0 && slot != 1) {
    slot = 0;
  }
  portENTER_CRITICAL(&s_micMux);
  if (micSlotLocked && !force && !persist) {
    portEXIT_CRITICAL(&s_micMux);
    return false;
  }
  int oldSlot = micSlot;
  micSlot = slot;
  if (persist) {
    micSlotLocked = true;
  }
  portEXIT_CRITICAL(&s_micMux);

  if (persist) {
    saveMicSlot(slot);
    Serial.printf("[MIC] Slot locked to %d (saved to NVS)\n", slot);
  } else if (oldSlot != slot) {
    Serial.printf("[MIC] Auto-detected slot changed: %d -> %d (unlocked, not saved to NVS)\n", oldSlot, slot);
  }
  return true;
}

int getMicSlot() {
  portENTER_CRITICAL(&s_micMux);
  int slot = micSlot;
  portEXIT_CRITICAL(&s_micMux);
  return slot;
}

bool isMicSlotLocked() {
  return micSlotLocked;
}

void setMicSlotLocked(bool locked) {
  portENTER_CRITICAL(&s_micMux);
  micSlotLocked = locked;
  portEXIT_CRITICAL(&s_micMux);
}

void detectMicSlot() {
  if (micSlotLocked || !rxHandle)
    return;

  const size_t tmpSamples = AUDIO_FRAMES * 2;
  int32_t *tmp = (int32_t *)malloc(tmpSamples * sizeof(int32_t));
  if (!tmp)
    return;
  float pL = 0, pR = 0, oL = 0, oR = 0;
  double sumL = 0, sumR = 0;
  uint32_t seen = 0, counted = 0;
  uint32_t t0 = millis();

  while (counted < SAMPLE_RATE / 4 && millis() - t0 < 900) {
    size_t br = 0;
    if (i2s_channel_read(rxHandle, tmp, tmpSamples * sizeof(int32_t), &br, 200) != ESP_OK ||
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

  free(tmp);

  if (counted < 1000)
    return;

  float aL = (float)(sumL / counted);
  float aR = (float)(sumR / counted);
  Serial.printf("[MIC] slot energy L=%.1f R=%.1f\n", aL, aR);

  if (aL > 1.2f && aL > 1.5f * aR) {
    setMicSlot(0, true, true);
    Serial.println("[MIC] Using LEFT slot 0 (locked & saved to NVS)");
  } else if (aR > 5.0f && aR > 2.5f * aL) {
    setMicSlot(1, true, true);
    Serial.println("[MIC] Using RIGHT slot 1 (locked & saved to NVS)");
  } else {
    // INMP441 standard is GND -> Left channel (slot 0)
    setMicSlot(0, false, true);
    Serial.println("[MIC] Ambiguous slot, defaulting to LEFT slot 0 (standard INMP441 GND)");
  }
}

// ============================================================
// SPEAKER WRITE & SPEECH PATH
// ============================================================
static int32_t txBuf[TX_FRAMES * 2];
static int txPos = 0;

// Gentle TTS amplitude envelope & zero-ramp state (prevents pops on start/abort)
constexpr uint16_t SPEECH_FADE_IN_SAMPLES = 240;   // 15 ms @ 16 kHz
constexpr uint16_t SPEECH_FADE_OUT_SAMPLES = 192;  // 12 ms @ 16 kHz
constexpr uint16_t SPEECH_TAIL_ZERO_SAMPLES = 128; // 8 ms silence tail @ 16 kHz
static uint16_t s_speechFadeInIdx = 0;
static float s_lastSpeechOut = 0.0f;

static void spkWriteRampToZero() {
  float startVal = s_lastSpeechOut;
  s_lastSpeechOut = 0.0f;
  if (!txHandle)
    return;

  const uint16_t rampLen =
      (fabsf(startVal) > 1.0f) ? SPEECH_FADE_OUT_SAMPLES : 0;
  const uint16_t totalFrames = rampLen + SPEECH_TAIL_ZERO_SAMPLES;
  for (uint16_t i = 0; i < rampLen; i++) {
    float t = (float)(i + 1) / (float)rampLen;
    float env = 0.5f * (1.0f + cosf((float)M_PI * t));
    int32_t v = toFrame32(startVal * env);
    txBuf[i * 2] = v;
    txBuf[i * 2 + 1] = v;
  }
  memset(&txBuf[rampLen * 2], 0,
         (size_t)SPEECH_TAIL_ZERO_SAMPLES * 2 * sizeof(int32_t));

  const uint8_t *p = (const uint8_t *)txBuf;
  size_t bytes = (size_t)totalFrames * 2 * sizeof(int32_t);
  size_t off = 0;
  int stalls = 0;
  while (off < bytes) {
    size_t w = 0;
    esp_err_t e = i2s_channel_write(txHandle, p + off, bytes - off, &w, 25);
    if (e != ESP_OK && e != ESP_ERR_TIMEOUT)
      break;
    off += w;
    if (w == 0 && ++stalls > 4)
      break;
  }
  txPos = 0;
}

bool spkWriteAll(const void *data, size_t bytes) {
  if (!txHandle)
    return false;

  const uint8_t *p = (const uint8_t *)data;
  size_t off = 0;
  int stalls = 0;

  while (off < bytes) {
    if (cancelRequest)
      return false;
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
  powerNotifyActivity();
  if (spkOwner != SPK_SPEECH) {
    tonePlaying = false;
    s_toneMouthAmplitude = 0.0f;
    resetLipSyncState();
    s_speechFadeInIdx = 0;
    s_lastSpeechOut = 0.0f;
    spkOwner = SPK_SPEECH;
  }
}

void toneStop() {
  tonePlaying = false;
  toneCount = 0;
  toneIndex = 0;
  toneNoteSamplesWritten = 0;
  toneNoteTotalSamples = 0;
  toneTotalSamplesSent = 0;
  toneTailUntilMs = 0;
  s_toneMouthAmplitude = 0.0f;
  if (spkOwner == SPK_TONE) {
    spkOwner = SPK_NONE;
  }
}

float getSpeechAmplitude() {
  return s_speechAmplitude;
}

float getSpeechBrightness() {
  return s_speechBrightness;
}

float getToneMouthAmplitude() {
  return s_toneMouthAmplitude;
}

float getTonePitchNorm() {
  return s_tonePitchNorm;
}

// Call when a speech stream ends. Flushes remaining samples (when not cancelled),
// smoothly ramps to zero, and releases the speaker so tones can play again.
void spkEndSpeech() {
  if (!cancelRequest && spkOwner == SPK_SPEECH) {
    spkFlush();
  } else {
    txPos = 0;
  }
  if (spkOwner == SPK_SPEECH || fabsf(s_lastSpeechOut) > 1.0f) {
    spkWriteRampToZero();
  }
  resetLipSyncState();
  s_speechFadeInIdx = 0;
  s_lastSpeechOut = 0.0f;
  if (spkOwner == SPK_SPEECH) {
    spkOwner = SPK_NONE;
  }
}

// Cuts off speech with a short pop-free fade-out without flushing leftover buffer
void spkAbortSpeech() {
  txPos = 0;
  tonePlaying = false;
  toneCount = 0;
  toneIndex = 0;
  toneNoteSamplesWritten = 0;
  toneNoteTotalSamples = 0;
  toneTotalSamplesSent = 0;
  toneTailUntilMs = 0;
  s_toneMouthAmplitude = 0.0f;
  if (spkOwner == SPK_SPEECH || fabsf(s_lastSpeechOut) > 1.0f) {
    spkWriteRampToZero();
  }
  resetLipSyncState();
  s_speechFadeInIdx = 0;
  s_lastSpeechOut = 0.0f;
  spkOwner = SPK_NONE;
}

// Prepares audio hardware for light or deep sleep: stops speech/tones,
// clears queues and disables I2S TX and RX DMA channels.
void audioSleepPrepare() {
  spkEndSpeech();
  tonePlaying = false;
  toneCount = 0;
  toneIndex = 0;
  toneNoteSamplesWritten = 0;
  toneNoteTotalSamples = 0;
  toneTotalSamplesSent = 0;
  toneTailUntilMs = 0;
  s_toneMouthAmplitude = 0.0f;
  resetLipSyncState();
  spkOwner = SPK_NONE;
  cancelRequest = false;
  if (txHandle) {
    i2s_channel_disable(txHandle);
  }
  if (rxHandle) {
    i2s_channel_disable(rxHandle);
  }
}

// Re-enables I2S TX and RX channels after waking from light sleep.
void audioSleepWake() {
  if (txHandle) {
    i2s_channel_enable(txHandle);
  }
  if (rxHandle) {
    i2s_channel_enable(rxHandle);
  }
}

void spkPushSample(int16_t s) {
  if (cancelRequest)
    return;
  if (spkOwner != SPK_SPEECH)
    spkBeginSpeech();

  // Delayed 2-band envelope tracking for DMA-aligned viseme lip sync
  float fs = (float)s * (1.0f / 32768.0f);
  float absSample = fabsf(fs);
  s_lpState = s_lpState * 0.82f + fs * 0.18f; // ~500 Hz crossover lowpass (DC gain = 1)
  float highDiff = fabsf(fs - s_lpState);     // >500 Hz highpass complement

  s_blkAbsSum += absSample;
  if (absSample > s_blkPeak)
    s_blkPeak = absSample;
  s_blkLowSum += fabsf(s_lpState);
  s_blkHighSum += highDiff;

  if (++s_blkSamples >= 256) {
    float meanAmp = s_blkAbsSum * (1.0f / 256.0f);
    // Blend peak & mean so syllables punch open cleanly without clipping out pauses
    float blkEnv = 0.55f * s_blkPeak + 0.45f * meanAmp;

    float lowMean = s_blkLowSum * (1.0f / 256.0f);
    float highMean = s_blkHighSum * (1.0f / 256.0f);
    float denom = lowMean + highMean + 0.002f;
    float rawBright = (highMean / denom - 0.34f) * 2.0f;
    if (rawBright < 0.0f) rawBright = 0.0f;
    if (rawBright > 1.0f) rawBright = 1.0f;

    if (blkEnv > s_envAmp) {
      s_envAmp = s_envAmp * 0.30f + blkEnv * 0.70f;
    } else {
      s_envAmp = s_envAmp * 0.65f + blkEnv * 0.35f;
    }
    s_envBright = s_envBright * 0.60f + rawBright * 0.40f;

    s_lipRing[s_lipWrite].amp = s_envAmp;
    s_lipRing[s_lipWrite].bright = s_envBright;
    s_lipWrite = (s_lipWrite + 1) & 15;
    if (s_lipCount < 16)
      s_lipCount++;

    int delaySlots = psramFound() ? 6 : 4;
    if (delaySlots >= s_lipCount)
      delaySlots = s_lipCount - 1;
    int readIdx = (s_lipWrite - 1 - delaySlots + 16) & 15;
    s_speechAmplitude = s_lipRing[readIdx].amp;
    s_speechBrightness = s_lipRing[readIdx].bright;

    s_blkSamples = 0;
    s_blkAbsSum = 0.0f;
    s_blkPeak = 0.0f;
    s_blkLowSum = 0.0f;
    s_blkHighSum = 0.0f;
  }

  float gain = volumeGain();
  if (s_speechFadeInIdx < SPEECH_FADE_IN_SAMPLES) {
    float t = (float)s_speechFadeInIdx / (float)SPEECH_FADE_IN_SAMPLES;
    gain *= (t * t * (3.0f - 2.0f * t));
    s_speechFadeInIdx++;
  }
  float outSample = (float)s * gain;
  s_lastSpeechOut = outSample;
  int32_t v = toFrame32(outSample);
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
  uint32_t fileSize = (dataSize == 0xFFFFFFFF) ? 0xFFFFFFFF : (dataSize + 36);
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