#pragma once
#include <Arduino.h>
#include "core/CoreTypes.h"

SttCfg resolveStt();
String recordAndTranscribe(uint32_t recordSeconds);
