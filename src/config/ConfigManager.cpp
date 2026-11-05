#include "config/ConfigManager.h"
#include "config/AppConfig.h"
#include "services/AiChatHistory.h"
#include <Preferences.h>
#include <WiFi.h>

static Preferences prefs;

String sanitizeHostname(const String &raw) {
  String out = "";
  out.reserve(32);
  for (size_t i = 0; i < raw.length() && out.length() < 31; i++) {
    char c = raw[i];
    if (c >= 'A' && c <= 'Z') c = c + 32;
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-') {
      out += c;
    }
  }
  while (out.startsWith("-")) out.remove(0, 1);
  while (out.endsWith("-")) out.remove(out.length() - 1);
  if (out.length() == 0) out = "kiwi";
  return out;
}



String normalizeSttLang(const String &v) {

  String s = v;

  s.trim();

  s.toLowerCase();

  if (s == "hi" || s == "en" || s == "auto")

    return s;

  if (s.startsWith("hi"))

    return "hi";

  if (s.startsWith("en"))

    return "en";

  return "auto";

}



String normalizeSttProvider(const String &v) {

  String s = v;

  s.trim();

  s.toLowerCase();

  if (s == "groq" || s == "openai" || s == "sarvam")

    return s;

  return "sarvam";

}



String normalizeTtsProvider(const String &v) {

  String s = v;

  s.trim();

  s.toLowerCase();

  if (s == "sarvam" || s == "openai" || s == "elevenlabs" || s == "fish" || s == "fishaudio")

    return (s == "fishaudio") ? "fish" : s;

  return "sarvam";

}



String sanitizeUrl(const String &val, const char *defaultUrl) {

  String s = val;

  s.trim();

  if (s.length() == 0 || s.startsWith("%") || (!s.startsWith("http://") && !s.startsWith("https://"))) {

    return String(defaultUrl);

  }

  return s;

}



// Gemini model sanitizer � rejects clearly hallucinated/empty model names
// Accepts any model starting with "gemini-" or known aliases (learnlm, gemma, etc.)
static String sanitizeGeminiModel(const String &val, const char *defaultVal) {
  String s = val;
  s.trim();
  if (s.length() == 0) return String(defaultVal);
  // Accept anything that starts with a known Gemini family prefix
  if (s.startsWith("gemini-") || s.startsWith("learnlm") || s.startsWith("gemma")) {
    return s;
  }
  Serial.printf("[CONFIG] Unrecognized Gemini model '%s', resetting to default '%s'\n",
                s.c_str(), defaultVal);
  return String(defaultVal);
}

String sanitizeField(const String &val, const char *defaultVal) {

  String s = val;

  s.trim();

  if (s.startsWith("%")) {

    return String(defaultVal);

  }

  return s;

}



