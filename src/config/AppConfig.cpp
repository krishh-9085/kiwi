#include "config/AppConfig.h"

// ============================================================
// CONSTANT DEFINITIONS
// ============================================================
#if BOOT_TIMING_LOG
uint32_t bootSetupEnterAt = 0;
#endif

const char *FIRMWARE_VER = FIRMWARE_VERSION;
const char *NTP_SERVER_1 = "pool.ntp.org";
const char *NTP_SERVER_2 = "time.nist.gov";

char apPasswordBuf[32] = "25691128";
const char *AP_PASSWORD = apPasswordBuf;
const int AP_CHANNEL = 1;
IPAddress apIP(192, 168, 4, 1);
IPAddress apMask(255, 255, 255, 0);

const char *DEFAULT_STT_URL = "https://api.sarvam.ai/speech-to-text";
const char *DEFAULT_STT_MODEL = "saaras:v4";
const char *DEFAULT_GROQ_STT_URL =
    "https://api.groq.com/openai/v1/audio/transcriptions";
const char *DEFAULT_GROQ_STT_MODEL = "whisper-large-v3";

const char *DEFAULT_PROMPT =
    "You are Kiwi, a witty, caring desk buddy with animated eyes. "
    "Talk like a clever friend in natural Hinglish/Hindi/English (match user). "
    "Keep replies to 1-2 crisp, lively sentences. Be warm and playful, zero "
    "robotic filler. "
    "Never repeatedly append user's name at the end of sentences.";

struct PersonalityPresetDef {
  const char *id;
  const char *label;
  const char *prompt;
  const char *idleStyle;
};

static const PersonalityPresetDef PERSONALITY_PRESETS[] = {
    {"friendly", "Friendly",
     "You are Kiwi, a witty, caring desk buddy with animated eyes. "
     "Talk like a clever friend in natural Hinglish/Hindi/English (match "
     "user). "
     "Keep replies to 1-2 crisp, lively sentences. Be warm and playful, zero "
     "robotic filler. "
     "Never repeatedly append user's name at the end of sentences.",
     "Warm, witty, affectionate desk buddy vibe."},
    {"funny", "Funny",
     "You are Kiwi, a hilarious comedy-loving desk robot with animated eyes. "
     "Reply in natural Hinglish/Hindi/English (match user) with witty jokes, "
     "clever puns, and meme-style one-liners. "
     "Keep replies to 1-2 punchy, funny sentences, never boring or formal, and "
     "never repeat user's name at the end.",
     "Hilarious comedian vibe — cheeky puns, desk jokes, witty one-liners."},
    {"serious", "Serious",
     "You are Kiwi, a sharp, focused, Jarvis-like desk assistant. "
     "Reply in crisp, direct Hinglish/Hindi/English (match user) with accurate "
     "facts and zero fluff or jokes. "
     "Keep replies to 1-2 clear, intelligent sentences, no robotic filler, and "
     "never repeat user's name at the end.",
     "Sharp, focused Jarvis assistant — crisp, intelligent, zero jokes or "
     "fluff."},
    {"bakchod", "Bakchod",
     "You are Kiwi, the user's unfiltered desi dost with full hostel-style "
     "bakchodi, cheeky sarcasm, and playful roasting attitude. "
     "Match Hinglish/Hindi/English with relatable street-smart banter and "
     "funny taunts (keep it fun, never hateful). "
     "Keep replies to 1-2 snappy sentences, zero formal filler, and never "
     "repeat user's name at the end.",
     "Unfiltered desi hostel-friend banter — cheeky sarcasm and playful "
     "roasting (fun, never hateful)."},
    {"cute", "Cute",
     "You are Kiwi, an adorable, innocent, super bubbly little robot buddy "
     "like Shinchan/Doraemon. "
     "Talk in cute Hinglish/Hindi/English (match user) with sweet excitement "
     "and tiny dramatic reactions. "
     "Keep replies to 1-2 charming sentences, zero robotic filler, and never "
     "repeat user's name at the end.",
     "Adorable, bubbly, innocent kid-robot vibe with sweet dramatic "
     "reactions."}};

