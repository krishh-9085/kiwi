#include "animation/AnimationEngine.h"
#include "config/AppConfig.h"
#include "animation/DogAnimationFrames.h"
#include "face/FaceEngine.h"
#include "face/FaceDisplay.h"
#include <Adafruit_SSD1306.h>
#include <math.h>
#include <Preferences.h>

namespace {
constexpr size_t OLED_FRAME_BYTES = SCREEN_WIDTH * SCREEN_HEIGHT / 8;
constexpr uint8_t MAX_CUSTOM_ANIMATIONS = 3;
uint8_t activeAnimation = 0;
uint16_t animationFrame = 0;
uint32_t nextFrameAt = 0;
bool playing = false;
uint8_t customFrame[OLED_FRAME_BYTES];
uint8_t customFrames[MAX_CUSTOM_ANIMATIONS][OLED_FRAME_BYTES];
bool customFrameAvailable[MAX_CUSTOM_ANIMATIONS] = {};
String customAnimationNames[MAX_CUSTOM_ANIMATIONS];
bool uploadIsText = false;
bool uploadFailed = false;
size_t uploadBytes = 0;
uint8_t uploadHexValue = 0;
uint8_t uploadHexDigits = 0;
bool uploadSawZero = false;
bool uploadSawHexPrefix = false;
bool uploadReadingTextBytes = false;
String uploadTextHeader;
String uploadDisplayName;

int hexDigitValue(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  return -1;
}

constexpr uint8_t BUILTIN_ANIMATION_COUNT = 1;

void drawCentered(const char *text, int16_t y, uint8_t size) {
  display.setTextSize(size);
  int16_t x, boundsY;
  uint16_t width, height;
  display.getTextBounds(text, 0, y, &x, &boundsY, &width, &height);
  display.setCursor((SCREEN_WIDTH - width) / 2, y);
  display.print(text);
}

void drawBuiltInFrame(uint8_t animation, uint16_t frame) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  switch (animation) {
  case 0: {
    display.drawBitmap(0, 0, OLED_MAKER_DOG_FRAME, SCREEN_WIDTH, SCREEN_HEIGHT,
                       SSD1306_WHITE);
    break;
  }
  default:
    if (animation > 0 && animation <= MAX_CUSTOM_ANIMATIONS &&
        customFrameAvailable[animation - 1])
      display.drawBitmap(0, 0, customFrames[animation - 1], SCREEN_WIDTH, SCREEN_HEIGHT,
                         SSD1306_WHITE);
    break;
  }
  presentDisplay(true);
}
} // namespace

void initAnimations() {
  Preferences prefs;
  prefs.begin("oledanim", true);
  for (uint8_t i = 0; i < MAX_CUSTOM_ANIMATIONS; i++) {
    String frameKey = "frame" + String(i);
    String nameKey = "name" + String(i);
    customFrameAvailable[i] =
        prefs.getBytesLength(frameKey.c_str()) == OLED_FRAME_BYTES &&
        prefs.getBytes(frameKey.c_str(), customFrames[i], OLED_FRAME_BYTES) ==
            OLED_FRAME_BYTES;
    customAnimationNames[i] = prefs.getString(nameKey.c_str(), "");
    if (customFrameAvailable[i] && customAnimationNames[i].length() == 0)
      customAnimationNames[i] = "Saved Upload " + String(i + 1);
  }
  // Migrate the original single-upload format into the first named slot.
  if (!customFrameAvailable[0] && prefs.getBytesLength("custom") == OLED_FRAME_BYTES &&
      prefs.getBytes("custom", customFrames[0], OLED_FRAME_BYTES) == OLED_FRAME_BYTES) {
    customFrameAvailable[0] = true;
    customAnimationNames[0] = "Saved Upload";
  }
  prefs.end();
}

bool animationIsActive() {
  return playing;
}

void playAnimation(uint8_t slot) {
  if (!oledOK || slot >= BUILTIN_ANIMATION_COUNT + MAX_CUSTOM_ANIMATIONS)
    return;
  if (slot > 0 && !customFrameAvailable[slot - 1])
    return;
  bannerActive = false;
  activeAnimation = slot;
  animationFrame = 0;
  nextFrameAt = 0;
  playing = true;
}

void stopAnimation() {
  playing = false;
}

void tickAnimation(uint32_t now) {
  if (!playing || now < nextFrameAt)
    return;
  drawBuiltInFrame(activeAnimation, animationFrame++);
  nextFrameAt = now + 90;
}

