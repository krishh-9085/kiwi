#pragma once
#include <Arduino.h>
#include "core/CoreTypes.h"

// ============================================================
// ONBOARD WS2812 RGB LED CONTROLLER (ESP32-S3 GPIO 48)
// Automatically morphs and pulses based on Kiwi's mood & states:
// - Listening Mode  -> Glowing Cyan pulse (Alexa / Siri style)
// - Thinking Mode   -> Mystical Purple breathing
// - Speaking/Answer -> Vibrant Emerald Green pulse
// - Moods & Smiles  -> Gold, Pink, Red, Blue, Amber, etc.
// - Sleeping Mode   -> Ultra-dim indigo breath or off
// ============================================================

// Initialize GPIO 48 RGB LED hardware
void rgbLedInit();

// Enable or disable RGB LED output (persisted in NVS)
void rgbLedSetEnabled(bool en);
bool rgbLedIsEnabled();

// Non-blocking smooth update called every loop iteration
void rgbLedUpdate();

// Set manual color override (disables mood sync until cleared)
void rgbLedSetOverride(uint8_t r, uint8_t g, uint8_t b);

// Clear manual override (restores mood / mode auto-sync)
void rgbLedClearOverride();
bool rgbLedGetOverride();
void rgbLedGetOverrideColor(uint8_t &r, uint8_t &g, uint8_t &b);

// Global brightness limit (0 - 255, default ~180 to avoid eye strain)
void rgbLedSetBrightness(uint8_t b);
uint8_t rgbLedGetBrightness();

// Force immediate output to LED
void rgbLedShow(uint8_t r, uint8_t g, uint8_t b);

