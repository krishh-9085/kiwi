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
      server.send(200, "application/json", "{\"ok\":false,\"reason\":\"busy\"}");
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
      server.send(200, "application/json", "{\"ok\":false,\"reason\":\"busy\"}");
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
  String j = "{\"emotion\":\"" + String(names[idx]) + "\",\"busy\":";
  j += busyProcessing ? "true," : "false,";
  j += "\"sleeping\":";
  j += (inCircadianSleep || manualSleep) ? "true," : "false,";
  j += "\"history\":" + String(getConversationHistoryCount());
  const char *skinNames[] = {
    "default", "cyberpunk", "cat", "retro", "anime", "hearts", "steampunk", "demon",
    "matrix", "cyborg", "mecha", "sunglasses", "puppy", "hypno", "alien", "ninja", "round",
    "kiwi"
  };
  int skIdx = (int)currentEyeSkin;
  if (skIdx < 0 || skIdx >= SKIN_COUNT) skIdx = 0;
  j += ",\"boredom\":" + String(boredomEnabled ? "true" : "false");
  j += ",\"boredom_mins\":" + String(boredomMinutes);
  j += ",\"skin\":\"" + String(skinNames[skIdx]) + "\"";
  j += ",\"state\":\"" + escJson(robotStateDesc) + "\"";
  j += ",\"question\":\"" + escJson(lastTranscribedQuestion) + "\"";
  j += ",\"answer\":\"" + escJson(lastLlmAnswer) + "\"";
  j += ",\"logs\":" + getTerminalJson();
  j += "}";
  server.send(200, "application/json", j);
}

void handleConfigApi() {
  String j = "{";
  j += "\"wifi\":{\"ssid\":\"" + escJson(wifiSSID) + "\",\"password\":\"" + escJson(wifiPassword) + "\"},";
  j += "\"stt\":{\"provider\":\"" + escJson(sttProvider) + "\",\"lang\":\"" + escJson(sttLang) + "\",";
  j += "\"url\":\"" + escJson(sttEndpoint) + "\",\"key\":\"" + escJson(sttKey) + "\",\"model\":\"" + escJson(sttModel) + "\",";
  j += "\"groqUrl\":\"" + escJson(groqSttEndpoint) + "\",\"groqKey\":\"" + escJson(groqSttKey) + "\",\"groqModel\":\"" + escJson(groqSttModel) + "\",";
  j += "\"openaiUrl\":\"" + escJson(openaiSttEndpoint) + "\",\"openaiKey\":\"" + escJson(openaiSttKey) + "\",\"openaiModel\":\"" + escJson(openaiSttModel) + "\"},";
  j += "\"llm\":{\"provider\":\"" + escJson(llmProvider) + "\",\"url\":\"" + escJson(llmEndpoint) + "\",\"key\":\"" + escJson(llmKey) + "\",\"model\":\"" + escJson(llmModel) + "\",";
  j += "\"groqUrl\":\"" + escJson(groqEndpoint) + "\",\"groqKey\":\"" + escJson(groqKey) + "\",\"groqModel\":\"" + escJson(groqModel) + "\"},";
  j += "\"tts\":{\"provider\":\"" + escJson(ttsProvider) + "\",\"url\":\"" + escJson(ttsEndpoint) + "\",\"key\":\"" + escJson(ttsKey) + "\",\"model\":\"" + escJson(ttsModel) + "\",\"voice\":\"" + escJson(ttsVoice) + "\",";
  j += "\"openaiUrl\":\"" + escJson(openaiTtsEndpoint) + "\",\"openaiKey\":\"" + escJson(openaiTtsKey) + "\",\"openaiModel\":\"" + escJson(openaiTtsModel) + "\",\"openaiVoice\":\"" + escJson(openaiTtsVoice) + "\",";
  j += "\"elevenUrl\":\"" + escJson(elevenEndpoint) + "\",\"elevenKey\":\"" + escJson(elevenKey) + "\",\"elevenModel\":\"" + escJson(elevenModel) + "\",\"elevenVoice\":\"" + escJson(elevenVoice) + "\"},";
  j += "\"volume\":" + String(speakerVolume) + ",\"replyLang\":\"" + escJson(replyLang) + "\",\"prompt\":\"" + escJson(systemPrompt) + "\",";
  j += "\"circadian\":{\"enabled\":" + String(circadianSleepEnabled ? "true" : "false") + ",\"start\":" + String(circadianSleepStart) + ",\"end\":" + String(circadianSleepEnd) + "},";
  j += "\"boredom\":{\"enabled\":" + String(boredomEnabled ? "true" : "false") + ",\"minutes\":" + String(boredomMinutes) + "},";
  j += "\"weather\":{\"city\":\"" + escJson(weatherCity) + "\",\"lat\":\"" + escJson(weatherLat) + "\",\"lon\":\"" + escJson(weatherLon) + "\"},";
  const char *skinNames[] = {"default", "cyberpunk", "cat", "retro", "anime", "hearts", "steampunk", "demon", "matrix", "cyborg", "mecha", "sunglasses", "puppy", "hypno", "alien", "ninja", "round", "kiwi"};
  int skinIndex = (int)currentEyeSkin;
  if (skinIndex < 0 || skinIndex >= SKIN_COUNT) skinIndex = 0;
  j += "\"skin\":\"" + String(skinNames[skinIndex]) + "\",\"memory\":{\"name\":\"" + escJson(getOwnerName()) + "\",\"facts\":[";
  for (int i = 0; i < kiwiMemory.memoryCount; i++) {
    if (i) j += ",";
    j += "\"" + escJson(kiwiMemory.memories[i]) + "\"";
  }
  j += "],\"followUp\":\"" + escJson(kiwiMemory.pendingFollowUp) + "\"}}";
  server.send(200, "application/json", j);
}

