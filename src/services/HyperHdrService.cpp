#include "services/HyperHdrService.h"
#include "services/WledService.h"
#include "config/AppConfig.h"
#include "core/Timing.h"
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static String s_discoveredHyperHdrIp = "";
static uint16_t s_discoveredHyperHdrPort = 8090;
static String s_manualHyperHdrIp = "";
static uint16_t s_manualHyperHdrPort = 8090;

static uint32_t s_lastHyperHdrDiscovery = 0;
static TaskHandle_t s_hyperHdrDiscoveryTaskHandle = NULL;
static volatile bool s_hyperHdrDiscoveryActive = false;
static bool s_screenSyncActive = true;

bool isHyperHdrDiscovered() {
  return (s_manualHyperHdrIp.length() > 0) || (s_discoveredHyperHdrIp.length() > 0);
}

String getHyperHdrIp() {
  if (s_manualHyperHdrIp.length() > 0)
    return s_manualHyperHdrIp;
  return s_discoveredHyperHdrIp;
}

uint16_t getHyperHdrPort() {
  if (s_manualHyperHdrIp.length() > 0)
    return s_manualHyperHdrPort;
  return s_discoveredHyperHdrPort;
}

void setHyperHdrManualHost(const String &ip, uint16_t port) {
  s_manualHyperHdrIp = ip;
  s_manualHyperHdrIp.trim();
  s_manualHyperHdrPort = (port > 0) ? port : 8090;
  if (s_manualHyperHdrIp.length() > 0) {
    s_discoveredHyperHdrIp = s_manualHyperHdrIp;
    s_discoveredHyperHdrPort = s_manualHyperHdrPort;
    Serial.printf("[HYPERHDR] Manual host set: %s:%u\n", s_manualHyperHdrIp.c_str(), s_manualHyperHdrPort);
  }
}

static void hyperHdrDiscoveryTask(void *param) {
  s_hyperHdrDiscoveryActive = true;
  Serial.println("[HYPERHDR] Auto-discovering HyperHDR in background via mDNS...");

  // 1. Primary mDNS service query for HyperHDR
  int n = MDNS.queryService("hyperhdr-http", "tcp");
  if (n > 0) {
    s_discoveredHyperHdrIp = MDNS.address(0).toString();
    uint16_t p = MDNS.port(0);
    s_discoveredHyperHdrPort = (p > 0) ? p : 8090;
    String host = MDNS.hostname(0);
    Serial.printf("[HYPERHDR] Found HyperHDR: %s:%u (%s)\n",
                  s_discoveredHyperHdrIp.c_str(), s_discoveredHyperHdrPort, host.c_str());
    addTerminalLog("[HYPERHDR] Service found at " + s_discoveredHyperHdrIp + ":" + String(s_discoveredHyperHdrPort));
    s_hyperHdrDiscoveryActive = false;
    s_hyperHdrDiscoveryTaskHandle = NULL;
    vTaskDelete(NULL);
    return;
  }

  // 2. Fallback query for hyperiond-http
  n = MDNS.queryService("hyperiond-http", "tcp");
  if (n > 0) {
    s_discoveredHyperHdrIp = MDNS.address(0).toString();
    uint16_t p = MDNS.port(0);
    s_discoveredHyperHdrPort = (p > 0) ? p : 8090;
    Serial.printf("[HYPERHDR] Found Hyperion service: %s:%u\n",
                  s_discoveredHyperHdrIp.c_str(), s_discoveredHyperHdrPort);
    addTerminalLog("[HYPERHDR] Server found at " + s_discoveredHyperHdrIp);
    s_hyperHdrDiscoveryActive = false;
    s_hyperHdrDiscoveryTaskHandle = NULL;
    vTaskDelete(NULL);
    return;
  }

  // 3. Fallback: Hostname query for 'hyperhdr.local'
  IPAddress hostIp = MDNS.queryHost("hyperhdr", 800);
  if (hostIp != INADDR_NONE && hostIp != IPAddress(0, 0, 0, 0)) {
    s_discoveredHyperHdrIp = hostIp.toString();
    s_discoveredHyperHdrPort = 8090;
    Serial.printf("[HYPERHDR] Resolved host 'hyperhdr.local': %s:8090\n", s_discoveredHyperHdrIp.c_str());
    addTerminalLog("[HYPERHDR] Server found at " + s_discoveredHyperHdrIp);
  } else {
    Serial.println("[HYPERHDR] No HyperHDR server found on network (scan complete)");
  }

  s_hyperHdrDiscoveryActive = false;
  s_hyperHdrDiscoveryTaskHandle = NULL;
  vTaskDelete(NULL);
}

