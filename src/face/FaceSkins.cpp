#include "face/FaceSkins.h"
#include "face/FaceDisplay.h"
#include "config/AppConfig.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

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

