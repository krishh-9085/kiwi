#include "net/WifiManager.h"
#include "behavior/OfflineBrain.h"
#include "config/AppConfig.h"
#include "config/ConfigManager.h"
#include "core/Timing.h"
#include "integrations/HyperHdrService.h"
#include "integrations/WeatherService.h"
#include "integrations/WledService.h"
#include "web/WebPortal.h"
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <esp_mac.h>
#include <time.h>

DNSServer dnsServer;

static const wifi_power_t WIFI_TX = WIFI_POWER_8_5dBm;
static bool wifiConnecting = false;
static uint32_t wifiConnectStartAt = 0;
static int netSuspendDepth = 0;
static bool originalApState = false;

void pumpDns() {
  if (apMode)
    dnsServer.processNextRequest();
}

// ============================================================
// mDNS RESPONDER
// ============================================================
void initMdns(const char *mode) {
  MDNS.end();
  String h = sanitizeHostname(mdnsHostname);
  if (h.length() == 0)
    h = "kiwi";
  if (!MDNS.begin(h.c_str())) {
    Serial.printf(
        "[MDNS] WARN: Failed to start mDNS responder for %s.local (%s)\n",
        h.c_str(), mode);
    return;
  }
  MDNS.addService("http", "tcp", 80);
  MDNS.addServiceTxt("http", "tcp", "path", "/");
  Serial.printf("[MDNS] %s responder active: http://%s.local\n", mode,
                h.c_str());
}

// ============================================================
// WIFI & AP MANAGEMENT
// ============================================================
void createAPName() {
  uint8_t mac[6];
  if (esp_read_mac(mac, ESP_MAC_WIFI_STA) != ESP_OK) {
    WiFi.macAddress(mac);
  }
  char s[5];
  snprintf(s, sizeof(s), "%02X%02X", mac[4], mac[5]);
  apSSID = "MyRobot-" + String(s);
}

void startAP() {
  createAPName();
  WiFi.persistent(false);
  WiFi.disconnect(true, false);
  waitMs(150);
  WiFi.mode(WIFI_OFF);
  waitMs(250);
  WiFi.mode(WIFI_AP);
  waitMs(250);
  WiFi.setTxPower(WIFI_TX);
  WiFi.softAPConfig(apIP, apIP, apMask);
  bool ok = false;
  for (int i = 0; i < 5 && !ok; i++) {
    ok = WiFi.softAP(apSSID.c_str(), AP_PASSWORD, AP_CHANNEL, 0, 4);
    if (!ok) {
      WiFi.mode(WIFI_OFF);
      waitMs(300);
      WiFi.mode(WIFI_AP);
      waitMs(300);
      WiFi.setTxPower(WIFI_TX);
      WiFi.softAPConfig(apIP, apIP, apMask);
    }
  }
  apMode = ok;
  if (ok) {
    dnsServer.start(53, "*", apIP);
    initMdns("AP");
  }
}

void startAPParallel() {
  createAPName();
  WiFi.softAPConfig(apIP, apIP, apMask);
  bool ok = WiFi.softAP(apSSID.c_str(), AP_PASSWORD, AP_CHANNEL, 0, 4);
  apMode = ok;
  if (ok) {
    dnsServer.start(53, "*", apIP);
    initMdns("AP");
    Serial.printf("[AP] Parallel AP up: %s -> http://192.168.4.1\n",
                  apSSID.c_str());
  }
}

void startConnectHomeAsync() {
  if (!wifiSSID.length()) {
    wifiConnecting = false;
    return;
  }
  Serial.printf("[WIFI] Connecting to %s (non-blocking)\n", wifiSSID.c_str());
  WiFi.persistent(false);
  WiFi.disconnect(true, false);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
  WiFi.setTxPower(WIFI_TX);
  wifiConnecting = true;
  wifiConnectStartAt = millis();
}

bool isWifiConnecting() {
  if (!wifiConnecting)
    return false;
  if (WiFi.status() == WL_CONNECTED) {
    return false;
  }
  if (millis() - wifiConnectStartAt >= 15000UL) {
    return false;
  }
  return true;
}

void finalizeWifiConnected() {
  wifiConnecting = false;
  apMode = false;
  WiFi.setSleep(false);
  initMdns("STA");
  startAPParallel();
  startNtpAsync();
  lastNtpTryAt = millis();
  lastWeatherFetchAt = 0;
  Serial.printf("[WIFI] Connected! IP: %s\n",
                WiFi.localIP().toString().c_str());
  discoverWledAsync();
  discoverHyperHdrAsync();
}

void finalizeWifiOffline() {
  wifiConnecting = false;
  if (WiFi.status() != WL_CONNECTED && !apMode) {
    Serial.println(
        "[WIFI] Connect timed out or unavailable. Starting AP mode.");
    startAP();
    initOfflineBrain();
  }
}