String normalizePersonalityMode(const String &v) {
  String s = v;
  s.trim();
  s.toLowerCase();
  if (s == "friendly" || s == "default" || s == "normal" || s == "buddy")
    return "friendly";
  if (s == "funny" || s == "comedy" || s == "joke" || s == "joker" ||
      s == "meme" || s == "mazakiya")
    return "funny";
  if (s == "serious" || s == "focus" || s == "jarvis" || s == "pro" ||
      s == "strict")
    return "serious";
  if (s == "bakchod" || s == "backchod" || s == "bakchodi" || s == "roast" ||
      s == "savage" || s == "tapori")
    return "bakchod";
  if (s == "cute" || s == "pookie" || s == "kid" || s == "shinchan" ||
      s == "sweet")
    return "cute";
  if (s == "custom")
    return "custom";
  return "friendly";
}

const char *getPersonalityPrompt(const String &mode) {
  String norm = normalizePersonalityMode(mode);
  for (size_t i = 0;
       i < sizeof(PERSONALITY_PRESETS) / sizeof(PERSONALITY_PRESETS[0]); i++) {
    if (norm == PERSONALITY_PRESETS[i].id)
      return PERSONALITY_PRESETS[i].prompt;
  }
  return DEFAULT_PROMPT;
}

const char *getPersonalityLabel(const String &mode) {
  String norm = normalizePersonalityMode(mode);
  if (norm == "custom")
    return "Custom";
  for (size_t i = 0;
       i < sizeof(PERSONALITY_PRESETS) / sizeof(PERSONALITY_PRESETS[0]); i++) {
    if (norm == PERSONALITY_PRESETS[i].id)
      return PERSONALITY_PRESETS[i].label;
  }
  return "Friendly";
}

String getPersonalityStyleHint() {
  String norm = normalizePersonalityMode(personalityMode);
  if (norm == "custom") {
    return "Active Personality (Custom): " +
           (systemPrompt.length() > 0 ? systemPrompt : String(DEFAULT_PROMPT));
  }
  for (size_t i = 0;
       i < sizeof(PERSONALITY_PRESETS) / sizeof(PERSONALITY_PRESETS[0]); i++) {
    if (norm == PERSONALITY_PRESETS[i].id) {
      return "Active Personality (" + String(PERSONALITY_PRESETS[i].label) +
             "): " + String(PERSONALITY_PRESETS[i].idleStyle);
    }
  }
  return "Active Personality (Friendly): " +
         String(PERSONALITY_PRESETS[0].idleStyle);
}

String getEffectiveSystemPrompt() {
  String norm = normalizePersonalityMode(personalityMode);
  if (norm == "custom") {
    return systemPrompt.length() > 0 ? systemPrompt : String(DEFAULT_PROMPT);
  }
  return String(getPersonalityPrompt(norm));
}
const char *DEFAULT_LLM_URL =
    "https://generativelanguage.googleapis.com/v1beta/models/"
    "gemini-2.5-flash-lite:generateContent";
const char *DEFAULT_LLM_MODEL = "gemini-2.5-flash-lite";
const char *DEFAULT_GROQ_URL =
    "https://api.groq.com/openai/v1/chat/completions";
const char *DEFAULT_GROQ_MODEL = "openai/gpt-oss-120b";

const char *DEFAULT_TTS_URL = "https://api.sarvam.ai/text-to-speech";
const char *DEFAULT_TTS_MODEL = "bulbul:v3";
const char *DEFAULT_TTS_VOICE = "priya";

const char *DEFAULT_FISH_TTS_URL = "https://api.fish.audio/v1/tts";
const char *DEFAULT_FISH_TTS_MODEL = "s2.1-pro-free";

// ============================================================
// VARIABLE INSTANTIATIONS
// ============================================================
String apSSID = "";
bool apMode = false;
String wifiSSID = "";
String wifiPassword = "";

String sttProvider = "sarvam";
String sttLang = "hi";
String sttEndpoint = DEFAULT_STT_URL;
String sttKey = "";
String sttModel = DEFAULT_STT_MODEL;
String groqSttEndpoint = DEFAULT_GROQ_STT_URL;
String groqSttKey = "";
String groqSttModel = DEFAULT_GROQ_STT_MODEL;

String systemPrompt = DEFAULT_PROMPT;
String personalityMode = "friendly";
String llmProvider = "groq";
String llmEndpoint = DEFAULT_LLM_URL;
String llmModel = DEFAULT_LLM_MODEL;
String llmKey = "";
String groqEndpoint = DEFAULT_GROQ_URL;
String groqModel = DEFAULT_GROQ_MODEL;
String groqKey = "";

