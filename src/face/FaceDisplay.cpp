#include "face/FaceDisplay.h"
#include "config/AppConfig.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>


// ---- Tunables (override in AppConfig.h or build flags) ---------------------
// I2C clock used while pushing a frame. 400000 is the safe default; many
// SSD1306 panels also work at 800000 or 1000000 for a higher frame rate.
#ifndef FACE_I2C_CLK_DURING
#define FACE_I2C_CLK_DURING 400000UL
#endif
#ifndef FACE_I2C_CLK_AFTER
#define FACE_I2C_CLK_AFTER 100000UL
#endif
// SSD1306 oscillator / clock divide (0xF0 = fastest refresh, 0x80 = library
// default). A faster internal refresh reduces flicker and motion shimmer.
#ifndef FACE_OLED_OSC
#define FACE_OLED_OSC 0xF0
#endif

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1,
                         FACE_I2C_CLK_DURING, FACE_I2C_CLK_AFTER);
bool oledOK = false;
bool displayPowerState = true;
uint8_t currentAppliedBrightness = 80;

static bool brightnessKnown = false;
static uint32_t brightnessSentAt = 0;
static bool powerKnown = false;
static uint32_t powerSentAt = 0;

static uint32_t lastFrameHash = 0;
static bool frameHashValid = false;
static uint32_t lastPresentAt = 0;

void markDisplayDirty() { frameHashValid = false; }

bool presentDisplay(bool force) {
  if (!oledOK)
    return false;
  uint32_t now = millis();
  uint8_t *buf = display.getBuffer();
  const size_t n = (size_t)SCREEN_WIDTH * (size_t)SCREEN_HEIGHT / 8;
  uint32_t hsh = 2166136261u; // FNV-1a
  for (size_t i = 0; i < n; i++) {
    hsh ^= buf[i];
    hsh *= 16777619u;
  }
  if (!force && frameHashValid && hsh == lastFrameHash &&
      (now - lastPresentAt) < 1000)
    return false;
  display.display();
  lastFrameHash = hsh;
  frameHashValid = true;
  lastPresentAt = now;
  return true;
}

void setDisplayBrightness(uint8_t percent) {
  if (!oledOK)
    return;
  if (percent > 100)
    percent = 100;
  uint32_t now = millis();
  // Skip redundant I2C commands (called every few hundred ms by some screens)
  if (brightnessKnown && percent == currentAppliedBrightness &&
      (now - brightnessSentAt) < 5000)
    return;
  currentAppliedBrightness = percent;
  brightnessKnown = true;
  brightnessSentAt = now;
  // SSD1306 contrast register 0x00 (lowest) to 0xFF (maximum)
  uint8_t contrast = (uint8_t)((percent * 255) / 100);
  display.ssd1306_command(SSD1306_SETCONTRAST);
  display.ssd1306_command(contrast);
}

void setDisplayPower(bool on) {
  if (!oledOK)
    return;
  uint32_t now = millis();
  if (powerKnown && on == displayPowerState && (now - powerSentAt) < 5000)
    return;
  displayPowerState = on;
  powerKnown = true;
  powerSentAt = now;
  display.ssd1306_command(on ? SSD1306_DISPLAYON : SSD1306_DISPLAYOFF);
  if (on)
    markDisplayDirty();
}

void updateDisplayBrightnessMode() {
  if (!oledOK)
    return;

  uint32_t now = millis();
  bool isTemporarilyAwake =
      (circadianTempWakeUntil > 0 && now < circadianTempWakeUntil);
  bool isNight = inCircadianSleep && !isTemporarilyAwake;

  if (isNight) {
    if (nightMode == NIGHT_SCREEN_OFF) {
      setDisplayPower(false);
      return;
    }
    setDisplayPower(true);
    if (nightMode == NIGHT_DIM) {
      setDisplayBrightness((uint8_t)nightBrightness);
    } else {
      setDisplayBrightness((uint8_t)displayBrightness);
    }
  } else {
    // Normal daytime or awake face (stays at full configured day brightness)
    setDisplayPower(true);
    setDisplayBrightness((uint8_t)displayBrightness);
  }
}

bool initDisplay() {
  Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
  Wire.setClock(FACE_I2C_CLK_DURING);
  oledOK = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  brightnessKnown = false;
  powerKnown = false;
  frameHashValid = false;
  if (oledOK) {
    display.ssd1306_command(SSD1306_SETDISPLAYCLOCKDIV);
    display.ssd1306_command(FACE_OLED_OSC);
    updateDisplayBrightnessMode();
  }
  return oledOK;
}

void setOledDim(bool dim) {
  if (!oledOK)
    return;
  if (dim) {
    setDisplayBrightness(10);
  } else {
    updateDisplayBrightnessMode();
  }
}