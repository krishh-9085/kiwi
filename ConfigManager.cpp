#include "ConfigManager.h"
#include "Config.h"
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
  if (s == "sarvam" || s == "openai" || s == "elevenlabs")
    return s;
  return "sarvam";
}

void loadConfig() {
  prefs.begin("myrobot", true);
  wifiSSID = prefs.getString("ssid", "");
  wifiPassword = prefs.getString("pass", "");
  sttProvider = normalizeSttProvider(prefs.getString("sttprov", "sarvam"));
  sttLang = normalizeSttLang(prefs.getString("sttlang", "auto"));
  sttEndpoint = prefs.getString("stturl", DEFAULT_STT_URL);
  sttKey = prefs.getString("sttkey", "");
  sttModel = prefs.getString("sttmodel", DEFAULT_STT_MODEL);
  groqSttEndpoint = prefs.getString("groqstturl", DEFAULT_GROQ_STT_URL);
  groqSttKey = prefs.getString("groqsttkey", "");
  groqSttModel = prefs.getString("groqsttmodel", DEFAULT_GROQ_STT_MODEL);
  openaiSttEndpoint = prefs.getString("opaistturl", DEFAULT_OPENAI_STT_URL);
  openaiSttKey = prefs.getString("opaisttkey", "");
  openaiSttModel = prefs.getString("opaisttmodel", DEFAULT_OPENAI_STT_MODEL);
  systemPrompt = prefs.getString("prompt", DEFAULT_PROMPT);
  // Replace the earlier rigid personality prompt on existing devices.
  if (systemPrompt.indexOf("Keep every reply strictly 1 to 2 short punchy sentences") >= 0)
    systemPrompt = DEFAULT_PROMPT;
  llmProvider = prefs.getString("llmprov", "groq");
  llmEndpoint = prefs.getString("llmurl", DEFAULT_LLM_URL);
  llmModel = prefs.getString("llmmodel", DEFAULT_LLM_MODEL);
  llmKey = prefs.getString("llmkey", "");
  groqEndpoint = prefs.getString("groqurl", DEFAULT_GROQ_URL);
  groqModel = prefs.getString("groqmodel", DEFAULT_GROQ_MODEL);
  groqKey = prefs.getString("groqkey", "");
  ttsProvider = normalizeTtsProvider(prefs.getString("ttsprov", "sarvam"));
  ttsEndpoint = prefs.getString("ttsurl", DEFAULT_TTS_URL);
  ttsModel = prefs.getString("ttsmodel", DEFAULT_TTS_MODEL);
  ttsVoice = prefs.getString("ttsvoice", DEFAULT_TTS_VOICE);
  ttsKey = prefs.getString("ttskey", "");
  openaiTtsEndpoint = prefs.getString("openaittsurl", DEFAULT_OPENAI_TTS_URL);
  openaiTtsModel = prefs.getString("openaittsmodel", DEFAULT_OPENAI_TTS_MODEL);
  openaiTtsVoice = prefs.getString("openaittsvoice", DEFAULT_OPENAI_TTS_VOICE);
  openaiTtsKey = prefs.getString("openaittskey", "");
  elevenEndpoint = prefs.getString("elevurl", DEFAULT_ELEVEN_URL);
  elevenModel = prefs.getString("elevmodel", DEFAULT_ELEVEN_MODEL);
  elevenVoice = prefs.getString("elevvoice", DEFAULT_ELEVEN_VOICE);
  elevenKey = prefs.getString("elevkey", "");
  replyLang = prefs.getString("replang", "");
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
  weatherCity = prefs.getString("wthr_city", "New Delhi");
  weatherLat = prefs.getString("wthr_lat", "28.6139");
  weatherLon = prefs.getString("wthr_lon", "77.2090");

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

  prefs.end();
}

void clearConfig() {
  prefs.begin("myrobot", false);
  prefs.clear();
  prefs.end();
}
