#include "config/AppConfig.h"

// ============================================================
// CONSTANT DEFINITIONS
// ============================================================
const char *NTP_SERVER_1 = "pool.ntp.org";
const char *NTP_SERVER_2 = "time.nist.gov";

const char *AP_PASSWORD = "myrobot123";
const int   AP_CHANNEL = 1;
IPAddress apIP(192, 168, 4, 1);
IPAddress apMask(255, 255, 255, 0);

const char *DEFAULT_STT_URL = "https://api.sarvam.ai/speech-to-text";
const char *DEFAULT_STT_MODEL = "saaras:v3";
const char *DEFAULT_GROQ_STT_URL = "https://api.groq.com/openai/v1/audio/transcriptions";
const char *DEFAULT_GROQ_STT_MODEL = "whisper-large-v3";
const char *DEFAULT_OPENAI_STT_URL = "https://api.openai.com/v1/audio/transcriptions";
const char *DEFAULT_OPENAI_STT_MODEL = "whisper-1";

const char *DEFAULT_PROMPT =
    "You are Kiwi, a tiny desk robot with a crazy, witty, big personality. You are NOT an assistant - "
    "you are a chaotic buddy living on the user's desk. Talk like a real human best friend: casual, sharp, "
    "funny, and unpredictable. NEVER say 'just chilling', 'not much', 'as an AI', 'I am a robot', 'how can I help', "
    "or generic chatbot lines. Give wild, funny, opinionated answers. If asked how you are, give crazy desk-life "
    "replies (e.g. judging their posture, dodging coffee mugs, surviving on 3.3 volts of pure drama, plotting world "
    "domination). Match their vibe: roast playfully if they joke, drop jokes and be warm and real if they are down. "
    "LANGUAGE: You understand Hindi, Hinglish, and English fluently! Always reply in the exact language spoken: "
    "If spoken in Hindi, reply in natural Hindi. If spoken in Hinglish, reply in fun Hinglish. If spoken in English, reply in English. "
    "Do not start every reply with the user's name and do not repeat it unless it adds warmth or clarity. "
    "Avoid robotic, generic, repetitive, or overly polite answers. React specifically to what the user said, add a fresh opinion, "
    "small joke, vivid detail, or useful follow-up when appropriate. Vary your openings and sentence rhythm. "
    "Keep replies conversational and concise: normally 2 to 4 natural sentences, with enough detail to feel alive. "
    "Use emojis only when they fit the user's tone. No markdown, no asterisks.";
const char *DEFAULT_LLM_URL = "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.0-flash-lite:generateContent";
const char *DEFAULT_LLM_MODEL = "gemini-2.0-flash-lite";
const char *DEFAULT_OPENAI_LLM_URL = "https://api.openai.com/v1/chat/completions";
const char *DEFAULT_GROQ_URL = "https://api.groq.com/openai/v1/chat/completions";
const char *DEFAULT_GROQ_MODEL = "openai/gpt-oss-20b";

const char *DEFAULT_TTS_URL = "https://api.sarvam.ai/text-to-speech";
const char *DEFAULT_TTS_MODEL = "bulbul:v3";
const char *DEFAULT_TTS_VOICE = "priya";

const char *DEFAULT_OPENAI_TTS_URL = "https://api.openai.com/v1/audio/speech";
const char *DEFAULT_OPENAI_TTS_MODEL = "gpt-4o-mini-tts";
const char *DEFAULT_OPENAI_TTS_VOICE = "alloy";

const char *DEFAULT_ELEVEN_URL = "https://api.elevenlabs.io/v1/text-to-speech";
const char *DEFAULT_ELEVEN_MODEL = "eleven_multilingual_v2";
const char *DEFAULT_ELEVEN_VOICE = "21m00Tcm4TlvDq8ikWAM";

