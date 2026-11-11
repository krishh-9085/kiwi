#pragma once
#include <Arduino.h>

// ============================================================
// HyperHDR Client Service for MyRobot
// Controls HyperHDR JSON-RPC API (default port 8090)
// Allows screen sync, priority color overrides, and WLED arbitration
// ============================================================

// Auto-Discovery & Manual Config
bool discoverHyperHdr(bool force = false);
bool discoverHyperHdrAsync(bool force = false);
bool isHyperHdrDiscoveryRunning();
bool isHyperHdrDiscovered();
String getHyperHdrIp();
uint16_t getHyperHdrPort();
void setHyperHdrManualHost(const String &ip, uint16_t port = 8090);

// Raw JSON-RPC API
bool sendHyperHdrJson(const String &jsonPayload, String *outResponse = nullptr);

// Component & State Control
bool setHyperHdrPower(bool on);
bool setHyperHdrComponentState(const String &component, bool state);
bool setHyperHdrLedDevice(bool state);
bool setHyperHdrScreenSync(bool enable);
bool isHyperHdrScreenSyncActive();

// Priority Channel Overrides (Higher precedence than screen grabber)
bool setHyperHdrColor(uint8_t r, uint8_t g, uint8_t b, int priority = 50, int durationMs = 0);
bool setHyperHdrEffect(const String &effectName, int priority = 50, int durationMs = 0);
bool clearHyperHdrPriority(int priority = 50);

// Adjustments & HDR
bool setHyperHdrBrightness(uint8_t brightness);
bool setHyperHdrHdr(bool enable);

// Unified Smart Execution (HyperHDR + WLED coordinated)
bool executeUnifiedLightAction(const String &param);

