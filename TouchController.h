#pragma once
#include <Arduino.h>
#include "Types.h"

extern TouchSensor touchL;
extern TouchSensor touchR;
extern TouchSensor touchT;

void initTouch();
void resyncTouch();
void updateTouch();
