#pragma once
#include <Arduino.h>

void checkGreetings();
String generateLlmGreeting(int hour, int minute, int wday, const String &ownerName);

