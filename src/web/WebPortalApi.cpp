#include "web/WebPortal.h"
#include "config/AppConfig.h"
#include "config/ConfigManager.h"
#include "hardware/AudioEngine.h"
#include "face/FaceEngine.h"
#include "face/FaceDisplay.h"
#include "services/AiService.h"
#include "services/MemoryService.h"
#include "animation/AnimationEngine.h"
#include "behavior/ActionExecutor.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Update.h>
#include <time.h>
#include <Preferences.h>
#include "services/WeatherService.h"
#include <vector>
#include <algorithm>

static String escJson(String s) {
  s.replace("\\", "\\\\");
  s.replace("\"", "\\\"");
  s.replace("\r", "");
  s.replace("\n", "\\n");
  return s;
}

void handleDoAction() {
  lastUserInteractionAt = millis();

  // Web Mic Audio Record Trigger
  if (server.hasArg("record")) {
    if (busyProcessing) {
      cancelRequest = true;
      // web record cancel
      addTerminalLog("[WEB] Mic clicked while busy -> Cancel request / Interrupt");
      server.send(200, "application/json", "{\"ok\":true,\"cancelled\":true}");
      return;
    }
    startRecording = true;
    addTerminalLog("[WEB] Mic record button clicked -> start listening");
    server.send(200, "application/json", "{\"ok\":true,\"recording\":true}");
    return;
  }

  // Web Text Prompt Trigger
  if (server.hasArg("ask")) {
    String q = server.arg("ask");
    q.trim();
    if (q.length() == 0) {
      server.send(200, "application/json", "{\"ok\":false,\"reason\":\"empty\"}");
      return;
    }
    if (busyProcessing) {
      cancelRequest = true;
      // web record cancel
      addTerminalLog("[WEB] Mic clicked while busy -> Cancel request / Interrupt");
      server.send(200, "application/json", "{\"ok\":true,\"cancelled\":true}");
      return;
    }
    webQueuedQuestion = q;
    addTerminalLog("[WEB] Queued prompt: \"" + q + "\"");
    server.send(200, "application/json", "{\"ok\":true,\"queued\":true}");
    return;
  }

  // Clear Terminal Output
  if (server.hasArg("clearlog")) {
    clearTerminalLog();
    server.send(200, "application/json", "{\"ok\":true,\"cleared\":true}");
    return;
  }

  // Timer Control (cancel or start)
  if (server.hasArg("timer")) {
    String tCmd = server.arg("timer");
    if (tCmd == "cancel" || tCmd == "stop") {
      if (isRobotTimerRinging()) {
        stopRobotTimerAlarm();
      } else {
        cancelRobotTimer();
      }
      server.send(200, "application/json", "{\"ok\":true,\"timer\":false}");
      return;
    }
    int sec = tCmd.toInt();
    if (sec > 0) {
      startRobotTimer(sec, "Portal Timer");
      server.send(200, "application/json", "{\"ok\":true,\"timer\":true,\"sec\":" + String(sec) + "}");
      return;
    }
  }

  // Soundboard FX
  if (server.hasArg("sound")) {
    String s = server.arg("sound");
    if (s == "fart") {
      setEmotion(EMOTION_SMUG);
      soundFart();
    } else if (s == "laser") {
      setEmotion(EMOTION_COOL);
      soundLaser();
    } else if (s == "fanfare") {
      setEmotion(EMOTION_PARTY);
      soundFanfare();
    } else if (s == "coin") {
      setEmotion(EMOTION_EXCITED);
      soundCoin();
    } else if (s == "giggle") {
      setEmotion(EMOTION_LAUGHING);
      soundPetGiggle();
    } else if (s == "violin") {
      setEmotion(EMOTION_CRYING);
      soundSadViolin();
    }
    returnIdleAt = millis() + 3000;
    server.send(200, "application/json", "{\"ok\":true}");
    return;
  }

  // Custom OLED Marquee Banner
  if (server.hasArg("animation")) {
    bannerActive = false;
    playAnimation((uint8_t)server.arg("animation").toInt());
    server.send(200, "application/json", "{\"ok\":true}");
    return;
  }
  if (server.hasArg("stopanimation")) {
    stopAnimation();
    setEmotion(EMOTION_IDLE);
    server.send(200, "application/json", "{\"ok\":true}");
    return;
  }
  if (server.hasArg("animations")) {
    server.send(200, "application/json", animationListJson());
    return;
  }
  if (server.hasArg("banner")) {
    stopAnimation();
    bannerText = server.arg("banner");
    bannerActive = true;
    bannerScrollX = SCREEN_WIDTH + 10;
    bannerLoops = 0;
    showClockScreen = false;
    showWeatherScreen = false;
    soundPetSparkle();
    server.send(200, "application/json", "{\"ok\":true}");
    return;
  }
  if (server.hasArg("stopbanner")) {
    bannerActive = false;
    setEmotion(EMOTION_IDLE);
    server.send(200, "application/json", "{\"ok\":true}");
    return;
  }

  // Autonomous Boredom Live Toggle
  if (server.hasArg("boredom")) {
    boredomEnabled = (server.arg("boredom") == "1" || server.arg("boredom") == "true");
    Preferences p;
    p.begin("myrobot", false);
    p.putBool("boredom", boredomEnabled);
    p.end();
    Serial.printf("[PORTAL] Boredom toggle live set to: %d\n", boredomEnabled);
    server.send(200, "application/json", boredomEnabled ? "{\"ok\":true,\"boredom\":true}" : "{\"ok\":true,\"boredom\":false}");
    return;
  }

  // Autonomous Boredom Interval Live Action
  if (server.hasArg("boredom_mins")) {
    int m = server.arg("boredom_mins").toInt();
    if (m >= 1 && m <= 60) {
      boredomMinutes = m;
      Preferences p;
      p.begin("myrobot", false);
      p.putInt("boredom_mins", boredomMinutes);
      p.end();
      resetBoredomInterval();
      Serial.printf("[PORTAL] Boredom interval live set to: %d minutes\n", boredomMinutes);
    }
    server.send(200, "application/json", "{\"ok\":true,\"boredom_mins\":" + String(boredomMinutes) + "}");
    return;
  }

  // Custom OLED Eye Skin Action
  if (server.hasArg("skin")) {
    String s = server.arg("skin");
    s.toLowerCase();
    if (s == "cyberpunk") currentEyeSkin = SKIN_CYBERPUNK;
    else if (s == "cat" || s == "neko") currentEyeSkin = SKIN_CAT;
    else if (s == "retro" || s == "8bit") currentEyeSkin = SKIN_RETRO8BIT;
    else if (s == "anime" || s == "star") currentEyeSkin = SKIN_ANIME;
    else if (s == "hearts" || s == "heart" || s == "love") currentEyeSkin = SKIN_HEARTS;
    else if (s == "steampunk" || s == "goggles") currentEyeSkin = SKIN_STEAMPUNK;
    else if (s == "demon" || s == "devil") currentEyeSkin = SKIN_DEMON;
    else if (s == "matrix" || s == "code" || s == "terminal") currentEyeSkin = SKIN_MATRIX;
    else if (s == "cyborg" || s == "terminator" || s == "t800") currentEyeSkin = SKIN_CYBORG;
    else if (s == "mecha" || s == "gundam" || s == "robot") currentEyeSkin = SKIN_MECHA;
    else if (s == "sunglasses" || s == "shades" || s == "cool") currentEyeSkin = SKIN_SUNGLASSES;
    else if (s == "puppy" || s == "dog" || s == "doggy") currentEyeSkin = SKIN_PUPPY;
    else if (s == "hypno" || s == "spiral" || s == "vortex") currentEyeSkin = SKIN_HYPNO;
    else if (s == "alien" || s == "ufo" || s == "cosmic") currentEyeSkin = SKIN_ALIEN;
    else if (s == "ninja" || s == "shinobi" || s == "naruto") currentEyeSkin = SKIN_NINJA;
    else if (s == "round" || s == "circle" || s == "ocular") currentEyeSkin = SKIN_ROUND;
    else if (s == "kiwi" || s == "deskbuddy" || s == "buddy" || s == "cute" || s == "relaxed" || s == "bored") currentEyeSkin = SKIN_KIWI;
    else if (s == "squircle_solid" || s == "squircle") currentEyeSkin = SKIN_SQUIRCLE_SOLID;
    else if (s == "squircle_outline") currentEyeSkin = SKIN_SQUIRCLE_OUTLINE;
    else if (s == "squircle_glint") currentEyeSkin = SKIN_SQUIRCLE_GLINT;
    else currentEyeSkin = SKIN_DEFAULT;

    Preferences p;
    p.begin("myrobot", false);
    p.putInt("eyeskin", (int)currentEyeSkin);
    p.end();

    soundPetSparkle();
    Serial.printf("[PORTAL] Eye skin switched to: %d\n", (int)currentEyeSkin);
    server.send(200, "application/json", "{\"ok\":true,\"skin\":\"" + s + "\"}");
    return;
  }

  // Live Toggle / Set Sleep Mode
  if (server.hasArg("sleep")) {
    String slp = server.arg("sleep");
    if (slp == "toggle") {
      manualSleep = !manualSleep;
    } else {
      manualSleep = (slp == "1" || slp == "true");
    }
    inCircadianSleep = manualSleep;
    if (manualSleep) {
      showClockScreen = false;
      showWeatherScreen = false;
      setOledDim(false);
      setEmotion(EMOTION_SLEEPING);
      soundPetSleepy();
    } else {
      setOledDim(false);
      setEmotion(EMOTION_HAPPY);
      soundPetSparkle();
    }
    Serial.printf("[PORTAL] Manual sleep mode set to: %d\n", manualSleep);
    server.send(200, "application/json", "{\"ok\":true,\"sleeping\":" + String(manualSleep ? "true" : "false") + "}");
    return;
  }

  // Live Circadian Night Sleep Schedule
  if (server.hasArg("circ_en") || server.hasArg("circ_start") || server.hasArg("circ_end")) {
    Preferences p;
    p.begin("myrobot", false);
    if (server.hasArg("circ_en")) {
      String en = server.arg("circ_en");
      circadianSleepEnabled = (en == "1" || en == "true" || en == "on");
      p.putBool("circ_en", circadianSleepEnabled);
      Serial.printf("[PORTAL] Circadian sleep toggle live set to: %d\n", circadianSleepEnabled);
    }
    if (server.hasArg("circ_start")) {
      int s = server.arg("circ_start").toInt();
      if (s >= 0 && s <= 23) {
        circadianSleepStart = s;
        p.putInt("circ_start", circadianSleepStart);
        Serial.printf("[PORTAL] Circadian start hour set to: %d\n", circadianSleepStart);
      }
    }
    if (server.hasArg("circ_end")) {
      int e = server.arg("circ_end").toInt();
      if (e >= 0 && e <= 23) {
        circadianSleepEnd = e;
        p.putInt("circ_end", circadianSleepEnd);
        Serial.printf("[PORTAL] Circadian end hour set to: %d\n", circadianSleepEnd);
      }
    }
    p.end();
    server.send(200, "application/json", "{\"ok\":true,\"circ_en\":" + String(circadianSleepEnabled ? "true" : "false") + ",\"circ_start\":" + String(circadianSleepStart) + ",\"circ_end\":" + String(circadianSleepEnd) + "}");
    return;
  }

  // Live Greetings Toggle
  if (server.hasArg("greet_en")) {
    String ge = server.arg("greet_en");
    greetingsEnabled = (ge == "1" || ge == "true" || ge == "on");
    Preferences p;
    p.begin("myrobot", false);
    p.putBool("greet_en", greetingsEnabled);
    p.end();
    Serial.printf("[PORTAL] Greetings toggle live set to: %d\n", greetingsEnabled);
    server.send(200, "application/json", "{\"ok\":true,\"greet_en\":" + String(greetingsEnabled ? "true" : "false") + "}");
    return;
  }

  // Live Sound-Reactive Face Toggle
  if (server.hasArg("snd_react")) {
    String sr = server.arg("snd_react");
    soundReactiveEnabled = (sr == "1" || sr == "true" || sr == "on");
    Preferences p;
    p.begin("myrobot", false);
    p.putBool("snd_react", soundReactiveEnabled);
    p.end();
    Serial.printf("[PORTAL] Sound reactive face live set to: %d\n", soundReactiveEnabled);
    server.send(200, "application/json", "{\"ok\":true,\"snd_react\":" + String(soundReactiveEnabled ? "true" : "false") + "}");
    return;
  }

  // Live Toggle Mouth & Lips
  if (server.hasArg("mouth")) {
    String m = server.arg("mouth");
    showMouth = (m == "1" || m == "true");
    Preferences p;
    p.begin("myrobot", false);
    p.putBool("showmouth", showMouth);
    p.end();
    server.send(200, "application/json", "{\"ok\":true,\"mouth\":" + String(showMouth ? "true" : "false") + "}");
    return;
  }

  // Live Display Day Brightness Action
  if (server.hasArg("brightness")) {
    int b = server.arg("brightness").toInt();
    if (b < 0) b = 0;
    if (b > 100) b = 100;
    displayBrightness = b;
    Preferences p;
    p.begin("myrobot", false);
    p.putInt("disp_bright", displayBrightness);
    p.end();
    updateDisplayBrightnessMode();
    Serial.printf("[PORTAL] Day Brightness live set to: %d%%\n", displayBrightness);
    server.send(200, "application/json", "{\"ok\":true,\"brightness\":" + String(displayBrightness) + "}");
    return;
  }

  // Live Display Night Brightness Action
  if (server.hasArg("night_bright")) {
    int nb = server.arg("night_bright").toInt();
    if (nb < 0) nb = 0;
    if (nb > 100) nb = 100;
    nightBrightness = nb;
    Preferences p;
    p.begin("myrobot", false);
    p.putInt("night_bright", nightBrightness);
    p.end();
    updateDisplayBrightnessMode();
    Serial.printf("[PORTAL] Night Brightness live set to: %d%%\n", nightBrightness);
    server.send(200, "application/json", "{\"ok\":true,\"night_bright\":" + String(nightBrightness) + "}");
    return;
  }

  // Live Night Mode Action (0: Same as day, 1: Dim, 2: Screen off)
  if (server.hasArg("night_mode")) {
    int nm = server.arg("night_mode").toInt();
    if (nm >= 0 && nm <= 2) {
      nightMode = nm;
      Preferences p;
      p.begin("myrobot", false);
      p.putInt("night_mode", nightMode);
      p.end();
      updateDisplayBrightnessMode();
      Serial.printf("[PORTAL] Night Mode live set to: %d\n", nightMode);
      server.send(200, "application/json", "{\"ok\":true,\"night_mode\":" + String(nightMode) + "}");
      return;
    }
  }

  // Live Speaker Volume Action
  if (server.hasArg("vol") || server.hasArg("volume")) {
    int v = server.hasArg("vol") ? server.arg("vol").toInt() : server.arg("volume").toInt();
    if (v < 0) v = 0;
    if (v > 100) v = 100;
    speakerVolume = v;
    Preferences p;
    p.begin("myrobot", false);
    p.putInt("volume", speakerVolume);
    p.end();
    Serial.printf("[PORTAL] Speaker volume live set to: %d%%\n", speakerVolume);
    server.send(200, "application/json", "{\"ok\":true,\"volume\":" + String(speakerVolume) + "}");
    return;
  }

  // Memory & Diary Actions
  if (server.hasArg("clearmem")) {
    clearAllMemory();
    server.send(200, "application/json", "{\"ok\":true,\"cleared\":true}");
    return;
  }
  if (server.hasArg("addmem")) {
    addMemoryFact(server.arg("addmem"));
    server.send(200, "application/json", "{\"ok\":true}");
    return;
  }
  if (server.hasArg("setname")) {
    setOwnerName(server.arg("setname"));
    server.send(200, "application/json", "{\"ok\":true}");
    return;
  }
  if (server.hasArg("setfriends")) {
    setFriendsFamily(server.arg("setfriends"));
    server.send(200, "application/json", "{\"ok\":true}");
    return;
  }

  String e = server.hasArg("e") ? server.arg("e") : "";
  uint32_t ms = server.hasArg("ms") ? (uint32_t)server.arg("ms").toInt() : 60000;
  bool snd = server.hasArg("s") ? (server.arg("s").toInt() != 0) : true;
  if (busyProcessing) {
    server.send(200, "application/json", "{\"ok\":false,\"reason\":\"busy\"}");
    return;
  }
  if (e == "sleep" || e == "sleeping") {
    manualSleep = true;
    inCircadianSleep = true;
    showClockScreen = false;
    showWeatherScreen = false;
    setOledDim(false);
    setEmotion(EMOTION_SLEEPING);
    if (snd) soundPetSleepy();
    server.send(200, "application/json", "{\"ok\":true,\"sleeping\":true}");
    return;
  }
  if (e == "wake") {
    manualSleep = false;
    inCircadianSleep = false;
    setOledDim(false);
    setEmotion(EMOTION_HAPPY);
    if (snd) soundPetSparkle();
    server.send(200, "application/json", "{\"ok\":true,\"sleeping\":false}");
    return;
  }
  if (e == "clock") {
    manualSleep = false;
    inCircadianSleep = false;
    setOledDim(false);
    showClockScreen = true;
    showWeatherScreen = false;
    if (snd) soundClock();
    if (!timeSynced && WiFi.status() == WL_CONNECTED) syncTimeFromNTP();
    server.send(200, "application/json", "{\"ok\":true}");
    return;
  }
  if (e == "weather") {
    manualSleep = false;
    inCircadianSleep = false;
    setOledDim(false);
    showWeatherScreen = true;
    showClockScreen = false;
    if (snd) soundPetSparkle();
    if (lastWeatherFetchAt == 0 || millis() - lastWeatherFetchAt > 10 * 60 * 1000 || weatherCode < 0) {
      fetchWeather();
    }
    server.send(200, "application/json", "{\"ok\":true}");
    return;
  }
  if (e == "clear" || e == "forget") {
    clearConversationHistory();
    server.send(200, "application/json", "{\"ok\":true,\"cleared\":true}");
    return;
  }
  bool ok = triggerEmotionByName(e, ms, snd);
  server.send(200, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false}");
}

