#include "web/WebPortal.h"
#include "animation/AnimationEngine.h"
#include "config/AppConfig.h"
#include "config/ConfigManager.h"
#include "face/FaceEngine.h"
#include "hardware/AudioEngine.h"
#include "services/AiService.h"
#include "services/MemoryService.h"
#include "services/WeatherService.h"
#include "web/icon.h"
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>
#include <time.h>

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
// mDNS & AUTHENTICATION HELPERS
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

String maskSecret(const String &s) {
  if (s.length() == 0)
    return "";
  if (s.length() <= 4)
    return "****";
  return "****" + s.substring(s.length() - 4);
}

bool isMaskedSecret(const String &s) {
  return s.startsWith("****") || s.startsWith("••••");
}

bool constantTimeStringEquals(const String &a, const String &b) {
  size_t lenA = a.length();
  size_t lenB = b.length();
  size_t maxLen = (lenA > lenB) ? lenA : lenB;
  volatile uint8_t diff = (lenA ^ lenB);
  for (size_t i = 0; i < maxLen; i++) {
    char ca = (i < lenA) ? a[i] : 0;
    char cb = (i < lenB) ? b[i] : 0;
    diff |= (uint8_t)(ca ^ cb);
  }
  return (diff == 0);
}

bool isAuthorized() {
  if (portalToken.length() == 0) {
    return true;
  }

  // Check Cookie header: "kiwi_auth=<token>"
  if (server.hasHeader("Cookie")) {
    String cookieHeader = server.header("Cookie");
    int idx = cookieHeader.indexOf("kiwi_auth=");
    if (idx >= 0) {
      int start = idx + 10;
      int end = cookieHeader.indexOf(';', start);
      String tok = (end == -1) ? cookieHeader.substring(start)
                               : cookieHeader.substring(start, end);
      tok.trim();
      if (constantTimeStringEquals(tok, portalToken)) {
        return true;
      }
    }
  }

  // Check X-Kiwi-Token header
  if (server.hasHeader("X-Kiwi-Token")) {
    String hdrTok = server.header("X-Kiwi-Token");
    hdrTok.trim();
    if (constantTimeStringEquals(hdrTok, portalToken)) {
      return true;
    }
  }

  // Auth failure rate limited to once per 10 seconds
  static uint32_t lastAuthFailLogAt = 0;
  uint32_t now = millis();
  if (now - lastAuthFailLogAt >= 10000) {
    lastAuthFailLogAt = now;
    Serial.printf("[AUTH] auth fail from %s\n",
                  server.client().remoteIP().toString().c_str());
  }
  return false;
}

