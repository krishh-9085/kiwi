#include "animation/AnimationEngine.h"
#include "behavior/ActionExecutor.h"
#include "behavior/HungerEngine.h"
#include "config/AppConfig.h"
#include "face/FaceEngine.h"
#include "hardware/AudioEngine.h"
#include "hardware/RgbLed.h"
#include "hardware/WakeWordEngine.h"
#include "services/AiService.h"
#include "services/MemoryService.h"
#include "web/WebPortal.h"
#include "web/WebPortalApiInternal.h"
#include <WebServer.h>
#include <WiFi.h>
#include <algorithm>
#include <esp_ota_ops.h>
#include <vector>

String escJson(String s) {
  s.replace("\\", "\\\\");
  s.replace("\"", "\\\"");
  s.replace("\r", "");
  s.replace("\n", "\\n");
  return s;
}

static uint32_t lastStatusGen = 0;
static String cachedStatusJson = "";

void invalidateStatusCache() {
  cachedStatusJson = "";
  lastStatusGen = 0;
}

EyeSkin parseEyeSkinArg(const String &raw) {
  String s = raw;
  s.toLowerCase();
  if (s == "kiwi" || s == "deskbuddy" || s == "buddy" || s == "cute" ||
      s == "relaxed" || s == "bored" || s == "vector" || s == "anki" ||
      s == "squircle_solid" || s == "squircle_bold" || s == "squircle")
    return SKIN_KIWI;
  if (s == "pixel" || s == "8bit" || s == "retro" || s == "retro8bit")
    return SKIN_PIXEL;
  if (s == "squircle_glint" || s == "lumina_spark" || s == "spark" ||
      s == "glint")
    return SKIN_LUMINA_SPARK;
  if (s == "mecha" || s == "circtangle" || s == "circrect")
    return SKIN_MECHA;
  if (s == "emo" || s == "emo_style" || s == "emo-style" || s == "emostyle")
    return SKIN_EMO;
  return SKIN_KIWI;
}

