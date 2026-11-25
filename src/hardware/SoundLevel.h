#pragma once
#include <Arduino.h>

void soundLevelTick();
float soundLevelGet();
void soundLevelFeedSamples(const int16_t *pcm, int count);

void soundLevelResetListening();
void soundLevelUpdateListening(float batchMeanAbs, bool speechLocked);
float soundLevelGetListening();
bool soundLevelIsSpeechLocked();

