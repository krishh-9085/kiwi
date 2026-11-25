#pragma once
#include <Arduino.h>

// ============================================================
// STRUCTS
// ============================================================
struct SttCfg {
  String url;
  String key;
  String model;
  bool sarvam;
};

enum ToneShape : uint8_t {
  SHAPE_SINE = 0,       // Pure sine wave (clean tone)
  SHAPE_WARM_VOCAL = 1, // Warm organic harmonics (voice / vowel formant style)
  SHAPE_PURR = 2,       // Low purring motor with warm tremolo & flutter
  SHAPE_CHIRP = 3,      // Bright expressive robot chirp (harmonic overtones)
  SHAPE_WARBLE = 4,     // Wall-E / R2-D2 expressive FM warble & robotic vocal
  SHAPE_BELL = 5        // Acoustic bell / marimba / chime with natural percussive decay
};

struct Tone {
  float freq = 0.0f; // Start frequency in Hz (0 = rest/pause)
  int ms = 0;        // Duration in milliseconds
  float endFreq =
      0.0f; // Target frequency (0 = keep constant; > 0 = smooth pitch glide)
  uint8_t shape = 0; // ToneShape timbre (SHAPE_SINE, SHAPE_WARM_VOCAL, etc.)
  float vibDepth = 0.0f; // Vibrato / FM modulation depth in Hz
  float vibRate =
      0.0f; // Vibrato / FM modulation rate in Hz (e.g. 15.0f - 35.0f)
};

struct FaceState {
  float w = 34, h = 32, gap = 30, y = 29, r = 6;
  float lidIn = 0, lidOut = 0;
  float cheek = 0, cheekR = 0;
  float hR = 1.0f;
  float wR = 1.0f;
  float glint = 0;
  float blush = 0;
  bool gazeLock = false;
  float lockX = 0, lockY = 0;
  float mouthW = 20;
  float mouthH = 2;
  float mouthCurve = 0.4f;
  float mouthY = 0;
  float pupilScale = 1.0f;
};

struct TouchSensor {
  uint8_t pin;
  bool state;
  bool lastRaw;
  uint32_t changedAt;
  uint32_t startedAt;
  bool holdHandled;
  bool armed;
  bool sleepHandled;
  uint32_t lastReleaseAt;
  bool pendingSingleTap;
};

// ============================================================
// ENUMS
// ============================================================
enum TouchAction {
  TOUCH_ACT_LISTEN = 0,
  TOUCH_ACT_CLOCK,
  TOUCH_ACT_WEATHER,
  TOUCH_ACT_CYCLE_SKIN,
  TOUCH_ACT_PET,
  TOUCH_ACT_TICKLE,
  TOUCH_ACT_VOL_UP,
  TOUCH_ACT_VOL_DOWN,
  TOUCH_ACT_DISMISS,
  TOUCH_ACT_NONE
};

enum Emotion {
  EMOTION_IDLE,
  EMOTION_LISTENING,
  EMOTION_THINKING,
  EMOTION_SPEAKING,
  EMOTION_HAPPY,
  EMOTION_EXCITED,
  EMOTION_SAD,
  EMOTION_CONFUSED,
  EMOTION_ANGRY,
  EMOTION_SLEEPING,
  EMOTION_ERROR,
  EMOTION_PETTED,
  EMOTION_SURPRISED,
  EMOTION_WINK,
  EMOTION_SMUG,
  EMOTION_CURIOUS,
  EMOTION_BORED,
  EMOTION_LAUGHING,
  EMOTION_SCARED,
  EMOTION_SUSPICIOUS,
  EMOTION_RELIEVED,
  EMOTION_PROUD,
  EMOTION_DIZZY,
  EMOTION_LOVESTRUCK,
  // New expressive emotions
  EMOTION_EVIL,
  EMOTION_CRYING,
  EMOTION_HEARTS,
  EMOTION_DEAD,
  EMOTION_COOL,
  EMOTION_SHOCKED,
  EMOTION_SINGING,
  EMOTION_PARTY,
  EMOTION_NERVOUS,
  EMOTION_CUTE
};

enum PetMood {
  PET_LOVE,
  PET_GIGGLE,
  PET_SHY,
  PET_SPARKLE,
  PET_KISS,
  PET_CUDDLE,
  PET_PLAYFUL,
  PET_SLEEPY,
  PET_PROUD,
  PET_SURPRISED
};
#define PET_MOOD_COUNT 10

enum Flourish { FL_NONE, FL_HEART, FL_SPARKLE, FL_NOTE, FL_MOON, FL_STAR };

enum EyeSkin : uint8_t {
  SKIN_KIWI = 0,          // Kiwi (Flagship Cute Desk Buddy) - Default #1
  SKIN_PIXEL = 1,         // Pixel 8-Bit (#2, arcade retro)
  SKIN_LUMINA_SPARK = 2,  // Lumina Spark (#3, dual glint)
  SKIN_MECHA = 3,         // Mecha (#4, Cirrectangle Solid with floating rounded pupil & glint)
  SKIN_EMO = 4            // EMO-style (#5, Large rounded-rectangular eyes, no pupils, expressive procedural eyelids)
};
#define SKIN_COUNT 5

// Compatibility aliases
#define SKIN_DEFAULT SKIN_KIWI
#define SKIN_VECTOR SKIN_KIWI
#define SKIN_SQUIRCLE_BOLD SKIN_KIWI
#define SKIN_RETRO8BIT SKIN_PIXEL
#define SKIN_SQUIRCLE_SOLID SKIN_KIWI
#define SKIN_CAPSULE SKIN_KIWI
#define SKIN_NEON_FRAME SKIN_KIWI
#define SKIN_SQUIRCLE_OUTLINE SKIN_KIWI
#define SKIN_SQUIRCLE_GLINT SKIN_LUMINA_SPARK
#define SKIN_EMO_STYLE SKIN_EMO
