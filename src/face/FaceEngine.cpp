#include "face/FaceEngine.h"
#include "hardware/AudioEngine.h"
#include "config/AppConfig.h"
#include "animation/AnimationEngine.h"
#include <Wire.h>
#include <WiFi.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
bool oledOK = false;

FaceState faceNow, faceTgt;
static uint32_t emotionStartedAt = 0;
static uint32_t nextRandomEmotionAt = 0;
static int eyeOpenPct = 100;
static bool eyeBlinking = false;
static uint32_t eyeBlinkT0 = 0;
static uint32_t eyeNextBlink = 0;
static uint32_t eyeNextGaze = 0;
static uint32_t petMoodT0 = 0;
static uint32_t flourishT0 = 0;
static uint32_t nextIdleLookAt = 0;
static uint32_t idleExpressionEnd = 0;
static int idleExpression = 0;

extern volatile int8_t pendingSound;
void pumpUI();

bool initDisplay() {
  Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
  Wire.setClock(400000);
  oledOK = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  return oledOK;
}

void setOledDim(bool dim) {
  if (oledOK)
    display.dim(dim);
}

static void setFace(float w, float h, float gap, float y, float r,
                    float lidIn = 0, float lidOut = 0, float cheek = 0,
                    float blush = 0) {
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

void setEmotion(Emotion e) {
  currentEmotion = e;
  emotionStartedAt = millis();
  faceTargetsForEmotion(e, petMood);
  if (e == EMOTION_IDLE) {
    nextRandomEmotionAt = millis() + 8000 + random(0, 5000);
    emotionAutoReturnAt = 0;
    nextIdleLookAt = millis() + 300 + random(0, 500);
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
    setFace(34, 4, 30, 34, 2);
    faceTgt.gazeLock = true;
    faceTgt.mouthW = 10;
    faceTgt.mouthH = 1;
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
    }
    break;
  }
}

