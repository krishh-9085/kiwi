#pragma once
#include <Arduino.h>

void loadConfig();
void saveConfig();
void clearConfig();

String normalizeSttLang(const String &v);
String normalizeSttProvider(const String &v);
String normalizeTtsProvider(const String &v);
String sanitizeUrl(const String &val, const char *defaultUrl);
String sanitizeField(const String &val, const char *defaultVal = "");