void loadConfig() {

  prefs.begin("myrobot", true);

  wifiSSID = sanitizeField(prefs.getString("ssid", ""));

  wifiPassword = sanitizeField(prefs.getString("pass", ""));

  sttProvider = normalizeSttProvider(sanitizeField(prefs.getString("sttprov", "sarvam"), "sarvam"));

  sttLang = normalizeSttLang(sanitizeField(prefs.getString("sttlang", "hi"), "auto"));

  sttEndpoint = sanitizeUrl(prefs.getString("stturl", DEFAULT_STT_URL), DEFAULT_STT_URL);

  sttKey = sanitizeField(prefs.getString("sttkey", ""));

  sttModel = sanitizeField(prefs.getString("sttmodel", DEFAULT_STT_MODEL), DEFAULT_STT_MODEL);

  groqSttEndpoint = sanitizeUrl(prefs.getString("groqstturl", DEFAULT_GROQ_STT_URL), DEFAULT_GROQ_STT_URL);

  groqSttKey = sanitizeField(prefs.getString("groqsttkey", ""));

  groqSttModel = sanitizeField(prefs.getString("groqsttmodel", DEFAULT_GROQ_STT_MODEL), DEFAULT_GROQ_STT_MODEL);

  openaiSttEndpoint = sanitizeUrl(prefs.getString("opaistturl", DEFAULT_OPENAI_STT_URL), DEFAULT_OPENAI_STT_URL);

  openaiSttKey = sanitizeField(prefs.getString("opaisttkey", ""));

  openaiSttModel = sanitizeField(prefs.getString("opaisttmodel", DEFAULT_OPENAI_STT_MODEL), DEFAULT_OPENAI_STT_MODEL);

  systemPrompt = sanitizeField(prefs.getString("prompt", DEFAULT_PROMPT), DEFAULT_PROMPT);
  if (systemPrompt.length() == 0 ||
      systemPrompt.indexOf("Keep every reply strictly") >= 0 ||
      systemPrompt.indexOf("chaotic buddy") >= 0 ||
      systemPrompt.indexOf("3.3 volts") >= 0 ||
      systemPrompt.indexOf("dodging coffee mugs") >= 0 ||
      systemPrompt.indexOf("croissant") >= 0) {
    systemPrompt = DEFAULT_PROMPT;
    prefs.end();
    prefs.begin("myrobot", false);
    prefs.putString("prompt", systemPrompt);
    prefs.end();
    prefs.begin("myrobot", true);
    Serial.println("[CONFIG] Replaced legacy rambling prompt with clean human-like prompt in NVS");
  }

  llmProvider = sanitizeField(prefs.getString("llmprov", "groq"), "groq");

  llmEndpoint = sanitizeUrl(prefs.getString("llmurl", DEFAULT_LLM_URL), DEFAULT_LLM_URL);

  {
    String rawModel = prefs.getString("llmmodel", DEFAULT_LLM_MODEL);
    llmModel = sanitizeGeminiModel(rawModel, DEFAULT_LLM_MODEL);
    // If the saved model was invalid, rewrite NVS so the bad value is gone
    if (llmModel != rawModel && rawModel.length() > 0) {
      prefs.end();
      prefs.begin("myrobot", false);
      prefs.putString("llmmodel", llmModel);
      prefs.end();
      prefs.begin("myrobot", true);
    }
  }

  llmKey = sanitizeField(prefs.getString("llmkey", ""));

  groqEndpoint = sanitizeUrl(prefs.getString("groqurl", DEFAULT_GROQ_URL), DEFAULT_GROQ_URL);

  groqModel = sanitizeField(prefs.getString("groqmodel", DEFAULT_GROQ_MODEL), DEFAULT_GROQ_MODEL);

  if (groqModel.length() == 0) {

    groqModel = DEFAULT_GROQ_MODEL;

  }

  groqKey = sanitizeField(prefs.getString("groqkey", ""));

  ttsProvider = normalizeTtsProvider(sanitizeField(prefs.getString("ttsprov", "sarvam"), "sarvam"));

  ttsEndpoint = sanitizeUrl(prefs.getString("ttsurl", DEFAULT_TTS_URL), DEFAULT_TTS_URL);

  ttsModel = sanitizeField(prefs.getString("ttsmodel", DEFAULT_TTS_MODEL), DEFAULT_TTS_MODEL);

  ttsVoice = sanitizeField(prefs.getString("ttsvoice", DEFAULT_TTS_VOICE), DEFAULT_TTS_VOICE);

  ttsKey = sanitizeField(prefs.getString("ttskey", ""));

  openaiTtsEndpoint = sanitizeUrl(prefs.getString("openaittsurl", DEFAULT_OPENAI_TTS_URL), DEFAULT_OPENAI_TTS_URL);

  openaiTtsModel = sanitizeField(prefs.getString("openaittsmodel", DEFAULT_OPENAI_TTS_MODEL), DEFAULT_OPENAI_TTS_MODEL);

  openaiTtsVoice = sanitizeField(prefs.getString("openaittsvoice", DEFAULT_OPENAI_TTS_VOICE), DEFAULT_OPENAI_TTS_VOICE);

  openaiTtsKey = sanitizeField(prefs.getString("openaittskey", ""));

  elevenEndpoint = sanitizeUrl(prefs.getString("elevurl", DEFAULT_ELEVEN_URL), DEFAULT_ELEVEN_URL);

  elevenModel = sanitizeField(prefs.getString("elevmodel", DEFAULT_ELEVEN_MODEL), DEFAULT_ELEVEN_MODEL);

  elevenVoice = sanitizeField(prefs.getString("elevvoice", DEFAULT_ELEVEN_VOICE), DEFAULT_ELEVEN_VOICE);

  elevenKey = sanitizeField(prefs.getString("elevkey", ""));

  fishEndpoint = sanitizeUrl(prefs.getString("fishurl", DEFAULT_FISH_TTS_URL), DEFAULT_FISH_TTS_URL);

  fishModel = sanitizeField(prefs.getString("fishmodel", DEFAULT_FISH_TTS_MODEL), DEFAULT_FISH_TTS_MODEL);

  fishVoice = sanitizeField(prefs.getString("fishvoice", ""));

  fishKey = sanitizeField(prefs.getString("fishkey", fishKey));

  replyLang = sanitizeField(prefs.getString("replang", ""));

  speakerVolume = prefs.getInt("volume", 70);

  if (speakerVolume < 0)

    speakerVolume = 0;

  if (speakerVolume > 100)

    speakerVolume = 100;



  displayBrightness = prefs.getInt("disp_bright", 80);

  if (displayBrightness < 0) displayBrightness = 0;

  if (displayBrightness > 100) displayBrightness = 100;

  nightMode = prefs.getInt("night_mode", 1);

  if (nightMode < 0 || nightMode > 2) nightMode = 1;

  nightBrightness = prefs.getInt("night_bright", 10);

  if (nightBrightness < 0) nightBrightness = 0;

  if (nightBrightness > 100) nightBrightness = 100;



  circadianSleepEnabled = prefs.getBool("circ_en", true);

  circadianSleepStart = prefs.getInt("circ_start", 23);

  circadianSleepEnd = prefs.getInt("circ_end", 7);

  boredomEnabled = prefs.getBool("boredom", false);

  boredomMinutes = prefs.getInt("boredom_mins", 2);

  if (boredomMinutes < 1) boredomMinutes = 1;

  if (boredomMinutes > 60) boredomMinutes = 60;

  currentEyeSkin = (EyeSkin)prefs.getInt("eyeskin", (int)SKIN_KIWI);

  showMouth = prefs.getBool("showmouth", true);

  weatherCity = sanitizeField(prefs.getString("wthr_city", "New Delhi"), "New Delhi");

  weatherLat = sanitizeField(prefs.getString("wthr_lat", "28.6139"), "28.6139");

  weatherLon = sanitizeField(prefs.getString("wthr_lon", "77.2090"), "77.2090");



  touchDebounceMs = prefs.getInt("tch_deb", 35);

  if (touchDebounceMs < 10) touchDebounceMs = 10;

  if (touchDebounceMs > 200) touchDebounceMs = 200;

  touchFeedbackSound = prefs.getBool("tch_snd", true);
  if (!touchFeedbackSound) {
    touchFeedbackSound = true;
    prefs.putBool("tch_snd", true);
  }



  touchActTopTap    = (TouchAction)prefs.getInt("act_top_tap", (int)TOUCH_ACT_LISTEN);
  touchActTopDbl    = (TouchAction)prefs.getInt("act_top_dbl", (int)TOUCH_ACT_NONE);
  touchActTopHold   = (TouchAction)prefs.getInt("act_top_hld", (int)TOUCH_ACT_CYCLE_SKIN);
  // Migrate from old default where top double tap was skin cycling
  if (touchActTopDbl == TOUCH_ACT_CYCLE_SKIN && touchActTopHold == TOUCH_ACT_CYCLE_SKIN) {
    touchActTopDbl = TOUCH_ACT_NONE;
  }
  touchActLeftTap   = (TouchAction)prefs.getInt("act_lt_tap",  (int)TOUCH_ACT_DISMISS);

  touchActLeftDbl   = (TouchAction)prefs.getInt("act_lt_dbl",  (int)TOUCH_ACT_CLOCK);

  touchActRightTap  = (TouchAction)prefs.getInt("act_rt_tap",  (int)TOUCH_ACT_DISMISS);

  touchActRightDbl  = (TouchAction)prefs.getInt("act_rt_dbl",  (int)TOUCH_ACT_WEATHER);

  touchActLeftHold  = (TouchAction)prefs.getInt("act_lt_hld",  (int)TOUCH_ACT_PET);

  touchActRightHold = (TouchAction)prefs.getInt("act_rt_hld",  (int)TOUCH_ACT_PET);



  int savedMic = prefs.getInt("micslot", -1);
  if (savedMic == 0 || savedMic == 1) {
    micSlot = savedMic;
    micSlotLocked = true;
    Serial.printf("[MIC] Slot %d cached from NVS\n", micSlot);
  }

  // AP password (MAC last 4 hex + random suffix)
  String savedApPass = prefs.getString("ap_pass", "");
  if (savedApPass.length() < 8 || savedApPass == "25691128") {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char mac4[5];
    snprintf(mac4, sizeof(mac4), "%02X%02X", mac[4], mac[5]);
    uint16_t r = (uint16_t)(esp_random() & 0xFFFF);
    char sfx[5];
    snprintf(sfx, sizeof(sfx), "%04X", r);
    savedApPass = "kiwi-" + String(mac4) + "-" + String(sfx);
    prefs.end();
    prefs.begin("myrobot", false);
    prefs.putString("ap_pass", savedApPass);
    prefs.end();
    prefs.begin("myrobot", true);
  }
  strncpy(apPasswordBuf, savedApPass.c_str(), sizeof(apPasswordBuf) - 1);
  apPasswordBuf[sizeof(apPasswordBuf) - 1] = 0;
  Serial.printf("[AP] AP Password: %s\n", apPasswordBuf);

  // Portal Token (16 random bytes hex-encoded on first boot)
  portalToken = prefs.getString("portal_tok", "");
  if (portalToken.length() == 0) {
    uint8_t randBytes[16];
    for (int i = 0; i < 4; i++) {
      uint32_t r = esp_random();
      memcpy(randBytes + i * 4, &r, 4);
    }
    char hexBuf[33];
    for (int i = 0; i < 16; i++) {
      snprintf(hexBuf + i * 2, 3, "%02x", randBytes[i]);
    }
    hexBuf[32] = 0;
    portalToken = String(hexBuf);
    prefs.end();
    prefs.begin("myrobot", false);
    prefs.putString("portal_tok", portalToken);
    prefs.end();
    prefs.begin("myrobot", true);
  }

  Serial.println("[AUTH] ========================================");
  Serial.printf("[AUTH] Portal Token: %s\n", portalToken.c_str());
  Serial.println("[AUTH] ========================================");

  mdnsHostname = sanitizeHostname(prefs.getString("hostname", "kiwi"));
  maxChatHistory = prefs.getInt("chat_hist", 4);
  if (maxChatHistory < 1 || maxChatHistory > 8) maxChatHistory = 4;

  faceTweenMs = prefs.getInt("face_tween", FACE_TWEEN_MS);
  if (faceTweenMs < 0 || faceTweenMs > 1000) faceTweenMs = FACE_TWEEN_MS;

  prefs.end();
}