bool isHyperHdrDiscoveryRunning() {
  return s_hyperHdrDiscoveryActive;
}

bool discoverHyperHdrAsync(bool force) {
  if (WiFi.status() != WL_CONNECTED)
    return false;

  if (s_manualHyperHdrIp.length() > 0) {
    s_discoveredHyperHdrIp = s_manualHyperHdrIp;
    s_discoveredHyperHdrPort = s_manualHyperHdrPort;
    return true;
  }

  if (s_discoveredHyperHdrIp.length() > 0 && !force)
    return true;

  if (s_hyperHdrDiscoveryActive)
    return true;

  if (!force && millis() - s_lastHyperHdrDiscovery < 15000)
    return false;
  s_lastHyperHdrDiscovery = millis();

  BaseType_t res = xTaskCreate(hyperHdrDiscoveryTask, "hhdr_disc_task", 4096, NULL, 1, &s_hyperHdrDiscoveryTaskHandle);
  if (res != pdPASS) {
    Serial.println("[HYPERHDR] Failed to spawn discovery task");
    s_hyperHdrDiscoveryActive = false;
    s_hyperHdrDiscoveryTaskHandle = NULL;
    return false;
  }
  return true;
}

bool discoverHyperHdr(bool force) {
  if (WiFi.status() != WL_CONNECTED)
    return false;

  if (s_manualHyperHdrIp.length() > 0) {
    s_discoveredHyperHdrIp = s_manualHyperHdrIp;
    s_discoveredHyperHdrPort = s_manualHyperHdrPort;
    return true;
  }

  if (s_discoveredHyperHdrIp.length() > 0 && !force)
    return true;

  if (!s_hyperHdrDiscoveryActive) {
    if (!discoverHyperHdrAsync(force)) {
      return isHyperHdrDiscovered();
    }
  }

  uint32_t t0 = millis();
  while (s_hyperHdrDiscoveryActive && (millis() - t0 < 3500)) {
    serviceUi();
    vTaskDelay(20 / portTICK_PERIOD_MS);
  }

  return isHyperHdrDiscovered();
}

bool sendHyperHdrJson(const String &jsonPayload, String *outResponse) {
  if (WiFi.status() != WL_CONNECTED)
    return false;

  String ip = getHyperHdrIp();
  if (!ip.length()) {
    if (!discoverHyperHdr(true))
      return false;
    ip = getHyperHdrIp();
  }
  if (!ip.length())
    return false;

  uint16_t port = getHyperHdrPort();
  HTTPClient http;
  WiFiClient client;
  http.setTimeout(1200);

  String url = "http://" + ip + ":" + String(port) + "/json-rpc";
  if (!http.begin(client, url))
    return false;

  http.addHeader("Content-Type", "application/json");
  int code = http.POST(jsonPayload);
  bool ok = false;
  if (code == 200 || code == 201) {
    ok = true;
    if (outResponse) {
      *outResponse = http.getString();
    }
    Serial.printf("[HYPERHDR] Command OK (%d): %s\n", code, jsonPayload.c_str());
  } else {
    Serial.printf("[HYPERHDR] Command failed (%d) url=%s body=%s\n", code, url.c_str(), jsonPayload.c_str());
    if (!s_manualHyperHdrIp.length()) {
      s_discoveredHyperHdrIp = ""; // allow re-scan next time
    }
  }
  http.end();
  return ok;
}

