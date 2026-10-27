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

extern bool displayPowerState;
extern uint8_t currentAppliedBrightness;