static void drawEye(int cx, int cy, int w, int h, int r, int lidIn, int lidOut,
                    int cheek, bool isLeft, bool glint) {
  if (h < 2)
    h = 2;
  if (w < 4)
    w = 4;
  if (r * 2 > h)
    r = h / 2;
  int x0 = cx - w / 2;
  int x1 = x0 + w;
  int top = cy - h / 2;
  int bot = top + h;

  // 0. DESK BUDDY / KIWI EYES: flat upper edge, rounded bottom base,
  // U-shaped deep pupil cutout with floating top-right catchlight glint dot.
  if (currentEyeSkin == SKIN_KIWI) {
    if (h <= 4) {
      display.fillRoundRect(x0, cy - 1, w, max(2, h), 1, SSD1306_WHITE);
      return;
    }

    int rBottom = (r > 2) ? r : min(7, h / 3);
    if (rBottom * 2 > h) rBottom = h / 2;

    // Outer sclera: flat top edge, rounded lower edge
    display.fillRect(x0, top, w, h - rBottom, SSD1306_WHITE);
    display.fillRoundRect(x0, bot - rBottom * 2, w, rBottom * 2, rBottom, SSD1306_WHITE);

    // Dynamic pupil cutout (proportional to eye size and reacting to gaze)
    int pw = (int)(w * 0.44f);
    if (pw < 6) pw = 6;
    int ph = (int)(h * 0.58f);
    if (ph < 4) ph = 4;
    int pr = min(4, max(2, pw / 4));

    int pupilShift = (int)(gazeX * 0.45f);
    int px = cx + pupilShift - pw / 2;
    if (px < x0 + 3) px = x0 + 3;
    if (px + pw > x1 - 3) px = x1 - 3 - pw;

    int py = top;
    if (gazeY > 1.5f && h > 16) {
      py += min(4, (int)(gazeY * 0.4f));
    }

    display.fillRect(px, py, pw, ph - pr, SSD1306_BLACK);
    display.fillRoundRect(px, py + ph - pr * 2, pw, pr * 2, pr, SSD1306_BLACK);

    // Specular catchlight / glint dot in upper-right corner of pupil
    int dotSize = (pw >= 12 && ph >= 12) ? 3 : 2;
    int dotX = px + pw - dotSize - 2;
    int dotY = py + 2;
    if (h > 12) {
      display.fillRect(dotX, dotY, dotSize, dotSize, SSD1306_WHITE);
      if (glint && dotSize >= 3) {
        display.drawPixel(dotX - 1, dotY + dotSize, SSD1306_WHITE);
      }
    }

    // Dynamic eyelid masking
    if (lidIn > 0 || lidOut > 0) {
      int lL = isLeft ? lidOut : lidIn;
      int lR = isLeft ? lidIn : lidOut;
      display.fillTriangle(x0 - 1, top - 1, x1 + 1, top - 1, x1 + 1, top + lR, SSD1306_BLACK);
      display.fillTriangle(x0 - 1, top - 1, x1 + 1, top + lR, x0 - 1, top + lL, SSD1306_BLACK);
    }
    // Dynamic cheek masking
    if (cheek > 1) {
      int R = w * 7 / 10;
      display.fillCircle(cx, bot - cheek + R, R, SSD1306_BLACK);
    }
    return;
  }

  // 1. RETRO 8-BIT ARCADE / PIXEL ART SKIN
  if (currentEyeSkin == SKIN_RETRO8BIT) {
    int step = (w >= 22) ? 4 : 2;
    if (h > step * 2) {
      display.fillRect(x0 + step, top, w - step * 2, h, SSD1306_WHITE);
      display.fillRect(x0, top + step, w, h - step * 2, SSD1306_WHITE);
    } else {
      display.fillRect(x0, top, w, h, SSD1306_WHITE);
    }

    if (lidIn > 0 || lidOut > 0) {
      int lL = isLeft ? lidOut : lidIn;
      int lR = isLeft ? lidIn : lidOut;
      display.fillTriangle(x0 - 1, top - 1, x1 + 1, top - 1, x1 + 1, top + lR,
                           SSD1306_BLACK);
      display.fillTriangle(x0 - 1, top - 1, x1 + 1, top + lR, x0 - 1, top + lL,
                           SSD1306_BLACK);
    }
    if (cheek > 1) {
      int R = w * 7 / 10;
      display.fillCircle(cx, bot - cheek + R, R, SSD1306_BLACK);
    }
    if (glint && h > 16) {
      display.fillRect(x0 + 4, top + 4, 3, 3, SSD1306_BLACK);
      display.fillRect(x0 + 8, top + 8, 2, 2, SSD1306_BLACK);
    }
    return;
  }

  // 2. CYBERPUNK SCI-FI VISOR / SCANLINES SKIN
  if (currentEyeSkin == SKIN_CYBERPUNK) {
    int chamfer = (r > 3) ? r : 3;
    display.fillRoundRect(x0, top, w, h, chamfer, SSD1306_WHITE);

    // Horizontal cyber scanlines for holographic HUD texture
    if (h > 6) {
      for (int y = top + 2; y < bot - 1; y += 3) {
        display.drawFastHLine(x0, y, w, SSD1306_BLACK);
      }
    }

    if (lidIn > 0 || lidOut > 0) {
      int lL = isLeft ? lidOut : lidIn;
      int lR = isLeft ? lidIn : lidOut;
      display.fillTriangle(x0 - 1, top - 1, x1 + 1, top - 1, x1 + 1, top + lR,
                           SSD1306_BLACK);
      display.fillTriangle(x0 - 1, top - 1, x1 + 1, top + lR, x0 - 1, top + lL,
                           SSD1306_BLACK);
    }
    if (cheek > 1) {
      int R = w * 7 / 10;
      display.fillCircle(cx, bot - cheek + R, R, SSD1306_BLACK);
    }

    // Horizontal cyber focus reticle
    if (h > 12) {
      display.fillRect(cx - 4, cy - 1, 8, 2, SSD1306_BLACK);
    }
    return;
  }

  // 3. KAWAII CAT NEKO EYES SKIN
  if (currentEyeSkin == SKIN_CAT) {
    display.fillRoundRect(x0, top, w, h, r, SSD1306_WHITE);

    if (lidIn > 0 || lidOut > 0) {
      int lL = isLeft ? lidOut : lidIn;
      int lR = isLeft ? lidIn : lidOut;
      display.fillTriangle(x0 - 1, top - 1, x1 + 1, top - 1, x1 + 1, top + lR,
                           SSD1306_BLACK);
      display.fillTriangle(x0 - 1, top - 1, x1 + 1, top + lR, x0 - 1, top + lL,
                           SSD1306_BLACK);
    }
    if (cheek > 1) {
      int R = w * 7 / 10;
      display.fillCircle(cx, bot - cheek + R, R, SSD1306_BLACK);
    }

    // Vertical cat slit pupil in black!
    if (h > 10) {
      int slitW = (h > 22) ? 4 : 3;
      int slitH = h * 6 / 10;
      display.fillRoundRect(cx - slitW / 2, cy - slitH / 2, slitW, slitH, 1,
                            SSD1306_BLACK);
    }

    // Anime sparkle double glint
    if (h > 18) {
      display.fillCircle(x0 + w / 4, top + h / 4, 2, SSD1306_BLACK);
      display.drawPixel(x0 + w / 4 + 3, top + h / 4 + 4, SSD1306_BLACK);
    }

    // Cat eye outer wing flick
    int flickX = isLeft ? (x0 - 3) : (x1 + 3);
    display.drawLine(isLeft ? x0 : x1, top + 2, flickX, top - 2, SSD1306_WHITE);
    return;
  }

  // 4. ANIME SHOUNEN / STARRY GLINT SKIN
  if (currentEyeSkin == SKIN_ANIME) {
    display.fillRoundRect(x0, top, w, h, r, SSD1306_WHITE);
    if (lidIn > 0 || lidOut > 0) {
      int lL = isLeft ? lidOut : lidIn;
      int lR = isLeft ? lidIn : lidOut;
      display.fillTriangle(x0 - 1, top - 1, x1 + 1, top - 1, x1 + 1, top + lR,
                           SSD1306_BLACK);
      display.fillTriangle(x0 - 1, top - 1, x1 + 1, top + lR, x0 - 1, top + lL,
                           SSD1306_BLACK);
    }
    if (cheek > 1) {
      int R = w * 7 / 10;
      display.fillCircle(cx, bot - cheek + R, R, SSD1306_BLACK);
    }
    // Starburst anime glint
    if (h > 14) {
      int sx = x0 + w / 3;
      int sy = top + h / 3;
      display.fillCircle(sx, sy, 2, SSD1306_BLACK);
      display.drawFastHLine(sx - 3, sy, 7, SSD1306_BLACK);
      display.drawFastVLine(sx, sy - 3, 7, SSD1306_BLACK);
      display.drawPixel(sx + 4, sy + 4, SSD1306_BLACK);
    }
    if (isLeft) {
      display.drawPixel(x0 + 3, bot, SSD1306_WHITE);
      display.drawPixel(x0 + 6, bot + 1, SSD1306_WHITE);
    } else {
      display.drawPixel(x1 - 3, bot, SSD1306_WHITE);
      display.drawPixel(x1 - 6, bot + 1, SSD1306_WHITE);
    }
    return;
  }

  // 5. LOVESTRUCK HEART EYES SKIN
  if (currentEyeSkin == SKIN_HEARTS) {
    display.fillRoundRect(x0, top, w, h, r, SSD1306_WHITE);
    if (lidIn > 0 || lidOut > 0) {
      int lL = isLeft ? lidOut : lidIn;
      int lR = isLeft ? lidIn : lidOut;
      display.fillTriangle(x0 - 1, top - 1, x1 + 1, top - 1, x1 + 1, top + lR,
                           SSD1306_BLACK);
      display.fillTriangle(x0 - 1, top - 1, x1 + 1, top + lR, x0 - 1, top + lL,
                           SSD1306_BLACK);
    }
    if (cheek > 1) {
      int R = w * 7 / 10;
      display.fillCircle(cx, bot - cheek + R, R, SSD1306_BLACK);
    }
    // Heart cutout pupil
    if (h > 14) {
      int hr = (h > 24) ? 3 : 2;
      display.fillCircle(cx - hr, cy - hr, hr, SSD1306_BLACK);
      display.fillCircle(cx + hr, cy - hr, hr, SSD1306_BLACK);
      display.fillTriangle(cx - hr * 2, cy - hr, cx + hr * 2, cy - hr, cx,
                           cy + hr * 2, SSD1306_BLACK);
    }
    return;
  }

  // 6. STEAMPUNK AVIATOR GOGGLES SKIN
  if (currentEyeSkin == SKIN_STEAMPUNK) {
    display.fillRoundRect(x0, top, w, h, r, SSD1306_WHITE);
    if (lidIn > 0 || lidOut > 0) {
      int lL = isLeft ? lidOut : lidIn;
      int lR = isLeft ? lidIn : lidOut;
      display.fillTriangle(x0 - 1, top - 1, x1 + 1, top - 1, x1 + 1, top + lR,
                           SSD1306_BLACK);
      display.fillTriangle(x0 - 1, top - 1, x1 + 1, top + lR, x0 - 1, top + lL,
                           SSD1306_BLACK);
    }
    if (cheek > 1) {
      int R = w * 7 / 10;
      display.fillCircle(cx, bot - cheek + R, R, SSD1306_BLACK);
    }
    int rim = (w > h ? w : h) / 2 + 2;
    display.drawCircle(cx, cy, rim, SSD1306_WHITE);
    display.drawPixel(cx - rim, cy, SSD1306_BLACK);
    display.drawPixel(cx + rim, cy, SSD1306_BLACK);
    display.drawPixel(cx, cy - rim, SSD1306_BLACK);
    display.drawPixel(cx, cy + rim, SSD1306_BLACK);
    if (glint && h > 20) {
      display.fillRect(x0 + w / 5, top + h / 5, 3, 3, SSD1306_BLACK);
    }
    return;
  }

  // 7. DEMON SPOOKY HORNS & FIERCE SLIT SKIN
  if (currentEyeSkin == SKIN_DEMON) {
    display.fillRoundRect(x0, top, w, h, 2, SSD1306_WHITE);
    if (isLeft) {
      display.fillTriangle(x0, top, x1, top, x0, top + 6, SSD1306_BLACK);
    } else {
      display.fillTriangle(x0, top, x1, top, x1, top + 6, SSD1306_BLACK);
    }
    if (lidIn > 0 || lidOut > 0) {
      int lL = isLeft ? lidOut : lidIn;
      int lR = isLeft ? lidIn : lidOut;
      display.fillTriangle(x0 - 1, top - 1, x1 + 1, top - 1, x1 + 1, top + lR,
                           SSD1306_BLACK);
      display.fillTriangle(x0 - 1, top - 1, x1 + 1, top + lR, x0 - 1, top + lL,
                           SSD1306_BLACK);
    }
    if (cheek > 1) {
      int R = w * 7 / 10;
      display.fillCircle(cx, bot - cheek + R, R, SSD1306_BLACK);
    }
    if (h > 12) {
      display.fillRect(cx - 1, cy - h / 3, 3, h * 2 / 3, SSD1306_BLACK);
    }
    return;
  }

  // 8. MATRIX CODE STREAM / TERMINAL SKIN
  if (currentEyeSkin == SKIN_MATRIX) {
    display.fillRoundRect(x0, top, w, h, (r > 3) ? 3 : r, SSD1306_WHITE);
    if (h > 8) {
      uint32_t t = millis() / 90;
      for (int x = x0 + 2; x < x1 - 1; x += 3) {
        int seed = (x * 7 + (int)isLeft * 13) % 8;
        for (int y = top + 1; y < bot - 1; y += 3) {
          if (((y + seed + t) % 4) != 0) {
            display.drawPixel(x, y, SSD1306_BLACK);
          }
        }
      }
    }
    if (lidIn > 0 || lidOut > 0) {
      int lL = isLeft ? lidOut : lidIn;
      int lR = isLeft ? lidIn : lidOut;
      display.fillTriangle(x0 - 1, top - 1, x1 + 1, top - 1, x1 + 1, top + lR, SSD1306_BLACK);
      display.fillTriangle(x0 - 1, top - 1, x1 + 1, top + lR, x0 - 1, top + lL, SSD1306_BLACK);
    }
    if (cheek > 1) {
      int R = w * 7 / 10;
      display.fillCircle(cx, bot - cheek + R, R, SSD1306_BLACK);
    }
    if (h > 12 && ((millis() / 400) % 2 == 0)) {
      display.fillRect(cx - 2, cy + 1, 4, 2, SSD1306_BLACK);
    }
    return;
  }

  // 9. CYBORG T-800 TARGETING APERTURE SKIN
  if (currentEyeSkin == SKIN_CYBORG) {
    display.fillRoundRect(x0, top, w, h, r, SSD1306_WHITE);
    if (lidIn > 0 || lidOut > 0) {
      int lL = isLeft ? lidOut : lidIn;
      int lR = isLeft ? lidIn : lidOut;
      display.fillTriangle(x0 - 1, top - 1, x1 + 1, top - 1, x1 + 1, top + lR, SSD1306_BLACK);
      display.fillTriangle(x0 - 1, top - 1, x1 + 1, top + lR, x0 - 1, top + lL, SSD1306_BLACK);
    }
    if (cheek > 1) {
      int R = w * 7 / 10;
      display.fillCircle(cx, bot - cheek + R, R, SSD1306_BLACK);
    }
    if (isLeft && h > 14) {
      int rad = (w < h ? w : h) / 3;
      display.drawCircle(cx, cy, rad, SSD1306_BLACK);
      display.drawFastHLine(cx - rad - 3, cy, (rad + 3) * 2 + 1, SSD1306_BLACK);
      display.drawFastVLine(cx, cy - rad - 3, (rad + 3) * 2 + 1, SSD1306_BLACK);
      display.fillCircle(cx, cy, 2, SSD1306_BLACK);
      display.drawPixel(cx, cy, SSD1306_WHITE);
    } else if (!isLeft && h > 14) {
      display.drawFastHLine(cx - 5, cy - 4, 10, SSD1306_BLACK);
      display.drawFastHLine(cx - 5, cy + 4, 10, SSD1306_BLACK);
      display.drawFastVLine(cx - 5, cy - 4, 9, SSD1306_BLACK);
      display.fillCircle(cx + 1, cy, 2, SSD1306_BLACK);
    }
    return;
  }

  // 10. GUNDAM MECHA POWER CORE SKIN
  if (currentEyeSkin == SKIN_MECHA) {
    display.fillRect(x0 + 2, top, w - 4, h, SSD1306_WHITE);
    display.fillRect(x0, top + 2, w, h - 4, SSD1306_WHITE);
    if (isLeft) {
      display.fillTriangle(x0, bot - 4, x0 + 4, bot, x0, bot, SSD1306_BLACK);
    } else {
      display.fillTriangle(x1, bot - 4, x1 - 4, bot, x1, bot, SSD1306_BLACK);
    }
    if (lidIn > 0 || lidOut > 0) {
      int lL = isLeft ? lidOut : lidIn;
      int lR = isLeft ? lidIn : lidOut;
      display.fillTriangle(x0 - 1, top - 1, x1 + 1, top - 1, x1 + 1, top + lR, SSD1306_BLACK);
      display.fillTriangle(x0 - 1, top - 1, x1 + 1, top + lR, x0 - 1, top + lL, SSD1306_BLACK);
    }
    if (cheek > 1) {
      int R = w * 7 / 10;
      display.fillCircle(cx, bot - cheek + R, R, SSD1306_BLACK);
    }
    if (h > 12) {
      int slitW = w * 6 / 10;
      display.fillRect(cx - slitW / 2, cy - 2, slitW, 4, SSD1306_BLACK);
      display.drawFastHLine(cx - slitW / 2 + 1, cy, slitW - 2, SSD1306_WHITE);
    }
    return;
  }

  // 11. BOSS SUNGLASSES / THUG LIFE SHADES
  if (currentEyeSkin == SKIN_SUNGLASSES) {
    display.fillRoundRect(x0, top, w, h, (r > 4) ? 4 : r, SSD1306_BLACK);
    display.drawRoundRect(x0, top, w, h, (r > 4) ? 4 : r, SSD1306_WHITE);
    display.drawRoundRect(x0 + 1, top + 1, w - 2, h - 2, (r > 3) ? 3 : r, SSD1306_WHITE);
    if (h > 10) {
      display.drawLine(x0 + 3, bot - 4, x1 - 4, top + 3, SSD1306_WHITE);
      display.drawLine(x0 + 4, bot - 4, x1 - 3, top + 3, SSD1306_WHITE);
      display.drawLine(x0 + 7, bot - 4, x1 - 4, top + 6, SSD1306_WHITE);
    }
    if (lidIn > 0 || lidOut > 0) {
      int lL = isLeft ? lidOut : lidIn;
      int lR = isLeft ? lidIn : lidOut;
      display.fillTriangle(x0 - 1, top - 1, x1 + 1, top - 1, x1 + 1, top + lR, SSD1306_BLACK);
      display.fillTriangle(x0 - 1, top - 1, x1 + 1, top + lR, x0 - 1, top + lL, SSD1306_BLACK);
    }
    return;
  }

  // 12. KAWAII PUPPY BOBA EYES SKIN
  if (currentEyeSkin == SKIN_PUPPY) {
    display.fillRoundRect(x0, top, w, h, r, SSD1306_WHITE);
    if (lidIn > 0 || lidOut > 0) {
      int lL = isLeft ? lidOut : lidIn;
      int lR = isLeft ? lidIn : lidOut;
      display.fillTriangle(x0 - 1, top - 1, x1 + 1, top - 1, x1 + 1, top + lR, SSD1306_BLACK);
      display.fillTriangle(x0 - 1, top - 1, x1 + 1, top + lR, x0 - 1, top + lL, SSD1306_BLACK);
    }
    if (cheek > 1) {
      int R = w * 7 / 10;
      display.fillCircle(cx, bot - cheek + R, R, SSD1306_BLACK);
    }
    if (h > 12) {
      int pupilR = (w < h ? w : h) * 4 / 10;
      display.fillCircle(cx, cy + 1, pupilR, SSD1306_BLACK);
      display.fillCircle(cx - pupilR / 2, cy - pupilR / 2, pupilR / 3 > 1 ? pupilR / 3 : 2, SSD1306_WHITE);
      display.fillCircle(cx + pupilR / 2, cy + pupilR / 3, 1, SSD1306_WHITE);
      display.drawPixel(cx, cy + pupilR - 1, SSD1306_WHITE);
    }
    return;
  }

  // 13. HYPNOTIC MESMERIZING SPIRAL VORTEX SKIN
  if (currentEyeSkin == SKIN_HYPNO) {
    display.fillRoundRect(x0, top, w, h, r, SSD1306_WHITE);
    if (lidIn > 0 || lidOut > 0) {
      int lL = isLeft ? lidOut : lidIn;
      int lR = isLeft ? lidIn : lidOut;
      display.fillTriangle(x0 - 1, top - 1, x1 + 1, top - 1, x1 + 1, top + lR, SSD1306_BLACK);
      display.fillTriangle(x0 - 1, top - 1, x1 + 1, top + lR, x0 - 1, top + lL, SSD1306_BLACK);
    }
    if (cheek > 1) {
      int R = w * 7 / 10;
      display.fillCircle(cx, bot - cheek + R, R, SSD1306_BLACK);
    }
    if (h > 12) {
      int maxR = (w < h ? w : h) / 2 - 2;
      float rot = (float)(millis() % 1600) / 1600.0f * 2.0f * PI;
      for (int rad = 3; rad <= maxR; rad += 3) {
        display.drawCircle(cx, cy, rad, SSD1306_BLACK);
      }
      int spx = (int)(cosf(rot) * maxR);
      int spy = (int)(sinf(rot) * maxR);
      display.drawLine(cx, cy, cx + spx, cy + spy, SSD1306_BLACK);
      display.drawLine(cx, cy, cx - spx, cy - spy, SSD1306_BLACK);
    }
    return;
  }

  // 14. COSMIC GALAXY / ALIEN SAUCER SKIN
  if (currentEyeSkin == SKIN_ALIEN) {
    display.fillRoundRect(x0, top, w, h, 2, SSD1306_WHITE);
    if (isLeft) {
      display.fillTriangle(x0, top, x0 + 6, top, x0, top + 6, SSD1306_BLACK);
      display.fillTriangle(x1, bot, x1 - 6, bot, x1, bot - 6, SSD1306_BLACK);
    } else {
      display.fillTriangle(x1, top, x1 - 6, top, x1, top + 6, SSD1306_BLACK);
      display.fillTriangle(x0, bot, x0 + 6, bot, x0, bot - 6, SSD1306_BLACK);
    }
    if (lidIn > 0 || lidOut > 0) {
      int lL = isLeft ? lidOut : lidIn;
      int lR = isLeft ? lidIn : lidOut;
      display.fillTriangle(x0 - 1, top - 1, x1 + 1, top - 1, x1 + 1, top + lR, SSD1306_BLACK);
      display.fillTriangle(x0 - 1, top - 1, x1 + 1, top + lR, x0 - 1, top + lL, SSD1306_BLACK);
    }
    if (cheek > 1) {
      int R = w * 7 / 10;
      display.fillCircle(cx, bot - cheek + R, R, SSD1306_BLACK);
    }
    if (h > 12) {
      int oR = (w < h ? w : h) / 3;
      display.fillCircle(cx, cy, 3, SSD1306_BLACK);
      display.drawCircle(cx, cy, oR, SSD1306_BLACK);
      float a = (float)(millis() % 2400) / 2400.0f * 2.0f * PI;
      int satX = cx + (int)(cosf(a) * oR);
      int satY = cy + (int)(sinf(a) * 3);
      display.fillCircle(satX, satY, 1, SSD1306_WHITE);
    }
    return;
  }

  // 15. SHINOBI NINJA STEALTH SKIN
  if (currentEyeSkin == SKIN_NINJA) {
    display.fillRoundRect(x0, top, w, h, 2, SSD1306_WHITE);
    if (isLeft) {
      display.fillTriangle(x0, top, x1, top, x1, top + 5, SSD1306_BLACK);
    } else {
      display.fillTriangle(x0, top, x1, top, x0, top + 5, SSD1306_BLACK);
    }
    if (lidIn > 0 || lidOut > 0) {
      int lL = isLeft ? lidOut : lidIn;
      int lR = isLeft ? lidIn : lidOut;
      display.fillTriangle(x0 - 1, top - 1, x1 + 1, top - 1, x1 + 1, top + lR, SSD1306_BLACK);
      display.fillTriangle(x0 - 1, top - 1, x1 + 1, top + lR, x0 - 1, top + lL, SSD1306_BLACK);
    }
    if (cheek > 1) {
      int R = w * 7 / 10;
      display.fillCircle(cx, bot - cheek + R, R, SSD1306_BLACK);
    }
    if (h > 12) {
      display.fillCircle(cx, cy, 3, SSD1306_BLACK);
      display.drawPixel(cx, cy, SSD1306_WHITE);
      display.drawFastHLine(cx - 5, cy, 3, SSD1306_BLACK);
      display.drawFastHLine(cx + 3, cy, 3, SSD1306_BLACK);
    }
    return;
  }

  // 16. ROUND CIRCLE EYES SKIN (High-Tech Ocular Lens)
  if (currentEyeSkin == SKIN_ROUND) {
    int rad = (w < h ? w : h) / 2;
    if (rad < 2) rad = 2;

    if (h <= 5) {
      // Squinting or full blink: sleek horizontal capsule
      display.fillRoundRect(x0, top, w, h, (h > 2 ? 2 : 1), SSD1306_WHITE);
    } else {
      // High-precision circular ocular eye
      display.fillCircle(cx, cy, rad, SSD1306_WHITE);

      // Deep intelligent pupil aperture (gives depth & robotic life)
      if (rad >= 7) {
        int pupilR = rad * 48 / 100;
        if (pupilR < 3) pupilR = 3;
        display.fillCircle(cx, cy, pupilR, SSD1306_BLACK);

        // Specular reflections / catchlights (dual white highlights)
        // Primary catchlight in upper-left
        int hl1R = (rad >= 12) ? 2 : 1;
        display.fillCircle(cx - pupilR / 2, cy - pupilR / 2, hl1R, SSD1306_WHITE);

        // Secondary subtle specular gleam in lower-right
        if (rad >= 10) {
          display.drawPixel(cx + pupilR / 2, cy + pupilR / 3, SSD1306_WHITE);
        }

        // Active glint sparkle (when excited / listening)
        if (glint && rad >= 10) {
          display.drawPixel(cx - pupilR / 2 + 2, cy - pupilR / 2, SSD1306_WHITE);
          display.drawPixel(cx - pupilR / 2, cy - pupilR / 2 + 2, SSD1306_WHITE);
        }
      }
    }

    // Dynamic eyelid masking (for sad, angry, sleepy, thinking)
    if (lidIn > 0 || lidOut > 0) {
      int lL = isLeft ? lidOut : lidIn;
      int lR = isLeft ? lidIn : lidOut;
      display.fillTriangle(x0 - 1, top - 1, x1 + 1, top - 1, x1 + 1, top + lR, SSD1306_BLACK);
      display.fillTriangle(x0 - 1, top - 1, x1 + 1, top + lR, x0 - 1, top + lL, SSD1306_BLACK);
    }
    // Dynamic cheek masking (for cheerful smiles & laughing)
    if (cheek > 1) {
      int R = w * 7 / 10;
      display.fillCircle(cx, bot - cheek + R, R, SSD1306_BLACK);
    }
    return;
  }

  // DEFAULT: Clean Round Vector Eyes
  display.fillRoundRect(x0, top, w, h, r, SSD1306_WHITE);
  if (lidIn > 0 || lidOut > 0) {
    int lL = isLeft ? lidOut : lidIn;
    int lR = isLeft ? lidIn : lidOut;
    display.fillTriangle(x0 - 1, top - 1, x1 + 1, top - 1, x1 + 1, top + lR,
                         SSD1306_BLACK);
    display.fillTriangle(x0 - 1, top - 1, x1 + 1, top + lR, x0 - 1, top + lL,
                         SSD1306_BLACK);
  }
  if (cheek > 1) {
    int R = w * 7 / 10;
    display.fillCircle(cx, bot - cheek + R, R, SSD1306_BLACK);
  }
  if (glint && h > 20) {
    display.fillRect(x0 + w / 5, top + h / 5, 4, 4, SSD1306_BLACK);
  }
}

