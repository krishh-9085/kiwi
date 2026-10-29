#pragma once
#include <Arduino.h>

class Adafruit_SSD1306;

extern Adafruit_SSD1306 display;
extern bool oledOK;

bool initDisplay();
void setOledDim(bool dim);
void setDisplayBrightness(uint8_t percent);
void setDisplayPower(bool on);
void updateDisplayBrightnessMode();

// Pushes the framebuffer to the OLED. Identical consecutive frames are skipped
// (saves I2C time, which keeps animation and audio smoother). Pass force=true
// for one-shot screens. A safety refresh is still sent about once per second.
bool presentDisplay(bool force = false);

// Call when something other than presentDisplay() has drawn to the panel
// (e.g. the animation engine) so the next frame is always sent.
void markDisplayDirty();

extern bool displayPowerState;
extern uint8_t currentAppliedBrightness;