#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

struct HttpStream {
  WiFiClient *c = nullptr;
  bool chunked = false;
  long contentLeft = -1;
  long chunkLeft = 0;
  bool finished = false;
};

bool httpReadLine(WiFiClient &c, String &line, uint32_t timeoutMs = 5000);
int httpReadBlock(HttpStream &s, uint8_t *dst, size_t maxLen);
bool ensureDns(const String &host);
bool tlsConnect(WiFiClientSecure &c, const String &host, const char *tag);
bool clientWriteAll(WiFiClient &c, const uint8_t *buf, size_t len);
