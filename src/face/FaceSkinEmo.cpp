#include "face/FaceSkinEmo.h"
#include "config/AppConfig.h"
#include "face/FaceDisplay.h"
#include "face/FaceExpressions.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

static inline int minI(int a, int b) { return a < b ? a : b; }
static inline int maxI(int a, int b) { return a > b ? a : b; }
static inline float clampf(float v, float lo, float hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

// Procedural solid glowing heart eye for Love / Lovestruck / Petted Love
static void drawEmoHeart(int cx, int cy, int w, int h) {
  int hr = maxI(3, w / 4);
  int circleY = cy - h / 8;
  int botY = cy + h / 2 - 1;

  // Upper heart lobes
  display.fillCircle(cx - hr, circleY, hr, SSD1306_WHITE);
  display.fillCircle(cx + hr, circleY, hr, SSD1306_WHITE);

  // Center connecting body
  int bodyW = hr * 4 + 1;
  int bodyH = maxI(2, h / 4);
  display.fillRect(cx - hr * 2, circleY, bodyW, bodyH, SSD1306_WHITE);

  // Tapered lower point
  display.fillTriangle(cx - hr * 2, circleY + bodyH - 1, cx + hr * 2,
                       circleY + bodyH - 1, cx, botY, SSD1306_WHITE);
}

// Slanted eyelid mask (angry / sad / sleepy / thinking).
// Two black triangles form an angled quad cutting the top edge.
static inline void maskEmoLids(int x0, int x1, int top, int lidIn, int lidOut,
                               bool isLeft) {
  if (lidIn <= 0 && lidOut <= 0)
    return;
  int lL = isLeft ? lidOut : lidIn;
  int lR = isLeft ? lidIn : lidOut;
  display.fillTriangle(x0 - 1, top - 1, x1 + 1, top - 1, x1 + 1, top + lR,
                       SSD1306_BLACK);
  display.fillTriangle(x0 - 1, top - 1, x1 + 1, top + lR, x0 - 1, top + lL,
                       SSD1306_BLACK);
}

// Cheek mask (happy / laughing upward crescent arch).
static inline void maskEmoCheek(int cx, int bot, int w, int cheek) {
  if (cheek > 1) {
    int R = w * 7 / 10;
    display.fillCircle(cx, bot - cheek + R, R, SSD1306_BLACK);
  }
}

void drawEmoEye(int cx, int cy, int w, int h, int r, int lidIn, int lidOut,
                int cheek, bool isLeft, bool glint) {
  int x0 = cx - w / 2;
  int x1 = x0 + w;
  int top = cy - h / 2;
  int bot = top + h;

  // 1. Blinking or fully shut eye: Sleek horizontal rounded line slit
  if (h <= 3) {
    display.fillRoundRect(x0, cy - 1, w, maxI(2, h), 1, SSD1306_WHITE);
    return;
  }

  // 2. Love / Hearts: EMO's signature glowing heart eyes
  if (currentEmotion == EMOTION_HEARTS ||
      currentEmotion == EMOTION_LOVESTRUCK ||
      (currentEmotion == EMOTION_PETTED && petMood == PET_LOVE)) {
    drawEmoHeart(cx, cy, w, h);
    return;
  }

  // 3. EMO Procedural Eye Geometry:
  // Solid glowing rounded rectangle with prominent rounded corners and NO
  // pupil. Corner radius scaled proportionally to eye dimensions (~30% of
  // dimension).
  int emoR = (r > 3) ? r : (int)clampf(minI(w, h) * 0.30f, 5.0f, 12.0f);
  if (emoR * 2 > h)
    emoR = h / 2;
  if (emoR * 2 > w)
    emoR = w / 2;

  display.fillRoundRect(x0, top, w, h, emoR, SSD1306_WHITE);

  // 4. Procedural Eyelids (Angled cuts for angry / sad / sleepy)
  maskEmoLids(x0, x1, top, lidIn, lidOut, isLeft);

  // 5. Procedural Cheeks (Upward crescent bite for happy / laughing)
  maskEmoCheek(cx, bot, w, cheek);
}