void handleLogin() {
  if (server.method() == HTTP_POST && server.hasArg("token")) {
    String candidate = server.arg("token");
    candidate.trim();
    if (constantTimeStringEquals(candidate, portalToken)) {
      server.sendHeader("Set-Cookie",
                        "kiwi_auth=" + portalToken +
                            "; Path=/; HttpOnly; SameSite=Strict");
      server.sendHeader("Location", "/");
      server.send(302, "text/plain", "Authorized");
      return;
    } else {
      static uint32_t lastAuthFailLogAt = 0;
      uint32_t now = millis();
      if (now - lastAuthFailLogAt >= 10000) {
        lastAuthFailLogAt = now;
        Serial.printf("[AUTH] auth fail from %s\n",
                      server.client().remoteIP().toString().c_str());
      }
    }
  }

  String html =
      "<!DOCTYPE html><html><head><meta charset='utf-8'>"
      "<meta name='viewport' content='width=device-width,initial-scale=1'>"
      "<title>Kiwi - Login</title>"
      "<style>"
      "body{margin:0;background:#0d1117;color:#e6edf3;font-family:-apple-"
      "system,BlinkMacSystemFont,Segoe "
      "UI,Helvetica,Arial,sans-serif;display:flex;align-items:center;justify-"
      "content:center;min-height:100vh;padding:20px;box-sizing:border-box}"
      ".box{background:#161b22;border:1px solid "
      "#30363d;border-radius:12px;padding:32px "
      "28px;width:100%;max-width:360px;box-shadow:0 8px 24px rgba(0,0,0,0.4)}"
      "h1{margin:0 0 8px 0;font-size:22px;color:#f2b45c;text-align:center}"
      "p{margin:0 0 20px "
      "0;font-size:13px;color:#8b949e;text-align:center;line-height:1.4}"
      "input{width:100%;padding:12px "
      "14px;box-sizing:border-box;border-radius:8px;border:1px solid "
      "#30363d;background:#0d1117;color:#fff;font-size:14px;margin-bottom:16px;"
      "outline:none}"
      "input:focus{border-color:#f2b45c}"
      "button{width:100%;padding:12px;background:#f2b45c;border:none;border-"
      "radius:8px;color:#161b22;font-size:14px;font-weight:700;cursor:pointer;"
      "transition:background .2s}"
      "button:hover{background:#ffc77d}"
      ".err{color:#ff7b72;font-size:12px;text-align:center;margin-bottom:12px}"
      "</style></head><body>"
      "<div class='box'>"
      "<h1>Unlock Kiwi</h1>"
      "<p>Enter your 32-character Portal Token to access settings</p>";
  if (server.method() == HTTP_POST) {
    html += "<div class='err'>Invalid token. Check Serial log from boot.</div>";
  }
  html += "<form method='POST' action='/login'>"
          "<input type='password' name='token' placeholder='Paste Portal "
          "Token' required autofocus autocomplete='off'>"
          "<button type='submit'>Unlock</button>"
          "</form></div></body></html>";

  server.send(server.method() == HTTP_POST ? 401 : 200, "text/html", html);
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
    initMdns("STA");
    drawConnectedNotice(wifiSSID.c_str(), 800);
    return true;
  }
  return false;
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
    }
    static uint32_t lastWeatherAttempt = 0;
    if ((lastWeatherFetchAt == 0 ||
         millis() - lastWeatherFetchAt > 30UL * 60UL * 1000UL) &&
        (millis() - lastWeatherAttempt > 60000UL || lastWeatherAttempt == 0)) {
      if (millis() > 3000) {
        lastWeatherAttempt = millis();
        fetchWeather();
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

void startNtpAsync() {
  if (WiFi.status() != WL_CONNECTED)
    return;
  Serial.println("[NTP] Background SNTP configured");
  configTime(TZ_OFFSET_SEC, TZ_DST_SEC, NTP_SERVER_1, NTP_SERVER_2);
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
    delay(50);
  }
  return false;
}

static int netSuspendDepth = 0;
static bool originalApState = false;

bool suspendLocalNet() {
  if (netSuspendDepth++ == 0) {
    originalApState = apMode;
    if (originalApState) {
      dnsServer.stop();
      WiFi.softAPdisconnect(true);
      apMode = false;
      delay(20);
    }
    // Note: Do NOT stop the WebServer during AI calls in STA mode.
    // Stopping server causes any in-flight web dashboard requests (like /save,
    // /status) to instantly fail with "Failed to fetch" / connection refused
    // while Kiwi is speaking.
  }
  return originalApState;
}

void restoreWebAndAP(bool apWasUp) {
  if (--netSuspendDepth <= 0) {
    netSuspendDepth = 0;
    if (apWasUp || originalApState)
      startAPParallel();
    // In STA mode the server was never stopped, so no need to restart it.
    // If not running, make sure it is up:
    startWeb();
    Serial.printf("[NET] Free heap: %u\n", (unsigned)ESP.getFreeHeap());
  }
}

// ============================================================

void startWeb() {
  if (!webRoutesSet) {
    const char *headers[] = {"Cookie", "X-Kiwi-Token", "Content-Length"};
    server.collectHeaders(headers, 3);

    // Apple Touch Icon & Favicon (Direct PROGMEM binary delivery, no template
    // processor)
    auto handleAppleIcon = []() {
      server.sendHeader("Access-Control-Allow-Origin", "*");
      server.sendHeader("Connection", "close");
      server.sendHeader("Content-Type", "image/png");
      server.sendHeader("Cache-Control", "public, max-age=86400");
      server.send_P(200, "image/png", (const char *)APPLE_ICON, APPLE_ICON_LEN);
    };
    server.on("/apple-touch-icon.png", HTTP_GET, handleAppleIcon);
    server.on("/apple-touch-icon-precomposed.png", HTTP_GET, handleAppleIcon);
    server.on("/favicon.ico", HTTP_GET, handleAppleIcon);

    server.on("/login", HTTP_ANY, handleLogin);

    server.on("/", HTTP_GET, []() {
      if (!isAuthorized()) {
        server.sendHeader("Location", "/login");
        server.send(302, "text/plain", "Please log in");
        return;
      }
      handleRoot();
    });

    server.on("/save", HTTP_POST, handleSave);

    server.on("/faces", HTTP_GET, []() {
      if (!isAuthorized()) {
        server.sendHeader("Location", "/login");
        server.send(302, "text/plain", "Please log in");
        return;
      }
      handleFacesPage();
    });

    server.on("/wiring", HTTP_GET, []() {
      if (!isAuthorized()) {
        server.sendHeader("Location", "/login");
        server.send(302, "text/plain", "Please log in");
        return;
      }
      handleWiring();
    });

    server.on("/do", HTTP_GET, handleDoAction);

    server.on("/animations", HTTP_GET, []() {
      server.send(200, "application/json", animationListJson());
    });

    server.on(
        "/animation-upload", HTTP_POST,
        []() {
          if (!isAuthorized()) {
            server.send(401, "application/json",
                        "{\"ok\":false,\"error\":\"Unauthorized\"}");
            return;
          }
          server.send(
              customAnimationUploadResult ? 200 : 400, "application/json",
              customAnimationUploadResult
                  ? "{\"ok\":true}"
                  : "{\"ok\":false,\"error\":\"Expected 1024 bitmap bytes\"}");
        },
        []() {
          if (!isAuthorized())
            return;
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
    server.on("/scan", HTTP_GET, handleScanNetworks);

    server.on("/update", HTTP_GET, []() {
      if (!isAuthorized()) {
        server.sendHeader("Location", "/login");
        server.send(302, "text/plain", "Please log in");
        return;
      }
      handleOtaPage();
    });

    server.on(
        "/update", HTTP_POST,
        []() {
          if (!isAuthorized()) {
            server.send(401, "text/plain", "Unauthorized");
            return;
          }
          if (busyProcessing) {
            server.send(503, "text/plain",
                        "FAIL: Robot is busy processing audio/AI. Please wait "
                        "or touch to cancel.");
            return;
          }
          server.sendHeader("Connection", "close");
          if (Update.hasError()) {
            server.send(500, "text/plain",
                        String("FAIL: ") + Update.errorString());
          } else {
            server.send(200, "text/plain", "OK");
            delay(500);
            ESP.restart();
          }
        },
        []() {
          if (!isAuthorized() || busyProcessing) {
            Update.abort();
            return;
          }
          HTTPUpload &upload = server.upload();
          if (upload.status == UPLOAD_FILE_START) {
            Serial.printf("[OTA] Update: %s\n", upload.filename.c_str());
            if (server.hasHeader("Content-Length") && (size_t)server.header("Content-Length").toInt() > 0x1E0000UL) {
              Serial.println("[OTA] Update rejected: upload size exceeds app "
                             "partition (1.9MB)");
              Update.abort();
              return;
            }
            if (!Update.begin(0x1E0000UL)) {
              Update.printError(Serial);
            }
          } else if (upload.status == UPLOAD_FILE_WRITE) {
            if (upload.totalSize > 0x1E0000UL) {
              Serial.println(
                  "[OTA] Update rejected: written size exceeds app partition");
              Update.abort();
              return;
            }
            if (Update.write(upload.buf, upload.currentSize) !=
                upload.currentSize) {
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

    // Captive Portal probes redirect to root (192.168.4.1) for Android, Apple,
    // Windows
    auto handleCaptiveRedirect = []() {
      server.sendHeader("Location", String("http://") + apIP.toString() + "/",
                        true);
      server.send(302, "text/plain", "");
    };
    server.on("/generate_204", HTTP_GET, handleCaptiveRedirect);
    server.on("/gen_204", HTTP_GET, handleCaptiveRedirect);
    server.on("/ncsi.txt", HTTP_GET, handleCaptiveRedirect);
    server.on("/hotspot-detect.html", HTTP_GET, handleCaptiveRedirect);
    server.on("/canonical.html", HTTP_GET, handleCaptiveRedirect);
    server.on("/connecttest.txt", HTTP_GET, handleCaptiveRedirect);

    server.onNotFound([]() {
      if (apMode) {
        server.sendHeader("Location", String("http://") + apIP.toString() + "/",
                          true);
        server.send(302, "text/plain", "");
        return;
      }
      server.send(404, "text/plain", "Not Found");
    });

    server.enableCORS(true);
    webRoutesSet = true;
  }
  server.begin();
}