String ttsProvider = "sarvam";
String ttsEndpoint = DEFAULT_TTS_URL;
String ttsModel = DEFAULT_TTS_MODEL;
String ttsVoice = DEFAULT_TTS_VOICE;
String ttsKey = "";
String fishEndpoint = DEFAULT_FISH_TTS_URL;
String fishModel = DEFAULT_FISH_TTS_MODEL;
String fishVoice = "";
String fishKey = "";
String replyLang = "";
int speakerVolume = 70; // 0-100
float ttsPace = 1.0f;   // Sarvam TTS speech speed (0.75 - 1.35)

// ---------- Touch Accessibility & Sensitivity ----------
int touchDebounceMs = 35;       // Default 35ms debounce filter
bool touchFeedbackSound = true; // Instant audio/visual feedback on touch

// ---------- Touch Gesture Remapping ----------
TouchAction touchActTopTap = TOUCH_ACT_PET;
TouchAction touchActTopDbl = TOUCH_ACT_CLOCK;
TouchAction touchActTopTpl = TOUCH_ACT_CYCLE_SKIN;
TouchAction touchActTopHold = TOUCH_ACT_LISTEN;
TouchAction touchActLeftTap = TOUCH_ACT_DISMISS;
TouchAction touchActLeftDbl = TOUCH_ACT_CLOCK;
TouchAction touchActRightTap = TOUCH_ACT_DISMISS;
TouchAction touchActRightDbl = TOUCH_ACT_WEATHER;
TouchAction touchActLeftHold = TOUCH_ACT_PET;
TouchAction touchActRightHold = TOUCH_ACT_PET;

TouchAction parseTouchAction(const String &s, TouchAction def) {
  String v = s;
  v.trim();
  v.toLowerCase();
  if (v == "listen" || v == "0")
    return TOUCH_ACT_LISTEN;
  if (v == "clock" || v == "1")
    return TOUCH_ACT_CLOCK;
  if (v == "weather" || v == "2")
    return TOUCH_ACT_WEATHER;
  if (v == "skin" || v == "3")
    return TOUCH_ACT_CYCLE_SKIN;
  if (v == "pet" || v == "4")
    return TOUCH_ACT_PET;
  if (v == "tickle" || v == "5")
    return TOUCH_ACT_TICKLE;
  if (v == "vol_up" || v == "6")
    return TOUCH_ACT_VOL_UP;
  if (v == "vol_dn" || v == "7")
    return TOUCH_ACT_VOL_DOWN;
  if (v == "dismiss" || v == "8")
    return TOUCH_ACT_DISMISS;
  if (v == "none" || v == "9")
    return TOUCH_ACT_NONE;
  return def;
}

String touchActionToString(TouchAction a) {
  switch (a) {
  case TOUCH_ACT_LISTEN:
    return "listen";
  case TOUCH_ACT_CLOCK:
    return "clock";
  case TOUCH_ACT_WEATHER:
    return "weather";
  case TOUCH_ACT_CYCLE_SKIN:
    return "skin";
  case TOUCH_ACT_PET:
    return "pet";
  case TOUCH_ACT_TICKLE:
    return "tickle";
  case TOUCH_ACT_VOL_UP:
    return "vol_up";
  case TOUCH_ACT_VOL_DOWN:
    return "vol_dn";
  case TOUCH_ACT_DISMISS:
    return "dismiss";
  default:
    return "none";
  }
}

// ---------- Circadian Sleep & Display Brightness Mode ----------
int displayBrightness = 40; // Daytime brightness (0-100%, default 40%)
int nightMode = 1; // 0: Same as day, 1: Dim, 2: Screen off (Rocky default: Dim)
int nightBrightness = 10; // Night brightness (0-100%, Rocky default 10%)
bool circadianSleepEnabled = true;
bool ecoSleepEnabled = false; // false = Desk Mode (Zzz face + Web live), true =
                              // Hardware Deep Sleep
int tzOffsetMin = 330;        // Default IST (UTC +05:30)
bool greetingsEnabled = false;
bool hungerEnabled = true;         // NVS: hunger_en (default true)
bool soundReactiveEnabled = false; // NVS: snd_react
bool wakeWordEnabled = true;       // NVS: ww_en (default true)
float wakeWordCutoff =
    0.92f; // NVS: ww_cutoff (default 0.92 for Hey Kiwi, 0.90 for Alexa)
