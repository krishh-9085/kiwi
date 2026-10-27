#include "face/FaceDisplay.h"
#include "config/AppConfig.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
bool oledOK = false;
bool displayPowerState = true;
uint8_t currentAppliedBrightness = 80;

void setDisplayBrightness(uint8_t percent) {
  if (!oledOK) return;
  if (percent > 100) percent = 100;
  currentAppliedBrightness = percent;
  // SSD1306 contrast register 0x00 (lowest) to 0xFF (maximum)
  uint8_t contrast = (uint8_t)((percent * 255) / 100);
  display.ssd1306_command(SSD1306_SETCONTRAST);
  display.ssd1306_command(contrast);
}

void setDisplayPower(bool on) {
  if (!oledOK) return;
  displayPowerState = on;
  display.ssd1306_command(on ? SSD1306_DISPLAYON : SSD1306_DISPLAYOFF);
}

void updateDisplayBrightnessMode() {
  if (!oledOK) return;

  uint32_t now = millis();
  bool isTemporarilyAwake = (circadianTempWakeUntil > 0 && now < circadianTempWakeUntil);
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
  Wire.setClock(400000);
  oledOK = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  if (oledOK) {
    updateDisplayBrightnessMode();
  }
  return oledOK;
}

void setOledDim(bool dim) {
  if (!oledOK) return;
  if (dim) {
    setDisplayBrightness(10);
  } else {
    updateDisplayBrightnessMode();
  }
}