void handleStatus() {
  static uint32_t lastStatusGen = 0;
  static String cachedStatusJson = "";

  uint32_t now = millis();
  // Fast throttle cache: if multiple devices (laptop + phone) poll simultaneously,
  // return cached JSON immediately to eliminate redundant string formatting & CPU load.
  if (now - lastStatusGen < 350 && cachedStatusJson.length() > 0) {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Connection", "close");
    server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    server.send(200, "application/json", cachedStatusJson);
    return;
  }

  static const char *names[] = {
      "idle", "listening", "thinking", "speaking", "happy", "excited",
      "sad", "confused", "angry", "sleeping", "error", "petted",
      "surprised", "wink", "smug", "curious", "bored", "laughing",
      "scared", "suspicious", "relieved", "proud", "dizzy", "lovestruck",
      "evil", "crying", "hearts", "dead", "cool", "shocked",
      "singing", "party", "nervous", "cute"};
  int idx = (int)currentEmotion;
  if (idx < 0 || idx > 33)
    idx = 0;
  String j;
  j.reserve(2048);
  j = "{\"host\":\"" + escJson(mdnsHostname + ".local") + "\",\"emotion\":\"" + String(names[idx]) + "\",\"busy\":";
  j += busyProcessing ? "true," : "false,";
  j += "\"sleeping\":";
  j += (inCircadianSleep || manualSleep) ? "true," : "false,";
  j += "\"history\":" + String(getConversationHistoryCount());
  const char *skinNames[] = {
    "default", "cyberpunk", "cat", "retro", "anime", "hearts", "steampunk", "demon",
    "matrix", "cyborg", "mecha", "sunglasses", "puppy", "hypno", "alien", "ninja", "round",
    "kiwi", "squircle_solid", "squircle_outline", "squircle_glint"
  };
  int skIdx = (int)currentEyeSkin;
  if (skIdx < 0 || skIdx >= SKIN_COUNT) skIdx = 0;
  j += ",\"boredom\":" + String(boredomEnabled ? "true" : "false");
  j += ",\"boredom_mins\":" + String(boredomMinutes);
  j += ",\"skin\":\"" + String(skinNames[skIdx]) + "\"";
  j += ",\"mouth\":" + String(showMouth ? "true" : "false");
  // Status JSON: soundReactive (snd_react)
  j += ",\"soundReactive\":{\"enabled\":" + String(soundReactiveEnabled ? "true" : "false") + "}";
  j += ",\"timer_active\":" + String(isRobotTimerActive() ? "true" : "false");
  j += ",\"timer_ringing\":" + String(isRobotTimerRinging() ? "true" : "false");
  j += ",\"timer_remaining\":" + String(getRobotTimerRemainingSec());
  j += ",\"state\":\"" + escJson(robotStateDesc) + "\"";
  j += ",\"question\":\"" + escJson(lastTranscribedQuestion) + "\"";
  j += ",\"answer\":\"" + escJson(lastLlmAnswer) + "\"";
  j += ",\"logs\":" + getTerminalJson();
  j += ",\"version\":\"" + String(FIRMWARE_VER) + "\"";
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

void handleConfigApi() {
  String j;
  j.reserve(2048);
  j = "{";
  j += "\"wifi\":{\"ssid\":\"" + escJson(wifiSSID) + "\",\"password\":\"" + escJson(maskSecret(wifiPassword)) + "\"},";
  j += "\"stt\":{\"provider\":\"" + escJson(sttProvider) + "\",\"lang\":\"" + escJson(sttLang) + "\",";
  j += "\"url\":\"" + escJson(sttEndpoint) + "\",\"key\":\"" + escJson(maskSecret(sttKey)) + "\",\"model\":\"" + escJson(sttModel) + "\",";
  j += "\"groqUrl\":\"" + escJson(groqSttEndpoint) + "\",\"groqKey\":\"" + escJson(maskSecret(groqSttKey)) + "\",\"groqModel\":\"" + escJson(groqSttModel) + "\",";
  j += "\"openaiUrl\":\"" + escJson(openaiSttEndpoint) + "\",\"openaiKey\":\"" + escJson(maskSecret(openaiSttKey)) + "\",\"openaiModel\":\"" + escJson(openaiSttModel) + "\"},";
  j += "\"llm\":{\"provider\":\"" + escJson(llmProvider) + "\",\"url\":\"" + escJson(llmEndpoint) + "\",\"key\":\"" + escJson(maskSecret(llmKey)) + "\",\"model\":\"" + escJson(llmModel) + "\",";
  j += "\"groqUrl\":\"" + escJson(groqEndpoint) + "\",\"groqKey\":\"" + escJson(maskSecret(groqKey)) + "\",\"groqModel\":\"" + escJson(groqModel) + "\"},";
  j += "\"tts\":{\"provider\":\"" + escJson(ttsProvider) + "\",\"url\":\"" + escJson(ttsEndpoint) + "\",\"key\":\"" + escJson(maskSecret(ttsKey)) + "\",\"model\":\"" + escJson(ttsModel) + "\",\"voice\":\"" + escJson(ttsVoice) + "\",";
  j += "\"openaiUrl\":\"" + escJson(openaiTtsEndpoint) + "\",\"openaiKey\":\"" + escJson(maskSecret(openaiTtsKey)) + "\",\"openaiModel\":\"" + escJson(openaiTtsModel) + "\",\"openaiVoice\":\"" + escJson(openaiTtsVoice) + "\",";
  j += "\"elevenUrl\":\"" + escJson(elevenEndpoint) + "\",\"elevenKey\":\"" + escJson(maskSecret(elevenKey)) + "\",\"elevenModel\":\"" + escJson(elevenModel) + "\",\"elevenVoice\":\"" + escJson(elevenVoice) + "\",";
  j += "\"fishUrl\":\"" + escJson(fishEndpoint) + "\",\"fishKey\":\"" + escJson(maskSecret(fishKey)) + "\",\"fishModel\":\"" + escJson(fishModel) + "\",\"fishVoice\":\"" + escJson(fishVoice) + "\"},";
  j += "\"volume\":" + String(speakerVolume) + ",\"replyLang\":\"" + escJson(replyLang) + "\",\"prompt\":\"" + escJson(systemPrompt) + "\",";
  j += "\"brightness\":" + String(displayBrightness) + ",\"nightMode\":" + String(nightMode) + ",\"nightBrightness\":" + String(nightBrightness) + ",";
  j += "\"circadian\":{\"enabled\":" + String(circadianSleepEnabled ? "true" : "false") + ",\"start\":" + String(circadianSleepStart) + ",\"end\":" + String(circadianSleepEnd) + "},";
  j += "\"greetings\":{\"enabled\":" + String(greetingsEnabled ? "true" : "false") + "},";
  j += "\"soundReactive\":{\"enabled\":" + String(soundReactiveEnabled ? "true" : "false") + "},";
  j += "\"boredom\":{\"enabled\":" + String(boredomEnabled ? "true" : "false") + ",\"minutes\":" + String(boredomMinutes) + "},";
  j += "\"weather\":{\"city\":\"" + escJson(weatherCity) + "\",\"lat\":\"" + escJson(weatherLat) + "\",\"lon\":\"" + escJson(weatherLon) + "\"},";
  j += "\"hostname\":\"" + escJson(mdnsHostname) + "\",\"host\":\"" + escJson(mdnsHostname + ".local") + "\",\"faceTweenMs\":" + String(faceTweenMs) + ",\"maxChatHistory\":" + String(maxChatHistory) + ",";
  const char *skinNames[] = {"default", "cyberpunk", "cat", "retro", "anime", "hearts", "steampunk", "demon", "matrix", "cyborg", "mecha", "sunglasses", "puppy", "hypno", "alien", "ninja", "round", "kiwi", "squircle_solid", "squircle_outline", "squircle_glint"};
  int skinIndex = (int)currentEyeSkin;
  if (skinIndex < 0 || skinIndex >= SKIN_COUNT) skinIndex = 0;
  j += "\"skin\":\"" + String(skinNames[skinIndex]) + "\",\"showMouth\":" + String(showMouth ? "true" : "false") + ",\"memory\":{\"name\":\"" + escJson(getOwnerName()) + "\",\"friends\":\"" + escJson(getFriendsFamily()) + "\",\"facts\":[";
  for (int i = 0; i < kiwiMemory.memoryCount; i++) {
    if (i) j += ",";
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

void handleSave() {
  if (!server.hasArg("ssid")) {
    server.send(400, "text/plain", "SSID required");
    return;
  }
  String incomingSsid = server.arg("ssid");
  incomingSsid.trim();
  if (incomingSsid.length() > 0 && incomingSsid != "%SSID%" && incomingSsid != wifiSSID) {
    wifiSSID = incomingSsid;
  }
  if (server.hasArg("pass")) {
    String incomingPass = server.arg("pass");
    incomingPass.trim();
    if (incomingPass.length() > 0 && incomingPass != "%PASS%" && !isMaskedSecret(incomingPass) && incomingPass != wifiPassword) {
      wifiPassword = incomingPass;
    }
  }

  if (server.hasArg("sttprovider"))
    sttProvider = normalizeSttProvider(server.arg("sttprovider"));
  if (server.hasArg("sttlang"))
    sttLang = normalizeSttLang(server.arg("sttlang"));

  sttEndpoint = sanitizeUrl(server.arg("stturl"), DEFAULT_STT_URL);
  if (server.hasArg("ttskey") && server.arg("ttskey").length() > 0 && !isMaskedSecret(server.arg("ttskey"))) {
    sttKey = sanitizeField(server.arg("ttskey"));
  } else if (server.hasArg("sttkey") && server.arg("sttkey").length() > 0 && !isMaskedSecret(server.arg("sttkey"))) {
    sttKey = sanitizeField(server.arg("sttkey"));
  }
  sttModel = sanitizeField(server.arg("sttmodel"), DEFAULT_STT_MODEL);

  groqSttEndpoint = sanitizeUrl(server.arg("groqstturl"), DEFAULT_GROQ_STT_URL);
  if (server.hasArg("groqkey") && server.arg("groqkey").length() > 0 && !isMaskedSecret(server.arg("groqkey"))) {
    groqSttKey = sanitizeField(server.arg("groqkey"));
  } else if (server.hasArg("groqsttkey") && server.arg("groqsttkey").length() > 0 && !isMaskedSecret(server.arg("groqsttkey"))) {
    groqSttKey = sanitizeField(server.arg("groqsttkey"));
  }
  groqSttModel = sanitizeField(server.arg("groqsttmodel"), DEFAULT_GROQ_STT_MODEL);

  openaiSttEndpoint = sanitizeUrl(server.arg("opaistturl"), DEFAULT_OPENAI_STT_URL);
  if (server.hasArg("openaikey") && server.arg("openaikey").length() > 0 && !isMaskedSecret(server.arg("openaikey"))) {
    openaiSttKey = sanitizeField(server.arg("openaikey"));
    openaiTtsKey = openaiSttKey;
  } else if (server.hasArg("opaisttkey") && server.arg("opaisttkey").length() > 0 && !isMaskedSecret(server.arg("opaisttkey"))) {
    openaiSttKey = sanitizeField(server.arg("opaisttkey"));
  }
  openaiSttModel = sanitizeField(server.arg("opaisttmodel"), DEFAULT_OPENAI_STT_MODEL);

  replyLang = sanitizeField(server.arg("replang"));
  systemPrompt = sanitizeField(server.arg("prompt"), DEFAULT_PROMPT);
  if (systemPrompt.length() == 0) systemPrompt = DEFAULT_PROMPT;

  if (server.hasArg("llmprovider")) {
    llmProvider = server.arg("llmprovider");
    llmProvider.trim();
    llmProvider.toLowerCase();
    if (llmProvider != "gemini" && llmProvider != "groq" && llmProvider != "openai")
      llmProvider = "groq";
  }
  if (llmProvider == "openai") {
    llmEndpoint = sanitizeUrl(server.arg("openaiurl"), DEFAULT_OPENAI_LLM_URL);
    if (server.hasArg("openaikey") && server.arg("openaikey").length() > 0 && !isMaskedSecret(server.arg("openaikey"))) {
      llmKey = sanitizeField(server.arg("openaikey"));
    }
    llmModel = sanitizeField(server.arg("openaimodel"), "gpt-4o-mini");
  } else {
    llmEndpoint = sanitizeUrl(server.arg("llmurl"), DEFAULT_LLM_URL);
    if (server.hasArg("llmkey") && server.arg("llmkey").length() > 0 && !isMaskedSecret(server.arg("llmkey"))) {
      llmKey = sanitizeField(server.arg("llmkey"));
    }
    llmModel = sanitizeField(server.arg("llmmodel"), DEFAULT_LLM_MODEL);
  }
  groqEndpoint = sanitizeUrl(server.arg("groqurl"), DEFAULT_GROQ_URL);
  if (server.hasArg("groqkey") && server.arg("groqkey").length() > 0 && !isMaskedSecret(server.arg("groqkey"))) {
    groqKey = sanitizeField(server.arg("groqkey"));
  }
  groqModel = sanitizeField(server.arg("groqmodel"), DEFAULT_GROQ_MODEL);

  if (server.hasArg("ttsprovider"))
    ttsProvider = normalizeTtsProvider(server.arg("ttsprovider"));
  ttsEndpoint = sanitizeUrl(server.arg("ttsurl"), DEFAULT_TTS_URL);
  if (server.hasArg("ttskey") && server.arg("ttskey").length() > 0 && !isMaskedSecret(server.arg("ttskey"))) {
    ttsKey = sanitizeField(server.arg("ttskey"));
  }
  ttsModel = sanitizeField(server.arg("ttsmodel"), DEFAULT_TTS_MODEL);
  ttsVoice = sanitizeField(server.arg("ttsvoice"), DEFAULT_TTS_VOICE);
  openaiTtsEndpoint = sanitizeUrl(server.arg("openaittsurl"), DEFAULT_OPENAI_TTS_URL);
  if (server.hasArg("openaittskey") && server.arg("openaittskey").length() > 0 && !isMaskedSecret(server.arg("openaittskey"))) {
    openaiTtsKey = sanitizeField(server.arg("openaittskey"));
  }
  openaiTtsModel = sanitizeField(server.arg("openaittsmodel"), DEFAULT_OPENAI_TTS_MODEL);
  openaiTtsVoice = sanitizeField(server.arg("openaittsvoice"), DEFAULT_OPENAI_TTS_VOICE);
  elevenEndpoint = sanitizeUrl(server.arg("elevurl"), DEFAULT_ELEVEN_URL);
  if (server.hasArg("elevkey") && server.arg("elevkey").length() > 0 && !isMaskedSecret(server.arg("elevkey"))) {
    elevenKey = sanitizeField(server.arg("elevkey"));
  }
  elevenModel = sanitizeField(server.arg("elevmodel"), DEFAULT_ELEVEN_MODEL);
  elevenVoice = sanitizeField(server.arg("elevvoice"), DEFAULT_ELEVEN_VOICE);
  fishEndpoint = sanitizeUrl(server.arg("fishurl"), DEFAULT_FISH_TTS_URL);
  if (server.hasArg("fishkey") && server.arg("fishkey").length() > 0 && !isMaskedSecret(server.arg("fishkey"))) {
    fishKey = sanitizeField(server.arg("fishkey"));
  }
  fishModel = sanitizeField(server.arg("fishmodel"), DEFAULT_FISH_TTS_MODEL);
  fishVoice = sanitizeField(server.arg("fishvoice"));

  // mDNS Hostname
  if (server.hasArg("hostname")) {
    String newHost = sanitizeHostname(server.arg("hostname"));
    if (newHost.length() > 0 && newHost != mdnsHostname) {
      mdnsHostname = newHost;
      initMdns(WiFi.status() == WL_CONNECTED ? "STA" : "AP");
    }
  }

  // Chat History Turns Limit
  if (server.hasArg("chat_hist")) {
    int ch = server.arg("chat_hist").toInt();
    if (ch >= 1 && ch <= 8) {
      maxChatHistory = ch;
    }
  }

  // Face Tween Duration Ms
  if (server.hasArg("face_tween")) {
    int ft = server.arg("face_tween").toInt();
    if (ft >= 0 && ft <= 1000) {
      faceTweenMs = ft;
    }
  }

  if (server.hasArg("vol")) {
    speakerVolume = server.arg("vol").toInt();
    if (speakerVolume < 0)
      speakerVolume = 0;
    if (speakerVolume > 100)
      speakerVolume = 100;
  }

  if (server.hasArg("disp_bright")) {
    int b = server.arg("disp_bright").toInt();
    if (b < 0) b = 0;
    if (b > 100) b = 100;
    displayBrightness = b;
  }
  if (server.hasArg("night_mode")) {
    int nm = server.arg("night_mode").toInt();
    if (nm >= 0 && nm <= 2) nightMode = nm;
  }
  if (server.hasArg("night_bright")) {
    int nb = server.arg("night_bright").toInt();
    if (nb < 0) nb = 0;
    if (nb > 100) nb = 100;
    nightBrightness = nb;
  }
  if (server.hasArg("circ_en")) {
    String ce = server.arg("circ_en");
    circadianSleepEnabled = (ce == "1" || ce == "true" || ce == "on");
  }
  if (server.hasArg("greet_en")) {
    String ge = server.arg("greet_en");
    greetingsEnabled = (ge == "1" || ge == "true" || ge == "on");
  }
  if (server.hasArg("snd_react")) {
    String sr = server.arg("snd_react");
    soundReactiveEnabled = (sr == "1" || sr == "true" || sr == "on");
  }
  if (server.hasArg("circ_start"))
    circadianSleepStart = server.arg("circ_start").toInt();
  if (server.hasArg("circ_end"))
    circadianSleepEnd = server.arg("circ_end").toInt();
  if (server.hasArg("boredom")) {
    String bStr = server.arg("boredom");
    boredomEnabled = (bStr == "1" || bStr == "true" || bStr == "on");
  } else {
    boredomEnabled = false;
  }
  if (server.hasArg("boredom_mins")) {
    int bm = server.arg("boredom_mins").toInt();
    if (bm >= 1 && bm <= 60) {
      boredomMinutes = bm;
      resetBoredomInterval();
    }
  }

  if (server.hasArg("tch_deb")) {
    int td = server.arg("tch_deb").toInt();
    if (td >= 10 && td <= 200) {
      touchDebounceMs = td;
    }
    touchFeedbackSound = server.hasArg("tch_snd");
  }

  if (server.hasArg("act_top_tap")) touchActTopTap = parseTouchAction(server.arg("act_top_tap"), TOUCH_ACT_LISTEN);
  if (server.hasArg("act_top_dbl")) touchActTopDbl = parseTouchAction(server.arg("act_top_dbl"), TOUCH_ACT_NONE);
  if (server.hasArg("act_top_hld")) touchActTopHold = parseTouchAction(server.arg("act_top_hld"), TOUCH_ACT_CYCLE_SKIN);
  if (server.hasArg("act_lt_tap"))  touchActLeftTap = parseTouchAction(server.arg("act_lt_tap"), TOUCH_ACT_DISMISS);
  if (server.hasArg("act_lt_dbl"))  touchActLeftDbl = parseTouchAction(server.arg("act_lt_dbl"), TOUCH_ACT_CLOCK);
  if (server.hasArg("act_rt_tap"))  touchActRightTap = parseTouchAction(server.arg("act_rt_tap"), TOUCH_ACT_DISMISS);
  if (server.hasArg("act_rt_dbl"))  touchActRightDbl = parseTouchAction(server.arg("act_rt_dbl"), TOUCH_ACT_WEATHER);
  if (server.hasArg("act_lt_hld"))  touchActLeftHold = parseTouchAction(server.arg("act_lt_hld"), TOUCH_ACT_PET);
  if (server.hasArg("act_rt_hld"))  touchActRightHold = parseTouchAction(server.arg("act_rt_hld"), TOUCH_ACT_PET);

  if (server.hasArg("skin")) {
    String s = server.arg("skin");
    s.toLowerCase();
    if (s == "cyberpunk") currentEyeSkin = SKIN_CYBERPUNK;
    else if (s == "cat" || s == "neko") currentEyeSkin = SKIN_CAT;
    else if (s == "retro" || s == "8bit") currentEyeSkin = SKIN_RETRO8BIT;
    else if (s == "anime" || s == "star") currentEyeSkin = SKIN_ANIME;
    else if (s == "hearts" || s == "heart" || s == "love") currentEyeSkin = SKIN_HEARTS;
    else if (s == "steampunk" || s == "goggles") currentEyeSkin = SKIN_STEAMPUNK;
    else if (s == "demon" || s == "devil") currentEyeSkin = SKIN_DEMON;
    else if (s == "matrix" || s == "code" || s == "terminal") currentEyeSkin = SKIN_MATRIX;
    else if (s == "cyborg" || s == "terminator" || s == "t800") currentEyeSkin = SKIN_CYBORG;
    else if (s == "mecha" || s == "gundam" || s == "robot") currentEyeSkin = SKIN_MECHA;
    else if (s == "sunglasses" || s == "shades" || s == "cool") currentEyeSkin = SKIN_SUNGLASSES;
    else if (s == "puppy" || s == "dog" || s == "doggy") currentEyeSkin = SKIN_PUPPY;
    else if (s == "hypno" || s == "spiral" || s == "vortex") currentEyeSkin = SKIN_HYPNO;
    else if (s == "alien" || s == "ufo" || s == "cosmic") currentEyeSkin = SKIN_ALIEN;
    else if (s == "ninja" || s == "shinobi" || s == "naruto") currentEyeSkin = SKIN_NINJA;
    else if (s == "round" || s == "circle" || s == "ocular") currentEyeSkin = SKIN_ROUND;
    else if (s == "kiwi" || s == "deskbuddy" || s == "buddy" || s == "cute" || s == "relaxed" || s == "bored") currentEyeSkin = SKIN_KIWI;
    else if (s == "squircle_solid" || s == "squircle") currentEyeSkin = SKIN_SQUIRCLE_SOLID;
    else if (s == "squircle_outline") currentEyeSkin = SKIN_SQUIRCLE_OUTLINE;
    else if (s == "squircle_glint") currentEyeSkin = SKIN_SQUIRCLE_GLINT;
    else currentEyeSkin = SKIN_DEFAULT;
  }

  if (server.hasArg("mem_name"))
    setOwnerName(server.arg("mem_name"));
  if (server.hasArg("mem_friends"))
    setFriendsFamily(server.arg("mem_friends"));
  if (server.hasArg("new_mem") && server.arg("new_mem").length() > 0)
    addMemoryFact(server.arg("new_mem"));

  if (server.hasArg("wthr_city"))
    weatherCity = server.arg("wthr_city");
  if (server.hasArg("wthr_lat"))
    weatherLat = server.arg("wthr_lat");
  if (server.hasArg("wthr_lon"))
    weatherLon = server.arg("wthr_lon");
  weatherCity.trim();
  weatherLat.trim();
  weatherLon.trim();

  wifiSSID.trim();
  wifiPassword.trim();
  replyLang.trim();
  sttEndpoint.trim();
  sttKey.trim();
  sttModel.trim();
  groqSttEndpoint.trim();
  groqSttKey.trim();
  groqSttModel.trim();
  openaiSttEndpoint.trim();
  openaiSttKey.trim();
  openaiSttModel.trim();
  llmEndpoint.trim();
  llmKey.trim();
  llmModel.trim();
  groqEndpoint.trim();
  groqKey.trim();
  groqModel.trim();
  ttsEndpoint.trim();
  ttsKey.trim();
  ttsModel.trim();
  ttsVoice.trim();
  openaiTtsEndpoint.trim();
  openaiTtsKey.trim();
  openaiTtsModel.trim();
  openaiTtsVoice.trim();
  elevenEndpoint.trim();
  elevenKey.trim();
  elevenModel.trim();
  elevenVoice.trim();

  saveConfig();
  updateDisplayBrightnessMode();

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Connection", "close");
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "0");

  server.send(200, "application/json", "{\"ok\":true,\"reboot\":true,\"message\":\"Settings saved! Kiwi is restarting to apply changes...\"}");
  Serial.println("[PORTAL] Settings saved. Restarting Kiwi to apply changes...");
  delay(1000);
  ESP.restart();
}


void handleScanNetworks() {
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
      if (s.length() == 0) continue;

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

    std::sort(nets.begin(), nets.end(), [](const ScannedNet &a, const ScannedNet &b) {
      return a.rssi > b.rssi;
    });

    for (size_t i = 0; i < nets.size(); ++i) {
      if (i > 0) json += ",";
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

void processPendingSound() {   if (pendingSound < 0)     return;   int8_t s = pendingSound;   pendingSound = -1;   if (!txHandle || busyProcessing)     return;   switch (s) {   case 0: soundPetLove(); break;   case 1: soundPetGiggle(); break;   case 2: soundPetShy(); break;   case 3: soundPetSparkle(); break;   case 4: soundPetKiss(); break;   case 5: soundPetCuddle(); break;   case 6: soundPetPlayful(); break;   case 7: soundPetSleepy(); break;   case 8: soundPetProud(); break;   case 9: soundPetSurprised(); break;   } }
