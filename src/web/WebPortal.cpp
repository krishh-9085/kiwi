#include "web/WebPortal.h"
#include "config/AppConfig.h"
#include "config/ConfigManager.h"
#include "hardware/AudioEngine.h"
#include "face/FaceEngine.h"
#include "services/AiService.h"
#include "services/MemoryService.h"
#include "animation/AnimationEngine.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Update.h>
#include <time.h>
#include <Preferences.h>
#include "services/WeatherService.h"

WebServer server(80);
DNSServer dnsServer;
volatile int8_t pendingSound = -1;
static bool webRoutesSet = false;
static bool customAnimationUploadResult = false;

void pumpWeb() {
  if (apMode)
    dnsServer.processNextRequest();
  server.handleClient();
}

// ============================================================
// WIFI & AP MANAGEMENT
// ============================================================
static const wifi_power_t WIFI_TX = WIFI_POWER_8_5dBm;

void createAPName() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char s[5];
  snprintf(s, sizeof(s), "%02X%02X", mac[4], mac[5]);
  apSSID = "MyRobot-" + String(s);
}

void startAP() {
  createAPName();
  WiFi.persistent(false);
  WiFi.disconnect(true, false);
  delay(150);
  WiFi.mode(WIFI_OFF);
  delay(250);
  WiFi.mode(WIFI_AP);
  delay(250);
  WiFi.setTxPower(WIFI_TX);
  WiFi.softAPConfig(apIP, apIP, apMask);
  bool ok = false;
  for (int i = 0; i < 5 && !ok; i++) {
    ok = WiFi.softAP(apSSID.c_str(), AP_PASSWORD, AP_CHANNEL, 0, 4);
    if (!ok) {
      WiFi.mode(WIFI_OFF);
      delay(300);
      WiFi.mode(WIFI_AP);
      delay(300);
      WiFi.setTxPower(WIFI_TX);
      WiFi.softAPConfig(apIP, apIP, apMask);
    }
  }
  apMode = ok;
  if (ok)
    dnsServer.start(53, "*", apIP);
}

void startAPParallel() {
  createAPName();
  WiFi.softAPConfig(apIP, apIP, apMask);
  bool ok = WiFi.softAP(apSSID.c_str(), AP_PASSWORD, AP_CHANNEL, 0, 4);
  apMode = ok;
  if (ok) {
    dnsServer.start(53, "*", apIP);
    Serial.printf("[AP] Parallel AP up: %s -> http://192.168.4.1\n", apSSID.c_str());
  }
}

bool connectHome() {
  if (!wifiSSID.length())
    return false;
  Serial.printf("[WIFI] Connecting to %s", wifiSSID.c_str());
  WiFi.persistent(false);
  WiFi.disconnect(true, false);
  delay(100);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
  WiFi.setTxPower(WIFI_TX);
  uint32_t t = millis();
  int animStep = 0;
  while (WiFi.status() != WL_CONNECTED && millis() - t < 15000) {
    drawConnectingFrame(wifiSSID.c_str(), animStep++);
    delay(160);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    apMode = false;
    WiFi.setSleep(false);
    drawConnectedNotice(wifiSSID.c_str(), 800);
    return true;
  }
  return false;
}

void maintainWiFi() {
  static uint32_t lastTry = 0;
  if (!wifiSSID.length())
    return;
  if (WiFi.status() == WL_CONNECTED && !timeSynced) {
    if (millis() - lastNtpTryAt > 60000) {
      lastNtpTryAt = millis();
      syncTimeFromNTP();
    }
  }
  if (WiFi.status() == WL_CONNECTED) {
    static uint32_t lastWeatherAttempt = 0;
    if ((lastWeatherFetchAt == 0 || millis() - lastWeatherFetchAt > 30UL * 60UL * 1000UL) &&
        (millis() - lastWeatherAttempt > 60000UL || lastWeatherAttempt == 0)) {
      lastWeatherAttempt = millis();
      fetchWeather();
    }
    return;
  }
  if (millis() - lastTry < 15000)
    return;
  lastTry = millis();
  WiFi.reconnect();
}

