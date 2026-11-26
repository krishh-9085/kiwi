#include "behavior/GreetingEngine.h"
#include "behavior/HungerEngine.h"
#include "commands/ActionExecutor.h"
#include "config/AppConfig.h"
#include "face/AnimationEngine.h"
#include "face/FaceEngine.h"
#include "face/SleepyWakeAnim.h"
#include "hardware/AudioEngine.h"
#include "hardware/TouchController.h"
#include "ai/AiTtsService.h"
#include "ai/AiLlmService.h"
#include "ai/AiTextTools.h"
#include "integrations/WeatherService.h"
#include "memory/MemoryService.h"
#include <WiFi.h>
#include <time.h>

static const char *const PERIOD_NAMES[] = {"morning", "afternoon", "evening",
                                           "night"};

static const char *const GREETINGS_MORNING[] = {
    "Good morning{n}! Aaj ka din achha ho.", "Good morning{n}! Chai pee li?",
    "Rise and shine{n}! Ready for the day?"};

static const char *const GREETINGS_AFTERNOON[] = {
    "Good afternoon{n}! Lunch ho gaya?", "Good afternoon{n}! Kaise ho?",
    "Hey{n}, dopahar ho gayi! Kaisa chal raha hai kaam?"};

static const char *const GREETINGS_EVENING[] = {
    "Good evening{n}! Shaam ho gayi, chai ya coffee?",
    "Good evening{n}! Din kaisa gaya?",
    "Evening{n}! Thoda break le lo, relax karo."};

static const char *const GREETINGS_NIGHT[] = {
    "Good night{n}! So jao, kal milte hain.",
    "Late night{n}! Sone ka time ho gaya hai.",
    "Hey night owl{n}! Ab so jao, good night!"};

static int lastGreetPeriod = -1;
static int lastGreetLogicalDay = -1;

static inline int getGreetPeriod(int hour) {
  if (hour >= 5 && hour < 12)
    return 0; // 05:00 to 11:59 -> morning
  if (hour >= 12 && hour < 17)
    return 1; // 12:00 to 16:59 -> afternoon
  if (hour >= 17 && hour < 21)
    return 2; // 17:00 to 20:59 -> evening
  return 3;   // 21:00 to 04:59 -> night
}

// Shifts clock back by 5h so 21:00..04:59 shares one continuous night key across midnight.
static int getLogicalGreetDayKey() {
  time_t shifted = time(nullptr) - 5L * 3600L;
  struct tm adj;
  localtime_r(&shifted, &adj);
  return adj.tm_year * 400 + adj.tm_yday;
}

static String buildPeriodicGreeting(int period, const String &ownerName) {
  const char *const *pool = nullptr;
  int count = 0;
  switch (period) {
  case 0:
    pool = GREETINGS_MORNING;
    count = sizeof(GREETINGS_MORNING) / sizeof(GREETINGS_MORNING[0]);
    break;
  case 1:
    pool = GREETINGS_AFTERNOON;
    count = sizeof(GREETINGS_AFTERNOON) / sizeof(GREETINGS_AFTERNOON[0]);
    break;
  case 2:
    pool = GREETINGS_EVENING;
    count = sizeof(GREETINGS_EVENING) / sizeof(GREETINGS_EVENING[0]);
    break;
  case 3:
  default:
    pool = GREETINGS_NIGHT;
    count = sizeof(GREETINGS_NIGHT) / sizeof(GREETINGS_NIGHT[0]);
    break;
  }
  int idx = random(count);
  String tpl = pool[idx];
  String nameTok = ownerName.length() > 0 ? (", " + ownerName) : "";
  tpl.replace("{n}", nameTok);
  return tpl;
}

static bool hasActiveLlmKey() {
  if (llmProvider == "groq") {
    return groqKey.length() > 0;
  }
  return llmKey.length() > 0;
}

static bool hasActiveTtsKey() {
  if (ttsProvider == "fish" || ttsProvider == "fishaudio") {
    return fishKey.length() > 0;
  }
  return (ttsKey.length() > 0 || sttKey.length() > 0 || llmKey.length() > 0);
}

