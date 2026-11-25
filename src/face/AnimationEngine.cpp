#include "face/AnimationEngine.h"
#include "config/AppConfig.h"
#include "face/FaceEngine.h"
#include "face/FaceDisplay.h"
#include <Adafruit_SSD1306.h>
#include <esp_heap_caps.h>
#include <math.h>
#include <Preferences.h>

namespace {
constexpr size_t OLED_FRAME_BYTES = SCREEN_WIDTH * SCREEN_HEIGHT / 8;
constexpr uint8_t MAX_CUSTOM_ANIMATIONS = 3;
uint8_t activeAnimation = 0;
uint16_t animationFrame = 0;
uint32_t nextFrameAt = 0;
bool playing = false;
uint8_t *customFrame = nullptr;
uint8_t *customFrames[MAX_CUSTOM_ANIMATIONS] = {};
bool customFrameAvailable[MAX_CUSTOM_ANIMATIONS] = {};
String customAnimationNames[MAX_CUSTOM_ANIMATIONS];

uint8_t *allocOledFrameBuf() {
  uint8_t *p = nullptr;
  if (psramFound()) {
    p = (uint8_t *)heap_caps_malloc(OLED_FRAME_BYTES,
                                    MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  }
  if (!p) {
    p = (uint8_t *)malloc(OLED_FRAME_BYTES);
  }
  return p;
}
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

void drawCustomFrame(uint8_t slot) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  if (slot < MAX_CUSTOM_ANIMATIONS && customFrameAvailable[slot] &&
      customFrames[slot]) {
    display.drawBitmap(0, 0, customFrames[slot], SCREEN_WIDTH, SCREEN_HEIGHT,
                       SSD1306_WHITE);
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
    if (prefs.getBytesLength(frameKey.c_str()) == OLED_FRAME_BYTES) {
      if (!customFrames[i])
        customFrames[i] = allocOledFrameBuf();
      customFrameAvailable[i] =
          customFrames[i] &&
          prefs.getBytes(frameKey.c_str(), customFrames[i], OLED_FRAME_BYTES) ==
              OLED_FRAME_BYTES;
    } else {
      customFrameAvailable[i] = false;
    }
    customAnimationNames[i] = prefs.getString(nameKey.c_str(), "");
    if (customFrameAvailable[i] && customAnimationNames[i].length() == 0)
      customAnimationNames[i] = "Saved Upload " + String(i + 1);
  }
  // Migrate the original single-upload format into the first named slot.
  if (!customFrameAvailable[0] && prefs.getBytesLength("custom") == OLED_FRAME_BYTES) {
    if (!customFrames[0])
      customFrames[0] = allocOledFrameBuf();
    if (customFrames[0] &&
        prefs.getBytes("custom", customFrames[0], OLED_FRAME_BYTES) == OLED_FRAME_BYTES) {
      customFrameAvailable[0] = true;
      customAnimationNames[0] = "Saved Upload";
    }
  }
  prefs.end();
}

bool animationIsActive() {
  return playing;
}

void playAnimation(uint8_t slot) {
  if (!oledOK || slot >= MAX_CUSTOM_ANIMATIONS)
    return;
  if (!customFrameAvailable[slot] || !customFrames[slot])
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
  drawCustomFrame(activeAnimation);
  nextFrameAt = now + 90;
}

String animationListJson() {
  String json = "[";
  bool first = true;
  for (uint8_t i = 0; i < MAX_CUSTOM_ANIMATIONS; i++) {
    if (!customFrameAvailable[i])
      continue;
    if (!first)
      json += ",";
    first = false;
    String safeName = customAnimationNames[i];
    safeName.replace("\\", "\\\\");
    safeName.replace("\"", "\\\"");
    json += "{\"slot\":" + String(i) + ",\"name\":\"" + safeName + "\"}";
  }
  json += "]";
  return json;
}

void beginCustomAnimationUpload(const String &filename, const String &displayName) {
  if (!customFrame) {
    customFrame = allocOledFrameBuf();
  }
  uploadIsText = !filename.endsWith(".bin") && !filename.endsWith(".raw");
  uploadFailed = (customFrame == nullptr);
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
  bool ok = false;
  if (!uploadFailed && uploadBytes == OLED_FRAME_BYTES && customFrame) {
    uint8_t slot = MAX_CUSTOM_ANIMATIONS;
    for (uint8_t i = 0; i < MAX_CUSTOM_ANIMATIONS; i++) {
      if (!customFrameAvailable[i]) {
        slot = i;
        break;
      }
    }
    if (slot < MAX_CUSTOM_ANIMATIONS) {
      Preferences prefs;
      prefs.begin("oledanim", false);
      String frameKey = "frame" + String(slot);
      String nameKey = "name" + String(slot);
      size_t saved = prefs.putBytes(frameKey.c_str(), customFrame, OLED_FRAME_BYTES);
      prefs.putString(nameKey.c_str(), uploadDisplayName);
      prefs.end();
      if (saved == OLED_FRAME_BYTES) {
        if (!customFrames[slot])
          customFrames[slot] = allocOledFrameBuf();
        if (customFrames[slot]) {
          memcpy(customFrames[slot], customFrame, OLED_FRAME_BYTES);
          customFrameAvailable[slot] = true;
          customAnimationNames[slot] = uploadDisplayName;
          ok = true;
        }
      }
    }
  }
  if (customFrame) {
    free(customFrame);
    customFrame = nullptr;
  }
  return ok;
}
