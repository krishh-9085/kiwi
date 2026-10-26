#include "config/ConfigManager.h"
#include "config/AppConfig.h"
#include <Preferences.h>

static Preferences prefs;

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
  sttLang = normalizeSttLang(sanitizeField(prefs.getString("sttlang", "auto"), "auto"));
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
  if (systemPrompt.length() == 0 || systemPrompt.indexOf("Keep every reply strictly 1 to 2 short punchy sentences") >= 0)
    systemPrompt = DEFAULT_PROMPT;
  llmProvider = sanitizeField(prefs.getString("llmprov", "groq"), "groq");
  llmEndpoint = sanitizeUrl(prefs.getString("llmurl", DEFAULT_LLM_URL), DEFAULT_LLM_URL);
  llmModel = sanitizeField(prefs.getString("llmmodel", DEFAULT_LLM_MODEL), DEFAULT_LLM_MODEL);
  llmKey = sanitizeField(prefs.getString("llmkey", ""));
  groqEndpoint = sanitizeUrl(prefs.getString("groqurl", DEFAULT_GROQ_URL), DEFAULT_GROQ_URL);
  groqModel = sanitizeField(prefs.getString("groqmodel", DEFAULT_GROQ_MODEL), DEFAULT_GROQ_MODEL);
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

  circadianSleepEnabled = prefs.getBool("circ_en", true);
  circadianSleepStart = prefs.getInt("circ_start", 23);
  circadianSleepEnd = prefs.getInt("circ_end", 7);
  boredomEnabled = prefs.getBool("boredom", true);
  boredomMinutes = prefs.getInt("boredom_mins", 2);
  if (boredomMinutes < 1) boredomMinutes = 1;
  if (boredomMinutes > 60) boredomMinutes = 60;
  currentEyeSkin = (EyeSkin)prefs.getInt("eyeskin", (int)SKIN_DEFAULT);
  weatherCity = sanitizeField(prefs.getString("wthr_city", "New Delhi"), "New Delhi");
  weatherLat = sanitizeField(prefs.getString("wthr_lat", "28.6139"), "28.6139");
  weatherLon = sanitizeField(prefs.getString("wthr_lon", "77.2090"), "77.2090");

  touchDebounceMs = prefs.getInt("tch_deb", 35);
  if (touchDebounceMs < 10) touchDebounceMs = 10;
  if (touchDebounceMs > 200) touchDebounceMs = 200;
  touchFeedbackSound = prefs.getBool("tch_snd", true);

  touchActTopTap    = (TouchAction)prefs.getInt("act_top_tap", (int)TOUCH_ACT_LISTEN);
  touchActTopDbl    = (TouchAction)prefs.getInt("act_top_dbl", (int)TOUCH_ACT_CYCLE_SKIN);
  touchActLeftTap   = (TouchAction)prefs.getInt("act_lt_tap",  (int)TOUCH_ACT_DISMISS);
  touchActLeftDbl   = (TouchAction)prefs.getInt("act_lt_dbl",  (int)TOUCH_ACT_CLOCK);
  touchActRightTap  = (TouchAction)prefs.getInt("act_rt_tap",  (int)TOUCH_ACT_DISMISS);
  touchActRightDbl  = (TouchAction)prefs.getInt("act_rt_dbl",  (int)TOUCH_ACT_WEATHER);
  touchActLeftHold  = (TouchAction)prefs.getInt("act_lt_hld",  (int)TOUCH_ACT_PET);
  touchActRightHold = (TouchAction)prefs.getInt("act_rt_hld",  (int)TOUCH_ACT_PET);

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

  prefs.putBool("circ_en", circadianSleepEnabled);
  prefs.putInt("circ_start", circadianSleepStart);
  prefs.putInt("circ_end", circadianSleepEnd);
  prefs.putBool("boredom", boredomEnabled);
  prefs.putInt("boredom_mins", boredomMinutes);
  prefs.putInt("eyeskin", (int)currentEyeSkin);
  prefs.putString("wthr_city", weatherCity);
  prefs.putString("wthr_lat", weatherLat);
  prefs.putString("wthr_lon", weatherLon);
  prefs.putInt("tch_deb", touchDebounceMs);
  prefs.putBool("tch_snd", touchFeedbackSound);

  prefs.putInt("act_top_tap", (int)touchActTopTap);
  prefs.putInt("act_top_dbl", (int)touchActTopDbl);
  prefs.putInt("act_lt_tap",  (int)touchActLeftTap);
  prefs.putInt("act_lt_dbl",  (int)touchActLeftDbl);
  prefs.putInt("act_rt_tap",  (int)touchActRightTap);
  prefs.putInt("act_rt_dbl",  (int)touchActRightDbl);
  prefs.putInt("act_lt_hld",  (int)touchActLeftHold);
  prefs.putInt("act_rt_hld",  (int)touchActRightHold);

  prefs.end();
}

void clearConfig() {
  prefs.begin("myrobot", false);
  prefs.clear();
  prefs.end();
}
