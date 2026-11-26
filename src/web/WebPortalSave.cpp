#include "behavior/HungerEngine.h"
#include "behavior/CircadianEngine.h"
#include "config/AppConfig.h"
#include "config/ConfigManager.h"
#include "face/FaceDisplay.h"
#include "hardware/AudioEngine.h"
#include "hardware/RgbLed.h"
#include "hardware/WakeWordEngine.h"
#include "ai/AiService.h"
#include "memory/MemoryService.h"
#include "net/WifiManager.h"
#include "web/WebPortal.h"
#include "web/WebPortalApiInternal.h"
#include <WebServer.h>
#include <WiFi.h>

void handleSave() {
  notifyWebRequest();
  if (!server.hasArg("ssid")) {
    server.send(400, "text/plain", "SSID required");
    return;
  }
  bool wifiChanged = false;
  String incomingSsid = server.arg("ssid");
  incomingSsid.trim();
  if (incomingSsid.length() > 0 && incomingSsid != "%SSID%" &&
      incomingSsid != wifiSSID) {
    wifiSSID = incomingSsid;
    wifiChanged = true;
  }
  if (server.hasArg("pass")) {
    String incomingPass = server.arg("pass");
    incomingPass.trim();
    if (incomingPass.length() > 0 && incomingPass != "%PASS%" &&
        !isMaskedSecret(incomingPass) && incomingPass != wifiPassword) {
      wifiPassword = incomingPass;
      wifiChanged = true;
    }
  }

  if (server.hasArg("sttprovider"))
    sttProvider = normalizeSttProvider(server.arg("sttprovider"));
  if (server.hasArg("sttlang"))
    sttLang = normalizeSttLang(server.arg("sttlang"));

  sttEndpoint = sanitizeUrl(server.arg("stturl"), DEFAULT_STT_URL);
  if (server.hasArg("ttskey") && server.arg("ttskey").length() > 0 &&
      !isMaskedSecret(server.arg("ttskey"))) {
    sttKey = sanitizeField(server.arg("ttskey"));
  } else if (server.hasArg("sttkey") && server.arg("sttkey").length() > 0 &&
             !isMaskedSecret(server.arg("sttkey"))) {
    sttKey = sanitizeField(server.arg("sttkey"));
  }
  sttModel = sanitizeField(server.arg("sttmodel"), DEFAULT_STT_MODEL);

  groqSttEndpoint = sanitizeUrl(server.arg("groqstturl"), DEFAULT_GROQ_STT_URL);
  if (server.hasArg("groqkey") && server.arg("groqkey").length() > 0 &&
      !isMaskedSecret(server.arg("groqkey"))) {
    groqSttKey = sanitizeField(server.arg("groqkey"));
  } else if (server.hasArg("groqsttkey") &&
             server.arg("groqsttkey").length() > 0 &&
             !isMaskedSecret(server.arg("groqsttkey"))) {
    groqSttKey = sanitizeField(server.arg("groqsttkey"));
  }
  groqSttModel =
      sanitizeField(server.arg("groqsttmodel"), DEFAULT_GROQ_STT_MODEL);

  if (server.hasArg("replang")) {
    replyLang = sanitizeField(server.arg("replang"));
  }
  if (server.hasArg("prompt")) {
    String p = server.arg("prompt");
    p.trim();
    if (p.length() > 0) {
      systemPrompt = p;
    } else {
      systemPrompt = DEFAULT_PROMPT;
    }
  }

  if (server.hasArg("llmprovider")) {
    llmProvider = server.arg("llmprovider");
    llmProvider.trim();
    llmProvider.toLowerCase();
    if (llmProvider != "gemini" && llmProvider != "groq")
      llmProvider = "groq";
  }
  llmEndpoint = sanitizeUrl(server.arg("llmurl"), DEFAULT_LLM_URL);
  if (server.hasArg("llmkey") && server.arg("llmkey").length() > 0 &&
      !isMaskedSecret(server.arg("llmkey"))) {
    llmKey = sanitizeField(server.arg("llmkey"));
  }
  llmModel = sanitizeField(server.arg("llmmodel"), DEFAULT_LLM_MODEL);
  if (llmProvider == "gemini" &&
      llmEndpoint.indexOf("generativelanguage.googleapis.com") >= 0) {
    llmEndpoint = "https://generativelanguage.googleapis.com/v1beta/models/" +
                  llmModel + ":generateContent";
  }
  groqEndpoint = sanitizeUrl(server.arg("groqurl"), DEFAULT_GROQ_URL);
  if (server.hasArg("groqkey") && server.arg("groqkey").length() > 0 &&
      !isMaskedSecret(server.arg("groqkey"))) {
    groqKey = sanitizeField(server.arg("groqkey"));
  }
  groqModel = sanitizeField(server.arg("groqmodel"), DEFAULT_GROQ_MODEL);

  if (server.hasArg("ttsprovider"))
    ttsProvider = normalizeTtsProvider(server.arg("ttsprovider"));
  ttsEndpoint = sanitizeUrl(server.arg("ttsurl"), DEFAULT_TTS_URL);
  if (server.hasArg("ttskey") && server.arg("ttskey").length() > 0 &&
      !isMaskedSecret(server.arg("ttskey"))) {
    ttsKey = sanitizeField(server.arg("ttskey"));
  }
  ttsModel = sanitizeField(server.arg("ttsmodel"), DEFAULT_TTS_MODEL);
  ttsVoice = sanitizeField(server.arg("ttsvoice"), DEFAULT_TTS_VOICE);
  fishEndpoint = sanitizeUrl(server.arg("fishurl"), DEFAULT_FISH_TTS_URL);
  if (server.hasArg("fishkey") && server.arg("fishkey").length() > 0 &&
      !isMaskedSecret(server.arg("fishkey"))) {
    fishKey = sanitizeField(server.arg("fishkey"));
  }
  fishModel = sanitizeField(server.arg("fishmodel"), DEFAULT_FISH_TTS_MODEL);
  fishVoice = sanitizeField(server.arg("fishvoice"));

  // mDNS Hostname
  bool hostChanged = false;
  if (server.hasArg("hostname")) {
    String newHost = sanitizeHostname(server.arg("hostname"));
    if (newHost.length() > 0 && newHost != mdnsHostname) {
      mdnsHostname = newHost;
      initMdns(WiFi.status() == WL_CONNECTED ? "STA" : "AP");
      hostChanged = true;
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
    if (b < 0)
      b = 0;
    if (b > 100)
      b = 100;
    displayBrightness = b;
  }
  if (server.hasArg("night_mode")) {
    int nm = server.arg("night_mode").toInt();
    if (nm >= 0 && nm <= 2)
      nightMode = nm;
  }
  if (server.hasArg("night_bright")) {
    int nb = server.arg("night_bright").toInt();
    if (nb < 0)
      nb = 0;
    if (nb > 100)
      nb = 100;
    nightBrightness = nb;
  }
  bool schedOrTzChanged = false;
  if (server.hasArg("circ_en")) {
    circadianSleepEnabled = parsePortalBool(server.arg("circ_en"));
    schedOrTzChanged = true;
  }
  if (server.hasArg("eco_sleep")) {
    ecoSleepEnabled = parsePortalBool(server.arg("eco_sleep"));
  }
  if (server.hasArg("tz_min")) {
    int tz = server.arg("tz_min").toInt();
    if (tz >= -720 && tz <= 840 && tz != tzOffsetMin) {
      tzOffsetMin = tz;
      startNtpAsync();
      ntpNeedsRefresh = true;
      schedOrTzChanged = true;
    }
  }
  if (server.hasArg("greet_en")) {
    greetingsEnabled = parsePortalBool(server.arg("greet_en"));
  }
  if (server.hasArg("hunger_en")) {
    hungerEnabled = parsePortalBool(server.arg("hunger_en"));
    if (!hungerEnabled && (isKiwiHungry() || isKiwiAngry())) {
      reconcileKiwi();
    }
  }
  if (server.hasArg("snd_react")) {
    soundReactiveEnabled = parsePortalBool(server.arg("snd_react"));
  }
  if (server.hasArg("ww_en")) {
    wakeWordEnabled = parsePortalBool(server.arg("ww_en"));
    wakeWordSetEnabled(wakeWordEnabled);
  }
  if (server.hasArg("ww_cutoff")) {
    wakeWordCutoff = server.arg("ww_cutoff").toFloat();
    wakeWordSetSensitivity(wakeWordCutoff);
  }
  // P3: Apply ww_model BEFORE ww_cutoff
  if (server.hasArg("ww_model")) {
    wakeWordModel = 0; // Alexa only
    wakeWordSetModel(wakeWordModel);
  }
  if (server.hasArg("ww_cutoff")) {
    wakeWordCutoff = server.arg("ww_cutoff").toFloat();
    wakeWordSetSensitivity(wakeWordCutoff);
  }
  if (server.hasArg("ww_gain")) {
    wakeWordGain = server.arg("ww_gain").toFloat();
    wakeWordSetGain(wakeWordGain);
  }
  if (server.hasArg("ww_spike")) {
    wakeWordSpikeTrigger = parsePortalBool(server.arg("ww_spike"));
    wakeWordSetSpikeTrigger(wakeWordSpikeTrigger);
  }
  // STT Adaptive Endpointer Parameters
  if (server.hasArg("stt_trail_ms")) {
    int v = server.arg("stt_trail_ms").toInt();
    if (v >= 100 && v <= 3000)
      sttTrailingMs = v;
  }
  if (server.hasArg("stt_noise_k")) {
    float v = server.arg("stt_noise_k").toFloat();
    if (v >= 1.1f && v <= 10.0f)
      sttNoiseK = v;
  }
  if (server.hasArg("stt_rel_k")) {
    float v = server.arg("stt_rel_k").toFloat();
    if (v >= 0.0f && v <= 1.0f)
      sttRelK = v;
  }
  if (server.hasArg("stt_max_sil")) {
    int v = server.arg("stt_max_sil").toInt();
    if (v >= 300 && v <= 5000)
      sttMaxSilenceMs = v;
  }
  if (server.hasArg("micslot")) {
    int s = server.arg("micslot").toInt();
    setMicSlot(s, true, true);
  }
  if (server.hasArg("micslot_lock")) {
    setMicSlotLocked(parsePortalBool(server.arg("micslot_lock")));
  }
  if (server.hasArg("rgb_en")) {
    rgbLedEnabled = parsePortalBool(server.arg("rgb_en"));
    rgbLedSetEnabled(rgbLedEnabled);
  }
  if (server.hasArg("rgb_bright")) {
    int bPct = server.arg("rgb_bright").toInt();
    if (bPct < 0)
      bPct = 0;
    if (bPct > 100)
      bPct = 100;
    rgbLedBrightness = (uint8_t)((bPct * 255) / 100);
    rgbLedSetBrightness(rgbLedBrightness);
  }
  if (server.hasArg("circ_start")) {
    int h = circadianSleepStart;
    int m = circadianSleepStartMin;
    if (parseScheduleTimeString(server.arg("circ_start"), h, m)) {
      circadianSleepStart = h;
      circadianSleepStartMin = m;
      schedOrTzChanged = true;
    }
  }
  if (server.hasArg("circ_smin")) {
    int sm = server.arg("circ_smin").toInt();
    if (sm >= 0 && sm <= 59) {
      circadianSleepStartMin = sm;
      schedOrTzChanged = true;
    }
  }
  if (server.hasArg("circ_end")) {
    int h = circadianSleepEnd;
    int m = circadianSleepEndMin;
    if (parseScheduleTimeString(server.arg("circ_end"), h, m)) {
      circadianSleepEnd = h;
      circadianSleepEndMin = m;
      schedOrTzChanged = true;
    }
  }
  if (server.hasArg("circ_emin")) {
    int em = server.arg("circ_emin").toInt();
    if (em >= 0 && em <= 59) {
      circadianSleepEndMin = em;
      schedOrTzChanged = true;
    }
  }
  if (server.hasArg("boredom")) {
    boredomEnabled = parsePortalBool(server.arg("boredom"));
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
    touchFeedbackSound = server.hasArg("tch_snd")
                             ? parsePortalBool(server.arg("tch_snd"))
                             : false;
  }

  if (server.hasArg("act_top_tap"))
    touchActTopTap =
        parseTouchAction(server.arg("act_top_tap"), TOUCH_ACT_LISTEN);
  if (server.hasArg("act_top_dbl"))
    touchActTopDbl =
        parseTouchAction(server.arg("act_top_dbl"), TOUCH_ACT_NONE);
  if (server.hasArg("act_top_hld"))
    touchActTopHold =
        parseTouchAction(server.arg("act_top_hld"), TOUCH_ACT_CYCLE_SKIN);
  if (server.hasArg("act_lt_tap"))
    touchActLeftTap =
        parseTouchAction(server.arg("act_lt_tap"), TOUCH_ACT_DISMISS);
  if (server.hasArg("act_lt_dbl"))
    touchActLeftDbl =
        parseTouchAction(server.arg("act_lt_dbl"), TOUCH_ACT_CLOCK);
  if (server.hasArg("act_rt_tap"))
    touchActRightTap =
        parseTouchAction(server.arg("act_rt_tap"), TOUCH_ACT_DISMISS);
  if (server.hasArg("act_rt_dbl"))
    touchActRightDbl =
        parseTouchAction(server.arg("act_rt_dbl"), TOUCH_ACT_WEATHER);
  if (server.hasArg("act_lt_hld"))
    touchActLeftHold =
        parseTouchAction(server.arg("act_lt_hld"), TOUCH_ACT_PET);
  if (server.hasArg("act_rt_hld"))
    touchActRightHold =
        parseTouchAction(server.arg("act_rt_hld"), TOUCH_ACT_PET);

  if (server.hasArg("skin")) {
    currentEyeSkin = parseEyeSkinArg(server.arg("skin"));
  }

  if (server.hasArg("mem_name"))
    setOwnerNameNoSave(server.arg("mem_name"));
  if (server.hasArg("mem_friends"))
    setFriendsFamilyNoSave(server.arg("mem_friends"));
  if (server.hasArg("mem_name") || server.hasArg("mem_friends"))
    saveMemory();
  if (server.hasArg("facts_present")) {
    String parsedFacts[MAX_PERSISTENT_MEMORIES];
    int factCount = 0;
    for (int i = 0; i < server.args() && factCount < MAX_PERSISTENT_MEMORIES; i++) {
      if (server.argName(i) == "facts[]") {
        String f = server.arg(i);
        f.trim();
        if (f.length() > 0)
          parsedFacts[factCount++] = f;
      }
    }
    syncMemoryFacts(parsedFacts, factCount);
  }
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

  saveConfigImmediate();
  if (schedOrTzChanged) {
    onCircadianScheduleChanged();
  } else {
    updateDisplayBrightnessMode();
  }
  invalidateStatusCache();

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Connection", "close");
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "0");

  if (wifiChanged || hostChanged) {
    server.send(
        200, "application/json",
        "{\"ok\":true,\"reboot\":true,\"message\":\"Network configuration "
        "updated! Kiwi is restarting to apply changes...\"}");
    Serial.println("[PORTAL] Network credentials changed. Restarting Kiwi to "
                   "apply changes...");
    delay(1000);
    ESP.restart();
  } else {
    server.send(200, "application/json",
                "{\"ok\":true,\"reboot\":false,\"message\":\"Settings saved "
                "and applied live!\"}");
    Serial.println(
        "[PORTAL] Settings saved and applied live (no reboot required).");
  }
}
