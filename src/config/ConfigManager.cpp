#include "config/ConfigManager.h"
#include "ai/AiChatHistory.h"
#include "config/AppConfig.h"
#include "hardware/AudioEngine.h"
#include "hardware/RgbLed.h"
#include "web/WebPortal.h"
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
  if (prefs.isKey("persona")) {
    personalityMode = normalizePersonalityMode(
        sanitizeField(prefs.getString("persona", "friendly"), "friendly"));
  } else {
    personalityMode = (systemPrompt != DEFAULT_PROMPT) ? "custom" : "friendly";
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
    String expectedEp =
        "https://generativelanguage.googleapis.com/v1beta/models/" + llmModel +
        ":generateContent";
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

  ttsPace = constrain(prefs.getFloat("tts_pace", 1.0f), 0.75f, 1.35f);

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
  ecoSleepEnabled = prefs.getBool("eco_slp", false);
  tzOffsetMin = prefs.getInt("tz_min", 330);
  if (tzOffsetMin < -720 || tzOffsetMin > 840)
    tzOffsetMin = 330;
  greetingsEnabled = prefs.getBool("greet_en", false);
  hungerEnabled = prefs.getBool("hunger_en", true);
  soundReactiveEnabled = prefs.getBool("snd_react", false);
  wakeWordEnabled = prefs.getBool("ww_en", true);
  wakeWordModel = prefs.getUChar("ww_model", WAKE_WORD_HEY_KIWI);
  if (wakeWordModel > WAKE_WORD_HEY_KIWI)
    wakeWordModel = WAKE_WORD_HEY_KIWI;

  float defCutoff = (wakeWordModel == WAKE_WORD_HEY_KIWI) ? 0.92f : 0.90f;
  wakeWordCutoff = prefs.getFloat("ww_cutoff", defCutoff);
  if (!prefs.getBool("ww_kiwi_v1", false)) {
    // One-time activation of newly trained Hey Kiwi model & 0-FAPH cutoff (0.92)
    wakeWordModel = WAKE_WORD_HEY_KIWI;
    wakeWordCutoff = 0.92f;
    prefs.end();
    prefs.begin("myrobot", false);
    prefs.putUChar("ww_model", wakeWordModel);
    prefs.putFloat("ww_cutoff", wakeWordCutoff);
    prefs.putBool("ww_mig_v1", true);
    prefs.putBool("ww_kiwi_v1", true);
    prefs.end();
    prefs.begin("myrobot", true);
    Serial.println("[CONFIG] Activated Hey Kiwi wake-word model (cutoff=0.92)");
  }

  rgbLedEnabled = prefs.getBool("rgb_en", true);
  rgbLedBrightness = prefs.getUChar("rgb_bright", 160);
  rgbLedSetBrightness(rgbLedBrightness);

  wakeWordGain = prefs.getFloat("ww_gain", 8.0f);
  wakeWordSpikeTrigger = prefs.getBool("ww_spike", false);

  circadianSleepStart = constrain(prefs.getInt("circ_start", 23), 0, 23);
  circadianSleepStartMin = constrain(prefs.getInt("circ_smin", 0), 0, 59);

  circadianSleepEnd = constrain(prefs.getInt("circ_end", 7), 0, 23);
  circadianSleepEndMin = constrain(prefs.getInt("circ_emin", 0), 0, 59);

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

  touchActTopTap =
      (TouchAction)prefs.getInt("act_top_tap", (int)TOUCH_ACT_PET);
  if ((int)touchActTopTap < 0 || (int)touchActTopTap > (int)TOUCH_ACT_NONE) {
    touchActTopTap = TOUCH_ACT_PET;
  }
  touchActTopDbl =
      (TouchAction)prefs.getInt("act_top_dbl", (int)TOUCH_ACT_CLOCK);
  if ((int)touchActTopDbl < 0 || (int)touchActTopDbl > (int)TOUCH_ACT_NONE) {
    touchActTopDbl = TOUCH_ACT_CLOCK;
  }
  touchActTopTpl =
      (TouchAction)prefs.getInt("act_top_tpl", (int)TOUCH_ACT_CYCLE_SKIN);
  if ((int)touchActTopTpl < 0 || (int)touchActTopTpl > (int)TOUCH_ACT_NONE) {
    touchActTopTpl = TOUCH_ACT_CYCLE_SKIN;
  }
  touchActTopHold =
      (TouchAction)prefs.getInt("act_top_hld", (int)TOUCH_ACT_LISTEN);
  if ((int)touchActTopHold < 0 || (int)touchActTopHold > (int)TOUCH_ACT_NONE) {
    touchActTopHold = TOUCH_ACT_LISTEN;
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
    bool lockMic = prefs.isKey("micslot_lock")
                       ? prefs.getBool("micslot_lock", true)
                       : true;
    setMicSlot(savedMic, false, true);
    setMicSlotLocked(lockMic);
    Serial.printf("[MIC] Slot %d cached from NVS (%s)\n", micSlot,
                  lockMic ? "locked" : "unlocked");
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
  sttTrailingMs = prefs.getInt("stt_trail_ms", 450);
  if (sttTrailingMs < 100 || sttTrailingMs > 3000)
    sttTrailingMs = 450;
  sttNoiseK = prefs.getFloat("stt_noise_k", 2.2f);
  if (sttNoiseK < 1.1f || sttNoiseK > 10.0f)
    sttNoiseK = 2.2f;
  sttRelK = prefs.getFloat("stt_rel_k", 0.20f);
  if (sttRelK < 0.0f || sttRelK > 1.0f)
    sttRelK = 0.20f;
  sttMaxSilenceMs = prefs.getInt("stt_max_sil", 1200);
  if (sttMaxSilenceMs < 300 || sttMaxSilenceMs > 5000)
    sttMaxSilenceMs = 1200;

  // One-time migration to Single-Sensor Hold-to-Listen layout (preserves future UI edits)
  if (!prefs.getBool("top_hold_v2", false)) {
    touchActTopTap = TOUCH_ACT_PET;
    touchActTopDbl = TOUCH_ACT_CLOCK;
    touchActTopTpl = TOUCH_ACT_CYCLE_SKIN;
    touchActTopHold = TOUCH_ACT_LISTEN;
    prefs.end();
    prefs.begin("myrobot", false);
    prefs.putInt("act_top_tap", (int)touchActTopTap);
    prefs.putInt("act_top_dbl", (int)touchActTopDbl);
    prefs.putInt("act_top_tpl", (int)touchActTopTpl);
    prefs.putInt("act_top_hld", (int)touchActTopHold);
    prefs.putBool("top_hold_v2", true);
    Serial.println("[CONFIG] Migrated Top Head to Single-Sensor Hold-to-Listen profile (v2)");
  }

  prefs.end();
}

bool isConfigDirty() { return configDirty; }

void saveConfig() {
  configDirty = true;
  configDirtyAt = millis();
}

void savePersonalityNvsOnly() {
  prefs.begin("myrobot", false);
  prefs.putString("persona", personalityMode);
  prefs.end();
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
  prefs.putString("persona", personalityMode);

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

  prefs.putFloat("tts_pace", ttsPace);

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
  prefs.putBool("eco_slp", ecoSleepEnabled);
  prefs.putInt("tz_min", tzOffsetMin);
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
  prefs.putInt("circ_smin", circadianSleepStartMin);

  prefs.putInt("circ_end", circadianSleepEnd);
  prefs.putInt("circ_emin", circadianSleepEndMin);

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
  prefs.putInt("act_top_tpl", (int)touchActTopTpl);
  prefs.putInt("act_top_hld", (int)touchActTopHold);
  prefs.putInt("act_lt_tap", (int)touchActLeftTap);

  prefs.putInt("act_lt_dbl", (int)touchActLeftDbl);

  prefs.putInt("act_rt_tap", (int)touchActRightTap);

  prefs.putInt("act_rt_dbl", (int)touchActRightDbl);

  prefs.putInt("act_lt_hld", (int)touchActLeftHold);

  prefs.putInt("act_rt_hld", (int)touchActRightHold);

  if (micSlotLocked) {
    prefs.putInt("micslot", micSlot);
    prefs.putBool("micslot_lock", true);
  } else {
    prefs.remove("micslot");
    prefs.putBool("micslot_lock", false);
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
  if (kiwiIsOutputting() || busyProcessing || isTonePlaying() ||
      otaInProgress) {
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
