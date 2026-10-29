#pragma once
#include <Arduino.h>

void drawClockScreen();
void drawWeatherScreen();
void drawBannerScreen();
void showErrorFace(uint32_t ms);
void drawMessage(const char *l1, const char *l2 = "");
void drawConnectingFrame(const char *targetSSID, int frame);
void drawConnectedNotice(const char *title, uint32_t holdMs = 800);
void showStatus(const char *l1, const char *l2, uint32_t ms);
