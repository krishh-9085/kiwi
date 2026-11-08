#pragma once
#include <Arduino.h>

void loadConfig();
void saveConfig();              // Debounced save: marks config dirty, commits to NVS after 10s idle
void saveConfigImmediate();     // Immediate synchronous commit to NVS (for /save, OTA, or shutdown)
void serviceConfigManager();    // Periodic tick to flush dirty config after debounce timeout
bool isConfigDirty();
void clearConfig();

String normalizeSttLang(const String &v);
String normalizeSttProvider(const String &v);
String normalizeTtsProvider(const String &v);
String sanitizeUrl(const String &val, const char *defaultUrl);
String sanitizeField(const String &val, const char *defaultVal = "");
String sanitizeHostname(const String &raw);
