#pragma once
#include <Arduino.h>
#include <Adafruit_SSD1306.h>

void sleepyWakeStart();
bool sleepyWakeIsActive();
void sleepyWakeCancel();
void sleepyWakeDraw(Adafruit_SSD1306 &d, uint32_t now);

void sleepyFallAsleepStart();
bool sleepyFallAsleepIsActive();
void sleepyFallAsleepCancel();
void sleepyFallAsleepDraw(Adafruit_SSD1306 &d, uint32_t now);
