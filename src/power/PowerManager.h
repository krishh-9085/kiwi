#pragma once
#include <Arduino.h>

void powerInit();      // call from setup() after loadConfig() and before initDisplay()
void powerUpdate();    // call from loop(), once per iteration
bool powerIsSleepRequested();

void powerNotifyActivity();
void powerSetCpuFreq(uint32_t mhz);
uint32_t powerGetCpuFreq();

extern bool suppressBootGreeting;
extern bool skipBootWakeAnim;
extern bool wakeFromDeepSleepWakeAnim;

