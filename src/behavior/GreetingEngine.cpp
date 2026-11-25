#include "behavior/GreetingEngine.h"
#include "behavior/ActionExecutor.h"
#include "config/AppConfig.h"
#include "face/FaceExpressions.h"
#include "hardware/AudioEngine.h"
#include "services/AiTtsService.h"
#include "services/AiLlmService.h"
#include "services/AiTextTools.h"
#include "services/WeatherService.h"
#include "services/MemoryService.h"
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
static int lastGreetDay = -1;

static inline int getGreetPeriod(int hour) {
  if (hour >= 5 && hour < 12)
    return 0; // 05:00 to 11:59 -> morning
  if (hour >= 12 && hour < 17)
    return 1; // 12:00 to 16:59 -> afternoon
  if (hour >= 17 && hour < 21)
    return 2; // 17:00 to 20:59 -> evening
  return 3;   // 21:00 to 04:59 -> night
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

  // If LLM returned multiple sentences, take only the first sentence for a punchy greeting
  int periodIdx = res.indexOf('.');
  int exclamIdx = res.indexOf('!');
  int qIdx = res.indexOf('?');
  int cut = -1;
  if (periodIdx > 0 && (cut == -1 || periodIdx < cut)) cut = periodIdx;
  if (exclamIdx > 0 && (cut == -1 || exclamIdx < cut)) cut = exclamIdx;
  if (qIdx > 0 && (cut == -1 || qIdx < cut)) cut = qIdx;
  if (cut >= 20 && cut < (int)res.length() - 1) {
    res = res.substring(0, cut + 1);
  }

  Serial.printf("[GREETING] Dynamic LLM greeting generated: \"%s\"\n", res.c_str());
  return res;
}

void checkGreetings() {
  if (!timeSynced)
    return;

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 10))
    return;

  int curHour = timeinfo.tm_hour;
  int curDay = timeinfo.tm_yday;
  int curPeriod = getGreetPeriod(curHour);

  // Do NOT greet at boot: on first valid time sync, record current period & day
  if (lastGreetPeriod == -1) {
    lastGreetPeriod = curPeriod;
    lastGreetDay = curDay;
    Serial.printf("[GREETINGS] Initialized at boot: period=%d (%s), day=%d. No "
                  "boot greeting.\n",
                  curPeriod, PERIOD_NAMES[curPeriod], curDay);
    return;
  }

  // Fire at most ONCE per period per day: only proceed if the period has
  // changed
  if (curPeriod == lastGreetPeriod)
    return;

  // Skip the greeting (and do not mark it as done) if conditions are not met;
  // retry on the next loop
  if (!greetingsEnabled)
    return;
  if (busyProcessing || isRobotTimerActive() || isRobotTimerRinging())
    return;
  if (inCircadianSleep || manualSleep)
    return;
  if (bannerActive || currentEmotion == EMOTION_SPEAKING || isTonePlaying())
    return;

  // Successfully triggering greeting: record period and day
  lastGreetPeriod = curPeriod;
  lastGreetDay = curDay;

  String owner = getOwnerName();
  owner.trim();
  String greeting = "";
  if (WiFi.status() == WL_CONNECTED && hasActiveLlmKey()) {
    greeting = generateLlmGreeting(curHour, timeinfo.tm_min, timeinfo.tm_wday, owner);
  }
  if (greeting.length() == 0) {
    greeting = buildPeriodicGreeting(curPeriod, owner);
  }

  Serial.printf("[GREETINGS] Firing %s greeting (hour=%02d:%02d, day=%d): %s\n",
                PERIOD_NAMES[curPeriod], curHour, timeinfo.tm_min, curDay,
                greeting.c_str());

  if (WiFi.status() == WL_CONNECTED) {
    setEmotion(EMOTION_HAPPY);
    speakText(greeting);
  } else {
    bannerText = greeting;
    bannerScrollX = SCREEN_WIDTH + 10;
    bannerLoops = 0;
    bannerActive = true;
    soundPetSparkle();
  }
}
