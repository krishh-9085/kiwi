#pragma once
#include <Arduino.h>

String askGemini(const String &prompt);
String askGroq(const String &prompt);
String askOpenAI(const String &prompt);
String askLLM(const String &prompt);
