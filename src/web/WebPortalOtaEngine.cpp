#include "config/AppConfig.h"
#include "config/ConfigManager.h"
#include "face/FaceDisplay.h"
#include "face/FaceScreens.h"
#include "web/WebPortal.h"
#include <Arduino.h>
#include <HTTPClient.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

enum OtaState {
  OTA_IDLE = 0,
  OTA_STARTING,
  OTA_DOWNLOADING,
  OTA_FLASHING,
  OTA_SUCCESS,
  OTA_ERROR
};

static volatile OtaState s_otaState = OTA_IDLE;
static volatile int s_otaProgress = 0;
static volatile size_t s_otaBytesRead = 0;
static volatile size_t s_otaBytesTotal = 0;
static String s_otaError = "";
static String s_otaTargetUrl = "";
static volatile bool s_otaTaskActive = false;
static TaskHandle_t s_otaTaskHandle = NULL;

static void otaShowOled(const char *l1, const char *l2) {
  if (displayLock(80)) {
    drawMessage(l1, l2);
    displayUnlock();
  }
}

static void otaDownloadTask(void *param) {
  otaInProgress = true;
  s_otaTaskActive = true;
  s_otaState = OTA_STARTING;
  s_otaProgress = 0;
  s_otaBytesRead = 0;
  s_otaBytesTotal = 0;
  s_otaError = "";
  busyProcessing = true;
  lastProgressAt = millis();

  Serial.printf("[OTA-URL] Starting OTA from: %s\n", s_otaTargetUrl.c_str());
  otaShowOled("OTA UPDATE", "Connecting...");

  WiFiClientSecure client;
  client.setInsecure(); // GitHub & AWS use public TLS; disable strict cert
                        // validation for memory
  client.setTimeout(25000);

  HTTPClient http;
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  http.setRedirectLimit(5);
  http.setTimeout(25000);
  http.setUserAgent("KiwiRobot-ESP32-OTA");

  if (!http.begin(client, s_otaTargetUrl)) {
    s_otaState = OTA_ERROR;
    s_otaError = "Failed to begin HTTPClient connection";
    Serial.println("[OTA-URL] http.begin failed");
    otaShowOled("OTA ERROR", "Connect Failed");
    busyProcessing = false;
    s_otaTaskActive = false;
    otaInProgress = false;
    vTaskDelete(NULL);
    return;
  }

  int httpCode = http.GET();
  Serial.printf("[OTA-URL] HTTP GET result code: %d\n", httpCode);

  if (httpCode != HTTP_CODE_OK) {
    s_otaState = OTA_ERROR;
    s_otaError =
        "HTTP error " + String(httpCode) + ": " + http.errorToString(httpCode);
    Serial.printf("[OTA-URL] %s\n", s_otaError.c_str());
    otaShowOled("OTA ERROR", ("HTTP " + String(httpCode)).c_str());
    http.end();
    busyProcessing = false;
    s_otaTaskActive = false;
    otaInProgress = false;
    vTaskDelete(NULL);
    return;
  }

  int totalLen = http.getSize();
  s_otaBytesTotal = (totalLen > 0) ? (size_t)totalLen : 0;
  Serial.printf("[OTA-URL] Binary size: %d bytes\n", totalLen);

  if (totalLen > 0x1F0000L) { // Dual OTA app partition limit (1.98MB)
    s_otaState = OTA_ERROR;
    s_otaError = "Firmware binary too large (" + String(totalLen / 1024) +
                 " KB). Partition limit is 1.98MB.";
    Serial.printf("[OTA-URL] %s\n", s_otaError.c_str());
    otaShowOled("OTA ERROR", "File Too Large");
    http.end();
    busyProcessing = false;
    s_otaTaskActive = false;
    otaInProgress = false;
    vTaskDelete(NULL);
    return;
  }

  size_t updateSize = (totalLen > 0) ? (size_t)totalLen : UPDATE_SIZE_UNKNOWN;
  if (!Update.begin(updateSize, U_FLASH)) {
    s_otaState = OTA_ERROR;
    s_otaError = String("Update.begin failed: ") + Update.errorString();
    Serial.printf("[OTA-URL] %s\n", s_otaError.c_str());
    otaShowOled("OTA ERROR", Update.errorString());
    http.end();
    busyProcessing = false;
    s_otaTaskActive = false;
    otaInProgress = false;
    vTaskDelete(NULL);
    return;
  }

  s_otaState = OTA_DOWNLOADING;
  WiFiClient *stream = http.getStreamPtr();
  const size_t BUFF_SZ = 1024;
  uint8_t *buff = (uint8_t *)malloc(BUFF_SZ);
  if (!buff) {
    s_otaState = OTA_ERROR;
    s_otaError = "Out of memory allocating network stream buffer";
    Update.abort();
    http.end();
    busyProcessing = false;
    s_otaTaskActive = false;
    otaInProgress = false;
    vTaskDelete(NULL);
    return;
  }

  uint32_t lastDataTime = millis();
  uint32_t lastDisplayUpdate = 0;

  while (http.connected() &&
         (totalLen <= 0 || s_otaBytesRead < (size_t)totalLen)) {
    size_t avail = stream->available();
    if (avail > 0) {
      size_t want = avail > BUFF_SZ ? BUFF_SZ : avail;
      int bytesRead = stream->read(buff, want);
      if (bytesRead > 0) {
        lastDataTime = millis();
        lastProgressAt = lastDataTime;
        size_t written = Update.write(buff, bytesRead);
        if (written != (size_t)bytesRead) {
          s_otaState = OTA_ERROR;
          s_otaError = String("Flash write failed: ") + Update.errorString();
          Serial.printf("[OTA-URL] %s\n", s_otaError.c_str());
          Update.abort();
          break;
        }
        s_otaBytesRead += bytesRead;
        if (totalLen > 0) {
          s_otaProgress = (int)((s_otaBytesRead * 100UL) / (size_t)totalLen);
        }

        if (millis() - lastDisplayUpdate >= 250) {
          lastDisplayUpdate = millis();
          char line2[32];
          snprintf(line2, sizeof(line2), "%d%% (%d KB)", s_otaProgress,
                   (int)(s_otaBytesRead / 1024));
          otaShowOled("OTA UPDATING", line2);
        }
      }
    } else {
      if (millis() - lastDataTime > 25000) {
        s_otaState = OTA_ERROR;
        s_otaError = "Network stream timeout while downloading firmware";
        Serial.println("[OTA-URL] Stream timeout");
        Update.abort();
        break;
      }
      vTaskDelay(10 / portTICK_PERIOD_MS);
    }
  }

  free(buff);
  http.end();

  if (s_otaState != OTA_ERROR) {
    s_otaState = OTA_FLASHING;
    otaShowOled("OTA VERIFYING", "Checking flash...");

    if (Update.end(true)) {
      Serial.printf("[OTA-URL] Flashed %u bytes successfully!\n",
                    (unsigned)s_otaBytesRead);
      s_otaProgress = 100;
      s_otaState = OTA_SUCCESS;
      otaShowOled("OTA COMPLETE", "Rebooting Kiwi...");
      saveConfigImmediate();
      vTaskDelay(2800 / portTICK_PERIOD_MS);
      ESP.restart();
    } else {
      s_otaState = OTA_ERROR;
      s_otaError = String("Update.end failed: ") + Update.errorString();
      Serial.printf("[OTA-URL] %s\n", s_otaError.c_str());
      otaShowOled("OTA FAILED", Update.errorString());
    }
  }

  busyProcessing = false;
  s_otaTaskActive = false;
  otaInProgress = false;
  vTaskDelete(NULL);
}