static void drawBlush(int cx, int cy, float amt) {
  if (amt < 0.2f)
    return;
  int n = (amt > 0.7f) ? 3 : 2;
  for (int i = 0; i < n; i++)
    display.drawFastHLine(cx - 6 + (i & 1) * 3, cy + i * 3, 9, SSD1306_WHITE);
}

static void drawMouth(int cx, int cy, int w, int h, float curve) {
  if (w < 4)
    return;
  int halfW = w / 2;
  if (halfW < 2)
    halfW = 2;
  int thick = (h < 1) ? 1 : h;
  int amp = thick * 2;
  if (amp < 2)
    amp = 2;
  for (int x = -halfW; x <= halfW; x++) {
    float t = (float)x / (float)halfW;
    int y = cy + (int)(curve * amp * (1.0f - t * t));
    for (int k = 0; k < thick; k++)
      display.drawPixel(cx + x, y + k, SSD1306_WHITE);
  }
}

static const uint8_t PROGMEM heartBmp[] = {0x6C, 0xFE, 0xFE, 0x7C, 0x38, 0x10};

static void drawFlourish(uint32_t now) {
  if (flourish == FL_NONE)
    return;
  uint32_t dt = now - flourishT0;
  if (dt > 1300) {
    flourish = FL_NONE;
    return;
  }
  if (currentEmotion != EMOTION_PETTED)
    return;
  float p = dt / 1300.0f;
  if (p > 0.85f)
    return;
  int y = 50 - (int)(p * 24);
  switch (flourish) {
  case FL_HEART:
    display.drawBitmap(61, y, heartBmp, 7, 6, SSD1306_WHITE);
    break;
  case FL_SPARKLE:
    display.drawFastHLine(62, y + 3, 5, SSD1306_WHITE);
    display.drawFastVLine(64, y + 1, 5, SSD1306_WHITE);
    break;
  case FL_NOTE:
    display.fillCircle(62, y + 4, 2, SSD1306_WHITE);
    display.drawFastVLine(64, y, 5, SSD1306_WHITE);
    break;
  case FL_MOON:
    display.drawCircle(64, y + 3, 3, SSD1306_WHITE);
    display.fillCircle(65, y + 2, 2, SSD1306_BLACK);
    break;
  case FL_STAR:
    display.drawPixel(64, y, SSD1306_WHITE);
    display.drawPixel(64, y + 2, SSD1306_WHITE);
    display.drawPixel(64, y + 4, SSD1306_WHITE);
    display.drawPixel(62, y + 2, SSD1306_WHITE);
    display.drawPixel(66, y + 2, SSD1306_WHITE);
    break;
  default:
    break;
  }
}

