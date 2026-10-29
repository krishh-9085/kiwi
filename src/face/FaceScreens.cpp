#include "face/FaceScreens.h"
#include "animation/AnimationEngine.h"
#include "behavior/InteractionManager.h"
#include "config/AppConfig.h"
#include "face/FaceDisplay.h"
#include "face/FaceExpressions.h"
#include "hardware/AudioEngine.h"
#include "hardware/TouchController.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <Wire.h>
#include <math.h>
#include <time.h>


// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static inline float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
static inline float smooth01(float t) {
  t = clamp01(t);
  return t * t * (3.0f - 2.0f * t);
}

// Trims a string with ".." so it never runs off the 128px panel.
static String fitText(const String &in, int maxPx) {
  String s = in;
  if (s.length() * 6 <= (unsigned)maxPx)
    return s;
  int maxChars = maxPx / 6 - 2;
  if (maxChars < 1)
    maxChars = 1;
  return s.substring(0, maxChars) + "..";
}

static void printCentered(const String &s, int y, int shiftX = 0) {
  int16_t x1, y1;
  uint16_t tw, th;
  display.getTextBounds(s, 0, 0, &x1, &y1, &tw, &th);
  display.setCursor((SCREEN_WIDTH - (int)tw) / 2 + shiftX, y);
  display.print(s);
}

// ---------------------------------------------------------------------------
// Clock
// ---------------------------------------------------------------------------
void drawClockScreen() {
  if (!oledOK)
    return;
  // Non-blocking time read (getLocalTime(..) would stall the UI while unsynced)
  struct tm timeinfo;
  time_t nowT = time(nullptr);
  localtime_r(&nowT, &timeinfo);
  bool synced = timeinfo.tm_year > (2016 - 1900);

  if (!synced) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    printCentered("Time not synced", 24);
    // Animated dots so it doesn't look frozen
    int dots = (millis() / 400) % 4;
    for (int i = 0; i < dots; i++)
      display.fillCircle(52 + i * 8, 40, 1, SSD1306_WHITE);
    presentDisplay();
    return;
  }

  // Check if current time falls in Night Schedule
  bool isNightTime = false;
  if (circadianSleepEnabled) {
    int curHour = timeinfo.tm_hour;
    if (circadianSleepStart > circadianSleepEnd) {
      isNightTime =
          (curHour >= circadianSleepStart || curHour < circadianSleepEnd);
    } else {
      isNightTime =
          (curHour >= circadianSleepStart && curHour < circadianSleepEnd);
    }
  }

  // Apply night mode to clock screen
  if (isNightTime) {
    if (nightMode == NIGHT_SCREEN_OFF) {
      setDisplayPower(false);
      return;
    } else if (nightMode == NIGHT_DIM) {
      setDisplayPower(true);
      setDisplayBrightness((uint8_t)nightBrightness);
    } else {
      setDisplayPower(true);
      setDisplayBrightness((uint8_t)displayBrightness);
    }
  } else {
    setDisplayPower(true);
    setDisplayBrightness((uint8_t)displayBrightness);
  }

  // Anti Burn-in pixel shifting: shifts clock position by 1-2 pixels every
  // minute
  int8_t shiftX = (timeinfo.tm_min % 5) - 2;       // -2, -1, 0, +1, +2 px
  int8_t shiftY = ((timeinfo.tm_min / 5) % 3) - 1; // -1, 0, +1 px

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  int hour12 = timeinfo.tm_hour % 12;
  if (hour12 == 0)
    hour12 = 12;
  const char *ampm = (timeinfo.tm_hour >= 12) ? "PM" : "AM";

  // Blinking colon (a space has the same width as ':' so nothing shifts)
  bool colonOn = (timeinfo.tm_sec & 1) == 0;
  char timeStr[8];
  snprintf(timeStr, sizeof(timeStr), colonOn ? "%d:%02d" : "%d %02d", hour12,
           timeinfo.tm_min);
  display.setTextSize(3);
  printCentered(String(timeStr), 6 + shiftY, shiftX);
  display.setTextSize(1);
  printCentered(String(ampm), 34 + shiftY, shiftX);

  const char *days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                          "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  char dateStr[32];
  snprintf(dateStr, sizeof(dateStr), "%s %02d %s %04d",
           days[timeinfo.tm_wday % 7], timeinfo.tm_mday,
           months[timeinfo.tm_mon % 12], timeinfo.tm_year + 1900);
  printCentered(String(dateStr), 50 + shiftY, shiftX);

  // Hairline seconds progress along the bottom edge
  int barW = (timeinfo.tm_sec * SCREEN_WIDTH) / 60 + 1;
  display.drawFastHLine(0, SCREEN_HEIGHT - 1, barW, SSD1306_WHITE);

  presentDisplay();
}

