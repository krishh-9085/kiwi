#include "config/ConfigManager.h"
#include "config/AppConfig.h"
#include "hardware/AudioEngine.h"
#include "hardware/RgbLed.h"
#include "services/AiChatHistory.h"
#include <Preferences.h>
#include <WiFi.h>

static Preferences prefs;
static bool configDirty = false;
static uint32_t configDirtyAt = 0;
static const uint32_t CONFIG_DEBOUNCE_MS = 10000; // 10 seconds of idle debounce

String sanitizeHostname(const String &raw) {
  String out = "";
  out.reserve(32);
  for (size_t i = 0; i < raw.length() && out.length() < 31; i++) {
    char c = raw[i];
    if (c >= 'A' && c <= 'Z')
      c = c + 32;
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-') {
      out += c;
    }
  }
  while (out.startsWith("-"))
    out.remove(0, 1);
  while (out.endsWith("-"))
    out.remove(out.length() - 1);
  if (out.length() == 0)
    out = "kiwi";
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

  if (s == "groq" || s == "sarvam")

    return s;

  return "sarvam";
}

String normalizeTtsProvider(const String &v) {

  String s = v;

  s.trim();

  s.toLowerCase();

  if (s == "sarvam" || s == "fish" || s == "fishaudio")

    return (s == "fishaudio") ? "fish" : s;

  return "sarvam";
}

String sanitizeUrl(const String &val, const char *defaultUrl) {

  String s = val;

  s.trim();

  if (s.length() == 0 || s.startsWith("%") ||
      (!s.startsWith("http://") && !s.startsWith("https://"))) {

    return String(defaultUrl);
  }

  return s;
}

