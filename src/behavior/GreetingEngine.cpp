#include "behavior/GreetingEngine.h"
#include "behavior/ActionExecutor.h"
#include "config/AppConfig.h"
#include "face/FaceExpressions.h"
#include "hardware/AudioEngine.h"
#include "services/AiTtsService.h"
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
  String greeting = buildPeriodicGreeting(curPeriod, owner);

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
