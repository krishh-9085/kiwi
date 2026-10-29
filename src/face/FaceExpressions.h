#pragma once
#include "core/CoreTypes.h"
#include <Arduino.h>


extern FaceState faceNow, faceTgt;
extern uint32_t emotionStartedAt;

void setFace(float w, float h, float gap, float y, float r, float lidIn = 0,
             float lidOut = 0, float cheek = 0, float blush = 0);
void setEmotion(Emotion e);
void faceTargetsForEmotion(Emotion e, PetMood pm);
void maybeChangeEmotion();
void holdFace(uint32_t ms);
void showEmotionFor(Emotion e, uint32_t ms = 4000);
void showPetMood(PetMood m, uint32_t ms = 4000);
bool triggerEmotionByName(const String &nameIn, uint32_t holdMs,
                          bool withSound);