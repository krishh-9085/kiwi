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
  SHAPE_WARBLE = 4      // Wall-E / R2-D2 expressive FM warble & robotic vocal
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
  float glint = 0;
  float blush = 0;
  bool gazeLock = false;
  float lockX = 0, lockY = 0;
  float mouthW = 20;
  float mouthH = 2;
  float mouthCurve = 0.4f;
  float mouthY = 0;
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
  SKIN_VECTOR = 1,        // Vector Classic (#2)
  SKIN_CAPSULE = 2,       // Neon Capsule (#3, new pill eye with glint)
  SKIN_PIXEL = 3,         // Pixel 8-Bit (#4, arcade retro)
  SKIN_SQUIRCLE_BOLD = 4, // Squircle Bold (#5, modern solid)
  SKIN_NEON_FRAME = 5,    // Neon Frame (#6, outline frame)
  SKIN_LUMINA_SPARK = 6   // Lumina Spark (#7, dual glint)
};
#define SKIN_COUNT 7

// Compatibility aliases
#define SKIN_DEFAULT SKIN_KIWI
#define SKIN_RETRO8BIT SKIN_PIXEL
#define SKIN_SQUIRCLE_SOLID SKIN_SQUIRCLE_BOLD
#define SKIN_SQUIRCLE_OUTLINE SKIN_NEON_FRAME
#define SKIN_SQUIRCLE_GLINT SKIN_LUMINA_SPARK

enum IdleEye : uint8_t {
  IE_NORMAL,
  IE_WIDE,
  IE_HALF,
  IE_SLEEPY,
  IE_WINKL,
  IE_WINKR,
  IE_ANGRY,
  IE_SAD,
  IE_CURIOUS,
  IE_STARRY,
  IE_LOOK_UP,
  IE_LOOK_DOWN,
  IE_SQUINT,
  IE_GLINT,
  IE_SURPRISED,
  IE_UNEVEN,
  // New Idle Eyes
  IE_HEARTS,
  IE_PUPPY,
  IE_HAPPY_ARCH,
  IE_SUSPICIOUS,
  IE_COOL_HALF,
  IE_SHOCKED_TINY
};
#define IE_COUNT 22

enum IdleMouth : uint8_t {
  IM_FLAT,
  IM_SMILE,
  IM_BIGSMILE,
  IM_FROWN,
  IM_OPEN_O,
  IM_OPEN_WIDE,
  IM_POUT,
  IM_SMIRK,
  IM_TONGUE,
  IM_YAWN,
  IM_WAVY,
  IM_TINY,
  // New Idle Mouths
  IM_KITTY_3,
  IM_WHISTLE,
  IM_ZIGZAG,
  IM_CHUCKLE,
  IM_PUPPY_POUT,
  IM_DEVIL_SMIRK
};
#define IM_COUNT 18