void handleStatus() {
  uint32_t now = millis();
  // Fast throttle cache: if multiple devices (laptop + phone) poll
  // simultaneously, return cached JSON immediately to eliminate redundant
  // string formatting & CPU load.
  if (now - lastStatusGen < 350 && cachedStatusJson.length() > 0) {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Connection", "close");
    server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    server.send(200, "application/json", cachedStatusJson);
    return;
  }

  static const char *names[] = {
      "idle",      "listening",  "thinking", "speaking", "happy", "excited",
      "sad",       "confused",   "angry",    "sleeping", "error", "petted",
      "surprised", "wink",       "smug",     "curious",  "bored", "laughing",
      "scared",    "suspicious", "relieved", "proud",    "dizzy", "lovestruck",
      "evil",      "crying",     "hearts",   "dead",     "cool",  "shocked",
      "singing",   "party",      "nervous",  "cute"};
  int idx = (int)currentEmotion;
  if (idx < 0 || idx > 33)
    idx = 0;
  String j;
  j.reserve(1024);
  j = "{\"host\":\"" + escJson(mdnsHostname + ".local") + "\",\"emotion\":\"" +
      String(names[idx]) + "\",\"busy\":";
  j += busyProcessing ? "true," : "false,";
  j += "\"sleeping\":";
  j += (inCircadianSleep || manualSleep) ? "true," : "false,";
  j += "\"history\":" + String(getConversationHistoryCount());
  const char *skinNames[SKIN_COUNT] = {"kiwi", "pixel", "squircle_glint",
                                       "mecha", "emo"};
  int skIdx = (int)currentEyeSkin;
  if (skIdx < 0 || skIdx >= SKIN_COUNT)
    skIdx = 0;
  j += ",\"boredom\":" + String(boredomEnabled ? "true" : "false");
  j += ",\"boredom_mins\":" + String(boredomMinutes);
  j += ",\"skin\":\"" + String(skinNames[skIdx]) + "\"";
  j += ",\"mouth\":" + String(showMouth ? "true" : "false");
  j += ",\"greetings\":{\"enabled\":" +
       String(greetingsEnabled ? "true" : "false") + "}";
  // Status JSON: soundReactive (snd_react)
  j += ",\"soundReactive\":{\"enabled\":" +
       String(soundReactiveEnabled ? "true" : "false") + "}";
  j += ",\"wakeWord\":{\"enabled\":" +
       String(wakeWordEnabled ? "true" : "false") +
       ",\"model\":" + String(wakeWordModel) + ",\"name\":\"" +
       String(wakeWordGetModelName()) + "\"" +
       ",\"cutoff\":" + String(wakeWordCutoff, 2) +
       ",\"listening\":" + String(wakeWordIsListening() ? "true" : "false") +
       "}";
  j += ",\"rgbLed\":{\"enabled\":" + String(rgbLedEnabled ? "true" : "false") +
       ",\"brightness\":" + String((int)((rgbLedBrightness * 100) / 255)) + "}";
  j += ",\"timer_active\":" + String(isRobotTimerActive() ? "true" : "false");
  j += ",\"timer_ringing\":" + String(isRobotTimerRinging() ? "true" : "false");
  j += ",\"timer_remaining\":" + String(getRobotTimerRemainingSec());
  j += ",\"hunger\":\"" +
       String(isKiwiAngry() ? "angry"
                            : (isKiwiHungry() ? "hungry" : "satisfied")) +
       "\"";
  j += ",\"hunger_sec\":" + String(getHungerRemainingSec());
  j += ",\"meal\":\"" + String(getCurrentMealName()) + "\"";
  j += ",\"state\":\"" + escJson(robotStateDesc) + "\"";
  j += ",\"question\":\"" + escJson(lastTranscribedQuestion) + "\"";
  j += ",\"answer\":\"" + escJson(lastLlmAnswer) + "\"";
  j += ",\"version\":\"" + String(FIRMWARE_VER) + "\"";

  // Real-time hardware telemetry: Heap RAM & On-chip CPU Temperature
  uint32_t freeHeap = ESP.getFreeHeap();
  uint32_t totalHeap = ESP.getHeapSize();
  uint32_t usedHeap = totalHeap > freeHeap ? totalHeap - freeHeap : 0;
  int heapPct = totalHeap > 0 ? (int)((usedHeap * 100) / totalHeap) : 0;
  float cpuTemp = temperatureRead();
  j += ",\"heap\":{\"free\":" + String(freeHeap) +
       ",\"total\":" + String(totalHeap) + ",\"used\":" + String(usedHeap) +
       ",\"pct\":" + String(heapPct) + "}";
  j += ",\"temp\":" + String(cpuTemp, 1);
  if (ESP.getPsramSize() > 0) {
    uint32_t freePsram = ESP.getFreePsram();
    uint32_t totalPsram = ESP.getPsramSize();
    uint32_t usedPsram = totalPsram > freePsram ? totalPsram - freePsram : 0;
    int psramPct =
        totalPsram > 0 ? (int)(((uint64_t)usedPsram * 100) / totalPsram) : 0;
    j += ",\"psram\":{\"free\":" + String(freePsram) +
         ",\"total\":" + String(totalPsram) + ",\"used\":" + String(usedPsram) +
         ",\"pct\":" + String(psramPct) + "}";
  }
  // Real-time flash storage telemetry (active OTA app partition slot)
  uint32_t sketchSize = ESP.getSketchSize();
  const esp_partition_t *runningPart = esp_ota_get_running_partition();
  uint32_t totalSketch =
      runningPart ? runningPart->size : ESP.getFreeSketchSpace();
  if (totalSketch < sketchSize)
    totalSketch = sketchSize;
  uint32_t freeSketch = totalSketch - sketchSize;
  int flashPct =
      totalSketch > 0 ? (int)(((uint64_t)sketchSize * 100) / totalSketch) : 0;
  uint32_t flashChip = ESP.getFlashChipSize();
  j += ",\"flash\":{\"used\":" + String(sketchSize) +
       ",\"free\":" + String(freeSketch) + ",\"total\":" + String(totalSketch) +
       ",\"chip\":" + String(flashChip) + ",\"pct\":" + String(flashPct) + "}";
  String owner = getOwnerName();
  if (owner.length() > 0) {
    j += ",\"owner\":\"" + escJson(owner) + "\"";
  }
  j += "}";

  cachedStatusJson = j;
  lastStatusGen = now;

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Connection", "close");
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "0");
  server.send(200, "application/json", j);
}

void handleLogs() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Connection", "close");
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "0");
  server.send(200, "application/json", "[]");
}