void eyesInit(uint32_t now) {
  gazeX = gazeY = gazeTgtX = gazeTgtY = 0;
  eyeOpenPct = 100;
  eyeBlinking = false;
  eyeNextBlink = now + 1500 + random(0, 2500);
  eyeNextGaze = now + 1000 + random(0, 1500);
}

static uint32_t activeNextWink = 0;
static uint32_t activeWinkEnd = 0;
static bool activeWinkLeft = false;

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
  if (idleExpression != 0 && idleExpressionEnd > 0 &&
      now >= idleExpressionEnd) {
    resetIdleFace();
    idleExpression = 0;
    idleExpressionEnd = 0;
    nextIdleLookAt = now + 400 + random(0, 800);
  }

  if (now >= nextIdleLookAt) {
    int roll = random(0, 100);
    if (roll < 35) {
      int expr = random(1, 21);
      idleExpression = expr;
      uint32_t dur = 1200 + random(0, 1000);
      idleExpressionEnd = now + dur;
      applyClassicIdle(expr);
      nextIdleLookAt = idleExpressionEnd + 250 + random(0, 500);
    } else if (roll < 82) {
      resetIdleFace();
      IdleEye e = (IdleEye)random(0, IE_COUNT);
      IdleMouth m = (IdleMouth)random(0, IM_COUNT);
      applyIdleEye(e);
      applyIdleMouth(m);
      idleExpression = 100;
      idleExpressionEnd = now + 1000 + random(0, 1200);
      nextIdleLookAt = idleExpressionEnd + 250 + random(0, 500);
    } else {
      static const float dx[] = {-9, 9,   0,  0, -6, 6,  -6, 6,
                                 0,  -12, 12, 0, 0,  -4, 4};
      static const float dy[] = {0, 0, -7, 4,   -5, -5, 4, 4,
                                 0, 0, 0,  -10, 7,  -3, -3};
      int dir = random(0, 15);
      gazeTgtX = dx[dir];
      gazeTgtY = dy[dir];
      uint32_t holdMs = 800 + random(0, 1400);
      gazeLockUntil = now + holdMs;
      nextIdleLookAt = now + holdMs + 300 + random(0, 700);
    }
  }
}