// Gemini model sanitizer � rejects clearly hallucinated/empty model names
// Accepts any model starting with "gemini-" or known aliases (learnlm, gemma,
// etc.)
static String sanitizeGeminiModel(const String &val, const char *defaultVal) {
  String s = val;
  s.trim();
  if (s.length() == 0)
    return String(defaultVal);
  // Accept anything that starts with a known Gemini family prefix
  if (s.startsWith("gemini-") || s.startsWith("learnlm") ||
      s.startsWith("gemma")) {
    return s;
  }
  Serial.printf(
      "[CONFIG] Unrecognized Gemini model '%s', resetting to default '%s'\n",
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

  sttProvider = normalizeSttProvider(
      sanitizeField(prefs.getString("sttprov", "sarvam"), "sarvam"));

  sttLang =
      normalizeSttLang(sanitizeField(prefs.getString("sttlang", "hi"), "auto"));

  sttEndpoint =
      sanitizeUrl(prefs.getString("stturl", DEFAULT_STT_URL), DEFAULT_STT_URL);

  sttKey = sanitizeField(prefs.getString("sttkey", ""));

  sttModel = sanitizeField(prefs.getString("sttmodel", DEFAULT_STT_MODEL),
                           DEFAULT_STT_MODEL);

  groqSttEndpoint =
      sanitizeUrl(prefs.getString("groqstturl", DEFAULT_GROQ_STT_URL),
                  DEFAULT_GROQ_STT_URL);

  groqSttKey = sanitizeField(prefs.getString("groqsttkey", ""));

  groqSttModel =
      sanitizeField(prefs.getString("groqsttmodel", DEFAULT_GROQ_STT_MODEL),
                    DEFAULT_GROQ_STT_MODEL);

  systemPrompt =
      sanitizeField(prefs.getString("prompt", DEFAULT_PROMPT), DEFAULT_PROMPT);
  if (systemPrompt.length() == 0) {
    systemPrompt = DEFAULT_PROMPT;
  }

  llmProvider = sanitizeField(prefs.getString("llmprov", "groq"), "groq");

  llmEndpoint =
      sanitizeUrl(prefs.getString("llmurl", DEFAULT_LLM_URL), DEFAULT_LLM_URL);

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

  // Auto-sync standard Google Gemini endpoint with the loaded llmModel
  if (llmEndpoint.indexOf("generativelanguage.googleapis.com") >= 0) {
    String expectedEp = "https://generativelanguage.googleapis.com/v1beta/models/" +
                        llmModel + ":generateContent";
    if (llmEndpoint != expectedEp) {
      llmEndpoint = expectedEp;
    }
  }

  llmKey = sanitizeField(prefs.getString("llmkey", ""));

  groqEndpoint = sanitizeUrl(prefs.getString("groqurl", DEFAULT_GROQ_URL),
                             DEFAULT_GROQ_URL);

  groqModel = sanitizeField(prefs.getString("groqmodel", DEFAULT_GROQ_MODEL),
                            DEFAULT_GROQ_MODEL);

  if (groqModel.length() == 0) {

    groqModel = DEFAULT_GROQ_MODEL;
  }

  groqKey = sanitizeField(prefs.getString("groqkey", ""));

  ttsProvider = normalizeTtsProvider(
      sanitizeField(prefs.getString("ttsprov", "sarvam"), "sarvam"));

  ttsEndpoint =
      sanitizeUrl(prefs.getString("ttsurl", DEFAULT_TTS_URL), DEFAULT_TTS_URL);

  ttsModel = sanitizeField(prefs.getString("ttsmodel", DEFAULT_TTS_MODEL),
                           DEFAULT_TTS_MODEL);

  ttsVoice = sanitizeField(prefs.getString("ttsvoice", DEFAULT_TTS_VOICE),
                           DEFAULT_TTS_VOICE);

  ttsKey = sanitizeField(prefs.getString("ttskey", ""));

  fishEndpoint = sanitizeUrl(prefs.getString("fishurl", DEFAULT_FISH_TTS_URL),
                             DEFAULT_FISH_TTS_URL);

  fishModel =
      sanitizeField(prefs.getString("fishmodel", DEFAULT_FISH_TTS_MODEL),
                    DEFAULT_FISH_TTS_MODEL);

  fishVoice = sanitizeField(prefs.getString("fishvoice", ""));

  fishKey = sanitizeField(prefs.getString("fishkey", fishKey));

  replyLang = sanitizeField(prefs.getString("replang", ""));

  speakerVolume = prefs.getInt("volume", 70);

  if (speakerVolume < 0)

    speakerVolume = 0;

  if (speakerVolume > 100)

    speakerVolume = 100;

  displayBrightness = prefs.getInt("disp_bright", 40);

  if (displayBrightness < 0)
    displayBrightness = 0;

  if (displayBrightness > 100)
    displayBrightness = 100;

  nightMode = prefs.getInt("night_mode", 1);

  if (nightMode < 0 || nightMode > 2)
    nightMode = 1;

  nightBrightness = prefs.getInt("night_bright", 10);

  if (nightBrightness < 0)
    nightBrightness = 0;

  if (nightBrightness > 100)
    nightBrightness = 100;

  circadianSleepEnabled = prefs.getBool("circ_en", true);
  greetingsEnabled = prefs.getBool("greet_en", false);
  hungerEnabled = prefs.getBool("hunger_en", true);
  soundReactiveEnabled = prefs.getBool("snd_react", false);
  wakeWordEnabled = prefs.getBool("ww_en", true);
  wakeWordModel = 0; // Alexa only

  float defCutoff = 0.90f;
  wakeWordCutoff = prefs.getFloat("ww_cutoff", defCutoff);
  if (fabsf(wakeWordCutoff - 0.40f) < 0.05f || fabsf(wakeWordCutoff - 0.82f) < 0.005f) {
    wakeWordCutoff = 0.90f;
    prefs.putFloat("ww_cutoff", 0.90f);
    Serial.println("[CONFIG] Migrated wake-word cutoff to 0.90 (Alexa)");
  }

  rgbLedEnabled = prefs.getBool("rgb_en", true);
  rgbLedBrightness = prefs.getUChar("rgb_bright", 160);
  rgbLedSetBrightness(rgbLedBrightness);

  wakeWordGain = prefs.getFloat("ww_gain", 8.0f);
  wakeWordSpikeTrigger = prefs.getBool("ww_spike", false);

  circadianSleepStart = prefs.getInt("circ_start", 23);

  circadianSleepEnd = prefs.getInt("circ_end", 7);

  boredomEnabled = prefs.getBool("boredom", false);

  boredomMinutes = prefs.getInt("boredom_mins", 2);

  if (boredomMinutes < 1)
    boredomMinutes = 1;

  if (boredomMinutes > 60)
    boredomMinutes = 60;

  int savedSkin = prefs.getInt("eyeskin", (int)SKIN_KIWI);
  if (savedSkin < 0 || savedSkin >= SKIN_COUNT) {
    savedSkin = (int)SKIN_KIWI;
  }
  currentEyeSkin = (EyeSkin)savedSkin;

  showMouth = prefs.getBool("showmouth", true);

  weatherCity =
      sanitizeField(prefs.getString("wthr_city", "New Delhi"), "New Delhi");

  weatherLat = sanitizeField(prefs.getString("wthr_lat", "28.6139"), "28.6139");

  weatherLon = sanitizeField(prefs.getString("wthr_lon", "77.2090"), "77.2090");

  touchDebounceMs = prefs.getInt("tch_deb", 35);

  if (touchDebounceMs < 10)
    touchDebounceMs = 10;

  if (touchDebounceMs > 200)
    touchDebounceMs = 200;

  touchFeedbackSound = prefs.getBool("tch_snd", true);
  if (!touchFeedbackSound) {
    touchFeedbackSound = true;
    prefs.putBool("tch_snd", true);
  }

  touchActTopTap =
      (TouchAction)prefs.getInt("act_top_tap", (int)TOUCH_ACT_LISTEN);
  if (touchActTopTap >= TOUCH_ACT_NONE) {
    touchActTopTap = TOUCH_ACT_LISTEN;
  }
  touchActTopDbl =
      (TouchAction)prefs.getInt("act_top_dbl", (int)TOUCH_ACT_NONE);
  touchActTopHold =
      (TouchAction)prefs.getInt("act_top_hld", (int)TOUCH_ACT_CYCLE_SKIN);
  if (touchActTopHold >= TOUCH_ACT_NONE) {
    touchActTopHold = TOUCH_ACT_CYCLE_SKIN;
  }
  if (touchActTopDbl == TOUCH_ACT_CYCLE_SKIN) {
    touchActTopDbl = TOUCH_ACT_NONE;
  }
  touchActLeftTap =
      (TouchAction)prefs.getInt("act_lt_tap", (int)TOUCH_ACT_DISMISS);

  touchActLeftDbl =
      (TouchAction)prefs.getInt("act_lt_dbl", (int)TOUCH_ACT_CLOCK);

  touchActRightTap =
      (TouchAction)prefs.getInt("act_rt_tap", (int)TOUCH_ACT_DISMISS);

  touchActRightDbl =
      (TouchAction)prefs.getInt("act_rt_dbl", (int)TOUCH_ACT_WEATHER);

  touchActLeftHold =
      (TouchAction)prefs.getInt("act_lt_hld", (int)TOUCH_ACT_PET);

  touchActRightHold =
      (TouchAction)prefs.getInt("act_rt_hld", (int)TOUCH_ACT_PET);

  int savedMic = prefs.getInt("micslot", -1);
  if (savedMic == 0 || savedMic == 1) {
    setMicSlot(savedMic, false, true);
    setMicSlotLocked(true);
    Serial.printf("[MIC] Slot %d cached from NVS (locked)\n", micSlot);
  } else {
    setMicSlot(0, false, true);
    setMicSlotLocked(false);
  }

  // AP password (default: 25691128)
  String savedApPass = prefs.getString("ap_pass", "25691128");
  if (savedApPass.length() < 8 || savedApPass.startsWith("kiwi-")) {
    savedApPass = "25691128";
    prefs.end();
    prefs.begin("myrobot", false);
    prefs.putString("ap_pass", savedApPass);
    prefs.end();
    prefs.begin("myrobot", true);
  }
  strncpy(apPasswordBuf, savedApPass.c_str(), sizeof(apPasswordBuf) - 1);
  apPasswordBuf[sizeof(apPasswordBuf) - 1] = 0;
  Serial.printf("[AP] AP Password: %s\n", apPasswordBuf);

  mdnsHostname = sanitizeHostname(prefs.getString("hostname", "kiwi"));
  maxChatHistory = prefs.getInt("chat_hist", 4);
  if (maxChatHistory < 1 || maxChatHistory > 8)
    maxChatHistory = 4;

  faceTweenMs = prefs.getInt("face_tween", FACE_TWEEN_MS);
  if (faceTweenMs < 0 || faceTweenMs > 1000)
    faceTweenMs = FACE_TWEEN_MS;

  // STT Adaptive Endpointer Parameters
  sttTrailingMs   = prefs.getInt("stt_trail_ms", 450);
  if (sttTrailingMs < 100 || sttTrailingMs > 3000) sttTrailingMs = 450;
  sttNoiseK       = prefs.getFloat("stt_noise_k", 2.2f);
  if (sttNoiseK < 1.1f || sttNoiseK > 10.0f) sttNoiseK = 2.2f;
  sttRelK         = prefs.getFloat("stt_rel_k", 0.20f);
  if (sttRelK < 0.0f || sttRelK > 1.0f) sttRelK = 0.20f;
  sttMaxSilenceMs = prefs.getInt("stt_max_sil", 1200);
  if (sttMaxSilenceMs < 300 || sttMaxSilenceMs > 5000) sttMaxSilenceMs = 1200;

  prefs.end();
}

bool isConfigDirty() { return configDirty; }

void saveConfig() {
  configDirty = true;
  configDirtyAt = millis();
}

void saveConfigImmediate() {
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
  prefs.putBool("greet_en", greetingsEnabled);
  prefs.putBool("hunger_en", hungerEnabled);
  prefs.putBool("snd_react", soundReactiveEnabled);
  prefs.putBool("ww_en", wakeWordEnabled);
  prefs.putFloat("ww_cutoff", wakeWordCutoff);
  prefs.putUChar("ww_model", wakeWordModel);
  prefs.putFloat("ww_gain", wakeWordGain);
  prefs.putBool("ww_spike", wakeWordSpikeTrigger);
  prefs.putBool("rgb_en", rgbLedEnabled);
  prefs.putUChar("rgb_bright", rgbLedBrightness);

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
  prefs.putInt("act_lt_tap", (int)touchActLeftTap);

  prefs.putInt("act_lt_dbl", (int)touchActLeftDbl);

  prefs.putInt("act_rt_tap", (int)touchActRightTap);

  prefs.putInt("act_rt_dbl", (int)touchActRightDbl);

  prefs.putInt("act_lt_hld", (int)touchActLeftHold);

  prefs.putInt("act_rt_hld", (int)touchActRightHold);

  if (micSlotLocked) {
    prefs.putInt("micslot", micSlot);
  }

  prefs.putString("hostname", mdnsHostname);
  prefs.putInt("face_tween", faceTweenMs);
  prefs.putInt("chat_hist", maxChatHistory);
  prefs.putString("ap_pass", String(apPasswordBuf));

  // STT Adaptive Endpointer Parameters
  prefs.putInt("stt_trail_ms", sttTrailingMs);
  prefs.putFloat("stt_noise_k", sttNoiseK);
  prefs.putFloat("stt_rel_k", sttRelK);
  prefs.putInt("stt_max_sil", sttMaxSilenceMs);

  prefs.end();
  configDirty = false;
  configDirtyAt = 0;
  Serial.println("[CONFIG] NVS flash write completed.");
}

void serviceConfigManager() {
  if (!configDirty)
    return;

  uint32_t now = millis();
  if (now - configDirtyAt < CONFIG_DEBOUNCE_MS)
    return;

  // Defer if audio or AI speech is outputting or processing to prevent SPI bus
  // latency clicks
  if (kiwiIsOutputting() || busyProcessing || isTonePlaying()) {
    return;
  }

  Serial.println("[CONFIG] Debounce timeout elapsed: committing dirty settings "
                 "to NVS flash...");
  saveConfigImmediate();
}

void clearConfig() {
  prefs.begin("myrobot", false);
  prefs.clear();
  prefs.end();
  configDirty = false;
  configDirtyAt = 0;
}
