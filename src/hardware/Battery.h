#pragma once
#include <Arduino.h>

// Returns estimated battery percentage (0-100) or -1 if battery sense is disabled
int batteryPercent();