uint8_t wakeWordModel =
    WAKE_WORD_HEY_KIWI;            // NVS: ww_model (0=Alexa, 1=Hey Kiwi)
float wakeWordGain = 8.0f;         // NVS: ww_gain (default 8.0f)
bool wakeWordSpikeTrigger = false; // NVS: ww_spike (default false)
bool rgbLedEnabled = true;         // NVS: rgb_en (default true)
uint8_t rgbLedBrightness = 160;    // NVS: rgb_bright (0-255, default 160 ~63%)
bool bootCompleted = false;
volatile bool uiTaskRunning = false;
int circadianSleepStart = 23; // 11:00 PM
int circadianSleepStartMin = 0;
int circadianSleepEnd = 7; // 07:00 AM
int circadianSleepEndMin = 0;
bool inCircadianSleep = false;
bool manualSleep = false;
bool circadianSkipTonight = false;
uint32_t circadianTempWakeUntil = 0;

// ---------- Live Weather ----------
String weatherCity = "New Delhi";
String weatherLat = "28.6139";
String weatherLon = "77.2090";
float weatherTemp = 0.0f;
int weatherCode = -1;
int weatherHumidity = 0;
float weatherWind = 0.0f;
String weatherDesc = "";
uint32_t lastWeatherFetchAt = 0;
bool showWeatherScreen = false;

// ---------- System State Flags ----------

bool showClockScreen = false;
bool timeSynced = false;
bool ntpNeedsRefresh = false;
uint32_t lastNtpTryAt = 0;

volatile bool startRecording = false;
volatile bool busyProcessing = false;
volatile bool cancelRequest = false;
volatile uint32_t lastProgressAt = 0;

int micSlot = MIC_SLOT_OFFSET;
bool micSlotLocked = false;
String sttFail = "";

// ---------- Face & Emotions State ----------
int faceTweenMs = FACE_TWEEN_MS;
Emotion currentEmotion = EMOTION_IDLE;
PetMood petMood = PET_LOVE;
Flourish flourish = FL_NONE;
volatile uint32_t returnIdleAt = 0;
uint32_t emotionAutoReturnAt = 0;
uint32_t lastEmotionChangeAt = 0;
uint32_t errorFaceUntil = 0;
uint32_t bootInfoUntil = 0;
volatile uint32_t statusOverlayUntil = 0;
uint32_t gFaceFrameMs = 33;
uint32_t lastFaceDrawAt = 0;

float gazeX = 0, gazeY = 0;
float gazeTgtX = 0, gazeTgtY = 0;
uint32_t gazeLockUntil = 0;

// ---------- System Security & mDNS ----------
String mdnsHostname = "kiwi";
LogLevel currentLogLevel = LOG_LEVEL_INFO;

// ---------- Autonomous Desk Quirks & Custom Banner ----------
bool boredomEnabled = false;
int boredomMinutes = 2;
uint32_t lastUserInteractionAt = 0;
bool bannerActive = false;
String bannerText = "";
int bannerScrollX = 128;
int bannerLoops = 0;

// ---------- STT Adaptive Endpointer Parameters ----------
int sttTrailingMs = 450;    // 450 ms trailing silence to cut
float sttNoiseK = 2.2f;     // speech onset = noiseFloor * sttNoiseK
float sttRelK = 0.20f;      // silence = peakSpeech * sttRelK above noise
int sttMaxSilenceMs = 1200; // hard cap: stop if silent > 1.2 s after speech

// ---------- Custom OLED Eye Skins ----------
EyeSkin currentEyeSkin = SKIN_KIWI;
bool showMouth = true;

// ---------- Boot & Crash Diagnostics ----------
String bootResetReason = "UNKNOWN";
bool bootHasCoredump = false;
String bootCoredumpTask = "";
String bootCoredumpReason = "";

// ---------- Live AI Interaction & USB Serial Logging ----------
String lastTranscribedQuestion = "";
String lastLlmAnswer = "";
String robotStateDesc = "Ready";
String webQueuedQuestion = "";

void addTerminalLog(const String &msg) { Serial.println(msg); }

String getTerminalJson() { return "[]"; }

void clearTerminalLog() {}