void drawFace() {
  if (!oledOK)
    return;
  if (bootInfoUntil && millis() < bootInfoUntil)
    return;
  uint32_t now = millis();

  if (currentEmotion == EMOTION_SLEEPING) {
    display.clearDisplay();
    float breathY = sinf(now / 750.0f) * 2.0f;
    int sleepY = 34 + (int)breathY;
    int gapVal = 30;
    int cL = 64 - gapVal;
    int cR = 64 + gapVal;

    // Cute downward curved sleeping eyelids
    for (int dx = -14; dx <= 14; dx++) {
      float norm = (float)dx / 14.0f;
      int dy = (int)(4.0f * (1.0f - norm * norm));
      for (int thick = 0; thick <= 2; thick++) {
        display.drawPixel(cL + dx, sleepY + dy + thick, SSD1306_WHITE);
        display.drawPixel(cR + dx, sleepY + dy + thick, SSD1306_WHITE);
      }
    }
    // Cute tiny sleeping eyelashes
    display.drawLine(cL - 14, sleepY, cL - 17, sleepY - 2, SSD1306_WHITE);
    display.drawLine(cR + 14, sleepY, cR + 17, sleepY - 2, SSD1306_WHITE);

    // Cute sleepy pink blush under eyes
    drawBlush(cL, sleepY + 9, 0.45f);
    drawBlush(cR, sleepY + 9, 0.45f);

    // Cute tiny sleeping mouth (gentle breathing)
    drawMouth(64, 52 + (int)breathY, 10, 1, 0.1f);

    // 3 animated floating "Z... z... z..." snoring bubbles
    for (int i = 0; i < 3; i++) {
      uint32_t zTime = (now + i * 900) % 2700;
      float p = (float)zTime / 2700.0f; // 0.0 to 1.0
      int zx = 86 + (int)(p * 26.0f + sinf(p * 6.28f) * 3.0f);
      int zy = 38 - (int)(p * 32.0f);
      if (zy >= 2 && zy <= 50 && zx <= 122) {
        display.setTextSize((i == 2 || p > 0.65f) ? 2 : 1);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(zx, zy);
        display.print((i == 2 || p > 0.65f) ? "Z" : "z");
      }
    }
    display.display();
    return;
  }

  auto lp = [](float &a, float b, float k) { a += (b - a) * k; };
  const float K = 0.28f;
  lp(faceNow.w, faceTgt.w, K);
  lp(faceNow.h, faceTgt.h, K);
  lp(faceNow.gap, faceTgt.gap, K);
  lp(faceNow.y, faceTgt.y, K);
  lp(faceNow.r, faceTgt.r, K);
  lp(faceNow.lidIn, faceTgt.lidIn, K);
  lp(faceNow.lidOut, faceTgt.lidOut, K);
  lp(faceNow.cheek, faceTgt.cheek, K);
  lp(faceNow.cheekR, faceTgt.cheekR, K);
  lp(faceNow.hR, faceTgt.hR, K);
  lp(faceNow.glint, faceTgt.glint, 0.3f);
  lp(faceNow.blush, faceTgt.blush, 0.15f);
  lp(faceNow.mouthW, faceTgt.mouthW, K);
  lp(faceNow.mouthH, faceTgt.mouthH, K);
  lp(faceNow.mouthCurve, faceTgt.mouthCurve, K);
  lp(faceNow.mouthY, faceTgt.mouthY, K);

  float tx = gazeTgtX, ty = gazeTgtY;
  if (faceTgt.gazeLock) {
    tx = faceTgt.lockX;
    ty = faceTgt.lockY;
  }
  switch (currentEmotion) {
  case EMOTION_THINKING:
    tx = sinf(now / 700.0f) * 5.0f;
    ty = -3;
    break;
  case EMOTION_LISTENING:
    tx = sinf(now / 500.0f) * 2.0f;
    ty = -3.0f;
    break;
  case EMOTION_SPEAKING:
    tx = 0;
    ty = 0;
    break;
  default:
    break;
  }
  gazeX += (tx - gazeX) * 0.18f;
  gazeY += (ty - gazeY) * 0.18f;

  float offX = 0, offY = 0, sc = 1.0f;
  switch (currentEmotion) {
  case EMOTION_IDLE:
    sc = 1.0f + 0.03f * sinf(now / 900.0f);
    break;
  case EMOTION_LISTENING:
    sc = 1.0f + 0.035f * sinf(now / 160.0f);
    break;
  case EMOTION_EXCITED:
    offY = sinf(now / 110.0f) * 2.0f;
    break;
  case EMOTION_SLEEPING:
    offY = sinf(now / 700.0f) * 1.0f;
    break;
  case EMOTION_ERROR:
    if (now - emotionStartedAt < 800)
      offX = ((now / 55) & 1) ? 2 : -2;
    break;
  case EMOTION_PETTED: {
    uint32_t t = now - petMoodT0;
    switch (petMood) {
    case PET_LOVE:
      offX = sinf(t / 230.0f) * 2.5f;
      break;
    case PET_GIGGLE:
      offY = sinf(t / 65.0f) * 2.0f;
      break;
    case PET_SPARKLE:
      sc = 1.0f + 0.05f * sinf(t / 90.0f);
      break;
    case PET_CUDDLE:
      sc = 1.0f + 0.02f * sinf(t / 500.0f);
      break;
    case PET_PLAYFUL:
      offX = sinf(t / 180.0f) * 3.0f;
      break;
    case PET_SLEEPY:
      offY = sinf(t / 900.0f) * 1.2f;
      break;
    case PET_PROUD:
      offY = -1 + sinf(t / 700.0f) * 0.5f;
      break;
    case PET_SURPRISED:
      sc = 1.0f + 0.03f * sinf(t / 150.0f);
      break;
    default:
      break;
    }
    break;
  }
  case EMOTION_SURPRISED:
    sc = 1.0f + 0.03f * sinf(now / 200.0f);
    break;
  case EMOTION_WINK:
    offX = -2;
    break;
  case EMOTION_SMUG:
    offY = -1;
    break;
  case EMOTION_CURIOUS:
    offX = sinf(now / 500.0f) * 3.0f;
    break;
  case EMOTION_BORED:
    offY = sinf(now / 1400.0f) * 0.8f;
    break;
  case EMOTION_LAUGHING:
    offY = sinf(now / 130.0f) * 2.5f;
    break;
  case EMOTION_SCARED:
    if (now - emotionStartedAt < 400)
      offX = ((now / 45) & 1) ? 3 : -3;
    break;
  case EMOTION_SUSPICIOUS:
    offX = sinf(now / 900.0f) * 2.0f;
    break;
  case EMOTION_RELIEVED:
    sc = 1.0f + 0.02f * sinf(now / 800.0f);
    break;
  case EMOTION_PROUD:
    offY = -2;
    break;
  case EMOTION_DIZZY:
    offX = sinf(now / 180.0f) * 4.0f;
    offY = cosf(now / 180.0f) * 3.0f;
    break;
  case EMOTION_LOVESTRUCK:
    sc = 1.0f + 0.04f * sinf(now / 500.0f);
    break;
  case EMOTION_EVIL:
    offY = sinf(now / 150.0f) * 1.5f;
    break;
  case EMOTION_CRYING:
    offY = sinf(now / 90.0f) * 1.5f;
    break;
  case EMOTION_HEARTS:
    sc = 1.0f + 0.05f * sinf(now / 350.0f);
    break;
  case EMOTION_SINGING:
    offX = sinf(now / 250.0f) * 3.0f;
    offY = fabsf(cosf(now / 250.0f)) * 2.0f;
    break;
  case EMOTION_PARTY:
    offY = -fabsf(sinf(now / 120.0f)) * 3.5f;
    break;
  case EMOTION_NERVOUS:
    offX = ((now / 40) & 1) ? 1.5f : -1.5f;
    break;
  case EMOTION_COOL:
    offY = -1;
    break;
  case EMOTION_CUTE:
    sc = 1.0f + 0.03f * sinf(now / 300.0f);
    break;
  default:
    break;
  }

  if (currentEmotion != EMOTION_IDLE && currentEmotion != EMOTION_SLEEPING) {
    offY += sinf(now / 480.0f) * 1.5f;
  }

  int w = (int)(faceNow.w * sc);
  int h = (int)(faceNow.h * sc);
  int r = (int)faceNow.r;
  int gap = (int)faceNow.gap;
  int cy = (int)(faceNow.y + gazeY + offY);
  int cxL = 64 - gap + (int)(gazeX + offX);
  int cxR = 64 + gap + (int)(gazeX + offX);

  int hL = h;
  int hR = (int)(h * faceNow.hR);
  if (eyeBlinking) {
    int bL = hL * eyeOpenPct / 100;
    int bR = hR * eyeOpenPct / 100;
    hL = (faceNow.cheek >= 2 && bL < 3) ? 3 : bL;
    hR = (faceNow.cheekR >= 2 && bR < 3) ? 3 : bR;
  } else {
    if (faceNow.cheek < 2)
      hL = hL * eyeOpenPct / 100;
    if (faceNow.cheekR < 2)
      hR = hR * eyeOpenPct / 100;
  }

  // Active micro-wink during non-idle emotions
  if (activeWinkEnd > now) {
    if (activeWinkLeft)
      hL = 3;
    else
      hR = 3;
  }

  display.clearDisplay();
  bool gl = faceNow.glint > 0.5f;
  drawEye(cxL, cy, w, hL, r, (int)faceNow.lidIn, (int)faceNow.lidOut,
          (int)faceNow.cheek, true, gl);
  drawEye(cxR, cy, w, hR, r, (int)faceNow.lidIn, (int)faceNow.lidOut,
          (int)faceNow.cheekR, false, gl);

  // Custom Skin Accents (Cat Ears & Cyberpunk HUD)
  if (currentEyeSkin == SKIN_CAT) {
    float earTwitch = sinf(now / 900.0f);
    int twL = (earTwitch > 0.88f) ? 2 : 0;
    int twR = (earTwitch < -0.88f) ? 2 : 0;
    int earBaseY = cy - h / 2;
    if (earBaseY > 15) {
      display.fillTriangle(cxL - w / 2 - 2, earBaseY, cxL - w / 4,
                           earBaseY - 12 - twL, cxL + 4, earBaseY,
                           SSD1306_WHITE);
      display.fillTriangle(cxL - w / 2, earBaseY, cxL - w / 4,
                           earBaseY - 9 - twL, cxL + 1, earBaseY,
                           SSD1306_BLACK);
      display.fillTriangle(cxR - 4, earBaseY, cxR + w / 4, earBaseY - 12 - twR,
                           cxR + w / 2 + 2, earBaseY, SSD1306_WHITE);
      display.fillTriangle(cxR - 1, earBaseY, cxR + w / 4, earBaseY - 9 - twR,
                           cxR + w / 2, earBaseY, SSD1306_BLACK);
    }
  } else if (currentEyeSkin == SKIN_CYBERPUNK) {
    display.drawFastHLine(3, 3, 7, SSD1306_WHITE);
    display.drawFastVLine(3, 3, 7, SSD1306_WHITE);
    display.drawFastHLine(118, 3, 7, SSD1306_WHITE);
    display.drawFastVLine(121, 3, 7, SSD1306_WHITE);
    display.drawFastHLine(3, 60, 7, SSD1306_WHITE);
    display.drawFastVLine(3, 54, 7, SSD1306_WHITE);
    display.drawFastHLine(118, 60, 7, SSD1306_WHITE);
    display.drawFastVLine(121, 54, 7, SSD1306_WHITE);
  } else if (currentEyeSkin == SKIN_DEMON) {
    int hornBaseY = cy - h / 2 - 1;
    if (hornBaseY > 14) {
      display.fillTriangle(cxL - 8, hornBaseY, cxL - 16, hornBaseY - 13,
                           cxL - 2, hornBaseY, SSD1306_WHITE);
      display.fillTriangle(cxR + 2, hornBaseY, cxR + 16, hornBaseY - 13,
                           cxR + 8, hornBaseY, SSD1306_WHITE);
    }
  } else if (currentEyeSkin == SKIN_STEAMPUNK) {
    int bridgeY = cy;
    display.drawFastHLine(cxL + w / 2 + 1, bridgeY - 2,
                          (cxR - w / 2) - (cxL + w / 2) - 2, SSD1306_WHITE);
    display.drawFastHLine(cxL + w / 2 + 1, bridgeY + 2,
                          (cxR - w / 2) - (cxL + w / 2) - 2, SSD1306_WHITE);
    display.drawFastHLine(cxL + w / 2 + 3, bridgeY,
                          (cxR - w / 2) - (cxL + w / 2) - 6, SSD1306_WHITE);
  } else if (currentEyeSkin == SKIN_MATRIX) {
    display.drawFastHLine(36, 1, 56, SSD1306_WHITE);
    display.drawFastVLine(36, 1, 4, SSD1306_WHITE);
    display.drawFastVLine(91, 1, 4, SSD1306_WHITE);
    display.drawPixel(6, 4, SSD1306_WHITE);
    display.drawPixel(10, 2, SSD1306_WHITE);
    display.drawPixel(118, 4, SSD1306_WHITE);
    display.drawPixel(122, 2, SSD1306_WHITE);
  } else if (currentEyeSkin == SKIN_CYBORG) {
    display.drawCircle(118, 9, 6, SSD1306_WHITE);
    display.drawFastHLine(110, 9, 17, SSD1306_WHITE);
    display.drawFastVLine(118, 1, 17, SSD1306_WHITE);
    display.drawPixel(118, 9, SSD1306_WHITE);
    display.drawFastHLine(2, 62, 12, SSD1306_WHITE);
    display.drawFastVLine(2, 54, 8, SSD1306_WHITE);
  } else if (currentEyeSkin == SKIN_MECHA) {
    int vY = cy - h / 2 - 2;
    if (vY > 10) {
      display.fillTriangle(63, vY, 48, vY - 9, 60, vY - 5, SSD1306_WHITE);
      display.fillTriangle(65, vY, 80, vY - 9, 68, vY - 5, SSD1306_WHITE);
      display.fillTriangle(61, vY - 2, 64, vY - 8, 67, vY - 2, SSD1306_WHITE);
      display.fillTriangle(61, vY - 2, 64, vY + 2, 67, vY - 2, SSD1306_WHITE);
    }
  } else if (currentEyeSkin == SKIN_SUNGLASSES) {
    int bridgeY = cy - 2;
    display.fillRect(cxL + w / 2 - 1, bridgeY, (cxR - w / 2) - (cxL + w / 2) + 2, 4, SSD1306_WHITE);
    display.drawFastHLine(cxL - w / 2, cy - h / 2 - 1, (cxR + w / 2) - (cxL - w / 2) + 1, SSD1306_WHITE);
  } else if (currentEyeSkin == SKIN_PUPPY) {
    float earSway = sinf(now / 800.0f);
    int sway = (int)(earSway * 2.0f);
    display.fillRoundRect(2, cy - 8 + sway, 9, 22, 4, SSD1306_WHITE);
    display.fillRoundRect(3, cy - 5 + sway, 7, 16, 3, SSD1306_BLACK);
    display.fillRoundRect(117, cy - 8 - sway, 9, 22, 4, SSD1306_WHITE);
    display.fillRoundRect(118, cy - 5 - sway, 7, 16, 3, SSD1306_BLACK);
    display.fillTriangle(62, cy + h / 2 + 3, 66, cy + h / 2 + 3, 64, cy + h / 2 + 6, SSD1306_WHITE);
  } else if (currentEyeSkin == SKIN_HYPNO) {
    float a = (float)(now % 1000) / 1000.0f * 2.0f * PI;
    int dX = (int)(cosf(a) * 4);
    int dY = (int)(sinf(a) * 2);
    display.drawPixel(20 + dX, 8 + dY, SSD1306_WHITE);
    display.drawPixel(108 - dX, 8 - dY, SSD1306_WHITE);
  } else if (currentEyeSkin == SKIN_ALIEN) {
    display.drawFastVLine(64, 1, 9, SSD1306_WHITE);
    display.drawCircle(64, 2, 2, SSD1306_WHITE);
    if ((now / 250) % 2 == 0) {
      display.fillCircle(64, 2, 1, SSD1306_WHITE);
      display.drawFastHLine(61, 2, 7, SSD1306_WHITE);
    }
  } else if (currentEyeSkin == SKIN_NINJA) {
    display.drawFastHLine(0, 6, 128, SSD1306_WHITE);
    display.drawFastHLine(0, 7, 128, SSD1306_WHITE);
    display.fillRoundRect(48, 2, 32, 9, 2, SSD1306_BLACK);
    display.drawRoundRect(48, 2, 32, 9, 2, SSD1306_WHITE);
    display.drawPixel(50, 4, SSD1306_WHITE);
    display.drawPixel(78, 4, SSD1306_WHITE);
    display.drawPixel(50, 8, SSD1306_WHITE);
    display.drawPixel(78, 8, SSD1306_WHITE);
    display.drawLine(60, 6, 68, 6, SSD1306_WHITE);
    display.drawLine(66, 4, 62, 8, SSD1306_WHITE);
  } else if (currentEyeSkin == SKIN_ROUND) {
    // Dynamic floating micro-brows that articulate with emotion
    int browY = cy - h / 2 - 3;
    if (browY >= 2 && h > 10) {
      int tiltL = (int)(faceNow.lidIn - faceNow.lidOut) / 3;
      int tiltR = (int)(faceNow.lidOut - faceNow.lidIn) / 3;
      display.drawLine(cxL - 7, browY + tiltL, cxL + 7, browY - tiltL, SSD1306_WHITE);
      display.drawLine(cxR - 7, browY - tiltR, cxR + 7, browY + tiltR, SSD1306_WHITE);
    }
  }

  if (faceNow.blush > 0.2f) {
    drawBlush(cxL, cy + h / 2 + 4, faceNow.blush);
    drawBlush(cxR, cy + h / 2 + 4, faceNow.blush);
  }

  if (currentEmotion == EMOTION_SPEAKING) {
    float t = now / 110.0f;
    int mw = 14 + (int)(6 * fabsf(sinf(t * 0.7f)));
    int mh = 3 + (int)(7 * fabsf(sinf(t * 1.9f)));
    display.fillRoundRect(64 - mw / 2, 52 - mh / 2, mw, mh, mh > 3 ? 2 : 1,
                          SSD1306_WHITE);
  } else if (faceNow.mouthW > 3) {
    int my = 52 + (int)faceNow.mouthY;
    drawMouth(64, my, (int)faceNow.mouthW, (int)faceNow.mouthH,
              faceNow.mouthCurve);
  }

  if (currentEmotion == EMOTION_LISTENING) {
    // Sleek, minimal voice spectrum pulse (5 micro-bars, subtle undulating wave)
    for (int i = 0; i < 5; i++) {
      float phase = (i - 2) * 0.9f;
      float wave = fabsf(sinf(now / 140.0f + phase));
      int bh = 1 + (int)(wave * (i == 2 ? 5 : (i == 1 || i == 3 ? 4 : 2)));
      int bx = 56 + i * 4; // x = 56, 60, 64, 68, 72
      display.fillRect(bx, 53 - bh, 2, bh * 2 + 1, SSD1306_WHITE);
    }
  }

  if (currentEmotion == EMOTION_DEAD) {
    for (int d = -1; d <= 1; d++) {
      display.drawLine(cxL - 7 + d, cy - 7, cxL + 7 + d, cy + 7, SSD1306_BLACK);
      display.drawLine(cxL - 7 + d, cy + 7, cxL + 7 + d, cy - 7, SSD1306_BLACK);
      display.drawLine(cxR - 7 + d, cy - 7, cxR + 7 + d, cy + 7, SSD1306_BLACK);
      display.drawLine(cxR - 7 + d, cy + 7, cxR + 7 + d, cy - 7, SSD1306_BLACK);
    }
  }

  if (currentEmotion == EMOTION_COOL) {
    display.fillRect(cxL + w / 2 - 2, cy - 5, (cxR - w / 2) - (cxL + w / 2) + 4,
                     3, SSD1306_WHITE);
    display.drawLine(cxL - w / 2 - 4, cy - 7, cxL - w / 2, cy - 4,
                     SSD1306_WHITE);
    display.drawLine(cxR + w / 2 + 4, cy - 7, cxR + w / 2, cy - 4,
                     SSD1306_WHITE);
  }

  if (currentEmotion == EMOTION_EVIL) {
    display.fillTriangle(cxL - 10, cy - h / 2 - 1, cxL - 16, cy - h / 2 - 9,
                         cxL - 4, cy - h / 2 - 1, SSD1306_WHITE);
    display.fillTriangle(cxR + 4, cy - h / 2 - 1, cxR + 16, cy - h / 2 - 9,
                         cxR + 10, cy - h / 2 - 1, SSD1306_WHITE);
  }

  if (currentEmotion == EMOTION_CRYING) {
    int tearY = (now / 40) % 18;
    display.fillCircle(cxL - w / 4, cy + h / 2 + 2 + tearY, 2, SSD1306_WHITE);
    display.fillCircle(cxR + w / 4, cy + h / 2 + 2 + ((tearY + 9) % 18), 2,
                       SSD1306_WHITE);
  }

  if (currentEmotion == EMOTION_HEARTS) {
    int hY = 10 + (int)(sinf(now / 180.0f) * 3);
    display.drawBitmap(12, hY, heartBmp, 7, 6, SSD1306_WHITE);
    display.drawBitmap(108, hY, heartBmp, 7, 6, SSD1306_WHITE);
  }

  if (currentEmotion == EMOTION_SINGING) {
    int noteY = 12 + (int)(sinf(now / 150.0f) * 4);
    display.fillCircle(112, noteY + 4, 2, SSD1306_WHITE);
    display.drawFastVLine(114, noteY, 5, SSD1306_WHITE);
    display.drawFastHLine(114, noteY, 3, SSD1306_WHITE);
  }

  if (currentEmotion == EMOTION_NERVOUS) {
    int swY = 10 + (int)(sinf(now / 200.0f) * 2);
    display.fillCircle(112, swY + 3, 2, SSD1306_WHITE);
    display.drawPixel(112, swY, SSD1306_WHITE);
  }

  if (currentEmotion == EMOTION_PARTY) {
    if (((now / 150) % 2) == 0) {
      display.drawPixel(16, 12, SSD1306_WHITE);
      display.drawPixel(112, 14, SSD1306_WHITE);
      display.drawPixel(20, 20, SSD1306_WHITE);
      display.drawPixel(108, 22, SSD1306_WHITE);
    } else {
      display.drawPixel(14, 16, SSD1306_WHITE);
      display.drawPixel(114, 10, SSD1306_WHITE);
      display.drawPixel(18, 24, SSD1306_WHITE);
      display.drawPixel(110, 18, SSD1306_WHITE);
    }
  }

  drawFlourish(now);
  display.display();
}

