#include "services/AiTransport.h"
#include "config/AppConfig.h"
#include "behavior/InteractionManager.h"

// ============================================================
bool httpReadLine(WiFiClient &c, String &line, uint32_t timeoutMs) {
  line = "";
  uint32_t start = millis();
  uint32_t idleT0 = millis();
  while ((uint32_t)(millis() - start) < timeoutMs) {
    if (c.available()) {
      char ch = c.read();
      if (ch == '\n') {
        line.trim();
        return true;
      }
      line += ch;
      idleT0 = millis();
    } else {
      if (!c.connected())
        return false;
      if ((uint32_t)(millis() - idleT0) > 5000)
        return false; // 5s idle gap limit
      delay(1);
    }
  }
  return false;
}

int httpReadBlock(HttpStream &s, uint8_t *buf, size_t maxLen) {
  if (s.finished)
    return -1;
  if (s.chunked && s.chunkLeft <= 0) {
    String line;
    do {
      if (!httpReadLine(*s.c, line, 10000)) {
        s.finished = true;
        return -1;
      }
    } while (line.length() == 0);
    s.chunkLeft = strtol(line.c_str(), NULL, 16);
    if (s.chunkLeft <= 0) {
      s.finished = true;
      return -1;
    }
  }
  if (!s.chunked && s.contentLeft == 0) {
    s.finished = true;
    return -1;
  }
  size_t want = maxLen;
  if (s.chunked && (long)want > s.chunkLeft)
    want = s.chunkLeft;
  if (!s.chunked && s.contentLeft > 0 && (long)want > s.contentLeft)
    want = s.contentLeft;
  if (!s.c->available()) {
    if (!s.c->connected()) {
      s.finished = true;
      return -1;
    }
    return 0;
  }
  int n = s.c->read(buf, want);
  if (n <= 0)
    return 0;
  if (s.chunked)
    s.chunkLeft -= n;
  else if (s.contentLeft > 0)
    s.contentLeft -= n;
  return n;
}

bool ensureDns(const String &host) {
  IPAddress ip;
  if (WiFi.hostByName(host.c_str(), ip) == 1) {
    Serial.printf("[DNS] %s -> %s\n", host.c_str(), ip.toString().c_str());
    return true;
  }
  Serial.println("[DNS] Failed, trying 8.8.8.8 / 1.1.1.1");
  WiFi.config(WiFi.localIP(), WiFi.gatewayIP(), WiFi.subnetMask(),
              IPAddress(8, 8, 8, 8), IPAddress(1, 1, 1, 1));
  delay(300);
  if (WiFi.hostByName(host.c_str(), ip) == 1) {
    Serial.printf("[DNS] %s -> %s (fallback)\n", host.c_str(),
                  ip.toString().c_str());
    return true;
  }
  Serial.println("[DNS] Still failing");
  return false;
}

bool tlsConnect(WiFiClientSecure &c, const String &host, const char *tag) {
  for (int attempt = 1; attempt <= 2; attempt++) {
    c.stop();
    Serial.printf("[%s] TLS connect %s try %d heap=%u maxblk=%u\n", tag,
                  host.c_str(), attempt, (unsigned)ESP.getFreeHeap(),
                  (unsigned)ESP.getMaxAllocHeap());
    uint32_t t0 = millis();
    bool ok = c.connect(host.c_str(), 443, 6000);
    Serial.printf("[%s] connect()=%d after %lu ms\n", tag, ok ? 1 : 0,
                  (unsigned long)(millis() - t0));
    if (ok)
      return true;
    char err[100];
    err[0] = 0;
    int code = c.lastError(err, sizeof(err));
    Serial.printf("[%s] TLS error %d: %s\n", tag, code, err);
    if (cancelRequest)
      return false;
    delay(400);
  }
  return false;
}

bool clientWriteAll(WiFiClient &c, const uint8_t *p, size_t n) {
  size_t off = 0;
  uint32_t t0 = millis();
  while (off < n) {
    int w = c.write(p + off, n - off);
    if (w <= 0) {
      if (millis() - t0 > 8000)
        return false;
      delay(2);
      continue;
    }
    off += w;
    t0 = millis();
  }
  return true;
}

// ============================================================
// CONVERSATION HISTORY MEMORY (MULTI-TURN CHAT CONTEXT)
