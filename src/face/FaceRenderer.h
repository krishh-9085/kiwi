#pragma once
#include <Arduino.h>
#include "core/CoreTypes.h"

void drawBlush(int x, int y, int r);
void drawMouth(int cx, int cy, int w, float curve, float open);
void drawFlourish(Flourish fl);
void drawFace();
