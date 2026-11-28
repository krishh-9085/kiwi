#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

void pumpUI();

extern volatile uint32_t lastProgressAt;
extern TaskHandle_t loopTaskHandle;
extern bool loopTaskWDTEnabled;

static inline void serviceUi() {
  static uint32_t lastServiceAt = 0;
  static uint32_t lastWdtAt = 0;
  uint32_t now = millis();
  lastProgressAt = now;
  if (loopTaskWDTEnabled && loopTaskHandle != nullptr &&
      xTaskGetCurrentTaskHandle() == loopTaskHandle &&
      (now - lastWdtAt >= 200)) {
    lastWdtAt = now;
    feedLoopWDT();
  }
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

