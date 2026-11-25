#pragma once
#include "core/CoreTypes.h"
#include <Arduino.h>

String escJson(String s);
void invalidateStatusCache();
EyeSkin parseEyeSkinArg(const String &raw);

inline bool parsePortalBool(const String &v) {
  return (v == "1" || v == "true" || v == "on");
}