String generateLlmGreeting(int hour, int minute, int wday, const String &ownerName) {
  if (WiFi.status() != WL_CONNECTED) {
    return "";
  }
  if (!hasActiveLlmKey()) {
    return "";
  }

  const char *DAYS[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
  String dayStr = (wday >= 0 && wday < 7) ? DAYS[wday] : "Today";

  const char *timeOfDay = "day";
  if (hour >= 5 && hour < 12) timeOfDay = "morning";
  else if (hour >= 12 && hour < 17) timeOfDay = "afternoon";
  else if (hour >= 17 && hour < 21) timeOfDay = "evening";
  else if (hour >= 21 || (hour >= 0 && hour < 5)) timeOfDay = "night";

  String target = ownerName.length() ? ownerName : "friend";

  static const char *const MOOD_SEEDS[] = {
    "playful & energetic wakeup",
    "cute sleepy yawn stretching into happiness",
    "witty & cheeky companion greeting",
    "affectionate robot excited to see its human",
    "curious & looking forward to hanging out",
    "dramatic little robot coming back to life",
    "cool friendly buddy vibe",
    "cheerful & enthusiastic wave"
  };
  const char *mood = MOOD_SEEDS[random(sizeof(MOOD_SEEDS) / sizeof(MOOD_SEEDS[0]))];
  uint32_t seed = random(1000, 9999);

  String prompt = "You are Kiwi, an adorable, witty, affectionate, living desktop AI robot pet companion.\n"
                  "You just woke up / turned on and saw your owner (" + target + ").\n"
                  "Mood inspiration for this greeting: " + String(mood) + " (Seed: " + String(seed) + ")\n"
                  "Time context (for greeting vibe only): " + String(timeOfDay) + " (" + dayStr + ")\n"
                  "Strict Rules:\n"
                  "- Speak in natural, warm, lively, modern conversational Hinglish (Hindi + English).\n"
                  "- Be creative, spontaneous, and fresh every time! NEVER repeat the same greeting phrasing.\n"
                  "- NEVER announce clock time, numbers, temperature, or weather forecasts! You are an adorable pet robot, NOT a news reporter, alarm clock, or weather bot.\n"
                  "- Give a cute, fresh 1-sentence wakeup greeting suited to the " + String(timeOfDay) + " vibe.\n"
                  "- Length: STRICTLY 1 single sentence, maximum 10-14 words. Keep it short, punchy, and quick to speak!\n"
                  "- Output ONLY the spoken words. No quotes, no asterisks, no emojis, no explanations.";

  Serial.println("[GREETING] Requesting dynamic LLM wakeup greeting...");
  String res = askLLM(prompt, nullptr, false); // false = do not pollute chat history
  res = stripRobotTags(res);
  res.replace("\"", "");
  res.replace("*", "");
  res.trim();

  // Keep a single punchy sentence (allow one short opener like "Hey!" before trimming).
  auto nextTerminator = [&](int from) -> int {
    int p = res.indexOf('.', from);
    int e = res.indexOf('!', from);
    int q = res.indexOf('?', from);
    int best = -1;
    if (p > 0 && (best == -1 || p < best)) best = p;
    if (e > 0 && (best == -1 || e < best)) best = e;
    if (q > 0 && (best == -1 || q < best)) best = q;
    return best;
  };
  int cut = nextTerminator(0);
  if (cut >= 0 && cut < 15) {
    int secondCut = nextTerminator(cut + 1);
    if (secondCut > 0) cut = secondCut;
  }
  if (cut >= 15 && cut < (int)res.length() - 1) {
    res = res.substring(0, cut + 1);
  }

  Serial.printf("[GREETING] Dynamic LLM greeting generated: \"%s\"\n", res.c_str());
  return res;
}

bool deliverGreeting(int hour, int minute, int wday, const String &ownerName,
                     const String &fallbackText, const char *logTag) {
  bool wifiOk = (WiFi.status() == WL_CONNECTED);
  bool canSpeak = wifiOk && (speakerVolume > 0) && hasActiveTtsKey();
  bool canLlm = canSpeak && hasActiveLlmKey();

  String greeting = "";
  bool spoken = false;
  bool cancelled = false;

  if (canSpeak) {
    busyProcessing = true;
    lastProgressAt = millis();
    cancelRequest = false;
    robotStateDesc = "Greeting...";
    setEmotion(EMOTION_HAPPY);
    flourish = FL_STAR;
    flourishT0 = millis();

    if (canLlm) {
      greeting = generateLlmGreeting(hour, minute, wday, ownerName);
      lastProgressAt = millis();
    }

    cancelled = cancelRequest || startRecording || inCircadianSleep || manualSleep;
    if (!cancelled) {
      if (greeting.length() == 0) {
        greeting = fallbackText;
      }
      Serial.printf("%s Firing spoken greeting: %s\n", logTag, greeting.c_str());
      spoken = speakText(greeting);
      cancelled = cancelRequest || startRecording;
    }

    resyncTouch();
    if (cancelled || cancelRequest) {
      cancelRequest = false;
      if (startRecording) {
        toneStop();
        setEmotion(EMOTION_LISTENING);
        robotStateDesc = "Listening...";
      } else {
        spkEndSpeech();
        setEmotion(EMOTION_IDLE);
        robotStateDesc = "Ready";
        returnIdleAt = 0;
      }
      busyProcessing = false;
      return false;
    }

    busyProcessing = false;
    robotStateDesc = "Ready";
    if (spoken) {
      setEmotion(EMOTION_HAPPY);
      returnIdleAt = millis() + 1200;
      emotionAutoReturnAt = returnIdleAt;
      return true;
    }
    Serial.printf("%s TTS failed -> falling back to OLED banner\n", logTag);
  }

  // Offline / Muted / Missing TTS key / TTS failure -> clean ASCII banner + sparkle fallback
  String visualText = fallbackText;
  visualText.trim();
  if (visualText.length() == 0) {
    visualText = greeting;
  }
  Serial.printf("%s Firing banner greeting: %s\n", logTag, visualText.c_str());
  setEmotion(EMOTION_HAPPY);
  flourish = FL_STAR;
  flourishT0 = millis();
  if (visualText.length() > 0) {
    bannerText = visualText;
    bannerScrollX = SCREEN_WIDTH + 10;
    bannerLoops = 0;
    bannerActive = true;
  } else {
    returnIdleAt = millis() + 1200;
    emotionAutoReturnAt = returnIdleAt;
  }
  if (speakerVolume > 0) {
    soundPetSparkle();
  }
  return true;
}

void checkGreetings() {
  if (!timeSynced)
    return;

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 10))
    return;

  int curHour = timeinfo.tm_hour;
  int curPeriod = getGreetPeriod(curHour);
  int curLogicalDay = getLogicalGreetDayKey();

  // Do NOT greet at boot: on first valid time sync, record current period & logical day
  if (lastGreetPeriod == -1) {
    lastGreetPeriod = curPeriod;
    lastGreetLogicalDay = curLogicalDay;
    Serial.printf("[GREETINGS] Initialized at boot: period=%d (%s), day=%d. No "
                  "boot greeting.\n",
                  curPeriod, PERIOD_NAMES[curPeriod], curLogicalDay);
    return;
  }

  // Fire at most ONCE per period per logical day
  if (curPeriod == lastGreetPeriod && curLogicalDay == lastGreetLogicalDay)
    return;

  // Skip (and retry later) if disabled, busy, sleeping, mid-animation, or displaying a screen
  if (!greetingsEnabled)
    return;
  if (busyProcessing || startRecording || isRobotTimerActive() || isRobotTimerRinging())
    return;
  if (inCircadianSleep || manualSleep || sleepyWakeIsActive() || sleepyFallAsleepIsActive())
    return;
  if (bannerActive || animationIsActive() || showClockScreen || showWeatherScreen ||
      isKiwiHungry() || isKiwiAngry())
    return;
  if (currentEmotion == EMOTION_SPEAKING || isSpeechPlaying() || isTonePlaying())
    return;

  // Record period & logical day before firing so cancelled/completed greetings don't re-trigger in the same slot
  lastGreetPeriod = curPeriod;
  lastGreetLogicalDay = curLogicalDay;

  String owner = getOwnerName();
  owner.trim();
  String fallback = buildPeriodicGreeting(curPeriod, owner);

  Serial.printf("[GREETINGS] Triggering %s greeting (hour=%02d:%02d, day=%d)\n",
                PERIOD_NAMES[curPeriod], curHour, timeinfo.tm_min, curLogicalDay);
  deliverGreeting(curHour, timeinfo.tm_min, timeinfo.tm_wday, owner, fallback, "[GREETINGS]");
}