bool setHyperHdrComponentState(const String &component, bool state) {
  String body = "{\"command\":\"componentstate\",\"componentstate\":{\"component\":\"" +
                component + "\",\"state\":" + (state ? "true" : "false") + "}}";
  return sendHyperHdrJson(body);
}

bool setHyperHdrLedDevice(bool state) {
  return setHyperHdrComponentState("LEDDEVICE", state);
}

bool setHyperHdrPower(bool on) {
  if (on) {
    setHyperHdrComponentState("ALL", true);
    setHyperHdrComponentState("LEDDEVICE", true);
    return true;
  } else {
    clearHyperHdrPriority(50);
    setHyperHdrComponentState("ALL", false);
    return true;
  }
}

bool setHyperHdrScreenSync(bool enable) {
  if (enable) {
    clearHyperHdrPriority(50);
    setHyperHdrComponentState("ALL", true);
    setHyperHdrComponentState("LEDDEVICE", true);
    setHyperHdrComponentState("SYSTEMGRABBER", true);
    s_screenSyncActive = true;
    Serial.println("[HYPERHDR] Screen sync ENABLED");
    addTerminalLog("[HYPERHDR] Screen sync active");
    return true;
  } else {
    setHyperHdrComponentState("SYSTEMGRABBER", false);
    s_screenSyncActive = false;
    Serial.println("[HYPERHDR] Screen sync DISABLED");
    addTerminalLog("[HYPERHDR] Screen sync paused");
    return true;
  }
}

bool isHyperHdrScreenSyncActive() {
  return s_screenSyncActive;
}

bool setHyperHdrColor(uint8_t r, uint8_t g, uint8_t b, int priority, int durationMs) {
  setHyperHdrComponentState("ALL", true);
  setHyperHdrComponentState("LEDDEVICE", true);

  String body = "{\"command\":\"color\",\"priority\":" + String(priority) +
                ",\"color\":[" + String(r) + "," + String(g) + "," + String(b) + "]" +
                ",\"duration\":" + String(durationMs) +
                ",\"origin\":\"MyRobot\"}";
  s_screenSyncActive = false;
  return sendHyperHdrJson(body);
}

bool setHyperHdrEffect(const String &effectName, int priority, int durationMs) {
  setHyperHdrComponentState("ALL", true);
  setHyperHdrComponentState("LEDDEVICE", true);

  String body = "{\"command\":\"effect\",\"priority\":" + String(priority) +
                ",\"effect\":{\"name\":\"" + effectName + "\"}" +
                ",\"duration\":" + String(durationMs) +
                ",\"origin\":\"MyRobot\"}";
  s_screenSyncActive = false;
  return sendHyperHdrJson(body);
}

bool clearHyperHdrPriority(int priority) {
  String body = "{\"command\":\"clear\",\"priority\":" + String(priority) + "}";
  return sendHyperHdrJson(body);
}

bool setHyperHdrBrightness(uint8_t brightness) {
  int val = constrain((int)brightness, 0, 100);
  String body = "{\"command\":\"adjustment\",\"adjustment\":{\"brightness\":" + String(val) + "}}";
  return sendHyperHdrJson(body);
}

bool setHyperHdrHdr(bool enable) {
  String body = "{\"command\":\"hdr\",\"hdr\":" + String(enable ? 1 : 0) + "}";
  return sendHyperHdrJson(body);
}

