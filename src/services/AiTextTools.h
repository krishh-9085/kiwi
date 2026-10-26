#pragma once
#include <Arduino.h>

void initBase64Lookup();
int utf8CharLen(uint8_t b);
int utf8Count(const String &s);
String utf8Truncate(const String &s, int maxChars);
String jsonTextField(const String &json, const char *key);
String jsonEscape(const String &s);
String cleanForSpeech(const String &s);
bool parseUrl(const String &url, String &host, String &path);
bool containsDevanagari(const String &s);
String sarvamSttLangCode();
String whisperSttLangCode();

extern int8_t b64_lookup[256];
