#include "face/FaceSkins.h"
#include "config/AppConfig.h"
#include "face/FaceDisplay.h"
#include "face/FaceExpressions.h"
#include "face/FaceSkinEmo.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include <math.h>

static inline int minI(int a, int b) { return a < b ? a : b; }
static inline int maxI(int a, int b) { return a > b ? a : b; }
static inline float clampf(float v, float lo, float hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

// Precomputed 32-point unit squircle table (n = 4 superellipse: |cos t|^(1/2),
// |sin t|^(1/2))
static float squircleUnitX[32];
static float squircleUnitY[32];
static bool squircleTableReady = false;

static void initSquircleTable() {
  if (squircleTableReady)
    return;
  for (int i = 0; i < 32; i++) {
    float t = (float)i * (6.283185307f / 32.0f);
    float c = cosf(t);
    float s = sinf(t);
    float signC = (c >= 0.0f) ? 1.0f : -1.0f;
    float signS = (s >= 0.0f) ? 1.0f : -1.0f;
    squircleUnitX[i] = signC * sqrtf(fabsf(c));
    squircleUnitY[i] = signS * sqrtf(fabsf(s));
  }
  squircleTableReady = true;
}

static void computeSquirclePoints(int cx, int cy, int w, int h, int ptsX[32],
                                  int ptsY[32]) {
  initSquircleTable();
  float a = (float)w * 0.5f;
  float b = (float)h * 0.5f;
  for (int i = 0; i < 32; i++) {
    ptsX[i] = (int)lroundf((float)cx + a * squircleUnitX[i]);
    ptsY[i] = (int)lroundf((float)cy + b * squircleUnitY[i]);
  }
}

static void fillSquircle(int x0, int top, int w, int h, int cx, int cy) {
  if (w < 12 || h < 10) {
    int rad = minI(w, h) / 3;
    if (rad < 1)
      rad = 1;
    display.fillRoundRect(x0, top, w, h, rad, SSD1306_WHITE);
    return;
  }
  int ptsX[32];
  int ptsY[32];
  computeSquirclePoints(cx, cy, w, h, ptsX, ptsY);

  int minY = ptsY[0], maxY = ptsY[0];
  for (int i = 1; i < 32; i++) {
    if (ptsY[i] < minY)
      minY = ptsY[i];
    if (ptsY[i] > maxY)
      maxY = ptsY[i];
  }
  if (minY < top)
    minY = top;
  if (maxY > top + h - 1)
    maxY = top + h - 1;

  for (int y = minY; y <= maxY; y++) {
    int minX = 9999;
    int maxX = -9999;
    for (int i = 0; i < 32; i++) {
      int j = (i + 1) % 32;
      int y1 = ptsY[i];
      int y2 = ptsY[j];
      int x1 = ptsX[i];
      int x2 = ptsX[j];
      if (y1 == y2) {
        if (y1 == y) {
          int lx = minI(x1, x2);
          int rx = maxI(x1, x2);
          if (lx < minX)
            minX = lx;
          if (rx > maxX)
            maxX = rx;
        }
        continue;
      }
      if ((y1 <= y && y <= y2) || (y2 <= y && y <= y1)) {
        int xCross = x1 + (int)((int64_t)(y - y1) * (x2 - x1) / (y2 - y1));
        if (xCross < minX)
          minX = xCross;
        if (xCross > maxX)
          maxX = xCross;
      }
    }
    if (minX <= maxX) {
      if (minX < x0)
        minX = x0;
      if (maxX > x0 + w - 1)
        maxX = x0 + w - 1;
      display.drawFastHLine(minX, y, maxX - minX + 1, SSD1306_WHITE);
    }
  }
}

// Slanted eyelid mask (angry / sad / sleepy / thinking). Two black triangles
// form a quad that slopes from the left lid height to the right lid height.
static inline void maskLids(int x0, int x1, int top, int lidIn, int lidOut,
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

// Cheek mask (happy / laughing arch). A big black circle bites the lower edge.
static inline void maskCheek(int cx, int bot, int w, int cheek) {
  if (cheek > 1) {
    int R = w * 7 / 10;
    display.fillCircle(cx, bot - cheek + R, R, SSD1306_BLACK);
  }
}

// Shared glowing heart-eye renderer for non-EMO skins (rounded or 8-bit pixel)
static void drawSkinHeartEye(int cx, int cy, int w, int h, bool pixelated) {
  if (pixelated) {
    int u = maxI(2, minI(w, h) / 7);
    int topY = cy - (u * 5) / 2;
    // Left and right upper pixel lobes
    display.fillRect(cx - 3 * u, topY, 2 * u, 2 * u, SSD1306_WHITE);
    display.fillRect(cx + u, topY, 2 * u, 2 * u, SSD1306_WHITE);
    // Middle full row
    display.fillRect(cx - 3 * u, topY + u, 6 * u, 2 * u, SSD1306_WHITE);
    // Tapered bottom rows
    display.fillRect(cx - 2 * u, topY + 3 * u, 4 * u, u, SSD1306_WHITE);
    display.fillRect(cx - u, topY + 4 * u, 2 * u, u, SSD1306_WHITE);
    // Tiny retro glint cutout
    display.fillRect(cx - 2 * u, topY + u, maxI(2, u / 2), maxI(2, u / 2),
                     SSD1306_BLACK);
    return;
  }

  int hr = maxI(3, w / 4);
  int circleY = cy - h / 8;
  int botY = cy + h / 2 - 1;
  display.fillCircle(cx - hr, circleY, hr, SSD1306_WHITE);
  display.fillCircle(cx + hr, circleY, hr, SSD1306_WHITE);
  int bodyW = hr * 4 + 1;
  int bodyH = maxI(2, h / 4);
  display.fillRect(cx - hr * 2, circleY, bodyW, bodyH, SSD1306_WHITE);
  display.fillTriangle(cx - hr * 2, circleY + bodyH - 1, cx + hr * 2,
                       circleY + bodyH - 1, cx, botY, SSD1306_WHITE);
  // Subtle dark heart shine dot
  if (w >= 20) {
    display.fillRect(cx - hr - 1, circleY - hr / 2, 2, 2, SSD1306_BLACK);
  }
}

void drawEye(int cx, int cy, int w, int h, int r, int lidIn, int lidOut,
             int cheek, bool isLeft, bool glint) {
  int x0 = cx - w / 2;
  int x1 = x0 + w;
  int top = cy - h / 2;
  int bot = top + h;

  bool isHeartMood =
      (h > 6) &&
      (currentEmotion == EMOTION_HEARTS ||
       currentEmotion == EMOTION_LOVESTRUCK ||
       (currentEmotion == EMOTION_PETTED && petMood == PET_LOVE));

  // 1. KIWI EYES (Desk Buddy): Flat upper edge, rounded bottom base,
  // U-shaped deep pupil cutout with floating top-right catchlight glint dot.
  if (currentEyeSkin == SKIN_KIWI) {
    if (h <= 3) {
      display.fillRoundRect(x0, cy - 1, w, maxI(2, h), 1, SSD1306_WHITE);
      return;
    }
    if (isHeartMood) {
      drawSkinHeartEye(cx, cy, w, h, false);
      return;
    }

    int rBottom = (r > 2) ? r : minI(7, h / 3);
    if (rBottom * 2 > h)
      rBottom = h / 2;

    // Outer sclera: flat top edge, rounded lower edge
    display.fillRect(x0, top, w, h - rBottom, SSD1306_WHITE);
    display.fillRoundRect(x0, bot - rBottom * 2, w, rBottom * 2, rBottom,
                          SSD1306_WHITE);

    // Dynamic pupil cutout (scales with emotional dilation and moves with 3D
    // parallax)
    float pScale = clampf(faceNow.pupilScale, 0.65f, 1.45f);
    int pw = (int)lroundf(w * 0.44f * pScale);
    if (pw < 5)
      pw = 5;
    if (pw > w - 4)
      pw = w - 4;
    int ph = (int)lroundf(h * 0.58f * pScale);
    if (ph < 4)
      ph = 4;
    if (ph > h - 2)
      ph = h - 2;
    int pr = minI(4, maxI(2, pw / 4));

    // 3D Parallax shift: pupil moves faster than sclera for 3D depth illusion
    int pupilShiftX = (int)lroundf(gazeX * 0.85f);
    int px = cx + pupilShiftX - pw / 2;
    if (px < x0 + 2)
      px = x0 + 2;
    if (px + pw > x1 - 2)
      px = x1 - 2 - pw;

    int pupilShiftY = (int)lroundf(gazeY * 0.75f);
    int py = top + pupilShiftY;
    if (py < top)
      py = top;
    if (py + ph > bot - 2)
      py = bot - 2 - ph;

    display.fillRect(px, py, pw, ph - pr, SSD1306_BLACK);
    display.fillRoundRect(px, py + ph - pr * 2, pw, pr * 2, pr, SSD1306_BLACK);

    // Specular catchlight / glint dot in upper-right corner of pupil
    int dotSize = (pw >= 12 && ph >= 12) ? 3 : 2;
    int dotX = px + pw - dotSize - 2;
    int dotY = py + 2;
    if (h > 12) {
      display.fillRect(dotX, dotY, dotSize, dotSize, SSD1306_WHITE);
      if ((glint || pScale > 1.20f) && dotSize >= 3) {
        display.drawPixel(dotX - 1, dotY + dotSize, SSD1306_WHITE);
      }
    }

    maskLids(x0, x1, top, lidIn, lidOut, isLeft);
    maskCheek(cx, bot, w, cheek);
    return;
  }

  // 2. PIXEL 8-BIT (Retro 8-Bit Arcade)
  if (currentEyeSkin == SKIN_PIXEL) {
    if (h <= 3) {
      display.fillRect(x0, cy - 1, w, maxI(2, h), SSD1306_WHITE);
      return;
    }
    if (isHeartMood) {
      drawSkinHeartEye(cx, cy, w, h, true);
      return;
    }
    int step = (w >= 22) ? 4 : 2;
    if (h > step * 2) {
      display.fillRect(x0 + step, top, w - step * 2, h, SSD1306_WHITE);
      display.fillRect(x0, top + step, w, h - step * 2, SSD1306_WHITE);
    } else {
      display.fillRect(x0, top, w, h, SSD1306_WHITE);
    }

    // Stepped 8-bit gaze-tracking pupil cutout
    if (h > 10) {
      float pScale = clampf(faceNow.pupilScale, 0.70f, 1.35f);
      int pw = ((int)lroundf(w * 0.34f * pScale) / 2) * 2;
      int ph = ((int)lroundf(h * 0.42f * pScale) / 2) * 2;
      if (pw < 4) pw = 4;
      if (ph < 4) ph = 4;
      if (pw > w - 8) pw = maxI(4, w - 8);
      if (ph > h - 6) ph = maxI(4, h - 6);

      int shiftX = ((int)lroundf(gazeX * 0.80f) / 2) * 2;
      int shiftY = ((int)lroundf(gazeY * 0.70f) / 2) * 2;
      int px = cx + shiftX - pw / 2;
      int py = cy + shiftY - ph / 2;
      if (px < x0 + step) px = x0 + step;
      if (px + pw > x1 - step) px = x1 - step - pw;
      if (py < top + 2) py = top + 2;
      if (py + ph > bot - 2) py = bot - 2 - ph;

      display.fillRect(px, py, pw, ph, SSD1306_BLACK);
      if (pw >= 6 && ph >= 6) {
        display.fillRect(px + pw - 3, py + 1, 2, 2, SSD1306_WHITE);
      }
    }

    maskLids(x0, x1, top, lidIn, lidOut, isLeft);
    maskCheek(cx, bot, w, cheek);
    return;
  }

  // 3. LUMINA SPARK (Squircle Glint)
  if (currentEyeSkin == SKIN_LUMINA_SPARK) {
    if (h <= 3) {
      display.fillRoundRect(x0, cy - 1, w, maxI(2, h), 1, SSD1306_WHITE);
      return;
    }
    if (isHeartMood) {
      drawSkinHeartEye(cx, cy, w, h, false);
      return;
    }

    fillSquircle(x0, top, w, h, cx, cy);

    // Black square pupil: dynamic scaling + 3D parallax
    float pScale = clampf(faceNow.pupilScale, 0.65f, 1.45f);
    int pupilSide = (int)lroundf(minI(w, h) * 0.38f * pScale);
    if (pupilSide < 3)
      pupilSide = 3;
    if (pupilSide > minI(w, h) - 4)
      pupilSide = minI(w, h) - 4;
    int maxOffX = maxI(0, (w - pupilSide) / 2 - 2);
    int maxOffY = maxI(0, (h - pupilSide) / 2 - 2);
    int shiftX = (int)lroundf(gazeX * 0.85f);
    int shiftY = (int)lroundf(gazeY * 0.80f);
    if (shiftX < -maxOffX)
      shiftX = -maxOffX;
    if (shiftX > maxOffX)
      shiftX = maxOffX;
    if (shiftY < -maxOffY)
      shiftY = -maxOffY;

    int px = cx + shiftX - pupilSide / 2;
    int py = cy + shiftY - pupilSide / 2;
    if (px < x0 + 2)
      px = x0 + 2;
    if (px + pupilSide > x1 - 2)
      px = x1 - 2 - pupilSide;
    if (py < top + 2)
      py = top + 2;
    if (py + pupilSide > bot - 2)
      py = bot - 2 - pupilSide;

    if (h > 6) {
      display.fillRect(px, py, pupilSide, pupilSide, SSD1306_BLACK);

      // Small white square glint inside the pupil (top-right, offset 1px from
      // edges)
      if (pupilSide >= 5) {
        int glintSide = maxI(1, pupilSide / 4);
        int gx = px + pupilSide - 1 - glintSide;
        int gy = py + 1;
        display.fillRect(gx, gy, glintSide, glintSide, SSD1306_WHITE);
      }
    }

    maskLids(x0, x1, top, lidIn, lidOut, isLeft);
    maskCheek(cx, bot, w, cheek);
    return;
  }

  // 6. MECHA: Tall rectangular solid outer eye with floating tall rounded
  // rectangular pupil, 3D parallax inertia physics, and specular glint dot
  // inside (extended upward into unused top space on 128x64 0.96" OLED).
  if (currentEyeSkin == SKIN_MECHA) {
    if (h <= 3) {
      display.fillRoundRect(x0, cy - 1, w, maxI(2, h), 1, SSD1306_WHITE);
      return;
    }
    if (isHeartMood) {
      drawSkinHeartEye(cx, cy, w, h, false);
      return;
    }

    int extraTop = 0;
    if (h > 8) {
      extraTop = (h * 32) / 100;
      if (h < 18) {
        extraTop = (extraTop * (h - 8)) / 10;
      }
      int maxUp = top - 2;
      if (maxUp < 0)
        maxUp = 0;
      if (extraTop > maxUp)
        extraTop = maxUp;
    }
    top -= extraTop;
    h = bot - top;
    int eyeCy = top + h / 2;

    // Outer smooth tall rectangular sclera (distinct tall box with smooth
    // corners)
    int cornerR = (minI(w, h) * 24) / 100;
    if (cornerR < 3)
      cornerR = 3;
    if (cornerR * 2 > h)
      cornerR = h / 2;
    if (cornerR * 2 > w)
      cornerR = w / 2;

    display.fillRoundRect(x0, top, w, h, cornerR, SSD1306_WHITE);

    // Inner floating tall rounded rectangular pupil (scaled with emotional
    // pupil dilation & physics)
    float pScale = clampf(faceNow.pupilScale, 0.65f, 1.40f);
    int pw = (int)lroundf(w * 0.40f * pScale);
    int ph = (int)lroundf(h * 0.60f * pScale);
    if (pw < 6)
      pw = 6;
    if (ph < 6)
      ph = 6;
    if (pw > w - 6)
      pw = w - 6;
    if (ph > h - 6)
      ph = h - 6;

    // Smooth micro-rounded pupil corners (distinctly tall rectangular with
    // soft corners)
    int pr = (minI(pw, ph) * 20) / 100;
    if (pr < 2)
      pr = 2;
    if (pr * 2 > ph)
      pr = ph / 2;
    if (pr * 2 > pw)
      pr = pw / 2;

    // Realistic 3D Parallax Physics: Pupil glides inside sclera with depth
    // shift
    int maxShiftX = maxI(0, (w - pw) / 2 - 2);
    int maxShiftY = maxI(0, (h - ph) / 2 - 2);
    int shiftX = (int)lroundf(gazeX * 0.95f);
    int shiftY = (int)lroundf(gazeY * 0.85f);
    if (shiftX < -maxShiftX)
      shiftX = -maxShiftX;
    if (shiftX > maxShiftX)
      shiftX = maxShiftX;
    if (shiftY < -maxShiftY)
      shiftY = -maxShiftY;
    if (shiftY > maxShiftY)
      shiftY = maxShiftY;

    int px = cx + shiftX - pw / 2;
    int py = eyeCy + shiftY - ph / 2;
    if (px < x0 + 2)
      px = x0 + 2;
    if (px + pw > x1 - 2)
      px = x1 - 2 - pw;
    if (py < top + 2)
      py = top + 2;
    if (py + ph > bot - 2)
      py = bot - 2 - ph;

    if (h > 6 && ph > 2 && pw > 2) {
      display.fillRoundRect(px, py, pw, ph, pr, SSD1306_BLACK);

      // Specular Glint Dot inside the pupil (dynamically shifts with real gaze
      // direction)
      if (pw >= 8 && ph >= 8) {
        int dotSize = (pw >= 12 && ph >= 18) ? 3 : 2;
        int pcx = px + pw / 2;
        int pcy = py + ph / 2;

        // Base highlight offset: friendly upper-right quadrant
        int baseOffX = maxI(1, pw / 5);
        int baseOffY = -maxI(1, ph / 5);

        // Real direction gaze shift: dot moves in the direction the robot
        // looks!
        int dotGazeShiftX = (int)lroundf(gazeX * 0.45f);
        int dotGazeShiftY = (int)lroundf(gazeY * 0.40f);

        int dotX = pcx + baseOffX + dotGazeShiftX - dotSize / 2;
        int dotY = pcy + baseOffY + dotGazeShiftY - dotSize / 2;

        // Safety clamp strictly inside pupil bounds (with 1px margin)
        int minDotX = px + 1;
        int maxDotX = px + pw - dotSize - 1;
        int minDotY = py + 1;
        int maxDotY = py + ph - dotSize - 1;
        if (dotX < minDotX)
          dotX = minDotX;
        if (dotX > maxDotX)
          dotX = maxDotX;
        if (dotY < minDotY)
          dotY = minDotY;
        if (dotY > maxDotY)
          dotY = maxDotY;

        display.fillRect(dotX, dotY, dotSize, dotSize, SSD1306_WHITE);

        // Subtle secondary micro-twinkle when excited or happy (also tracks
        // gaze organically)
        if ((glint || pScale > 1.15f) && pw >= 12 && ph >= 12) {
          int secBaseX = pcx - maxI(2, pw / 4);
          int secBaseY = pcy + maxI(2, ph / 4);
          int secX = secBaseX + (int)lroundf(gazeX * 0.25f);
          int secY = secBaseY + (int)lroundf(gazeY * 0.25f);
          if (secX >= px + 1 && secX <= px + pw - 2 && secY >= py + 1 &&
              secY <= py + ph - 2) {
            display.drawPixel(secX, secY, SSD1306_WHITE);
          }
        }
      }
    }

    maskLids(x0, x1, top, lidIn, lidOut, isLeft);
    maskCheek(cx, bot, w, cheek);
    return;
  }

  // 7. EMO-STYLE: Large solid rounded-rectangular eyes, no pupils, with
  // prominent corner radius
  if (currentEyeSkin == SKIN_EMO) {
    drawEmoEye(cx, cy, w, h, r, lidIn, lidOut, cheek, isLeft, glint);
    return;
  }

  // 8. KIWI+ (Kiwi Evolution): Soft-shouldered plush U-sclera, bright upper
  // rim, deep rounded U-iris with 3D parallax & dual glossy anime catchlights.
  if (currentEyeSkin == SKIN_KIWI_PLUS) {
    if (h <= 3) {
      display.fillRoundRect(x0, cy - 1, w, maxI(2, h), 1, SSD1306_WHITE);
      return;
    }
    if (isHeartMood) {
      drawSkinHeartEye(cx, cy, w, h, false);
      return;
    }

    // Gentle upward expansion for plush fullness when open
    int extraTop = 0;
    if (h >= 20) {
      extraTop = minI(2, maxI(0, top - 2));
    }
    top -= extraTop;
    h = bot - top;

    // Soft curved top shoulders + deep pillowy U-bottom contour
    int rTop = maxI(2, minI(5, w / 7));
    int rBottom = maxI(rTop + 1, minI(13, (minI(w, h) * 42) / 100));
    if (rBottom * 2 > h) rBottom = h / 2;
    if (rBottom * 2 > w) rBottom = w / 2;
    if (rTop * 2 > h) rTop = h / 2;

    int upperH = maxI(rTop * 2, (h * 2) / 3);
    int lowerY = top + h / 4;
    int lowerH = h - h / 4;
    display.fillRoundRect(x0, top, w, upperH, rTop, SSD1306_WHITE);
    display.fillRoundRect(x0, lowerY, w, lowerH, rBottom, SSD1306_WHITE);

    // Oversized plush rounded iris tucked cleanly 1-2px below upper rim
    float pScale = clampf(faceNow.pupilScale, 0.65f, 1.42f);
    int pw = maxI(6, minI((int)lroundf(w * 0.54f * pScale), w - 6));
    int ph = maxI(5, minI((int)lroundf(h * 0.62f * pScale), h - 4));
    int prTop = maxI(2, minI(5, pw / 4));
    int prBot = maxI(prTop, minI(8, (minI(pw, ph) * 45) / 100));
    if (prBot * 2 > ph) prBot = ph / 2;
    if (prBot * 2 > pw) prBot = pw / 2;

    int maxShiftX = maxI(0, (w - pw) / 2 - 2);
    int shiftX = maxI(-maxShiftX, minI(maxShiftX, (int)lroundf(gazeX * 0.85f)));
    int px = cx + shiftX - pw / 2;

    int rimGap = (h >= 18) ? 2 : 1;
    int maxShiftY = maxI(0, h - ph - rimGap - 2);
    int shiftY = maxI(0, minI(maxShiftY, (int)lroundf((gazeY + 1.0f) * 0.70f)));
    int py = top + rimGap + shiftY;
    if (py + ph > bot - 2) py = maxI(top + 1, bot - 2 - ph);

    int pUpperH = maxI(prTop * 2, ph / 2 + 1);
    if (pUpperH > ph) pUpperH = ph;
    int pLowerY = py + ph / 3;
    int pLowerH = ph - ph / 3;
    display.fillRoundRect(px, py, pw, pUpperH, prTop, SSD1306_BLACK);
    display.fillRoundRect(px, pLowerY, pw, pLowerH, prBot, SSD1306_BLACK);

    // Dual liquid-glass anime catchlights (main upper-right pearl + lower-left sparkle)
    if (h > 12 && pw >= 10 && ph >= 10) {
      int dotSize = (pw >= 14 && ph >= 14) ? 4 : 3;
      int dotX = px + pw - dotSize - 2;
      int dotY = py + 2;
      if (dotSize >= 4) {
        display.fillRoundRect(dotX, dotY, 4, 4, 1, SSD1306_WHITE);
      } else {
        display.fillRect(dotX, dotY, dotSize, dotSize, SSD1306_WHITE);
      }

      // Secondary diagonal bounce glint
      if (pw >= 12 && ph >= 13) {
        int secX = px + 3;
        int secY = py + ph - 4;
        if (secX + 1 < dotX && secY > dotY + 2) {
          display.fillRect(secX, secY, 2, 2, SSD1306_WHITE);
        }
      }

      // Excited sparkle wing pixel
      if ((glint || pScale > 1.15f) && dotX > px + 2 && dotY + dotSize + 1 < py + ph) {
        display.drawPixel(dotX - 1, dotY + dotSize, SSD1306_WHITE);
      }
    }

    maskLids(x0, x1, top, lidIn, lidOut, isLeft);
    maskCheek(cx, bot, w, cheek);
    return;
  }

  // DEFAULT FALLBACK: Render Kiwi eyes if unknown skin enum is encountered
  if (h <= 3) {
    display.fillRoundRect(x0, cy - 1, w, maxI(2, h), 1, SSD1306_WHITE);
    return;
  }
  int rBottom = (r > 2) ? r : minI(7, h / 3);
  if (rBottom * 2 > h)
    rBottom = h / 2;
  display.fillRect(x0, top, w, h - rBottom, SSD1306_WHITE);
  display.fillRoundRect(x0, bot - rBottom * 2, w, rBottom * 2, rBottom,
                        SSD1306_WHITE);
  float pScale = clampf(faceNow.pupilScale, 0.65f, 1.45f);
  int pw = (int)lroundf(w * 0.44f * pScale);
  if (pw < 6)
    pw = 6;
  if (pw > w - 4)
    pw = w - 4;
  int ph = (int)lroundf(h * 0.58f * pScale);
  if (ph < 4)
    ph = 4;
  if (ph > h - 2)
    ph = h - 2;
  int pr = minI(4, maxI(2, pw / 4));
  int pupilShiftX = (int)lroundf(gazeX * 0.85f);
  int px = cx + pupilShiftX - pw / 2;
  if (px < x0 + 2)
    px = x0 + 2;
  if (px + pw > x1 - 2)
    px = x1 - 2 - pw;
  int pupilShiftY = (int)lroundf(gazeY * 0.75f);
  int py = top + pupilShiftY;
  if (py < top)
    py = top;
  if (py + ph > bot - 2)
    py = bot - 2 - ph;
  display.fillRect(px, py, pw, ph - pr, SSD1306_BLACK);
  display.fillRoundRect(px, py + ph - pr * 2, pw, pr * 2, pr, SSD1306_BLACK);
  int dotSize = (pw >= 12 && ph >= 12) ? 3 : 2;
  int dotX = px + pw - dotSize - 2;
  int dotY = py + 2;
  if (h > 12) {
    display.fillRect(dotX, dotY, dotSize, dotSize, SSD1306_WHITE);
  }
  maskLids(x0, x1, top, lidIn, lidOut, isLeft);
  maskCheek(cx, bot, w, cheek);
}