bool executeUnifiedLightAction(const String &paramIn) {
  String p = paramIn;
  p.trim();
  p.toLowerCase();

  if (!p.length())
    return false;

  // 1. Explicit Screen Sync / Ambilight commands
  if (p == "screensync" || p == "sync" || p == "ambilight" || p == "pc" || p == "screen") {
    return setHyperHdrScreenSync(true);
  }
  if (p == "screensyncoff" || p == "syncoff" || p == "ambilightoff" || p == "standalone") {
    return setHyperHdrScreenSync(false);
  }
  if (p == "hdr on" || p == "hdr") {
    return setHyperHdrHdr(true);
  }
  if (p == "hdr off") {
    return setHyperHdrHdr(false);
  }

  // 2. Power On / Off
  if (p == "off" || p == "0" || p == "false" || p == "stop" || p == "band" || p == "turn off") {
    if (isHyperHdrDiscovered()) {
      clearHyperHdrPriority(50);
      setHyperHdrComponentState("ALL", false);
      setHyperHdrLedDevice(false);
    }
    return setWledPower(false);
  }

  if (p == "on" || p == "1" || p == "true" || p == "start" || p == "chalu" || p == "turn on") {
    if (isHyperHdrDiscovered()) {
      setHyperHdrPower(true);
      clearHyperHdrPriority(50);
    }
    return setWledPower(true);
  }

  // 3. Brightness
  if (p.startsWith("bri") || p.indexOf("brightness") >= 0 || p.endsWith("%") ||
      p == "dim" || p == "low" || p == "kam" || p == "full" || p == "tez" || p == "medium") {
    int val = 50;
    if (p == "dim" || p == "low" || p == "kam") val = 18;
    else if (p == "full" || p == "tez") val = 100;
    else if (p == "medium") val = 50;
    else {
      for (int i = 0; i < (int)p.length(); i++) {
        if (isdigit(p[i])) {
          int v = 0;
          while (i < (int)p.length() && isdigit(p[i])) {
            v = v * 10 + (p[i] - '0');
            i++;
          }
          val = constrain(v, 0, 100);
          break;
        }
      }
    }
    if (isHyperHdrDiscovered()) {
      setHyperHdrBrightness(val);
    }
    uint8_t wledBri = (uint8_t)((val * 255) / 100);
    return setWledBrightness(wledBri);
  }

  // 4. Colors - If HyperHDR is running, inject priority override so it displays immediately!
  uint8_t r = 0, g = 0, b = 0;
  bool isColor = false;
  if (p.indexOf("red") >= 0 || p.indexOf("laal") >= 0) { r = 255; g = 0; b = 0; isColor = true; }
  else if (p.indexOf("green") >= 0 || p.indexOf("hara") >= 0) { r = 0; g = 255; b = 0; isColor = true; }
  else if (p.indexOf("blue") >= 0 || p.indexOf("neela") >= 0) { r = 0; g = 80; b = 255; isColor = true; }
  else if (p.indexOf("yellow") >= 0 || p.indexOf("peela") >= 0) { r = 255; g = 200; b = 0; isColor = true; }
  else if (p.indexOf("orange") >= 0 || p.indexOf("narangi") >= 0) { r = 255; g = 90; b = 0; isColor = true; }
  else if (p.indexOf("pink") >= 0 || p.indexOf("gulabi") >= 0) { r = 255; g = 40; b = 140; isColor = true; }
  else if (p.indexOf("purple") >= 0 || p.indexOf("violet") >= 0 || p.indexOf("baingani") >= 0) { r = 160; g = 0; b = 255; isColor = true; }
  else if (p.indexOf("cyan") >= 0 || p.indexOf("aqua") >= 0 || p.indexOf("firozi") >= 0) { r = 0; g = 255; b = 255; isColor = true; }
  else if (p.indexOf("white") >= 0 || p.indexOf("safed") >= 0) { r = 255; g = 255; b = 255; isColor = true; }
  else if (p.indexOf("warm") >= 0) { r = 255; g = 160; b = 60; isColor = true; }
  else if (p.indexOf("cool") >= 0) { r = 200; g = 220; b = 255; isColor = true; }
  else {
    int tr = 0, tg = 0, tb = 0;
    if (sscanf(p.c_str(), "%d %d %d", &tr, &tg, &tb) == 3) {
      r = constrain(tr, 0, 255);
      g = constrain(tg, 0, 255);
      b = constrain(tb, 0, 255);
      isColor = true;
    }
  }

  if (isColor) {
    if (isHyperHdrDiscovered()) {
      setHyperHdrColor(r, g, b, 50);
    }
    setWledColor(r, g, b);
    return true;
  }

  // 5. Effects / Animations
  // When running a standalone WLED animation, pause HyperHDR's LED streaming so WLED isn't overridden!
  if (isHyperHdrDiscovered()) {
    setHyperHdrLedDevice(false);
  }
  return executeWledAction(p);
}