bool syncTimeFromNTP() {
  if (WiFi.status() != WL_CONNECTED)
    return false;
  Serial.println("[NTP] Syncing...");
  configTime(TZ_OFFSET_SEC, TZ_DST_SEC, NTP_SERVER_1, NTP_SERVER_2);
  struct tm timeinfo;
  int tries = 0;
  while (!getLocalTime(&timeinfo, 500) && tries < 20) {
    tries++;
    delay(250);
  }
  if (tries >= 20) {
    Serial.println("[NTP] Failed");
    timeSynced = false;
    return false;
  }
  timeSynced = true;
  Serial.printf("[NTP] %02d:%02d:%02d %02d/%02d/%04d\n", timeinfo.tm_hour,
                timeinfo.tm_min, timeinfo.tm_sec, timeinfo.tm_mday,
                timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);
  return true;
}

bool suspendLocalNet() {
  bool apWasUp = apMode;
  if (apWasUp) {
    dnsServer.stop();
    WiFi.softAPdisconnect(true);
    apMode = false;
    delay(300);
  }
  server.stop();
  delay(100);
  return apWasUp;
}

void restoreWebAndAP(bool apWasUp) {
  if (apWasUp)
    startAPParallel();
  startWeb();
  Serial.printf("[NET] Free heap after restore: %u\n", (unsigned)ESP.getFreeHeap());
}

// ============================================================


void startWeb() {
  if (!webRoutesSet) {
    server.on("/", HTTP_GET, handleRoot);
    server.on("/save", HTTP_POST, handleSave);
    server.on("/faces", HTTP_GET, handleFacesPage);
    server.on("/wiring", HTTP_GET, handleWiring);
    server.on("/do", HTTP_GET, handleDoAction);
    server.on("/animations", HTTP_GET, []() {
      server.send(200, "application/json", animationListJson());
    });
    server.on(
        "/animation-upload", HTTP_POST,
        []() {
          server.send(customAnimationUploadResult ? 200 : 400,
                      "application/json",
                      customAnimationUploadResult
                          ? "{\"ok\":true}"
                          : "{\"ok\":false,\"error\":\"Expected 1024 bitmap bytes\"}");
        },
        []() {
          HTTPUpload &upload = server.upload();
          if (upload.status == UPLOAD_FILE_START) {
            stopAnimation();
            beginCustomAnimationUpload(upload.filename, server.arg("name"));
            customAnimationUploadResult = false;
          } else if (upload.status == UPLOAD_FILE_WRITE) {
            if (!writeCustomAnimationUpload(upload.buf, upload.currentSize))
              customAnimationUploadResult = false;
          } else if (upload.status == UPLOAD_FILE_END) {
            customAnimationUploadResult = finishCustomAnimationUpload();
          } else if (upload.status == UPLOAD_FILE_ABORTED) {
            customAnimationUploadResult = false;
          }
        });
    server.on("/status", HTTP_GET, handleStatus);
    server.on("/config", HTTP_GET, handleConfigApi);
    server.on("/update", HTTP_GET, handleOtaPage);
    server.on(
        "/update", HTTP_POST,
        []() {
          server.sendHeader("Connection", "close");
          if (Update.hasError()) {
            server.send(500, "text/plain", String("FAIL: ") + Update.errorString());
          } else {
            server.send(200, "text/plain", "OK");
            delay(1000);
            ESP.restart();
          }
        },
        []() {
          HTTPUpload &upload = server.upload();
          if (upload.status == UPLOAD_FILE_START) {
            Serial.printf("[OTA] Update: %s\n", upload.filename.c_str());
            if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
              Update.printError(Serial);
            }
          } else if (upload.status == UPLOAD_FILE_WRITE) {
            if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
              Update.printError(Serial);
            }
          } else if (upload.status == UPLOAD_FILE_END) {
            if (Update.end(true)) {
              Serial.printf("[OTA] Success: %u bytes\n", upload.totalSize);
            } else {
              Update.printError(Serial);
            }
          }
        });
    webRoutesSet = true;
  }
  server.begin();
}