// ---------------------------------------------------------------------------
// Weather
// ---------------------------------------------------------------------------
void drawWeatherScreen() {
  if (!oledOK)
    return;
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // If weather hasn't loaded yet
  if (lastWeatherFetchAt == 0 || weatherCode < 0 || weatherDesc.length() == 0) {
    display.setTextSize(1);
    printCentered(fitText(weatherCity, 124), 8);
    display.drawFastHLine(14, 20, SCREEN_WIDTH - 28, SSD1306_WHITE);
    printCentered("Fetching live weather...", 32);

    // Smooth eased ping-pong loading bar
    display.drawRoundRect(20, 48, 88, 8, 3, SSD1306_WHITE);
    float ph = (float)(millis() % 1600) / 1600.0f;
    float tri = (ph < 0.5f) ? ph * 2.0f : (1.0f - ph) * 2.0f;
    int bx = 22 + (int)lroundf(smooth01(tri) * 70.0f);
    display.fillRoundRect(bx, 50, 14, 4, 2, SSD1306_WHITE);
    presentDisplay();
    return;
  }

  // Header: City Name
  display.setTextSize(1);
  printCentered(fitText(weatherCity, 124), 2);
  display.drawFastHLine(10, 12, SCREEN_WIDTH - 20, SSD1306_WHITE);

  // Center: Temperature with a proper degree ring: "23" + o + "C"
  char numStr[12];
  snprintf(numStr, sizeof(numStr), "%d", (int)lroundf(weatherTemp));
  display.setTextSize(2);
  int16_t x1, y1;
  uint16_t tw, th;
  display.getTextBounds(numStr, 0, 0, &x1, &y1, &tw, &th);
  int totalW = (int)tw + 6 + 12; // number + ring gap + 'C' (12px at size 2)
  int sx = (SCREEN_WIDTH - totalW) / 2;
  display.setCursor(sx, 18);
  display.print(numStr);
  display.drawCircle(sx + (int)tw + 3, 20, 2, SSD1306_WHITE);
  display.setCursor(sx + (int)tw + 7, 18);
  display.print("C");

  // Subtitle: Condition
  display.setTextSize(1);
  printCentered(fitText(weatherDesc, 124), 38);

  // Footer: Humidity & Wind
  char footStr[32];
  snprintf(footStr, sizeof(footStr), "H:%d%%  W:%.0fkm/h", weatherHumidity,
           weatherWind);
  printCentered(String(footStr), 52);

  presentDisplay();
}

// ---------------------------------------------------------------------------
// Banner (marquee)
// ---------------------------------------------------------------------------
void drawBannerScreen() {
  if (!oledOK || !bannerActive || bannerText.length() == 0)
    return;

  // Time-based scrolling: constant speed no matter how long a frame took
  static float posF = 0.0f;
  static int lastInt = -100000;
  static uint32_t lastT = 0;
  uint32_t now = millis();
  if ((int)bannerScrollX != lastInt || lastT == 0 || (now - lastT) > 500) {
    posF = (float)bannerScrollX; // text was (re)started elsewhere
    lastT = now;
  }
  float dt = (float)(now - lastT) / 1000.0f;
  lastT = now;
  if (dt > 0.1f)
    dt = 0.1f;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // Marquee scrolling text only
  display.setTextSize(2);
  display.setTextWrap(false);
  int16_t x1, y1;
  uint16_t tw, th;
  display.getTextBounds(bannerText, 0, 0, &x1, &y1, &tw, &th);
  display.setCursor((int)lroundf(posF), 24);
  display.print(bannerText);
  display.setTextWrap(true);

  presentDisplay();

  // Advance scroll (~90 px/s)
  posF -= 90.0f * dt;
  if (posF < -(float)tw - 20.0f) {
    posF = (float)(SCREEN_WIDTH + 10);
    bannerLoops++;
    if (bannerLoops >= 4) {
      bannerActive = false;
      setEmotion(EMOTION_IDLE);
    }
  }
  bannerScrollX = (int)lroundf(posF);
  lastInt = (int)bannerScrollX;
}

// ---------------------------------------------------------------------------
// Error / status / boot screens
// ---------------------------------------------------------------------------
void showErrorFace(uint32_t ms) {
  setEmotion(EMOTION_ERROR);
  soundError();
  errorFaceUntil = millis() + ms + 300;
  uint32_t t0 = millis();
  while (millis() - t0 < ms) {
    if (cancelRequest)
      break;
    pumpUI();
    delay(2);
  }
}

