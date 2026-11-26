#include "ai/AiTransport.h"
#include "config/AppConfig.h"
#include "core/Timing.h"
#include "behavior/InteractionManager.h"
#include <esp_heap_caps.h>
#include <mbedtls/platform.h>

// ============================================================
// MBEDTLS PSRAM DYNAMIC ALLOCATOR
// Routes TLS handshake context, buffers, and cert tables to 2MB PSRAM
// ============================================================
static void *psram_mbedtls_calloc(size_t n, size_t size) {
  if (psramFound()) {
    void *p = heap_caps_calloc(n, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (p) return p;
  }
  return heap_caps_calloc(n, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}

static void psram_mbedtls_free(void *ptr) {
  heap_caps_free(ptr);
}

void initMbedTlsPsram() {
  if (psramFound()) {
    mbedtls_platform_set_calloc_free(psram_mbedtls_calloc, psram_mbedtls_free);
    Serial.printf("[TLS] MbedTLS PSRAM dynamic memory active (Free PSRAM: %u KB)\n",
                  (unsigned)(ESP.getFreePsram() / 1024));
  }
}

// ============================================================
bool httpReadLine(WiFiClient &c, String &line, uint32_t timeoutMs) {
  line = "";
  uint32_t start = millis();
  uint32_t idleT0 = start;
  while ((uint32_t)(millis() - start) < timeoutMs) {
    if (cancelRequest)
      return false;
    if (c.available()) {
      char ch = c.read();
      if (ch == '\n') {
        line.trim();
        return true;
      }
      if (line.length() < 4096)
        line += ch;
      idleT0 = millis();
    } else {
      if (!c.connected())
        return false;
      if ((uint32_t)(millis() - idleT0) > timeoutMs)
        return false; // Idle gap limit aligned with caller timeout (prevents premature TTS stream cutoff)
      serviceUi();
      yield();
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
  if (n < 0) {
    s.finished = true;
    return -1;
  }
  if (n == 0)
    return 0;
  if (s.chunked)
    s.chunkLeft -= n;
  else if (s.contentLeft > 0)
    s.contentLeft -= n;
  return n;
}

bool ensureDns(const String &host) {
  if (!host.length() || WiFi.status() != WL_CONNECTED)
    return false;

  struct DnsCacheSlot {
    String host;
    IPAddress ip;
    uint32_t cachedAt;
    bool valid;
  };
  static DnsCacheSlot cache[4];
  uint32_t now = millis();

  for (int i = 0; i < 4; i++) {
    if (cache[i].valid && cache[i].host == host &&
        (now - cache[i].cachedAt) < 300000UL) {
      return true;
    }
  }

  IPAddress ip;
  if (WiFi.hostByName(host.c_str(), ip) == 1) {
    Serial.printf("[DNS] %s -> %s\n", host.c_str(), ip.toString().c_str());
    int slot = 0;
    uint32_t oldestAge = 0;
    for (int i = 0; i < 4; i++) {
      if (!cache[i].valid || cache[i].host == host) {
        slot = i;
        break;
      }
      uint32_t age = now - cache[i].cachedAt;
      if (age >= oldestAge) {
        oldestAge = age;
        slot = i;
      }
    }
    cache[slot] = {host, ip, now, true};
    return true;
  }
  Serial.println("[DNS] Failed, trying 8.8.8.8 / 1.1.1.1");
  WiFi.config(WiFi.localIP(), WiFi.gatewayIP(), WiFi.subnetMask(),
              IPAddress(8, 8, 8, 8), IPAddress(1, 1, 1, 1));
  waitMs(300);
  if (WiFi.hostByName(host.c_str(), ip) == 1) {
    Serial.printf("[DNS] %s -> %s (fallback)\n", host.c_str(),
                  ip.toString().c_str());
    cache[0] = {host, ip, millis(), true};
    return true;
  }
  Serial.println("[DNS] Still failing");
  return false;
}

bool tlsConnect(WiFiClientSecure &c, const String &host, const char *tag) {
  for (int attempt = 1; attempt <= 2; attempt++) {
    c.stop();
    serviceUi();  // Keep face animating during TLS setup
    Serial.printf("[%s] TLS connect %s try %d heap=%u maxblk=%u\n", tag,
                  host.c_str(), attempt, (unsigned)ESP.getFreeHeap(),
                  (unsigned)ESP.getMaxAllocHeap());
    uint32_t t0 = millis();
    bool ok = c.connect(host.c_str(), 443, 6000);
    serviceUi();  // Refresh face right after blocking connect returns
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
    waitMs(400);
  }
  return false;
}

bool clientWriteAll(WiFiClient &c, const uint8_t *p, size_t n) {
  size_t off = 0;
  uint32_t t0 = millis();
  while (off < n) {
    if (cancelRequest || !c.connected())
      return false;
    int w = c.write(p + off, n - off);
    if (w <= 0) {
      if (millis() - t0 > 8000)
        return false;
      serviceUi();
      yield();
      continue;
    }
    off += w;
    t0 = millis();
  }
  return true;
}

// ============================================================
// CONVERSATION HISTORY MEMORY (MULTI-TURN CHAT CONTEXT)
