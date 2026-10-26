#pragma once
#include <Arduino.h>
#include <initializer_list>

bool hasAny(const String &s, std::initializer_list<const char *> words);
bool handleTimeCommand(const String &qIn);
bool handleWeatherCommand(const String &qIn);
bool handleEmotionCommand(const String &qIn);
bool handleSkinCommand(const String &qIn);
bool handleVolumeCommand(const String &qIn);
bool handleTimerCommand(const String &qIn);