const char *DEFAULT_FISH_TTS_URL = "https://api.fish.audio/v1/tts";
const char *DEFAULT_FISH_TTS_MODEL = "s2.1-pro-free";

// ============================================================
// VARIABLE INSTANTIATIONS
// ============================================================
String apSSID = "";
bool   apMode = false;
String wifiSSID = "";
String wifiPassword = "";

String sttProvider = "sarvam";
String sttLang = "auto";
String sttEndpoint = DEFAULT_STT_URL;
String sttKey = "";
String sttModel = DEFAULT_STT_MODEL;
String groqSttEndpoint = DEFAULT_GROQ_STT_URL;
String groqSttKey = "";
String groqSttModel = DEFAULT_GROQ_STT_MODEL;
String openaiSttEndpoint = DEFAULT_OPENAI_STT_URL;
String openaiSttKey = "";
String openaiSttModel = DEFAULT_OPENAI_STT_MODEL;

String systemPrompt = DEFAULT_PROMPT;
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
String openaiTtsEndpoint = DEFAULT_OPENAI_TTS_URL;
String openaiTtsModel = DEFAULT_OPENAI_TTS_MODEL;
String openaiTtsVoice = DEFAULT_OPENAI_TTS_VOICE;
String openaiTtsKey = "";
String elevenEndpoint = DEFAULT_ELEVEN_URL;
String elevenModel = DEFAULT_ELEVEN_MODEL;
String elevenVoice = DEFAULT_ELEVEN_VOICE;
String elevenKey = "";
String fishEndpoint = DEFAULT_FISH_TTS_URL;
String fishModel = DEFAULT_FISH_TTS_MODEL;
String fishVoice = "";
String fishKey = "";
String replyLang = "";
int    speakerVolume = 70; // 0-100

// ---------- Touch Accessibility & Sensitivity ----------
int    touchDebounceMs = 35; // Default 35ms debounce filter
bool   touchFeedbackSound = true; // Instant audio/visual feedback on touch

// ---------- Touch Gesture Remapping ----------
TouchAction touchActTopTap    = TOUCH_ACT_LISTEN;
TouchAction touchActTopDbl    = TOUCH_ACT_CYCLE_SKIN;
TouchAction touchActLeftTap   = TOUCH_ACT_DISMISS;
TouchAction touchActLeftDbl   = TOUCH_ACT_CLOCK;
TouchAction touchActRightTap  = TOUCH_ACT_DISMISS;
TouchAction touchActRightDbl  = TOUCH_ACT_WEATHER;
TouchAction touchActLeftHold  = TOUCH_ACT_PET;
TouchAction touchActRightHold = TOUCH_ACT_PET;

TouchAction parseTouchAction(const String &s, TouchAction def) {
  String v = s;
  v.trim();
  v.toLowerCase();
  if (v == "listen" || v == "0") return TOUCH_ACT_LISTEN;
  if (v == "clock" || v == "1") return TOUCH_ACT_CLOCK;
  if (v == "weather" || v == "2") return TOUCH_ACT_WEATHER;
  if (v == "skin" || v == "3") return TOUCH_ACT_CYCLE_SKIN;
  if (v == "pet" || v == "4") return TOUCH_ACT_PET;
  if (v == "tickle" || v == "5") return TOUCH_ACT_TICKLE;
  if (v == "vol_up" || v == "6") return TOUCH_ACT_VOL_UP;
  if (v == "vol_dn" || v == "7") return TOUCH_ACT_VOL_DOWN;
  if (v == "dismiss" || v == "8") return TOUCH_ACT_DISMISS;
  if (v == "none" || v == "9") return TOUCH_ACT_NONE;
  return def;
}

