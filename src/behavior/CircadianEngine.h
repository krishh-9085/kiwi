#pragma once
#include <Arduino.h>
#include "config/AppConfig.h"

void checkCircadianSleep();
bool isCircadianScheduleActiveNow();
int computeSecondsUntilScheduleWake();
bool isCircadianTempWakeActive();

void startManualSleep();
void wakeKiwiFromSleep(bool skipScheduleTonight,
                       uint32_t tempWakeMs = CIRCADIAN_TEMP_WAKE_MS,
                       bool playWakeAnim = true);
void extendTempWakeIfActive(uint32_t extraMs = CIRCADIAN_TEMP_WAKE_MS);
void onCircadianScheduleChanged();
bool parseScheduleTimeString(const String &raw, int &outHour, int &outMin);
