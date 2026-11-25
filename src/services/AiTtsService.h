#pragma once
#include <Arduino.h>

void initTtsBuffers();
bool speakText(const String &text);
bool speakFishAudio(const String &text, uint32_t maxPlayMs = 0);
int ttsSarvam(const String &text);
int ttsFishAudio(const String &text, uint32_t maxPlayMs = 0);