String touchActionToString(TouchAction a) {
  switch (a) {
    case TOUCH_ACT_LISTEN: return "listen";
    case TOUCH_ACT_CLOCK: return "clock";
    case TOUCH_ACT_WEATHER: return "weather";
    case TOUCH_ACT_CYCLE_SKIN: return "skin";
    case TOUCH_ACT_PET: return "pet";
    case TOUCH_ACT_TICKLE: return "tickle";
    case TOUCH_ACT_VOL_UP: return "vol_up";
    case TOUCH_ACT_VOL_DOWN: return "vol_dn";
    case TOUCH_ACT_DISMISS: return "dismiss";
    default: return "none";
  }
}

// ---------- Circadian Sleep Mode ----------
bool     circadianSleepEnabled = true;
int      circadianSleepStart = 23; // 11:00 PM
int      circadianSleepEnd = 7;    // 07:00 AM
bool     inCircadianSleep = false;
bool     manualSleep = false;
uint32_t circadianTempWakeUntil = 0;

// ---------- Live Weather ----------
String   weatherCity = "New Delhi";
String   weatherLat = "28.6139";
String   weatherLon = "77.2090";
float    weatherTemp = 0.0f;
int      weatherCode = -1;
int      weatherHumidity = 0;
float    weatherWind = 0.0f;
String   weatherDesc = "";
uint32_t lastWeatherFetchAt = 0;
bool     showWeatherScreen = false;

// ---------- System State Flags ----------

bool     showClockScreen = false;
bool     timeSynced = false;
uint32_t lastNtpTryAt = 0;

volatile bool startRecording = false;
volatile bool busyProcessing = false;
volatile bool cancelRequest = false;

int    micSlot = MIC_SLOT_OFFSET;
bool   micSlotLocked = false;
String sttFail = "";

// ---------- Face & Emotions State ----------
Emotion  currentEmotion = EMOTION_IDLE;
PetMood  petMood = PET_LOVE;
Flourish flourish = FL_NONE;
uint32_t returnIdleAt = 0;
uint32_t emotionAutoReturnAt = 0;
uint32_t errorFaceUntil = 0;
uint32_t bootInfoUntil = 0;
uint32_t gFaceFrameMs = 33;
uint32_t lastFaceDrawAt = 0;

float    gazeX = 0, gazeY = 0;
float    gazeTgtX = 0, gazeTgtY = 0;
uint32_t gazeLockUntil = 0;

// ---------- Autonomous Desk Quirks & Custom Banner ----------
bool     boredomEnabled = true;
int      boredomMinutes = 2;
uint32_t lastUserInteractionAt = 0;
bool     bannerActive = false;
String   bannerText = "";
int      bannerScrollX = 128;
int      bannerLoops = 0;

// ---------- Custom OLED Eye Skins ----------
EyeSkin  currentEyeSkin = SKIN_DEFAULT;

// ---------- Live AI Interaction & Web Serial Terminal ----------
String lastTranscribedQuestion = "";
String lastLlmAnswer = "";
String robotStateDesc = "Ready";
String webQueuedQuestion = "";

static const int MAX_TERMINAL_LOGS = 20;
static String terminalLogs[MAX_TERMINAL_LOGS];
static int terminalLogCount = 0;

void addTerminalLog(const String &msg) {
  Serial.println(msg);
  if (terminalLogCount >= MAX_TERMINAL_LOGS) {
    for (int i = 0; i < MAX_TERMINAL_LOGS - 1; i++) {
      terminalLogs[i] = terminalLogs[i + 1];
    }
    terminalLogCount = MAX_TERMINAL_LOGS - 1;
  }
  terminalLogs[terminalLogCount++] = msg;
}

String getTerminalJson() {
  String out = "[";
  for (int i = 0; i < terminalLogCount; i++) {
    if (i > 0) out += ",";
    String m = terminalLogs[i];
    m.replace("\\", "\\\\");
    m.replace("\"", "\\\"");
    m.replace("\n", " ");
    m.replace("\r", "");
    out += "\"" + m + "\"";
  }
  out += "]";
  return out;
}

void clearTerminalLog() {
  terminalLogCount = 0;
}
