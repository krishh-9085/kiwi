#pragma once
#include <Arduino.h>

void checkGreetings();
bool isGreetingMismatchForHour(const String &text, int hour);
String generateLlmGreeting(int hour, int minute, int wday, const String &ownerName);
bool deliverGreeting(int hour, int minute, int wday, const String &ownerName,
                     const String &fallbackText, const char *logTag = "[GREETINGS]");