bool connectHome() {
  startConnectHomeAsync();
  return (WiFi.status() == WL_CONNECTED);
}

void maintainWiFi() {
  static uint32_t lastTry = 0;
  static bool wasConnected = false;
  if (!wifiSSID.length())
    return;
  if (WiFi.status() == WL_CONNECTED) {
    if (!wasConnected) {
      wasConnected = true;
      initMdns("STA");
    }
    if (!timeSynced) {
      if (checkNtpSync()) {
        Serial.println("[NTP] Time Synced (background)");
      } else if (millis() - lastNtpTryAt > 20000) {
        lastNtpTryAt = millis();
        startNtpAsync();
      }
    } else if (ntpNeedsRefresh && millis() - lastNtpTryAt > 30000) {
      ntpNeedsRefresh = false;
      lastNtpTryAt = millis();
      startNtpAsync();
      Serial.println("[NTP] Refreshing SNTP after RTC wake/config change");
    } else {
      static uint32_t lastPeriodicNtpAt = 0;
      if (millis() - lastPeriodicNtpAt > 6UL * 3600UL * 1000UL) {
        lastPeriodicNtpAt = millis();
        startNtpAsync();
      }
    }
    static uint32_t lastWeatherAttempt = 0;
    if (bootCompleted &&
        (lastWeatherFetchAt == 0 ||
         millis() - lastWeatherFetchAt > 30UL * 60UL * 1000UL) &&
        (millis() - lastWeatherAttempt > 60000UL || lastWeatherAttempt == 0)) {
      if (millis() > 8000) {
        lastWeatherAttempt = millis();
        fetchWeatherAsync();
      }
    }
    return;
  }
  wasConnected = false;
  if (millis() - lastTry < 15000)
    return;
  lastTry = millis();
  WiFi.reconnect();
}

// ============================================================
// NTP TIME SYNC
// ============================================================
void startNtpAsync() {
  long offsetSec = (long)tzOffsetMin * 60L;
  if (WiFi.status() != WL_CONNECTED) {
    char tzStr[16];
    int absMin = abs(tzOffsetMin);
    snprintf(tzStr, sizeof(tzStr), "UTC%c%d:%02d", (tzOffsetMin >= 0) ? '-' : '+',
             absMin / 60, absMin % 60);
    setenv("TZ", tzStr, 1);
    tzset();
    return;
  }
  Serial.printf("[NTP] Background SNTP configured (UTC%+d:%02d)\n",
                tzOffsetMin / 60, abs(tzOffsetMin) % 60);
  configTime(offsetSec, TZ_DST_SEC, NTP_SERVER_1, NTP_SERVER_2);
}

bool checkNtpSync() {
  if (timeSynced)
    return true;
  if (WiFi.status() != WL_CONNECTED)
    return false;
  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 10)) {
    timeSynced = true;
    Serial.printf("[NTP] Synced: %02d:%02d:%02d %02d/%02d/%04d\n",
                  timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
                  timeinfo.tm_mday, timeinfo.tm_mon + 1,
                  timeinfo.tm_year + 1900);
    return true;
  }
  return false;
}

bool syncTimeFromNTP() {
  if (WiFi.status() != WL_CONNECTED)
    return false;
  if (checkNtpSync())
    return true;
  startNtpAsync();
  struct tm timeinfo;
  for (int tries = 0; tries < 8; tries++) {
    if (getLocalTime(&timeinfo, 100)) {
      timeSynced = true;
      Serial.printf("[NTP] %02d:%02d:%02d %02d/%02d/%04d\n", timeinfo.tm_hour,
                    timeinfo.tm_min, timeinfo.tm_sec, timeinfo.tm_mday,
                    timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);
      return true;
    }
    waitMs(50);
  }
  return false;
}

// ============================================================
// HEAP / SOFT-AP SUSPEND GUARDS FOR AI REQUESTS
// ============================================================
bool suspendLocalNet() {
  // If PSRAM is available, SoftAP, DNS server, and WebPortal run continuously
  // without pausing or socket starvation during AI calls.
  if (psramFound()) {
    return false;
  }
  if (netSuspendDepth++ == 0) {
    originalApState = apMode;
    if (originalApState) {
      dnsServer.stop();
      WiFi.softAPdisconnect(true);
      apMode = false;
      delay(20);
    }
  }
  return originalApState;
}

void restoreWebAndAP(bool apWasUp) {
  if (psramFound()) {
    startWeb();
    return;
  }
  if (--netSuspendDepth <= 0) {
    netSuspendDepth = 0;
    if (apWasUp || originalApState)
      startAPParallel();
    startWeb();
    Serial.printf("[NET] Free heap: %u\n", (unsigned)ESP.getFreeHeap());
  }
}
