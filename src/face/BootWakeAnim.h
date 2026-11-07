#pragma once
#include <Arduino.h>
#include <Adafruit_SSD1306.h>

enum class BootWakeState { Idle, Running, Done };

void bootWakeStart();
bool bootWakeTick();
void bootWakeDraw(Adafruit_SSD1306 &d);
bool bootWakeDone();
void bootAnimPump();