static void drawStatusHeader(const char *badge, bool inverted, int badgeX,
                             int badgeW, int textX) {
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(4, 2);
  display.print("KIWI OS");
  if (inverted) {
    display.fillRoundRect(badgeX, 0, badgeW, 10, 2, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
    display.setCursor(textX, 1);
    display.print(badge);
    display.setTextColor(SSD1306_WHITE);
  } else {
    display.drawRoundRect(badgeX, 0, badgeW, 10, 2, SSD1306_WHITE);
    display.setCursor(textX, 1);
    display.print(badge);
  }
  display.drawFastHLine(0, 12, 128, SSD1306_WHITE);
}

void drawMessage(const char *l1, const char *l2) {
  if (!oledOK)
    return;
  bootInfoUntil = 0;
  display.clearDisplay();

  // Top Status Bar: KIWI OS . [STATUS]
  drawStatusHeader("STATUS", false, 84, 44, 89);

  // High-Tech Rounded Frame Card
  display.drawRoundRect(0, 15, 128, 48, 4, SSD1306_WHITE);
  display.setCursor(8, 25);
  display.print(fitText(String(l1 ? l1 : ""), 112));
  if (l2 && strlen(l2)) {
    display.setCursor(8, 41);
    display.print(fitText(String(l2), 112));
  }
  presentDisplay(true);
}

void drawConnectingFrame(const char *targetSSID, int frame) {
  if (!oledOK)
    return;
  bootInfoUntil = 0;
  display.clearDisplay();

  // Top Status Rail: KIWI OS . [SEARCH]
  drawStatusHeader("SEARCH", false, 84, 44, 88);

  // Center Animated Wi-Fi Radar Broadcast Pulse (cx = 64, cy = 29)
  int cx = 64;
  int cy = 29;
  display.fillCircle(cx, cy, 2, SSD1306_WHITE);

  int phase = (frame / 2) % 4; // Cycles 0, 1, 2, 3
  if (phase >= 1) {
    for (int a = -45; a <= 45; a += 5) {
      float r = (a - 90) * 0.0174533f;
      display.drawPixel(cx + (int)lroundf(cosf(r) * 6),
                        cy + (int)lroundf(sinf(r) * 6), SSD1306_WHITE);
    }
  }
  if (phase >= 2) {
    for (int a = -50; a <= 50; a += 4) {
      float r = (a - 90) * 0.0174533f;
      display.drawPixel(cx + (int)lroundf(cosf(r) * 11),
                        cy + (int)lroundf(sinf(r) * 11), SSD1306_WHITE);
    }
  }
  if (phase >= 3) {
    for (int a = -55; a <= 55; a += 3) {
      float r = (a - 90) * 0.0174533f;
      display.drawPixel(cx + (int)lroundf(cosf(r) * 16),
                        cy + (int)lroundf(sinf(r) * 16), SSD1306_WHITE);
    }
  }

  // Network SSID Label (Centered)
  String s =
      (targetSSID && strlen(targetSSID)) ? String(targetSSID) : String("Wi-Fi");
  if (s.length() > 16)
    s = s.substring(0, 14) + "..";
  int sLen = s.length() * 6;
  int sx = (128 - sLen) / 2;
  display.setCursor(sx > 2 ? sx : 2, 38);
  display.print(s);

  // Bottom Sleek Laser Scanner Progress Bar (x=14 to 114), eased ping-pong
  display.drawRoundRect(14, 52, 100, 7, 3, SSD1306_WHITE);
  float ph = (float)(frame % 18) / 18.0f;
  float tri = (ph < 0.5f) ? ph * 2.0f : (1.0f - ph) * 2.0f;
  int bx = (int)lroundf(smooth01(tri) * 72.0f);
  display.fillRoundRect(16 + bx, 54, 18, 3, 1, SSD1306_WHITE);

  presentDisplay(true);
}

static void drawConnectedBase(const char *title, float checkProgress) {
  display.clearDisplay();

  // Top Status Bar: KIWI OS . [ONLINE]
  drawStatusHeader("ONLINE", true, 78, 50, 83);

  // Center Checkmark Rings
  display.drawCircle(64, 30, 13, SSD1306_WHITE);
  display.drawCircle(64, 30, 11, SSD1306_WHITE);

  // Checkmark is drawn progressively: (58,30)->(62,34)->(71,25)
  float p = clamp01(checkProgress);
  float seg1 = clamp01(p / 0.31f);
  float seg2 = clamp01((p - 0.31f) / 0.69f);
  if (seg1 > 0.0f) {
    int ex = 58 + (int)lroundf(4.0f * seg1);
    int ey = 30 + (int)lroundf(4.0f * seg1);
    display.drawLine(58, 30, ex, ey, SSD1306_WHITE);
    display.drawLine(58, 31, ex, ey + 1, SSD1306_WHITE);
  }
  if (seg2 > 0.0f) {
    int ex = 62 + (int)lroundf(9.0f * seg2);
    int ey = 34 - (int)lroundf(9.0f * seg2);
    display.drawLine(62, 34, ex, ey, SSD1306_WHITE);
    display.drawLine(62, 35, ex, ey + 1, SSD1306_WHITE);
  }

  // Network name below
  String s =
      (title && strlen(title)) ? String(title) : String("Wi-Fi Connected");
  if (s.length() > 18)
    s = s.substring(0, 16) + "..";
  int sLen = s.length() * 6;
  int sx = (128 - sLen) / 2;
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(sx > 2 ? sx : 2, 50);
  display.print(s);
}

void drawConnectedNotice(const char *title, uint32_t holdMs) {
  if (!oledOK)
    return;
  // Quick draw-in of the checkmark (~200 ms), then hold
  const int STEPS = 8;
  for (int i = 1; i <= STEPS; i++) {
    drawConnectedBase(title, smooth01((float)i / (float)STEPS));
    presentDisplay(true);
    delay(24);
  }
  if (holdMs > 200)
    delay(holdMs - 200);
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
    delay(2);
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

  presentDisplay(true);
  bootInfoUntil = millis() + holdMs;
}