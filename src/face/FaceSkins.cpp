#include "face/FaceSkins.h"
#include "config/AppConfig.h"
#include "face/FaceDisplay.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include <math.h>


static inline int minI(int a, int b) { return a < b ? a : b; }
static inline int maxI(int a, int b) { return a > b ? a : b; }

// Precomputed 32-point unit squircle table (n = 4 superellipse: |cos t|^(1/2), |sin t|^(1/2))
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

static void computeSquirclePoints(int cx, int cy, int w, int h, int ptsX[32], int ptsY[32]) {
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
    if (rad < 1) rad = 1;
    display.fillRoundRect(x0, top, w, h, rad, SSD1306_WHITE);
    return;
  }
  int ptsX[32];
  int ptsY[32];
  computeSquirclePoints(cx, cy, w, h, ptsX, ptsY);

  int minY = ptsY[0], maxY = ptsY[0];
  for (int i = 1; i < 32; i++) {
    if (ptsY[i] < minY) minY = ptsY[i];
    if (ptsY[i] > maxY) maxY = ptsY[i];
  }
  if (minY < top) minY = top;
  if (maxY > top + h - 1) maxY = top + h - 1;

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
          if (lx < minX) minX = lx;
          if (rx > maxX) maxX = rx;
        }
        continue;
      }
      if ((y1 <= y && y <= y2) || (y2 <= y && y <= y1)) {
        int xCross = x1 + (int)((int64_t)(y - y1) * (x2 - x1) / (y2 - y1));
        if (xCross < minX) minX = xCross;
        if (xCross > maxX) maxX = xCross;
      }
    }
    if (minX <= maxX) {
      if (minX < x0) minX = x0;
      if (maxX > x0 + w - 1) maxX = x0 + w - 1;
      display.drawFastHLine(minX, y, maxX - minX + 1, SSD1306_WHITE);
    }
  }
}

