#pragma once
#include "Types.h"
#include <Arduino.h>

// ============================================================
// HARDWARE PIN MAP (ESP32-C3 Super Mini)
// ============================================================
#define PIN_TOUCH_LEFT 20
#define PIN_TOUCH_RIGHT 21
#define PIN_TOUCH_TOP 3
#define PIN_MIC_SD 4
#define PIN_I2S_BCLK 5
#define PIN_I2S_WS 6
#define PIN_AMP_DIN 7
#define PIN_OLED_SDA 8
#define PIN_OLED_SCL 9

// ---------- Audio Amplifier Limit ----------
#define SPEAKER_MAX_GAIN 2.0f  // Full-scale gain cap (8ohm 1.5W safe)

// ============================================================
// DISPLAY & AUDIO SPECIFICATIONS
// ============================================================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C

#define TZ_OFFSET_SEC (5 * 3600 + 30 * 60) // IST (UTC +5:30)
#define TZ_DST_SEC 0
extern const char *NTP_SERVER_1;
extern const char *NTP_SERVER_2;

#define SAMPLE_RATE 24000
#define RECORD_SECONDS 4
#define AUDIO_FRAMES 256
#define MIC_SLOT_OFFSET 0
#define MIC_GAIN 2.2f
#define UPLOAD_BATCH 1024
#define LLM_MAX_TOKENS 80
#define TTS_MAX_CHARS 360
#define NOISE_GATE_THRESHOLD 60

// ============================================================
// TOUCH TIMING CONSTANTS
// ============================================================
constexpr uint32_t TOUCH_DEBOUNCE = 30;
constexpr uint32_t TOUCH_HOLD_MS = 1500;
constexpr uint32_t TOUCH_TAP_MAX_MS = 1200;
constexpr uint32_t TOUCH_CLOCK_MS = 3000;
constexpr uint32_t TICKLE_CHEEK_WINDOW_MS = 1800;
constexpr uint32_t TICKLE_COOLDOWN_MS = 6000;
constexpr uint32_t DUAL_CHEEK_HOLD_MS = 600;
constexpr uint32_t CIRCADIAN_TEMP_WAKE_MS = 45000;
constexpr uint32_t SLEEP_HOLD_MS = 7000;

// ============================================================
// DEFAULT AI SERVICE ENDPOINTS & MODELS
// ============================================================
extern const char *AP_PASSWORD;
extern const int AP_CHANNEL;
extern IPAddress apIP;
extern IPAddress apMask;

extern const char *DEFAULT_STT_URL;
extern const char *DEFAULT_STT_MODEL;
extern const char *DEFAULT_GROQ_STT_URL;
extern const char *DEFAULT_GROQ_STT_MODEL;
extern const char *DEFAULT_OPENAI_STT_URL;
extern const char *DEFAULT_OPENAI_STT_MODEL;

extern const char *DEFAULT_PROMPT;
extern const char *DEFAULT_LLM_URL;
extern const char *DEFAULT_LLM_MODEL;
extern const char *DEFAULT_OPENAI_LLM_URL;
extern const char *DEFAULT_GROQ_URL;
extern const char *DEFAULT_GROQ_MODEL;

extern const char *DEFAULT_TTS_URL;
extern const char *DEFAULT_TTS_MODEL;
extern const char *DEFAULT_TTS_VOICE;

extern const char *DEFAULT_OPENAI_TTS_URL;
extern const char *DEFAULT_OPENAI_TTS_MODEL;
extern const char *DEFAULT_OPENAI_TTS_VOICE;

extern const char *DEFAULT_ELEVEN_URL;
extern const char *DEFAULT_ELEVEN_MODEL;
extern const char *DEFAULT_ELEVEN_VOICE;

// ============================================================
// GLOBAL CONFIGURATION VARIABLES (Shared Across Modules)
// ============================================================
extern String apSSID;
extern bool apMode;
extern String wifiSSID, wifiPassword;

extern String sttProvider;
extern String sttLang;
extern String sttEndpoint;
extern String sttKey;
extern String sttModel;
extern String groqSttEndpoint;
extern String groqSttKey;
extern String groqSttModel;
extern String openaiSttEndpoint;
extern String openaiSttKey;
extern String openaiSttModel;

extern String systemPrompt;
extern String llmProvider;
extern String llmEndpoint;
extern String llmModel;
extern String llmKey;
extern String groqEndpoint;
extern String groqModel;
extern String groqKey;

extern String ttsProvider;
extern String ttsEndpoint;
extern String ttsModel;
extern String ttsVoice;
extern String ttsKey;
extern String openaiTtsEndpoint;
extern String openaiTtsModel;
extern String openaiTtsVoice;
extern String openaiTtsKey;
extern String elevenEndpoint;
extern String elevenModel;
extern String elevenVoice;
extern String elevenKey;
extern String replyLang;
extern int speakerVolume;

// ---------- Circadian Sleep Mode ----------
extern bool circadianSleepEnabled;
extern int circadianSleepStart;
extern int circadianSleepEnd;
extern bool inCircadianSleep;
extern bool manualSleep;
extern uint32_t circadianTempWakeUntil;

// ---------- Live Weather ----------
extern String weatherCity;
extern String weatherLat;
extern String weatherLon;
extern float weatherTemp;
extern int weatherCode;
extern int weatherHumidity;
extern float weatherWind;
extern String weatherDesc;
extern uint32_t lastWeatherFetchAt;
extern bool showWeatherScreen;

// ---------- System State Flags ----------

extern bool showClockScreen;
extern bool timeSynced;
extern uint32_t lastNtpTryAt;

// Single-core ESP32-C3 safe; upgrade to std::atomic<bool> if dispatched across FreeRTOS tasks
extern volatile bool startRecording;
extern volatile bool busyProcessing;
extern volatile bool cancelRequest;

extern int micSlot;
extern bool micSlotLocked;
extern String sttFail;

// ---------- Face & Emotions State ----------
extern Emotion currentEmotion;
extern PetMood petMood;
extern Flourish flourish;
extern uint32_t returnIdleAt;
extern uint32_t emotionAutoReturnAt;
extern uint32_t errorFaceUntil;
extern uint32_t bootInfoUntil;
extern uint32_t gFaceFrameMs;
extern uint32_t lastFaceDrawAt;

extern float gazeX, gazeY;
extern float gazeTgtX, gazeTgtY;
extern uint32_t gazeLockUntil;

// ---------- Autonomous Desk Quirks & Custom Banner ----------
extern bool boredomEnabled;
extern int boredomMinutes;
void resetBoredomInterval();
extern uint32_t lastUserInteractionAt;
extern bool bannerActive;
extern String bannerText;
extern int bannerScrollX;
extern int bannerLoops;

// ---------- Custom OLED Eye Skins ----------
extern EyeSkin currentEyeSkin;

// ---------- Live AI Interaction & Web Serial Terminal ----------
extern String lastTranscribedQuestion;
extern String lastLlmAnswer;
extern String robotStateDesc;
extern String webQueuedQuestion;
void addTerminalLog(const String &msg);
String getTerminalJson();
void clearTerminalLog();