void drawClockScreen() {
  if (!oledOK)
    return;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 100)) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    const char *l1 = "Time not synced";
    int16_t x1, y1;
    uint16_t tw, th;
    display.getTextBounds(l1, 0, 0, &x1, &y1, &tw, &th);
    display.setCursor((SCREEN_WIDTH - tw) / 2, 24);
    display.print(l1);
    display.display();
    return;
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  int16_t x1, y1;
  uint16_t tw, th;
  int hour12 = timeinfo.tm_hour % 12;
  if (hour12 == 0)
    hour12 = 12;
  const char *ampm = (timeinfo.tm_hour >= 12) ? "PM" : "AM";
  char timeStr[8];
  snprintf(timeStr, sizeof(timeStr), "%d:%02d", hour12, timeinfo.tm_min);
  display.setTextSize(3);
  display.getTextBounds(timeStr, 0, 0, &x1, &y1, &tw, &th);
  display.setCursor((SCREEN_WIDTH - tw) / 2, 6);
  display.print(timeStr);
  display.setTextSize(1);
  display.getTextBounds(ampm, 0, 0, &x1, &y1, &tw, &th);
  display.setCursor((SCREEN_WIDTH - tw) / 2, 34);
  display.print(ampm);
  const char *days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                          "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  char dateStr[32];
  snprintf(dateStr, sizeof(dateStr), "%s %02d %s %04d", days[timeinfo.tm_wday],
           timeinfo.tm_mday, months[timeinfo.tm_mon], timeinfo.tm_year + 1900);
  display.getTextBounds(dateStr, 0, 0, &x1, &y1, &tw, &th);
  display.setCursor((SCREEN_WIDTH - tw) / 2, 50);
  display.print(dateStr);
  display.display();
}

