#pragma once
#include <Arduino.h>
#include "core/CoreTypes.h"

extern TouchSensor touchL;
extern TouchSensor touchR;
extern TouchSensor touchT;

void initTouch();
void resyncTouch();
void updateTouch();
extern uint32_t lastTickleTriggeredAt;
