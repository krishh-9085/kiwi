#pragma once
#include <Arduino.h>
#include "core/CoreTypes.h"

// ---------- Base64 & Text Processing ----------
void initBase64Lookup();
int utf8CharLen(unsigned char c);
int utf8Count(const String &s);
String utf8Truncate(const String &s, int maxChars);
String jsonTextField(const String &json, const char *key);
String jsonEscape(const String &s);
String cleanForSpeech(const String &in);
bool parseUrl(const String &url, String &host, String &path);

// ---------- Speech-to-Text (STT) ----------
SttCfg resolveStt();
String recordAndTranscribe(uint32_t recordSeconds);

// ---------- Large Language Models (LLM) & Context Memory ----------
String askLLM(const String &question);
String askGroq(const String &question);
String askGemini(const String &question);
void clearConversationHistory();
int getConversationHistoryCount();

// ---------- Text-to-Speech (TTS) ----------
bool speakText(const String &text);
int ttsSarvam(const String &text);
int ttsPcmOpenAI(const String &text);
int ttsPcmEleven(const String &text);
