#pragma once
#include <Arduino.h>

void pumpUI();

extern volatile uint32_t lastProgressAt;

static inline void serviceUi() {
  static uint32_t lastServiceAt = 0;
  uint32_t now = millis();
  lastProgressAt = now;
  if (now - lastServiceAt >= 4) {
    lastServiceAt = now;
    pumpUI();
  }
}

static inline bool waitMs(uint32_t ms, bool (*cond)() = nullptr) {
  uint32_t t0 = millis();
  while (millis() - t0 < ms) {
    serviceUi();
    if (cond != nullptr && cond()) {
      return true;
    }
    yield();
  }
  return false;
}

