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
static SpkOwner spkOwner = SPK_NONE;

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
static bool tonePlaying = false;
static float tonePhase = 0.0f;
static float vibPhase = 0.0f;
static float tremPhase = 0.0f;

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

void updateTonePlayer() {
  if (!tonePlaying)
    return;

  // Auto-recover if speaker owner changed or hardware was detached
  if (!txHandle || spkOwner != SPK_TONE) {
    tonePlaying = false;
    toneCount = 0;
    toneIndex = 0;
    return;
  }

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

  static int32_t buf[TONE_BLOCK_FRAMES * 2];

  for (int i = 0; i < TONE_BLOCK_FRAMES; i++) {
    // Silence / rest pause if freq <= 1.0f
    if (cur.freq <= 1.0f) {
      buf[i * 2] = 0;
      buf[i * 2 + 1] = 0;
      continue;
    }

    // Per-sample time keeps the envelope and pitch smooth within the block.
    float t = elapsed + (float)i / (float)SAMPLE_RATE;

    // Smooth Hann-tapered envelope eliminates sharp digital click transients
    float env = 1.0f;
    if (cur.shape == SHAPE_BELL) {
      // Natural acoustic bell/chime percussive envelope:
      // Rapid 4ms anti-click attack, followed by natural exponential decay
      const float bellAttack = 0.004f;
      if (t < bellAttack) {
        env = 0.5f * (1.0f - cosf((float)M_PI * (t / bellAttack)));
      } else {
        float decayProgress = (duration > bellAttack) ? ((t - bellAttack) / (duration - bellAttack)) : 1.0f;
        if (decayProgress < 0.0f) decayProgress = 0.0f;
        if (decayProgress > 1.0f) decayProgress = 1.0f;
        float d = 1.0f - decayProgress;
        env = d * d * (0.85f * d + 0.15f);
      }
    } else {
      if (attack > 0.001f && t < attack) {
        env = 0.5f * (1.0f - cosf((float)M_PI * (t / attack)));
      } else if (release > 0.001f && t > duration - release) {
        float rProg = (t - (duration - release)) / release;
        if (rProg > 1.0f) rProg = 1.0f;
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
        // Organic vocal formant overtones (Wall-E warm coo / vowel timbre)
        raw = sinf(tonePhase) * 0.70f +
              sinf(2.0f * tonePhase) * 0.22f +
              sinf(3.0f * tonePhase) * 0.08f;
        break;
      }
      case SHAPE_PURR: {
        // Soft kitten robot motor: low rumble with amplitude tremolo
        tremPhase += 2.0f * (float)M_PI * 25.0f / (float)SAMPLE_RATE;
        if (tremPhase >= 2.0f * (float)M_PI)
          tremPhase -= 2.0f * (float)M_PI;
        float tremolo = 0.55f + 0.45f * sinf(tremPhase);
        raw = (sinf(tonePhase) * 0.75f + sinf(2.0f * tonePhase) * 0.25f) * tremolo;
        break;
      }
      case SHAPE_CHIRP: {
        // Bright expressive robot chirp (bright 2nd harmonic)
        raw = sinf(tonePhase) * 0.82f + sinf(2.0f * tonePhase) * 0.18f;
        break;
      }
      case SHAPE_WARBLE: {
        // Dynamic FM robotic warble (R2-D2 / Wall-E chatter)
        float fmMod = sinf(tonePhase * 1.5f) * 1.6f;
        raw = sinf(tonePhase + fmMod) * 0.85f;
        break;
      }
      case SHAPE_BELL: {
        // Acoustic glockenspiel / chime / marimba timbre:
        // Fundamental (pure warm core) + inharmonic bell overtone (2.756x) + 2nd harmonic (2x)
        raw = sinf(tonePhase) * 0.74f +
              sinf(2.0f * tonePhase) * 0.16f +
              sinf(2.756f * tonePhase) * 0.10f;
        break;
      }
      default: // SHAPE_SINE
        raw = sinf(tonePhase);
        break;
    }

    float s = raw * amp * env;
    int32_t v = toFrame32(s);
    buf[i * 2] = v;
    buf[i * 2 + 1] = v;
  }

  spkWriteAll(buf, sizeof(buf));
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

static portMUX_TYPE s_micMux = portMUX_INITIALIZER_UNLOCKED;

static void saveMicSlot(int slot) {
  Preferences p;
  p.begin("myrobot", false);
  p.putInt("micslot", slot);
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
  return micSlot;
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
    spkOwner = SPK_SPEECH;
  }
}

void toneStop() {
  tonePlaying = false;
  toneCount = 0;
  toneIndex = 0;
  if (spkOwner == SPK_TONE) {
    spkOwner = SPK_NONE;
  }
}

// Live speech envelope follower for real-time lip sync & speaking gestures
static volatile float s_speechAmplitude = 0.0f;

float getSpeechAmplitude() {
  return s_speechAmplitude;
}

// Call when a speech stream ends. Flushes remaining samples and releases
// the speaker so tones can play again.
void spkEndSpeech() {
  spkFlush();
  s_speechAmplitude = 0.0f;
  if (spkOwner == SPK_SPEECH) {
    spkOwner = SPK_NONE;
  }
}

// Instantly cuts off speech without flushing remaining buffer (for fast cancel)
void spkAbortSpeech() {
  txPos = 0;
  tonePlaying = false;
  toneCount = 0;
  toneIndex = 0;
  s_speechAmplitude = 0.0f;
  spkOwner = SPK_NONE;
}

// Prepares audio hardware for light or deep sleep: stops speech/tones,
// clears queues and disables I2S TX and RX DMA channels.
void audioSleepPrepare() {
  spkEndSpeech();
  tonePlaying = false;
  toneCount = 0;
  toneIndex = 0;
  s_speechAmplitude = 0.0f;
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

  // Fast-attack, smooth-decay envelope tracking for live lip sync
  float absSample = fabsf((float)s) / 32768.0f;
  if (absSample > s_speechAmplitude) {
    s_speechAmplitude = s_speechAmplitude * 0.70f + absSample * 0.30f;
  } else {
    s_speechAmplitude *= 0.9994f; // ~45ms leaky decay
  }

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