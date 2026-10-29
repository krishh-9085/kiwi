#pragma once
#include "core/CoreTypes.h"
#include <Arduino.h>


extern uint32_t nextIdleLookAt;
extern uint32_t idleExpressionEnd;
extern int idleExpression;
extern uint32_t petMoodT0;
extern uint32_t flourishT0;
extern int eyeOpenPct;   // 0..100 (kept for compatibility)
extern float eyeOpenAmt; // 0.0..1.0 smooth eyelid openness
extern bool eyeBlinking;
extern uint32_t activeWinkEnd;
extern bool activeWinkLeft;
extern float gazeMicroX; // tiny involuntary eye drift (micro-saccades)
extern float gazeMicroY;

void eyesInit(uint32_t now);
void eyesUpdateAnim(uint32_t now);
void updateIdleVariety();
void triggerTouchBlink(uint32_t now);