void handleConfigApi() {
  notifyWebRequest();
  String j;
  j.reserve(2048);
  j = "{";
  j += "\"wifi\":{\"ssid\":\"" + escJson(wifiSSID) + "\",\"password\":\"" +
       escJson(maskSecret(wifiPassword)) + "\"},";
  j += "\"stt\":{\"provider\":\"" + escJson(sttProvider) + "\",\"lang\":\"" +
       escJson(sttLang) + "\",";
  j += "\"url\":\"" + escJson(sttEndpoint) + "\",\"key\":\"" +
       escJson(maskSecret(sttKey)) + "\",\"model\":\"" + escJson(sttModel) +
       "\",";
  j += "\"groqUrl\":\"" + escJson(groqSttEndpoint) + "\",\"groqKey\":\"" +
       escJson(maskSecret(groqSttKey)) + "\",\"groqModel\":\"" +
       escJson(groqSttModel) + "\"},";
  j += "\"llm\":{\"provider\":\"" + escJson(llmProvider) + "\",\"url\":\"" +
       escJson(llmEndpoint) + "\",\"key\":\"" + escJson(maskSecret(llmKey)) +
       "\",\"model\":\"" + escJson(llmModel) + "\",";
  j += "\"groqUrl\":\"" + escJson(groqEndpoint) + "\",\"groqKey\":\"" +
       escJson(maskSecret(groqKey)) + "\",\"groqModel\":\"" +
       escJson(groqModel) + "\"},";
  j += "\"tts\":{\"provider\":\"" + escJson(ttsProvider) + "\",\"url\":\"" +
       escJson(ttsEndpoint) + "\",\"key\":\"" + escJson(maskSecret(ttsKey)) +
       "\",\"model\":\"" + escJson(ttsModel) + "\",\"voice\":\"" +
       escJson(ttsVoice) + "\",";
  j += "\"fishUrl\":\"" + escJson(fishEndpoint) + "\",\"fishKey\":\"" +
       escJson(maskSecret(fishKey)) + "\",\"fishModel\":\"" +
       escJson(fishModel) + "\",\"fishVoice\":\"" + escJson(fishVoice) + "\"},";
  j += "\"volume\":" + String(speakerVolume) + ",\"replyLang\":\"" +
       escJson(replyLang) + "\",\"prompt\":\"" + escJson(systemPrompt) + "\",";
  j += "\"brightness\":" + String(displayBrightness) +
       ",\"nightMode\":" + String(nightMode) +
       ",\"nightBrightness\":" + String(nightBrightness) + ",";
  j += "\"circadian\":{\"enabled\":" +
       String(circadianSleepEnabled ? "true" : "false") +
       ",\"start\":" + String(circadianSleepStart) +
       ",\"end\":" + String(circadianSleepEnd) + "},";
  j += "\"greetings\":{\"enabled\":" +
       String(greetingsEnabled ? "true" : "false") + "},";
  j += "\"hunger\":{\"enabled\":" + String(hungerEnabled ? "true" : "false") +
       "},";
  j += "\"soundReactive\":{\"enabled\":" +
       String(soundReactiveEnabled ? "true" : "false") + "},";
  j += "\"wakeWord\":{\"enabled\":" +
       String(wakeWordEnabled ? "true" : "false") +
       ",\"model\":" + String(wakeWordModel) + ",\"name\":\"" +
       String(wakeWordGetModelName()) + "\"" +
       ",\"cutoff\":" + String(wakeWordCutoff, 2) + "},";
  j += "\"rgbLed\":{\"enabled\":" + String(rgbLedEnabled ? "true" : "false") +
       ",\"brightness\":" + String((int)((rgbLedBrightness * 100) / 255)) +
       ",\"pin\":48},";
  j += "\"boredom\":{\"enabled\":" + String(boredomEnabled ? "true" : "false") +
       ",\"minutes\":" + String(boredomMinutes) + "},";
  j += "\"weather\":{\"city\":\"" + escJson(weatherCity) + "\",\"lat\":\"" +
       escJson(weatherLat) + "\",\"lon\":\"" + escJson(weatherLon) + "\"},";
  j += "\"hostname\":\"" + escJson(mdnsHostname) + "\",\"host\":\"" +
       escJson(mdnsHostname + ".local") +
       "\",\"faceTweenMs\":" + String(faceTweenMs) +
       ",\"maxChatHistory\":" + String(maxChatHistory) + ",";
  j += "\"sttVad\":{\"trailingMs\":" + String(sttTrailingMs) +
       ",\"noiseK\":" + String(sttNoiseK, 2) +
       ",\"relK\":" + String(sttRelK, 2) +
       ",\"maxSilenceMs\":" + String(sttMaxSilenceMs) + "},";
  j += "\"apSSID\":\"" + escJson(apSSID) + "\",";
  j += "\"touch\":{\"topTap\":\"" + touchActionToString(touchActTopTap) +
       "\",\"topDbl\":\"" + touchActionToString(touchActTopDbl) +
       "\",\"topHld\":\"" + touchActionToString(touchActTopHold) +
       "\",\"ltTap\":\"" + touchActionToString(touchActLeftTap) +
       "\",\"ltDbl\":\"" + touchActionToString(touchActLeftDbl) +
       "\",\"ltHld\":\"" + touchActionToString(touchActLeftHold) +
       "\",\"rtTap\":\"" + touchActionToString(touchActRightTap) +
       "\",\"rtDbl\":\"" + touchActionToString(touchActRightDbl) +
       "\",\"rtHld\":\"" + touchActionToString(touchActRightHold) +
       "\",\"sound\":" + String(touchFeedbackSound ? "true" : "false") +
       ",\"deb\":" + String(touchDebounceMs) + "},";
  const char *skinNames[SKIN_COUNT] = {"kiwi", "pixel", "squircle_glint",
                                       "mecha", "emo"};
  int skinIndex = (int)currentEyeSkin;
  if (skinIndex < 0 || skinIndex >= SKIN_COUNT)
    skinIndex = 0;
  j += "\"skin\":\"" + String(skinNames[skinIndex]) +
       "\",\"showMouth\":" + String(showMouth ? "true" : "false") +
       ",\"memory\":{\"name\":\"" + escJson(getOwnerName()) +
       "\",\"friends\":\"" + escJson(getFriendsFamily()) + "\",\"facts\":[";
  for (int i = 0; i < kiwiMemory.memoryCount; i++) {
    if (i)
      j += ",";
    j += "\"" + escJson(kiwiMemory.memories[i]) + "\"";
  }
  j += "],\"followUp\":\"" + escJson(kiwiMemory.pendingFollowUp) + "\"}}";
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Connection", "close");
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "0");
  server.send(200, "application/json", j);
}