void handleOtaStatus() {
  notifyWebRequest();
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");

  const char *stateStr = "idle";
  switch (s_otaState) {
  case OTA_STARTING:
    stateStr = "starting";
    break;
  case OTA_DOWNLOADING:
    stateStr = "downloading";
    break;
  case OTA_FLASHING:
    stateStr = "flashing";
    break;
  case OTA_SUCCESS:
    stateStr = "success";
    break;
  case OTA_ERROR:
    stateStr = "error";
    break;
  default:
    stateStr = "idle";
    break;
  }

  String json = "{";
  json += "\"state\":\"" + String(stateStr) + "\",";
  json += "\"progress\":" + String(s_otaProgress) + ",";
  json += "\"bytes_read\":" + String(s_otaBytesRead) + ",";
  json += "\"bytes_total\":" + String(s_otaBytesTotal) + ",";
  json += "\"running\":" + String(s_otaTaskActive ? "true" : "false") + ",";
  json += "\"error\":\"";
  for (size_t i = 0; i < s_otaError.length(); i++) {
    unsigned char uc = (unsigned char)s_otaError[i];
    if (uc == '"' || uc == '\\') {
      json += '\\';
      json += (char)uc;
    } else if (uc == '\n') {
      json += "\\n";
    } else if (uc == '\t') {
      json += "\\t";
    } else if (uc >= 0x20) {
      json += (char)uc;
    }
  }
  json += "\"}";

  server.send(200, "application/json", json);
}

void handleOtaInstall() {
  notifyWebRequest();
  server.sendHeader("Access-Control-Allow-Origin", "*");

  if (busyProcessing || s_otaTaskActive) {
    server.send(409, "application/json",
                "{\"ok\":false,\"error\":\"Robot is currently busy or OTA is "
                "already running.\"}");
    return;
  }

  String url = "";
  if (server.hasArg("url")) {
    url = server.arg("url");
  } else if (server.hasArg("plain")) {
    String plain = server.arg("plain");
    int urlIdx = plain.indexOf("\"url\"");
    if (urlIdx != -1) {
      int colon = plain.indexOf(':', urlIdx);
      int q1 = plain.indexOf('"', colon);
      int q2 = plain.indexOf('"', q1 + 1);
      if (q1 != -1 && q2 != -1) {
        url = plain.substring(q1 + 1, q2);
      }
    }
  }

  url.trim();
  if (url.length() == 0) {
    url = "https://github.com/krishh-9085/kiwi/releases/latest/download/KIWI_V1.2.bin";
  }
  if (!url.startsWith("http://") && !url.startsWith("https://")) {
    server.send(400, "application/json",
                "{\"ok\":false,\"error\":\"Invalid URL. Must start with "
                "http:// or https://\"}");
    return;
  }

  s_otaTargetUrl = url;
  s_otaState = OTA_STARTING;
  s_otaProgress = 0;
  s_otaBytesRead = 0;
  s_otaBytesTotal = 0;
  s_otaError = "";

  BaseType_t res = xTaskCreate(otaDownloadTask, "ota_url_task", 8192, NULL, 1,
                               &s_otaTaskHandle);
  if (res != pdPASS) {
    s_otaState = OTA_ERROR;
    s_otaError = "Failed to create FreeRTOS OTA background task";
    server.send(500, "application/json",
                "{\"ok\":false,\"error\":\"Failed to create OTA task\"}");
    return;
  }

  server.send(200, "application/json",
              "{\"ok\":true,\"message\":\"OTA update initiated\"}");
}