void drawWeatherScreen() {
  if (!oledOK)
    return;
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // If weather hasn't loaded yet
  if (lastWeatherFetchAt == 0 || weatherCode < 0 || weatherDesc.length() == 0) {
    display.setTextSize(1);
    int16_t x1, y1;
    uint16_t tw, th;
    display.getTextBounds(weatherCity, 0, 0, &x1, &y1, &tw, &th);
    display.setCursor((SCREEN_WIDTH - tw) / 2, 8);
    display.print(weatherCity);
    display.drawFastHLine(14, 20, SCREEN_WIDTH - 28, SSD1306_WHITE);

    const char *loadMsg = "Fetching live weather...";
    display.getTextBounds(loadMsg, 0, 0, &x1, &y1, &tw, &th);
    display.setCursor((SCREEN_WIDTH - tw) / 2, 32);
    display.print(loadMsg);

    display.drawRoundRect(20, 48, 88, 8, 3, SSD1306_WHITE);
    int p = (millis() / 50) % 84;
    display.fillRoundRect(22 + (p % 72), 50, 14, 4, 2, SSD1306_WHITE);
    display.display();
    return;
  }

  // Header: City Name
  display.setTextSize(1);
  int16_t x1, y1;
  uint16_t tw, th;
  display.getTextBounds(weatherCity, 0, 0, &x1, &y1, &tw, &th);
  display.setCursor((SCREEN_WIDTH - tw) / 2, 2);
  display.print(weatherCity);
  display.drawFastHLine(10, 12, SCREEN_WIDTH - 20, SSD1306_WHITE);

  // Center: Temperature
  char tempStr[16];
  snprintf(tempStr, sizeof(tempStr), "%d C", (int)round(weatherTemp));
  display.setTextSize(2);
  display.getTextBounds(tempStr, 0, 0, &x1, &y1, &tw, &th);
  int cx = (SCREEN_WIDTH - tw) / 2;
  display.setCursor(cx, 18);
  display.print(tempStr);
  display.drawCircle(cx + tw - 12, 19, 2, SSD1306_WHITE);

  // Subtitle: Condition
  display.setTextSize(1);
  display.getTextBounds(weatherDesc, 0, 0, &x1, &y1, &tw, &th);
  display.setCursor((SCREEN_WIDTH - tw) / 2, 38);
  display.print(weatherDesc);

  // Footer: Humidity & Wind
  char footStr[32];
  snprintf(footStr, sizeof(footStr), "H:%d%%  W:%.0fkm/h", weatherHumidity,
           weatherWind);
  display.getTextBounds(footStr, 0, 0, &x1, &y1, &tw, &th);
  display.setCursor((SCREEN_WIDTH - tw) / 2, 52);
  display.print(footStr);

  display.display();
}

void drawBannerScreen() {
  if (!oledOK || !bannerActive || bannerText.length() == 0)
    return;
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // Marquee scrolling text only
  display.setTextSize(2);
  display.setTextWrap(false);
  int16_t x1, y1;
  uint16_t tw, th;
  display.getTextBounds(bannerText, 0, 0, &x1, &y1, &tw, &th);
  display.setCursor(bannerScrollX, 24);
  display.print(bannerText);

  display.display();

  // Advance scroll
  bannerScrollX -= 3;
  if (bannerScrollX < -(int)tw - 20) {
    bannerScrollX = SCREEN_WIDTH + 10;
    bannerLoops++;
    if (bannerLoops >= 4) {
      bannerActive = false;
      setEmotion(EMOTION_IDLE);
    }
  }
}

void tickFace() {
  if (!oledOK)
    return;
  if (bootInfoUntil) {
    if (millis() < bootInfoUntil)
      return;
    bootInfoUntil = 0;
  }
  if (animationIsActive()) {
    uint32_t now = millis();
    tickAnimation(now);
    return;
  }
  if (bannerActive) {
    uint32_t now = millis();
    if (now - lastFaceDrawAt < 35)
      return;
    lastFaceDrawAt = now;
    drawBannerScreen();
    return;
  }
  if (showClockScreen) {
    uint32_t now = millis();
    if (now - lastFaceDrawAt < 200)
      return;
    lastFaceDrawAt = now;
    drawClockScreen();
    return;
  }
  if (showWeatherScreen) {
    uint32_t now = millis();
    if (now - lastFaceDrawAt < 200)
      return;
    lastFaceDrawAt = now;
    drawWeatherScreen();
    return;
  }
  uint32_t now = millis();
  if (now - lastFaceDrawAt < gFaceFrameMs)
    return;
  lastFaceDrawAt = now;
  eyesUpdateAnim(now);
  drawFace();
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
  if (currentEmotion != EMOTION_IDLE)
    return;
  if (busyProcessing)
    return;
  if (returnIdleAt)
    return;
  if (now < nextRandomEmotionAt)
    return;
  static const Emotion pool[] = {
      EMOTION_HAPPY,    EMOTION_EXCITED,    EMOTION_CONFUSED,
      EMOTION_CURIOUS,  EMOTION_WINK,       EMOTION_SMUG,
      EMOTION_LAUGHING, EMOTION_LOVESTRUCK, EMOTION_SURPRISED};
  const int poolN = sizeof(pool) / sizeof(pool[0]);
  setEmotion(pool[random(0, poolN)]);
  emotionAutoReturnAt = now + 1500 + random(0, 1500);
  nextRandomEmotionAt = emotionAutoReturnAt + 8000 + random(0, 6000);
}

void showErrorFace(uint32_t ms) {
  setEmotion(EMOTION_ERROR);
  soundError();
  errorFaceUntil = millis() + ms + 300;
  uint32_t t0 = millis();
  while (millis() - t0 < ms) {
    if (cancelRequest)
      break;
    pumpUI();
    delay(10);
  }
}

