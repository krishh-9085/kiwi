#pragma once
#include <Arduino.h>

bool speakText(const String &text);
int ttsSarvam(const String &text, const String &speaker);
int ttsPcmOpenAI(const String &text, const String &voice, float speed);
int ttsPcmEleven(const String &text, const String &voiceId);