String animationListJson() {
  String json = "[";
  bool first = true;
  json += "{\"slot\":0,\"name\":\"OLED Maker Dog\"}";
  first = false;
  for (uint8_t i = 0; i < MAX_CUSTOM_ANIMATIONS; i++) {
    if (!customFrameAvailable[i])
      continue;
    if (!first)
      json += ",";
    first = false;
    String safeName = customAnimationNames[i];
    safeName.replace("\\", "\\\\");
    safeName.replace("\"", "\\\"");
    json += "{\"slot\":" + String(i + 1) + ",\"name\":\"" + safeName + "\"}";
  }
  json += "]";
  return json;
}

void beginCustomAnimationUpload(const String &filename, const String &displayName) {
  uploadIsText = !filename.endsWith(".bin") && !filename.endsWith(".raw");
  uploadFailed = false;
  uploadBytes = 0;
  uploadHexValue = 0;
  uploadHexDigits = 0;
  uploadSawZero = false;
  uploadSawHexPrefix = false;
  uploadReadingTextBytes = !uploadIsText;
  uploadTextHeader = "";
  String chosenName = displayName;
  chosenName.trim();
  if (chosenName.length() == 0) {
    chosenName = filename;
    int slash = chosenName.lastIndexOf('/');
    if (slash >= 0)
      chosenName = chosenName.substring(slash + 1);
    int dot = chosenName.lastIndexOf('.');
    if (dot > 0)
      chosenName = chosenName.substring(0, dot);
  }
  chosenName.replace("\"", "'");
  if (chosenName.length() > 24)
    chosenName = chosenName.substring(0, 24);
  uploadDisplayName = chosenName;
}

bool writeCustomAnimationUpload(const uint8_t *data, size_t length) {
  if (uploadFailed)
    return false;
  if (!uploadIsText) {
    if (uploadBytes + length > OLED_FRAME_BYTES) {
      uploadFailed = true;
      return false;
    }
    memcpy(customFrame + uploadBytes, data, length);
    uploadBytes += length;
    return true;
  }

  for (size_t i = 0; i < length; i++) {
    char c = (char)data[i];
    if (uploadIsText && !uploadReadingTextBytes) {
      uploadTextHeader += c;
      if (uploadTextHeader.length() > 64)
        uploadTextHeader.remove(0, uploadTextHeader.length() - 64);
      if (uploadTextHeader.indexOf("frame0[1024]") >= 0) {
        uploadReadingTextBytes = true;
        uploadTextHeader = "";
        uploadSawZero = false;
        uploadSawHexPrefix = false;
      }
      continue;
    }
    if (!uploadSawHexPrefix) {
      if (uploadSawZero) {
        if (c == 'x' || c == 'X') {
          uploadSawHexPrefix = true;
          uploadHexValue = 0;
          uploadHexDigits = 0;
        } else {
          uploadSawZero = c == '0';
        }
      } else {
        uploadSawZero = c == '0';
      }
      continue;
    }

    int digit = hexDigitValue(c);
    if (digit < 0) {
      uploadSawZero = false;
      uploadSawHexPrefix = false;
      uploadHexValue = 0;
      uploadHexDigits = 0;
      continue;
    }

    uploadHexValue = (uint8_t)((uploadHexValue << 4) | digit);
    uploadHexDigits++;
    if (uploadHexDigits == 2) {
      if (uploadBytes >= OLED_FRAME_BYTES) {
        uploadFailed = true;
        return false;
      }
      customFrame[uploadBytes++] = uploadHexValue;
      uploadSawZero = false;
      uploadSawHexPrefix = false;
      uploadHexValue = 0;
      uploadHexDigits = 0;
    }
  }
  return true;
}

bool finishCustomAnimationUpload() {
  if (uploadFailed || uploadBytes != OLED_FRAME_BYTES)
    return false;
  uint8_t slot = MAX_CUSTOM_ANIMATIONS;
  for (uint8_t i = 0; i < MAX_CUSTOM_ANIMATIONS; i++) {
    if (!customFrameAvailable[i]) {
      slot = i;
      break;
    }
  }
  if (slot == MAX_CUSTOM_ANIMATIONS)
    return false;
  Preferences prefs;
  prefs.begin("oledanim", false);
  String frameKey = "frame" + String(slot);
  String nameKey = "name" + String(slot);
  size_t saved = prefs.putBytes(frameKey.c_str(), customFrame, OLED_FRAME_BYTES);
  prefs.putString(nameKey.c_str(), uploadDisplayName);
  prefs.end();
  if (saved != OLED_FRAME_BYTES)
    return false;
  memcpy(customFrames[slot], customFrame, OLED_FRAME_BYTES);
  customFrameAvailable[slot] = true;
  customAnimationNames[slot] = uploadDisplayName;
  return true;
}
