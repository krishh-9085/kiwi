#pragma once
#include <Arduino.h>

void initTtsBuffers();
bool speakText(const String &text);
int ttsSarvam(const String &text);
int ttsFishAudio(const String &text);
