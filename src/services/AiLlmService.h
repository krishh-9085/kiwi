#pragma once
#include <Arduino.h>
#include <functional>

typedef std::function<void(const String &sentence, bool isFinal)> SentenceCallback;

String askGemini(const String &prompt, SentenceCallback onSentence = nullptr);
String askGroq(const String &prompt, SentenceCallback onSentence = nullptr);
String askOpenAI(const String &prompt, SentenceCallback onSentence = nullptr);
String askLLM(const String &prompt, SentenceCallback onSentence = nullptr);
