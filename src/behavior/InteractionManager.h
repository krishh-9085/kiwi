#pragma once
#include <Arduino.h>
#include "core/CoreTypes.h"

void pumpUI();
void triggerPetReaction();
void handleQuestion(const String &q);
Emotion detectVocalEmotion(const String &text);
