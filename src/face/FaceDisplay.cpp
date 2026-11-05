#include "face/FaceDisplay.h"
#include "config/AppConfig.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include <math.h>

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

// Perceptual gamma for the brightness -> contrast mapping. ~2.2 matches typical
// perceived luminance. Raise it if low levels still look too bright.
#ifndef FACE_BRIGHTNESS_GAMMA
#define FACE_BRIGHTNESS_GAMMA 2.2f
#endif

// Lowest non-zero contrast so pixels remain visible at very low brightness.
#ifndef FACE_CONTRAST_MIN
#define FACE_CONTRAST_MIN 2
#endif

// VCOMH deselect level (0xDB). Higher = brighter, more usable contrast range.
#ifndef FACE_OLED_VCOMH
#define FACE_OLED_VCOMH 0x30
#endif

// Pre-charge period (0xD9). 0xF1 is recommended with the internal charge pump.
#ifndef FACE_OLED_PRECHARGE
#define FACE_OLED_PRECHARGE 0xF1
#endif

// Brightness used by setOledDim(true).
#ifndef FACE_OLED_DIM_PERCENT
#define FACE_OLED_DIM_PERCENT 10
#endif

// ---- State -----------------------------------------------------------------

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

// ---- Frame presentation ----------------------------------------------------

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

// ---- Brightness and power --------------------------------------------------

// Convert a 0-100 perceived brightness into an SSD1306 contrast value (0-255).
static uint8_t percentToContrast(uint8_t percent) {
  if (percent == 0)
    return 0;

  float x = percent / 100.0f;
  float c = powf(x, FACE_BRIGHTNESS_GAMMA) * 255.0f;
  uint8_t out = (uint8_t)(c + 0.5f);

  if (out < FACE_CONTRAST_MIN)
    out = FACE_CONTRAST_MIN;
  return out;
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

void setDisplayBrightness(uint8_t percent) {
  if (!oledOK)
    return;

  if (percent > 100)
    percent = 100;

  uint32_t now = millis();

  // 0% means fully black: turn the panel off rather than leaving pixels faintly
  // lit.
  if (percent == 0) {
    if (!(brightnessKnown && currentAppliedBrightness == 0 &&
          !displayPowerState && (now - brightnessSentAt) < 5000)) {
      currentAppliedBrightness = 0;
      brightnessKnown = true;
      brightnessSentAt = now;
      setDisplayPower(false);
    }
    return;
  }

  // Non-zero brightness requires the panel to be on. This check comes before
  // the cache so a power-off is never left in place by a cached brightness.
  if (!displayPowerState)
    setDisplayPower(true);

  // Skip redundant I2C traffic; refresh every 5 s in case the panel reset.
  if (brightnessKnown && percent == currentAppliedBrightness &&
      (now - brightnessSentAt) < 5000)
    return;

  currentAppliedBrightness = percent;
  brightnessKnown = true;
  brightnessSentAt = now;

  display.ssd1306_command(SSD1306_SETCONTRAST);
  display.ssd1306_command(percentToContrast(percent));
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
    // Normal daytime or awake face: stays at full configured day brightness.
    setDisplayPower(true);
    setDisplayBrightness((uint8_t)displayBrightness);
  }
}

// ---- Initialization --------------------------------------------------------

bool initDisplay() {
  Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
  uint32_t targetClock = FACE_I2C_CLK_DURING;
  Wire.setClock(targetClock);

  oledOK = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  if (oledOK && targetClock > 400000UL) {
    // Self-test first frame at high speed
    display.clearDisplay();
    display.display();
    Wire.beginTransmission(OLED_ADDR);
    if (Wire.endTransmission() != 0) {
      oledOK = false;
    }
  }

  if (!oledOK && targetClock > 400000UL) {
    Serial.printf("[OLED] %lu kHz failed, falling back to 400 kHz\n", targetClock / 1000UL);
    Wire.setClock(400000UL);
    oledOK = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
    if (oledOK) {
      Serial.println("[OLED] Fallback to 400 kHz succeeded");
    } else {
      Serial.println("[OLED] Fallback to 400 kHz also failed");
    }
  }

  brightnessKnown = false;
  powerKnown = false;
  frameHashValid = false;

  if (oledOK) {
    display.ssd1306_command(SSD1306_SETDISPLAYCLOCKDIV);
    display.ssd1306_command(FACE_OLED_OSC);

    display.ssd1306_command(SSD1306_SETPRECHARGE);
    display.ssd1306_command(FACE_OLED_PRECHARGE);

    display.ssd1306_command(SSD1306_SETVCOMDETECT);
    display.ssd1306_command(FACE_OLED_VCOMH);

    updateDisplayBrightnessMode();
  }

  return oledOK;
}

void setOledDim(bool dim) {
  if (!oledOK)
    return;

  if (dim) {
    setDisplayBrightness(FACE_OLED_DIM_PERCENT);
  } else {
    updateDisplayBrightnessMode();
  }
}