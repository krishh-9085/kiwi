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

// Set / switch wake word model (0=Alexa, 1=Hey Kiwi)
bool wakeWordSetModel(uint8_t modelIndex);
bool wakeWordParseModelIndex(const String &arg, uint8_t &outIdx);
uint8_t wakeWordGetModel();
const char* wakeWordGetModelName();

// Check if currently actively listening on mic
bool wakeWordIsListening();

// Reset audio priming & probability buffers
void wakeWordReset();

// A/B Input Gain Tuning (default: 8.0f; bypass soft-knee when 1.0f)
void wakeWordSetGain(float gain);
float wakeWordGetGain();

// Secondary voice spike trigger clause (default: false / pure average)
void wakeWordSetSpikeTrigger(bool en);
bool wakeWordGetSpikeTrigger();

// Diagnostics & Serial Console Tuning
void wakeWordPrintStatus();
void wakeWordProcessSerialCommand(const String &cmd);

// Suppress wake word triggers until millis() >= untilMs (e.g. echo guard after actions)
void wakeWordSuppressUntil(uint32_t untilMs);

// Synchronized RX microphone handoff between Core 0 wakeWordTask and Core 1 STT/Sleep
bool wakeWordAcquireMic(uint32_t maxWaitMs = 150);
void wakeWordReleaseMic();

// Pre-Wake Audio Ring-Buffer in PSRAM (Never Miss First Words)
bool preWakeBufferInit();
void preWakePush(const int16_t *samples, size_t count);
bool preWakeIsBridgeActive();
void preWakeStopBridge();
bool preWakeWaitBridgeStop(uint32_t maxWaitMs = 60);
bool preWakeHasData();
void preWakeReset();
size_t preWakeDrain(int16_t *dest, size_t maxSamples);