void handleScanNetworks() {
  notifyWebRequest();
  lastUserInteractionAt = millis();
  int n = WiFi.scanNetworks(false, false);
  String json = "[";
  if (n > 0) {
    struct ScannedNet {
      String ssid;
      int32_t rssi;
      bool secure;
    };
    std::vector<ScannedNet> nets;
    nets.reserve(n);

    for (int i = 0; i < n; ++i) {
      String s = WiFi.SSID(i);
      s.trim();
      if (s.length() == 0)
        continue;

      int32_t r = WiFi.RSSI(i);
      bool sec = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);

      bool found = false;
      for (size_t k = 0; k < nets.size(); ++k) {
        if (nets[k].ssid == s) {
          found = true;
          if (r > nets[k].rssi) {
            nets[k].rssi = r;
            nets[k].secure = sec;
          }
          break;
        }
      }
      if (!found) {
        nets.push_back({s, r, sec});
      }
    }

    std::sort(nets.begin(), nets.end(),
              [](const ScannedNet &a, const ScannedNet &b) {
                return a.rssi > b.rssi;
              });

    for (size_t i = 0; i < nets.size(); ++i) {
      if (i > 0)
        json += ",";
      json += "{\"ssid\":\"";
      json += escJson(nets[i].ssid);
      json += "\",\"rssi\":";
      json += String(nets[i].rssi);
      json += ",\"secure\":";
      json += (nets[i].secure ? "true" : "false");
      json += "}";
    }
  }
  json += "]";
  WiFi.scanDelete();
  server.send(200, "application/json", json);
}

void processPendingSound() {
  if (pendingSound < 0)
    return;
  int8_t s = pendingSound;
  pendingSound = -1;
  if (!txHandle || busyProcessing)
    return;
  switch (s) {
  case 0:
    soundPetLove();
    break;
  case 1:
    soundPetGiggle();
    break;
  case 2:
    soundPetShy();
    break;
  case 3:
    soundPetSparkle();
    break;
  case 4:
    soundPetKiss();
    break;
  case 5:
    soundPetCuddle();
    break;
  case 6:
    soundPetPlayful();
    break;
  case 7:
    soundPetSleepy();
    break;
  case 8:
    soundPetProud();
    break;
  case 9:
    soundPetSurprised();
    break;
  }
}
