#include "web/WebPortal.h"
#include "config/AppConfig.h"
#include "config/ConfigManager.h"
#include "face/AnimationEngine.h"
#include "net/WifiManager.h"
#include "power/PowerManager.h"
#include "web/dashboard_html_gz.h"
#include "web/icon.h"
#include <Update.h>
#include <WebServer.h>

WebServer server(80);
volatile int8_t pendingSound = -1;
volatile bool otaInProgress = false;
volatile uint32_t lastWebRequestAt = 0;
static bool webRoutesSet = false;
static bool customAnimationUploadResult = false;
static bool manualOtaRejectedBusy = false;

void notifyWebRequest() {
  lastWebRequestAt = millis();
  powerNotifyActivity();
}

void pumpWeb() {
  pumpDns();
  server.handleClient();
}

// ============================================================
// SECRET MASKING HELPERS
// ============================================================
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

// ============================================================
// SINGLE-PAGE PREACT DASHBOARD DELIVERY
// ============================================================
void handleRoot() {
  notifyWebRequest();

  bool forceFresh = server.hasArg("_r");
  String etag =
      "\"" + String(FIRMWARE_VER) + "-" + String(DASHBOARD_HTML_GZ_LEN) + "\"";

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Cache-Control",
                    forceFresh ? "no-cache, no-store, must-revalidate"
                               : "public, max-age=3600, must-revalidate");
  server.sendHeader("ETag", etag);

  if (!forceFresh && server.hasHeader("If-None-Match")) {
    String reqEtag = server.header("If-None-Match");
    if (reqEtag == etag || reqEtag.indexOf(etag) >= 0 || reqEtag == "*") {
      server.send(304, "text/plain", "");
      return;
    }
  }

  server.sendHeader("Connection", "close");
  server.sendHeader("Content-Encoding", "gzip");
  server.send_P(200, "text/html; charset=utf-8",
                (const char *)DASHBOARD_HTML_GZ, DASHBOARD_HTML_GZ_LEN);
}