void drawMessage(const char *l1, const char *l2) {
  if (!oledOK)
    return;
  bootInfoUntil = 0;
  display.clearDisplay();

  // Top Status Bar: KIWI OS · [STATUS]
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(4, 2);
  display.print("KIWI OS");

  display.drawRoundRect(84, 0, 44, 10, 2, SSD1306_WHITE);
  display.setCursor(89, 1);
  display.print("STATUS");

  display.drawFastHLine(0, 12, 128, SSD1306_WHITE);

  // High-Tech Rounded Frame Card
  display.drawRoundRect(0, 15, 128, 48, 4, SSD1306_WHITE);
  display.setCursor(8, 25);
  display.print(l1);
  if (l2 && strlen(l2)) {
    display.setCursor(8, 41);
    display.print(l2);
  }
  display.display();
}

void drawConnectingFrame(const char *targetSSID, int frame) {
  if (!oledOK)
    return;
  bootInfoUntil = 0;
  display.clearDisplay();

  // Top Status Rail: KIWI OS · [SEARCH]
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(4, 2);
  display.print("KIWI OS");

  // Right pill badge: [SEARCH]
  display.drawRoundRect(84, 0, 44, 10, 2, SSD1306_WHITE);
  display.setCursor(88, 1);
  display.print("SEARCH");

  display.drawFastHLine(0, 12, 128, SSD1306_WHITE);

  // Center Animated Wi-Fi Radar Broadcast Pulse (cx = 64, cy = 29)
  int cx = 64;
  int cy = 29;
  display.fillCircle(cx, cy, 2, SSD1306_WHITE);

  int phase = (frame / 2) % 4; // Cycles 0, 1, 2, 3
  if (phase >= 1) {
    for (int a = -45; a <= 45; a += 5) {
      float r = (a - 90) * 0.0174533f;
      display.drawPixel(cx + (int)(cosf(r) * 6), cy + (int)(sinf(r) * 6), SSD1306_WHITE);
    }
  }
  if (phase >= 2) {
    for (int a = -50; a <= 50; a += 4) {
      float r = (a - 90) * 0.0174533f;
      display.drawPixel(cx + (int)(cosf(r) * 11), cy + (int)(sinf(r) * 11), SSD1306_WHITE);
    }
  }
  if (phase >= 3) {
    for (int a = -55; a <= 55; a += 3) {
      float r = (a - 90) * 0.0174533f;
      display.drawPixel(cx + (int)(cosf(r) * 16), cy + (int)(sinf(r) * 16), SSD1306_WHITE);
    }
  }

  // Network SSID Label (Centered)
  display.setCursor(4, 38);
  String s = (targetSSID && strlen(targetSSID)) ? targetSSID : "Wi-Fi";
  if (s.length() > 16)
    s = s.substring(0, 14) + "..";
  int sLen = s.length() * 6;
  int sx = (128 - sLen) / 2;
  display.setCursor(sx > 2 ? sx : 2, 38);
  display.print(s);

  // Bottom Sleek Laser Scanner Progress Bar (x=14 to 114)
  display.drawRoundRect(14, 52, 100, 7, 3, SSD1306_WHITE);
  int pingPong = frame % 18;
  int bx = (pingPong < 9) ? (pingPong * 9) : ((18 - pingPong) * 9);
  display.fillRoundRect(16 + bx, 54, 18, 3, 1, SSD1306_WHITE);

  display.display();
}

void drawConnectedNotice(const char *title, uint32_t holdMs) {
  if (!oledOK)
    return;
  display.clearDisplay();

  // Top Status Bar: KIWI OS · [ONLINE]
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(4, 2);
  display.print("KIWI OS");

  display.fillRoundRect(78, 0, 50, 10, 2, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  display.setCursor(83, 1);
  display.print("ONLINE");
  display.setTextColor(SSD1306_WHITE);

  display.drawFastHLine(0, 12, 128, SSD1306_WHITE);

  // Center Checkmark Rings
  display.drawCircle(64, 30, 13, SSD1306_WHITE);
  display.drawCircle(64, 30, 11, SSD1306_WHITE);
  display.drawLine(58, 30, 62, 34, SSD1306_WHITE);
  display.drawLine(62, 34, 71, 25, SSD1306_WHITE);
  display.drawLine(58, 31, 62, 35, SSD1306_WHITE);
  display.drawLine(62, 35, 71, 26, SSD1306_WHITE);

  // Network name below
  String s = (title && strlen(title)) ? title : "Wi-Fi Connected";
  if (s.length() > 18)
    s = s.substring(0, 16) + "..";
  int sLen = s.length() * 6;
  int sx = (128 - sLen) / 2;
  display.setCursor(sx > 2 ? sx : 2, 50);
  display.print(s);

  display.display();
  delay(holdMs);
}

void showStatus(const char *l1, const char *l2, uint32_t ms) {
  drawMessage(l1, l2);
  soundError();
  uint32_t t0 = millis();
  while (millis() - t0 < ms) {
    if (cancelRequest)
      break;
    updateTonePlayer();
    pumpUI();
    delay(5);
  }
}

void showBootInfo(uint32_t holdMs) {
  if (!oledOK)
    return;
  display.clearDisplay();
  display.setTextSize(1);

  // Top Header Rail (y=0 to 11)
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(2, 2);
  display.print("KIWI");
  display.drawPixel(28, 5, SSD1306_WHITE);
  display.setCursor(33, 2);
  display.print("v4.6");

  // Inverted Status Pill Badge
  if (WiFi.status() == WL_CONNECTED) {
    display.fillRoundRect(78, 0, 50, 10, 2, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
    display.setCursor(83, 1);
    display.print("ONLINE");
  } else {
    display.fillRoundRect(72, 0, 56, 10, 2, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
    display.setCursor(76, 1);
    display.print("AP MODE");
  }
  display.setTextColor(SSD1306_WHITE);
  display.drawFastHLine(0, 12, 128, SSD1306_WHITE);

  // High-Tech Diagnostics Card (y=14 to 49)
  display.drawRoundRect(0, 14, 128, 36, 3, SSD1306_WHITE);

  if (WiFi.status() == WL_CONNECTED) {
    String s = WiFi.SSID();
    if (s.length() > 14)
      s = s.substring(0, 12) + "..";
    display.setCursor(6, 17);
    display.print("NET: " + s);

    display.setCursor(6, 27);
    display.print("IP:  " + WiFi.localIP().toString());

    display.setCursor(6, 37);
    display.print("WEB: myrobot.local");
  } else {
    display.setCursor(6, 17);
    String ap = apSSID;
    if (ap.length() > 14)
      ap = ap.substring(0, 12) + "..";
    display.print("AP:  " + ap);

    display.setCursor(6, 27);
    display.print("IP:  " + WiFi.softAPIP().toString());

    display.setCursor(6, 37);
    display.print("CFG: 192.168.4.1");
  }

  // Bottom Status Footer Rail (y=51 to 63)
  display.drawFastHLine(0, 51, 128, SSD1306_WHITE);
  display.setCursor(4, 54);
  String prov = llmProvider;
  prov.toUpperCase();
  if (prov.length() > 6)
    prov = prov.substring(0, 6);
  display.printf("AI:%s  VOL:%d%%", prov.c_str(), speakerVolume);

  // Heartbeat ready indicator dot
  display.fillCircle(123, 57, 2, SSD1306_WHITE);

  display.display();
  bootInfoUntil = millis() + holdMs;
}

void holdFace(uint32_t ms) {
  uint32_t t0 = millis();
  while (millis() - t0 < ms) {
    if (cancelRequest)
      break;
    pumpUI();
    delay(5);
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
  switch (m) {
  case PET_LOVE:
    flourish = FL_HEART;
    break;
  case PET_GIGGLE:
    flourish = FL_NOTE;
    break;
  case PET_SHY:
    flourish = FL_HEART;
    break;
  case PET_SPARKLE:
    flourish = FL_SPARKLE;
    break;
  case PET_KISS:
    flourish = FL_HEART;
    break;
  case PET_CUDDLE:
    flourish = FL_MOON;
    break;
  case PET_PLAYFUL:
    flourish = FL_NOTE;
    break;
  case PET_SLEEPY:
    flourish = FL_MOON;
    break;
  case PET_PROUD:
    flourish = FL_STAR;
    break;
  case PET_SURPRISED:
    flourish = FL_SPARKLE;
    break;
  }
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
  struct {
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
  for (auto &m : emotes) {
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
    switch (mood) {
    case PET_LOVE:
      flourish = FL_HEART;
      break;
    case PET_GIGGLE:
      flourish = FL_NOTE;
      break;
    case PET_SHY:
      flourish = FL_HEART;
      break;
    case PET_SPARKLE:
      flourish = FL_SPARKLE;
      break;
    case PET_KISS:
      flourish = FL_HEART;
      break;
    case PET_CUDDLE:
      flourish = FL_MOON;
      break;
    case PET_PLAYFUL:
      flourish = FL_NOTE;
      break;
    case PET_SLEEPY:
      flourish = FL_MOON;
      break;
    case PET_PROUD:
      flourish = FL_STAR;
      break;
    case PET_SURPRISED:
      flourish = FL_SPARKLE;
      break;
    }
    setEmotion(EMOTION_PETTED);
    emotionAutoReturnAt = millis() + (holdMs > 0 ? holdMs : 60000);
    if (withSound)
      pendingSound = (int8_t)mood;
    return true;
  }
  return false;
}