static void drawSquircleOutline(int x0, int top, int w, int h, int cx, int cy) {
  if (w < 12 || h < 10) {
    int rad = minI(w, h) / 3;
    if (rad < 1) rad = 1;
    display.drawRoundRect(x0, top, w, h, rad, SSD1306_WHITE);
    return;
  }
  int ptsX[32];
  int ptsY[32];
  computeSquirclePoints(cx, cy, w, h, ptsX, ptsY);

  for (int i = 0; i < 32; i++) {
    int j = (i + 1) % 32;
    display.drawLine(ptsX[i], ptsY[i], ptsX[j], ptsY[j], SSD1306_WHITE);
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

void drawEye(int cx, int cy, int w, int h, int r, int lidIn, int lidOut,
             int cheek, bool isLeft, bool glint) {
  int x0 = cx - w / 2;
  int x1 = x0 + w;
  int top = cy - h / 2;
  int bot = top + h;

  // 1. KIWI EYES (Desk Buddy): Flat upper edge, rounded bottom base,
  // U-shaped deep pupil cutout with floating top-right catchlight glint dot.
  if (currentEyeSkin == SKIN_KIWI) {
    if (h <= 3) {
      display.fillRoundRect(x0, cy - 1, w, maxI(2, h), 1, SSD1306_WHITE);
      return;
    }

    int rBottom = (r > 2) ? r : minI(7, h / 3);
    if (rBottom * 2 > h)
      rBottom = h / 2;

    // Outer sclera: flat top edge, rounded lower edge
    display.fillRect(x0, top, w, h - rBottom, SSD1306_WHITE);
    display.fillRoundRect(x0, bot - rBottom * 2, w, rBottom * 2, rBottom,
                          SSD1306_WHITE);

    // Dynamic pupil cutout (proportional to eye size and reacting to gaze)
    int pw = (int)(w * 0.44f);
    if (pw < 6)
      pw = 6;
    int ph = (int)(h * 0.58f);
    if (ph < 4)
      ph = 4;
    int pr = minI(4, maxI(2, pw / 4));

    int pupilShift = (int)lroundf(gazeX * 0.45f);
    int px = cx + pupilShift - pw / 2;
    if (px < x0 + 3)
      px = x0 + 3;
    if (px + pw > x1 - 3)
      px = x1 - 3 - pw;

    int py = top;
    if (gazeY > 1.5f && h > 16) {
      py += minI(4, (int)(gazeY * 0.4f));
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

    maskLids(x0, x1, top, lidIn, lidOut, isLeft);
    maskCheek(cx, bot, w, cheek);
    return;
  }

  // 2. VECTOR CLASSIC (Default Vector): Clean Round Vector Eyes
  if (currentEyeSkin == SKIN_VECTOR) {
    display.fillRoundRect(x0, top, w, h, r, SSD1306_WHITE);
    maskLids(x0, x1, top, lidIn, lidOut, isLeft);
    maskCheek(cx, bot, w, cheek);
    if (glint && h > 20) {
      display.fillRect(x0 + w / 5, top + h / 5, 4, 4, SSD1306_BLACK);
    }
    return;
  }

  // 3. NEON CAPSULE (User requested LED stadium pill with inner glint block)
  if (currentEyeSkin == SKIN_CAPSULE) {
    if (h <= 3) {
      display.fillRoundRect(x0, cy - 1, w, maxI(2, h), 1, SSD1306_WHITE);
      return;
    }

    int cornerR = w / 2;
    if (cornerR * 2 > h)
      cornerR = h / 2;
    if (cornerR < 2)
      cornerR = 2;

    int borderThick = 3;
    if (w < 14 || h < 14)
      borderThick = 2;

    // Outer filled stadium pill
    display.fillRoundRect(x0, top, w, h, cornerR, SSD1306_WHITE);

    // Inner hollow black cavity
    int innerW = w - borderThick * 2;
    int innerH = h - borderThick * 2;
    int innerR = cornerR - borderThick;
    if (innerR < 1) innerR = 1;
    if (innerW > 0 && innerH > 0) {
      display.fillRoundRect(x0 + borderThick, top + borderThick, innerW, innerH, innerR, SSD1306_BLACK);

      // Inner upper glint block (solid white square/rect pupil as seen in user image)
      if (innerH >= 6) {
        int gw = maxI(3, (int)(innerW * 0.40f));
        int gh = maxI(4, (int)(innerH * 0.32f));
        int gr = 1;

        // Base anchor: top-right inside cavity
        int gxBase = (x0 + borderThick + innerW) - gw - 1;
        int gyBase = top + borderThick + maxI(2, innerH / 6);

        // Gaze shift
        int maxShiftX = maxI(0, (innerW - gw) / 2);
        int maxShiftY = maxI(0, (innerH - gh) / 3);
        int shiftX = (int)lroundf(gazeX * 0.35f);
        int shiftY = (int)lroundf(gazeY * 0.35f);
        if (shiftX < -maxShiftX) shiftX = -maxShiftX;
        if (shiftX > maxShiftX) shiftX = maxShiftX;
        if (shiftY < -maxShiftY) shiftY = -maxShiftY;

        int gx = gxBase + shiftX;
        int gy = gyBase + shiftY;

        // Keep inside black cavity
        if (gx < x0 + borderThick + 1) gx = x0 + borderThick + 1;
        if (gx + gw > x0 + w - borderThick - 1) gx = x0 + w - borderThick - 1 - gw;
        if (gy < top + borderThick + 1) gy = top + borderThick + 1;
        if (gy + gh > top + h - borderThick - 1) gy = top + h - borderThick - 1 - gh;

        display.fillRoundRect(gx, gy, gw, gh, gr, SSD1306_WHITE);
      }
    }

    maskLids(x0, x1, top, lidIn, lidOut, isLeft);
    maskCheek(cx, bot, w, cheek);
    if (lidIn > 0 || lidOut > 0) {
      int lL = isLeft ? lidOut : lidIn;
      int lR = isLeft ? lidIn : lidOut;
      display.drawLine(x0, top + lL, x1, top + lR, SSD1306_WHITE);
    }
    return;
  }

  // 4. PIXEL 8-BIT (Retro 8-Bit Arcade)
  if (currentEyeSkin == SKIN_PIXEL) {
    int step = (w >= 22) ? 4 : 2;
    if (h > step * 2) {
      display.fillRect(x0 + step, top, w - step * 2, h, SSD1306_WHITE);
      display.fillRect(x0, top + step, w, h - step * 2, SSD1306_WHITE);
    } else {
      display.fillRect(x0, top, w, h, SSD1306_WHITE);
    }
    maskLids(x0, x1, top, lidIn, lidOut, isLeft);
    maskCheek(cx, bot, w, cheek);
    if (glint && h > 16) {
      display.fillRect(x0 + 4, top + 4, 3, 3, SSD1306_BLACK);
      display.fillRect(x0 + 8, top + 8, 2, 2, SSD1306_BLACK);
    }
    return;
  }

  // 5. SQUIRCLE BOLD
  if (currentEyeSkin == SKIN_SQUIRCLE_BOLD) {
    if (h <= 3) {
      display.fillRoundRect(x0, cy - 1, w, maxI(2, h), 1, SSD1306_WHITE);
      return;
    }

    fillSquircle(x0, top, w, h, cx, cy);

    // Black square pupil: side = min(w, h) * 0.38
    int pupilSide = (int)lroundf(minI(w, h) * 0.38f);
    if (pupilSide < 3)
      pupilSide = 3;
    int maxOffX = maxI(0, (w - pupilSide) / 2 - 2);
    int maxOffY = maxI(0, (h - pupilSide) / 2 - 2);
    int shiftX = (int)lroundf(gazeX * 0.45f);
    int shiftY = (int)lroundf(gazeY * 0.45f);
    if (shiftX < -maxOffX) shiftX = -maxOffX;
    if (shiftX > maxOffX) shiftX = maxOffX;
    if (shiftY < -maxOffY) shiftY = -maxOffY;

    int px = cx + shiftX - pupilSide / 2;
    int py = cy + shiftY - pupilSide / 2;
    if (px < x0 + 2) px = x0 + 2;
    if (px + pupilSide > x1 - 2) px = x1 - 2 - pupilSide;
    if (py < top + 2) py = top + 2;
    if (py + pupilSide > bot - 2) py = bot - 2 - pupilSide;

    if (h > 6) {
      display.fillRect(px, py, pupilSide, pupilSide, SSD1306_BLACK);
    }

    maskLids(x0, x1, top, lidIn, lidOut, isLeft);
    maskCheek(cx, bot, w, cheek);
    return;
  }

  // 6. NEON FRAME (Squircle Outline)
  if (currentEyeSkin == SKIN_NEON_FRAME) {
    if (h <= 3) {
      display.drawFastHLine(x0, cy, w, SSD1306_WHITE);
      return;
    }

    drawSquircleOutline(x0, top, w, h, cx, cy);

    // Solid white circular pupil: radius = min(w, h) * 0.18
    int pupilR = (int)lroundf(minI(w, h) * 0.18f);
    if (pupilR < 1)
      pupilR = 1;
    int maxOffX = maxI(0, w / 2 - pupilR - 2);
    int maxOffY = maxI(0, h / 2 - pupilR - 2);
    int shiftX = (int)lroundf(gazeX * 0.45f);
    int shiftY = (int)lroundf(gazeY * 0.45f);
    if (shiftX < -maxOffX) shiftX = -maxOffX;
    if (shiftX > maxOffX) shiftX = maxOffX;
    if (shiftY < -maxOffY) shiftY = -maxOffY;

    int px = cx + shiftX;
    int py = cy + shiftY;
    if (px - pupilR < x0 + 2) px = x0 + 2 + pupilR;
    if (px + pupilR > x1 - 2) px = x1 - 2 - pupilR;
    if (py - pupilR < top + 2) py = top + 2 + pupilR;
    if (py + pupilR > bot - 2) py = bot - 2 - pupilR;

    if (h > 6) {
      display.fillCircle(px, py, pupilR, SSD1306_WHITE);
    }

    maskLids(x0, x1, top, lidIn, lidOut, isLeft);
    maskCheek(cx, bot, w, cheek);
    if (lidIn > 0 || lidOut > 0) {
      int lL = isLeft ? lidOut : lidIn;
      int lR = isLeft ? lidIn : lidOut;
      display.drawLine(x0, top + lL, x1, top + lR, SSD1306_WHITE);
    }
    return;
  }

  // 7. LUMINA SPARK (Squircle Glint)
  if (currentEyeSkin == SKIN_LUMINA_SPARK) {
    if (h <= 3) {
      display.fillRoundRect(x0, cy - 1, w, maxI(2, h), 1, SSD1306_WHITE);
      return;
    }

    fillSquircle(x0, top, w, h, cx, cy);

    // Black square pupil: side = min(w, h) * 0.38
    int pupilSide = (int)lroundf(minI(w, h) * 0.38f);
    if (pupilSide < 3)
      pupilSide = 3;
    int maxOffX = maxI(0, (w - pupilSide) / 2 - 2);
    int maxOffY = maxI(0, (h - pupilSide) / 2 - 2);
    int shiftX = (int)lroundf(gazeX * 0.45f);
    int shiftY = (int)lroundf(gazeY * 0.45f);
    if (shiftX < -maxOffX) shiftX = -maxOffX;
    if (shiftX > maxOffX) shiftX = maxOffX;
    if (shiftY < -maxOffY) shiftY = -maxOffY;

    int px = cx + shiftX - pupilSide / 2;
    int py = cy + shiftY - pupilSide / 2;
    if (px < x0 + 2) px = x0 + 2;
    if (px + pupilSide > x1 - 2) px = x1 - 2 - pupilSide;
    if (py < top + 2) py = top + 2;
    if (py + pupilSide > bot - 2) py = bot - 2 - pupilSide;

    if (h > 6) {
      display.fillRect(px, py, pupilSide, pupilSide, SSD1306_BLACK);

      // Small white square glint inside the pupil (top-right, offset 1px from edges)
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

  // DEFAULT FALLBACK: Render Kiwi eyes if unknown skin enum is encountered
  if (h <= 3) {
    display.fillRoundRect(x0, cy - 1, w, maxI(2, h), 1, SSD1306_WHITE);
    return;
  }
  int rBottom = (r > 2) ? r : minI(7, h / 3);
  if (rBottom * 2 > h)
    rBottom = h / 2;
  display.fillRect(x0, top, w, h - rBottom, SSD1306_WHITE);
  display.fillRoundRect(x0, bot - rBottom * 2, w, rBottom * 2, rBottom, SSD1306_WHITE);
  int pw = (int)(w * 0.44f);
  if (pw < 6) pw = 6;
  int ph = (int)(h * 0.58f);
  if (ph < 4) ph = 4;
  int pr = minI(4, maxI(2, pw / 4));
  int px = cx - pw / 2;
  if (px < x0 + 3) px = x0 + 3;
  if (px + pw > x1 - 3) px = x1 - 3 - pw;
  int py = top;
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
