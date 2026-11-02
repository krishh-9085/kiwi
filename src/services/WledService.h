#pragma once
#include <Arduino.h>

bool discoverWled(bool force = false);
String getWledIp();
bool isWledDiscovered();
void setWledManualIp(const String &ip);

bool sendWledCommand(const String &urlParams);
bool setWledPower(bool on);
bool setWledBrightness(uint8_t brightness);
bool adjustWledBrightness(int delta);
bool setWledColor(uint8_t r, uint8_t g, uint8_t b, int brightness = -1);
bool fetchWledEffectsList();
int findWledEffectId(const String &query, String *outMatchedName = nullptr);
bool setWledEffectById(int fxId, const String &effectName = "");
bool setWledEffect(const String &effectName);
bool setWledNextEffect();
bool executeWledAction(const String &param);

