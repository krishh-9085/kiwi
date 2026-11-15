#pragma once
#include <Arduino.h>
#include <ESP_TF.h>

// ============================================================
// MICRO-WAKE-WORD ENGINE ("Hey Jarvis" / Offline Hotword)
// ============================================================

// Initialize WakeWord engine (TFLite Micro + Frontend + background task on Core 0)
bool wakeWordInit();

// Start or resume listening
void wakeWordStart();

// Pause listening
void wakeWordStop();

// Sensitivity cutoff (0.0 to 1.0). Default: 0.85
// Higher = stricter (fewer false positives). Lower = easier trigger.
void wakeWordSetSensitivity(float cutoff);
float wakeWordGetSensitivity();

// Enable / Disable wake word completely
bool wakeWordIsEnabled();
void wakeWordSetEnabled(bool en);

// Set / switch wake word model (0=Hey Jarvis, 1=Hey Edith, 2=Alexa)
bool wakeWordSetModel(uint8_t modelIndex);
uint8_t wakeWordGetModel();
const char* wakeWordGetModelName();

// Check if currently actively listening on mic
bool wakeWordIsListening();

// Reset audio priming & probability buffers
void wakeWordReset();