void saveConfig() {

  prefs.begin("myrobot", false);

  prefs.putString("ssid", wifiSSID);

  prefs.putString("pass", wifiPassword);

  prefs.putString("sttprov", sttProvider);

  prefs.putString("sttlang", sttLang);

  prefs.putString("stturl", sttEndpoint);

  prefs.putString("sttkey", sttKey);

  prefs.putString("sttmodel", sttModel);

  prefs.putString("groqstturl", groqSttEndpoint);

  prefs.putString("groqsttkey", groqSttKey);

  prefs.putString("groqsttmodel", groqSttModel);

  prefs.putString("opaistturl", openaiSttEndpoint);

  prefs.putString("opaisttkey", openaiSttKey);

  prefs.putString("opaisttmodel", openaiSttModel);

  prefs.putString("prompt", systemPrompt);

  prefs.putString("llmprov", llmProvider);

  prefs.putString("llmurl", llmEndpoint);

  prefs.putString("llmmodel", llmModel);

  prefs.putString("llmkey", llmKey);

  prefs.putString("groqurl", groqEndpoint);

  prefs.putString("groqmodel", groqModel);

  prefs.putString("groqkey", groqKey);

  prefs.putString("ttsprov", ttsProvider);

  prefs.putString("ttsurl", ttsEndpoint);

  prefs.putString("ttsmodel", ttsModel);

  prefs.putString("ttsvoice", ttsVoice);

  prefs.putString("ttskey", ttsKey);

  prefs.putString("openaittsurl", openaiTtsEndpoint);

  prefs.putString("openaittsmodel", openaiTtsModel);

  prefs.putString("openaittsvoice", openaiTtsVoice);

  prefs.putString("openaittskey", openaiTtsKey);

  prefs.putString("elevurl", elevenEndpoint);

  prefs.putString("elevmodel", elevenModel);

  prefs.putString("elevvoice", elevenVoice);

  prefs.putString("elevkey", elevenKey);

  prefs.putString("fishurl", fishEndpoint);

  prefs.putString("fishmodel", fishModel);

  prefs.putString("fishvoice", fishVoice);

  prefs.putString("fishkey", fishKey);

  prefs.putString("replang", replyLang);

  prefs.putInt("volume", speakerVolume);



  prefs.putInt("disp_bright", displayBrightness);

  prefs.putInt("night_mode", nightMode);

  prefs.putInt("night_bright", nightBrightness);

  prefs.putBool("circ_en", circadianSleepEnabled);

  prefs.putInt("circ_start", circadianSleepStart);

  prefs.putInt("circ_end", circadianSleepEnd);

  prefs.putBool("boredom", boredomEnabled);

  prefs.putInt("boredom_mins", boredomMinutes);

  prefs.putInt("eyeskin", (int)currentEyeSkin);

  prefs.putBool("showmouth", showMouth);

  prefs.putString("wthr_city", weatherCity);

  prefs.putString("wthr_lat", weatherLat);

  prefs.putString("wthr_lon", weatherLon);

  prefs.putInt("tch_deb", touchDebounceMs);

  prefs.putBool("tch_snd", touchFeedbackSound);



  prefs.putInt("act_top_tap", (int)touchActTopTap);
  prefs.putInt("act_top_dbl", (int)touchActTopDbl);
  prefs.putInt("act_top_hld", (int)touchActTopHold);
  prefs.putInt("act_lt_tap",  (int)touchActLeftTap);

  prefs.putInt("act_lt_dbl",  (int)touchActLeftDbl);

  prefs.putInt("act_rt_tap",  (int)touchActRightTap);

  prefs.putInt("act_rt_dbl",  (int)touchActRightDbl);

  prefs.putInt("act_lt_hld",  (int)touchActLeftHold);

  prefs.putInt("act_rt_hld",  (int)touchActRightHold);



  if (micSlotLocked) {
    prefs.putInt("micslot", micSlot);
  }

  prefs.putString("hostname", mdnsHostname);
  prefs.putInt("face_tween", faceTweenMs);
  prefs.putInt("chat_hist", maxChatHistory);
  prefs.putString("portal_tok", portalToken);
  prefs.putString("ap_pass", String(apPasswordBuf));

  prefs.end();

}



void clearConfig() {

  prefs.begin("myrobot", false);

  prefs.clear();

  prefs.end();

}

