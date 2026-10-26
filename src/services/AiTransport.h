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

bool httpReadLine(WiFiClient *c, char *buf, size_t maxLen, uint32_t timeoutMs = 5000);
int httpReadBlock(HttpStream &s, uint8_t *dst, size_t maxLen, uint32_t timeoutMs = 8000);
bool ensureDns(const char *host, IPAddress &ip, uint32_t timeoutMs = 4000);
bool tlsConnect(WiFiClientSecure &c, const char *host, int port, uint32_t timeoutMs = 8000);
bool clientWriteAll(WiFiClientSecure &c, const uint8_t *buf, size_t len, uint32_t timeoutMs = 6000);
