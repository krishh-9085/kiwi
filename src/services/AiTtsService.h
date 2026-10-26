#pragma once
#include <Arduino.h>

bool speakText(const String &text);
int ttsSarvam(const String &text);
int ttsPcmOpenAI(const String &text);
int ttsPcmEleven(const String &text);
int ttsFishAudio(const String &text);
