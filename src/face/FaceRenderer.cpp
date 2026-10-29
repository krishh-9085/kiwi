#include "face/FaceRenderer.h"
#include "face/FaceDisplay.h"
#include "face/FaceSkins.h"
#include "face/FaceExpressions.h"
#include "face/FaceIdle.h"
#include "config/AppConfig.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

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
  case EMOTION_THINKING: {
    // Human cognitive recall: looking UPWARD while pondering with gentle left/right scan
    float thinkPhase = now / 750.0f;
    tx = sinf(thinkPhase) * 5.5f + cosf(thinkPhase * 1.8f) * 1.6f;
    ty = -6.5f + sinf(thinkPhase * 1.3f) * 1.2f;
    break;
  }
  case EMOTION_LISTENING:
    tx = sinf(now / 500.0f) * 2.0f;
    ty = -3.0f;
    break;
  case EMOTION_SPEAKING: {
    // Human conversational engagement: looking at user, fluidly glancing left/right while speaking
    float spkPhase = now / 580.0f;
    tx = sinf(spkPhase) * 6.2f + sinf(spkPhase * 2.1f) * 2.0f;
    ty = sinf(spkPhase * 1.4f) * 1.6f;
    break;
  }
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
  case EMOTION_SPEAKING:
    // Subtle conversational speech cadence and head bounce
    offY = sinf(now / 150.0f) * 1.2f;
    sc = 1.0f + 0.025f * sinf(now / 210.0f);
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
  } else if (showMouth && faceNow.mouthW > 3) {
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

