#pragma once
#include <Arduino.h>
#include "core/CoreTypes.h"

extern uint32_t nextIdleLookAt;
extern uint32_t idleExpressionEnd;
extern int idleExpression;
extern uint32_t petMoodT0;
extern uint32_t flourishT0;
extern int eyeOpenPct;
extern bool eyeBlinking;
extern uint32_t activeWinkEnd;
extern bool activeWinkLeft;

void eyesInit(uint32_t now);
void eyesUpdateAnim(uint32_t now);
void resetIdleFace(FaceState &tgt, uint32_t now);
void applyIdleEye(FaceState &tgt, int eyeIdx, uint32_t now);
void applyIdleMouth(FaceState &tgt, uint32_t now);
void applyClassicIdle(FaceState &tgt, uint32_t now);
void updateIdleVariety();
