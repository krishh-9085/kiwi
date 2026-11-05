#include "behavior/GreetingEngine.h"
#include "config/AppConfig.h"
#include "behavior/ActionExecutor.h"
#include "hardware/AudioEngine.h"
#include "face/FaceExpressions.h"
#include "services/AiTtsService.h"
#include <WiFi.h>
#include <time.h>

struct GreetingEntry {
  const char *periodName;
  const char *text;
};

static const GreetingEntry GREETINGS[] = {
  { "morning",   "Good morning! Aaj ka din achha ho." },
  { "afternoon", "Good afternoon! Kaise ho?" },
  { "night",     "Good night! So jao, kal milte hain." }
};

static int lastGreetPeriod = -1;
static int lastGreetDay = -1;

static inline int getGreetPeriod(int hour) {
  if (hour >= 5 && hour < 12)
    return 0; // 05:00 to 11:59 -> morning
  if (hour >= 12 && hour < 17)
    return 1; // 12:00 to 16:59 -> afternoon
  return 2;   // 17:00 to 04:59 -> night
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
    Serial.printf("[GREETINGS] Initialized at boot: period=%d (%s), day=%d. No boot greeting.\n",
                  curPeriod, GREETINGS[curPeriod].periodName, curDay);
    return;
  }

  // Fire at most ONCE per period per day: only proceed if the period has changed
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

  Serial.printf("[GREETINGS] Firing %s greeting (hour=%02d:%02d, day=%d): %s\n",
                GREETINGS[curPeriod].periodName, curHour, timeinfo.tm_min, curDay,
                GREETINGS[curPeriod].text);

  if (WiFi.status() == WL_CONNECTED) {
    setEmotion(EMOTION_HAPPY);
    speakText(GREETINGS[curPeriod].text);
  } else {
    bannerText = GREETINGS[curPeriod].text;
    bannerScrollX = SCREEN_WIDTH + 10;
    bannerLoops = 0;
    bannerActive = true;
    soundPetSparkle();
  }
}

