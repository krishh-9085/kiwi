#include "face/FaceScreens.h"
#include "face/FaceDisplay.h"
#include "face/FaceExpressions.h"
#include "config/AppConfig.h"
#include "behavior/InteractionManager.h"
#include "hardware/AudioEngine.h"
#include "hardware/TouchController.h"
#include "animation/AnimationEngine.h"
#include <Wire.h>
#include <WiFi.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

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

  // Check if current time falls in Night Schedule
  bool isNightTime = false;
  if (circadianSleepEnabled) {
    int curHour = timeinfo.tm_hour;
    if (circadianSleepStart > circadianSleepEnd) {
      isNightTime = (curHour >= circadianSleepStart || curHour < circadianSleepEnd);
    } else {
      isNightTime = (curHour >= circadianSleepStart && curHour < circadianSleepEnd);
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

  // Anti Burn-in pixel shifting: shifts clock position by 1-2 pixels every minute
  int8_t shiftX = (timeinfo.tm_min % 5) - 2; // -2, -1, 0, +1, +2 px
  int8_t shiftY = ((timeinfo.tm_min / 5) % 3) - 1; // -1, 0, +1 px

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
  display.setCursor((SCREEN_WIDTH - tw) / 2 + shiftX, 6 + shiftY);
  display.print(timeStr);
  display.setTextSize(1);
  display.getTextBounds(ampm, 0, 0, &x1, &y1, &tw, &th);
  display.setCursor((SCREEN_WIDTH - tw) / 2 + shiftX, 34 + shiftY);
  display.print(ampm);
  const char *days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                          "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  char dateStr[32];
  snprintf(dateStr, sizeof(dateStr), "%s %02d %s %04d", days[timeinfo.tm_wday],
           timeinfo.tm_mday, months[timeinfo.tm_mon], timeinfo.tm_year + 1900);
  display.getTextBounds(dateStr, 0, 0, &x1, &y1, &tw, &th);
  display.setCursor((SCREEN_WIDTH - tw) / 2 + shiftX, 50 + shiftY);
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

