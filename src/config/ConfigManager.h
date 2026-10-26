#pragma once
#include <Arduino.h>

void loadConfig();
void saveConfig();
void clearConfig();

String normalizeSttLang(const String &v);
String normalizeSttProvider(const String &v);
String normalizeTtsProvider(const String &v);