// ============================================================
// WEB SERVER ROUTING
// ============================================================
void startWeb() {
  if (!webRoutesSet) {
    const char *headers[] = {"Content-Length", "If-None-Match"};
    server.collectHeaders(headers, 2);

    // Apple Touch Icon & Favicon (Direct PROGMEM binary delivery)
    auto handleAppleIcon = []() {
      notifyWebRequest();
      server.sendHeader("Access-Control-Allow-Origin", "*");
      server.sendHeader("Connection", "close");
      server.sendHeader("Content-Type", "image/png");
      server.sendHeader("Cache-Control", "public, max-age=86400");
      server.send_P(200, "image/png", (const char *)APPLE_ICON, APPLE_ICON_LEN);
    };
    server.on("/apple-touch-icon.png", HTTP_GET, handleAppleIcon);
    server.on("/apple-touch-icon-precomposed.png", HTTP_GET, handleAppleIcon);
    server.on("/favicon.ico", HTTP_GET, handleAppleIcon);
    server.on("/manifest.json", HTTP_GET, []() {
      notifyWebRequest();
      server.sendHeader("Access-Control-Allow-Origin", "*");
      server.sendHeader("Cache-Control", "no-cache");
      server.send(200, "application/manifest+json",
                  "{\"name\":\"Kiwi "
                  "Robot\",\"short_name\":\"Kiwi\",\"start_url\":\"/"
                  "\",\"display\":\"standalone\",\"background_color\":\"#"
                  "0e1015\",\"theme_color\":\"#0e1015\",\"icons\":[{\"src\":\"/"
                  "apple-touch-icon.png?v=white\",\"sizes\":\"180x180\","
                  "\"type\":\"image/png\",\"purpose\":\"any maskable\"}]}");
    });

    server.on("/", HTTP_GET, handleRoot);
    server.on("/faces", HTTP_GET, handleRoot);
    server.on("/update", HTTP_GET, handleRoot);
    server.on("/wiring", HTTP_GET, []() {
      notifyWebRequest();
      server.sendHeader("Location", "/", true);
      server.send(302, "text/plain", "");
    });

    server.on("/save", HTTP_POST, handleSave);

    server.on("/do", HTTP_GET, handleDoAction);
    server.on("/do", HTTP_POST, handleDoAction);

    server.on("/animations", HTTP_GET, []() {
      notifyWebRequest();
      server.send(200, "application/json", animationListJson());
    });

    server.on(
        "/animation-upload", HTTP_POST,
        []() {
          server.send(
              customAnimationUploadResult ? 200 : 400, "application/json",
              customAnimationUploadResult
                  ? "{\"ok\":true}"
                  : "{\"ok\":false,\"error\":\"Expected 1024 bitmap bytes (or all 12 slots full)\"}");
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
            cancelCustomAnimationUpload();
            customAnimationUploadResult = false;
          }
        });

    server.on("/status", HTTP_GET, handleStatus);
    server.on("/logs", HTTP_GET, handleLogs);
    server.on("/config", HTTP_GET, handleConfigApi);
    server.on("/scan", HTTP_GET, handleScanNetworks);

    server.on("/ota/status", HTTP_GET, handleOtaStatus);
    server.on("/ota/install", HTTP_POST, handleOtaInstall);
    server.on("/ota/rollback", HTTP_POST, handleOtaRollback);

    server.on(
        "/update", HTTP_POST,
        []() {
          notifyWebRequest();
          otaInProgress = false;
          if (manualOtaRejectedBusy) {
            manualOtaRejectedBusy = false;
            busyProcessing = false;
            Update.abort();
            server.send(503, "text/plain",
                        "FAIL: Robot is busy processing audio/AI. Please wait "
                        "or touch to cancel.");
            return;
          }
          busyProcessing = false;
          server.sendHeader("Connection", "close");
          if (Update.hasError()) {
            server.send(500, "text/plain",
                        String("FAIL: ") + Update.errorString());
          } else {
            saveConfigImmediate();
            server.send(200, "text/plain", "OK");
            delay(500);
            ESP.restart();
          }
        },
        []() {
          notifyWebRequest();
          HTTPUpload &upload = server.upload();
          if (upload.status == UPLOAD_FILE_START) {
            if (busyProcessing || otaInProgress) {
              manualOtaRejectedBusy = true;
              otaInProgress = false;
              Update.abort();
              return;
            }
            manualOtaRejectedBusy = false;
            otaInProgress = true;
            busyProcessing = true;
            lastProgressAt = millis();
            Serial.printf("[OTA] Update: %s\n", upload.filename.c_str());
            if (server.hasHeader("Content-Length") &&
                (size_t)server.header("Content-Length").toInt() > (0x1F0000UL + 8192UL)) {
              Serial.println("[OTA] Update rejected: upload size exceeds app "
                             "partition (1.98MB)");
              otaInProgress = false;
              busyProcessing = false;
              Update.abort();
              return;
            }
            if (!Update.begin(0x1F0000UL)) {
              otaInProgress = false;
              busyProcessing = false;
              Update.printError(Serial);
            }
          } else if (upload.status == UPLOAD_FILE_WRITE) {
            if (manualOtaRejectedBusy || !otaInProgress) {
              Update.abort();
              return;
            }
            lastProgressAt = millis();
            if (upload.totalSize + upload.currentSize > 0x1F0000UL) {
              Serial.println(
                  "[OTA] Update rejected: written size exceeds app partition");
              otaInProgress = false;
              busyProcessing = false;
              Update.abort();
              return;
            }
            if (Update.write(upload.buf, upload.currentSize) !=
                upload.currentSize) {
              otaInProgress = false;
              busyProcessing = false;
              Update.printError(Serial);
              Update.abort();
            }
          } else if (upload.status == UPLOAD_FILE_END) {
            otaInProgress = false;
            busyProcessing = false;
            if (manualOtaRejectedBusy || upload.totalSize < 4096) {
              Serial.println("[OTA] Update aborted: rejected or truncated payload");
              Update.abort();
            } else if (Update.end(true)) {
              Serial.printf("[OTA] Success: %u bytes\n", upload.totalSize);
            } else {
              Update.printError(Serial);
            }
          } else if (upload.status == UPLOAD_FILE_ABORTED) {
            otaInProgress = false;
            busyProcessing = false;
            manualOtaRejectedBusy = false;
            Update.abort();
          }
        });

    // Captive Portal probes redirect to root (192.168.4.1) for Android, Apple,
    // Windows
    auto handleCaptiveRedirect = []() {
      notifyWebRequest();
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