void handleSave() {
  if (!server.hasArg("ssid")) {
    server.send(400, "text/plain", "SSID required");
    return;
  }
  wifiSSID = server.arg("ssid");
  wifiPassword = server.arg("pass");

  if (server.hasArg("sttprovider"))
    sttProvider = normalizeSttProvider(server.arg("sttprovider"));
  if (server.hasArg("sttlang"))
    sttLang = normalizeSttLang(server.arg("sttlang"));

  sttEndpoint = server.arg("stturl");
  sttKey = server.arg("sttkey");
  sttModel = server.arg("sttmodel");
  groqSttEndpoint = server.arg("groqstturl");
  groqSttKey = server.arg("groqsttkey");
  groqSttModel = server.arg("groqsttmodel");
  openaiSttEndpoint = server.arg("opaistturl");
  openaiSttKey = server.arg("opaisttkey");
  openaiSttModel = server.arg("opaisttmodel");

  replyLang = server.arg("replang");
  systemPrompt = server.arg("prompt");

  if (server.hasArg("llmprovider")) {
    llmProvider = server.arg("llmprovider");
    llmProvider.trim();
    llmProvider.toLowerCase();
    if (llmProvider != "gemini" && llmProvider != "groq" && llmProvider != "openai")
      llmProvider = "groq";
  }
  if (llmProvider == "openai") {
    llmEndpoint = server.arg("openaiurl");
    llmKey = server.arg("openaikey");
    llmModel = server.arg("openaimodel");
  } else {
    llmEndpoint = server.arg("llmurl");
    llmKey = server.arg("llmkey");
    llmModel = server.arg("llmmodel");
  }
  groqEndpoint = server.arg("groqurl");
  groqKey = server.arg("groqkey");
  groqModel = server.arg("groqmodel");

  if (server.hasArg("ttsprovider"))
    ttsProvider = normalizeTtsProvider(server.arg("ttsprovider"));
  ttsEndpoint = server.arg("ttsurl");
  ttsKey = server.arg("ttskey");
  ttsModel = server.arg("ttsmodel");
  ttsVoice = server.arg("ttsvoice");
  openaiTtsEndpoint = server.arg("openaittsurl");
  openaiTtsKey = server.arg("openaittskey");
  openaiTtsModel = server.arg("openaittsmodel");
  openaiTtsVoice = server.arg("openaittsvoice");
  elevenEndpoint = server.arg("elevurl");
  elevenKey = server.arg("elevkey");
  elevenModel = server.arg("elevmodel");
  elevenVoice = server.arg("elevvoice");

  if (server.hasArg("vol")) {
    speakerVolume = server.arg("vol").toInt();
    if (speakerVolume < 0)
      speakerVolume = 0;
    if (speakerVolume > 100)
      speakerVolume = 100;
  }

  circadianSleepEnabled = server.hasArg("circ_en");
  if (server.hasArg("circ_start"))
    circadianSleepStart = server.arg("circ_start").toInt();
  if (server.hasArg("circ_end"))
    circadianSleepEnd = server.arg("circ_end").toInt();
  boredomEnabled = server.hasArg("boredom");
  if (server.hasArg("boredom_mins")) {
    int bm = server.arg("boredom_mins").toInt();
    if (bm >= 1 && bm <= 60) {
      boredomMinutes = bm;
      resetBoredomInterval();
    }
  }

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
    else currentEyeSkin = SKIN_DEFAULT;
  }

  if (server.hasArg("mem_name"))
    setOwnerName(server.arg("mem_name"));
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
  server.send(200, "text/html",
              "<html><body style='font-family:Arial;text-align:center;background:#111;color:white;padding:50px'>"
              "<h1>Saved</h1><p>Restarting Kiwi...</p></body></html>");
  delay(1200);
  ESP.restart();
}


void processPendingSound() {   if (pendingSound < 0)     return;   int8_t s = pendingSound;   pendingSound = -1;   if (!txHandle || busyProcessing)     return;   switch (s) {   case 0: soundPetLove(); break;   case 1: soundPetGiggle(); break;   case 2: soundPetShy(); break;   case 3: soundPetSparkle(); break;   case 4: soundPetKiss(); break;   case 5: soundPetCuddle(); break;   case 6: soundPetPlayful(); break;   case 7: soundPetSleepy(); break;   case 8: soundPetProud(); break;   case 9: soundPetSurprised(); break;   } }
