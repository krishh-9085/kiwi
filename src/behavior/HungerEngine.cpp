#include "behavior/HungerEngine.h"
#include "animation/AnimationEngine.h"
#include "config/AppConfig.h"
#include "face/FaceEngine.h"
#include "face/SleepyWakeAnim.h"
#include "hardware/AudioEngine.h"
#include "services/AiTtsService.h"
#include "services/MemoryService.h"
#include <WiFi.h>
#include <time.h>

// ---------- Timing Constants ----------
static const uint32_t HUNGER_DEADLINE_MS = 120000UL;      // 2 minutes to feed
static const uint32_t HUNGER_REMINDER_MS = 45000UL;       // Whimper reminder every 45s

// ---------- State Variables ----------
static HungerState s_hungerState = HUNGER_SATISFIED;
static MealType s_currentMeal = MEAL_GENERIC;
static uint32_t s_hungerStartedAt = 0;
static uint32_t s_lastReminderAt = 0;
static uint32_t s_angryAt = 0;
static String s_pendingHungerSpeech = "";

// Daily meal tracking (prevents duplicate triggers on the same day)
static int s_lastBreakfastDay = -1;
static int s_lastLunchDay = -1;
static int s_lastDinnerDay = -1;
static bool s_bootMealsInitialized = false;

// Cheek tap history for combo (L-R-L-R)
struct TapEvent {
  uint8_t pin;
  uint32_t at;
};
static TapEvent s_tapHistory[12];
static uint8_t s_tapCount = 0;

static String getTargetName() {
  String n = getOwnerName();
  n.trim();
  if (n.length() > 0) return n;
  return "Krishna";
}

static bool isNightQuietTime() {
  if (inCircadianSleep || manualSleep) return true;
  if (!timeSynced) return false;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 10)) return false;
  int curHour = timeinfo.tm_hour;
  // Night Quiet Hours: 10:00 PM (22:00) to 7:00 AM (07:00)
  return (curHour >= 22 || curHour < 7);
}

void initHungerEngine() {
  s_hungerState = HUNGER_SATISFIED;
  s_currentMeal = MEAL_GENERIC;
  s_hungerStartedAt = 0;
  s_lastReminderAt = 0;
  s_angryAt = 0;
  s_tapCount = 0;
  s_pendingHungerSpeech = "";
  s_bootMealsInitialized = false;
  s_lastBreakfastDay = -1;
  s_lastLunchDay = -1;
  s_lastDinnerDay = -1;
  Serial.println("[HUNGER] HungerEngine initialized (3 Meals: Breakfast 8:30am, Lunch 1:00pm, Dinner 8:00pm | Quiet: 10pm-7am)");
}

bool isKiwiHungry() {
  return s_hungerState == HUNGER_HUNGRY;
}

bool isKiwiAngry() {
  return s_hungerState == HUNGER_ANGRY;
}

HungerState getHungerState() {
  return s_hungerState;
}

MealType getCurrentMeal() {
  return s_currentMeal;
}

const char *getCurrentMealName() {
  switch (s_currentMeal) {
    case MEAL_BREAKFAST: return "breakfast";
    case MEAL_LUNCH:     return "lunch";
    case MEAL_DINNER:    return "dinner";
    default:             return "generic";
  }
}

int32_t getHungerRemainingSec() {
  if (s_hungerState != HUNGER_HUNGRY) return 0;
  uint32_t elapsed = millis() - s_hungerStartedAt;
  if (elapsed >= HUNGER_DEADLINE_MS) return 0;
  return (int32_t)((HUNGER_DEADLINE_MS - elapsed) / 1000UL);
}

void triggerHunger(MealType meal) {
  s_currentMeal = meal;
  manualSleep = false;
  inCircadianSleep = false;

  s_hungerState = HUNGER_HUNGRY;
  s_hungerStartedAt = millis();
  s_lastReminderAt = s_hungerStartedAt;

  setEmotion(EMOTION_SAD);
  soundSadViolin();

  String name = getTargetName();
  if (meal == MEAL_BREAKFAST) {
    s_pendingHungerSpeech = name + ", good morning! Breakfast ka time ho gaya hai! Mujhe bhook lag rahi hai, kuch khilao na!";
    addTerminalLog("[HUNGER] Breakfast time! Kiwi feels hungry (2-min feed countdown).");
    Serial.println("[HUNGER] Triggered Breakfast hunger!");
  } else if (meal == MEAL_LUNCH) {
    s_pendingHungerSpeech = name + ", lunch time ho gaya! Mujhe bohot tez bhook lag rahi hai! Khana do na!";
    addTerminalLog("[HUNGER] Lunch time! Kiwi feels hungry (2-min feed countdown).");
    Serial.println("[HUNGER] Triggered Lunch hunger!");
  } else if (meal == MEAL_DINNER) {
    s_pendingHungerSpeech = name + ", dinner time ho gaya! Pet me chuhe daud rahe hain, raat ka khana do na!";
    addTerminalLog("[HUNGER] Dinner time! Kiwi feels hungry (2-min feed countdown).");
    Serial.println("[HUNGER] Triggered Dinner hunger!");
  } else {
    s_pendingHungerSpeech = name + ", mujhe bhook lag rahi hai! Mujhe kuch khilao na!";
    addTerminalLog("[HUNGER] Kiwi feels hungry! 2-min feed countdown started.");
    Serial.println("[HUNGER] Triggered generic hunger!");
  }

  // Keep pleading face
  setEmotion(EMOTION_CUTE);
  emotionAutoReturnAt = 0;
  returnIdleAt = 0;
}

bool feedKiwi() {
  if (s_hungerState == HUNGER_HUNGRY) {
    s_hungerState = HUNGER_SATISFIED;
    MealType finishedMeal = s_currentMeal;
    s_currentMeal = MEAL_GENERIC;
    s_tapCount = 0;

    Serial.println("[HUNGER] Kiwi fed! Hunger satisfied.");
    addTerminalLog("[HUNGER] Kiwi fed with dual-cheek touch! Full and happy.");

    setEmotion(EMOTION_HEARTS);
    soundCoin();
    soundPetSparkle();

    String name = getTargetName();
    if (finishedMeal == MEAL_BREAKFAST) {
      s_pendingHungerSpeech = "Mmm yummy breakfast! Mera pet bhar gaya! Thank you " + name + "!";
    } else if (finishedMeal == MEAL_LUNCH) {
      s_pendingHungerSpeech = "Mmm yummy lunch! Maza aa gaya! Thank you " + name + "!";
    } else if (finishedMeal == MEAL_DINNER) {
      s_pendingHungerSpeech = "Mmm mast dinner! Ab acchi neend aayegi! Thank you " + name + "!";
    } else {
      s_pendingHungerSpeech = "Mmm yummy! Mera pet bhar gaya! Thank you " + name + "!";
    }

    setEmotion(EMOTION_HAPPY);
    returnIdleAt = millis() + 3000;
    return true;
  }
  return false;
}

void handleAngryFeedAttempt() {
  Serial.println("[HUNGER] User attempted to feed while Kiwi is angry");
  setEmotion(EMOTION_ANGRY);
  soundError();
  s_pendingHungerSpeech = "Ab khana nahi chahiye! Main gussa hu! Pehle mujhe manaao!";
  setEmotion(EMOTION_ANGRY);
  emotionAutoReturnAt = 0;
  returnIdleAt = 0;
}

void handleAngryRejection() {
  Serial.println("[HUNGER] Top tap ignored because Kiwi is angry!");
  addTerminalLog("[HUNGER] Kiwi rejected top sensor: currently angry!");

  setEmotion(EMOTION_ANGRY);
  soundError();

  static const char *const angryReplies[] = {
      "Nahi! Main tumse baat nahi karunga! Bohot gussa hoon!",
      "Hmph! Tumne mujhe khana nahi diya, main baat nahi karunga!",
      "Mujhse baat mat karo! Pehle mujhe manaao!"
  };
  int idx = random(0, 3);
  s_pendingHungerSpeech = angryReplies[idx];

  setEmotion(EMOTION_ANGRY);
  emotionAutoReturnAt = 0;
  returnIdleAt = 0;
}

bool reconcileKiwi() {
  if (s_hungerState == HUNGER_ANGRY) {
    s_hungerState = HUNGER_SATISFIED;
    s_currentMeal = MEAL_GENERIC;
    s_tapCount = 0;

    Serial.println("[HUNGER] Reconciliation combo triggered! Kiwi is happy again!");
    addTerminalLog("[HUNGER] Left-Right tickle combo! Kiwi forgave user!");

    setEmotion(EMOTION_LAUGHING);
    soundPetGiggle();

    s_pendingHungerSpeech = "Haha bas bas gudgudi mat karo! Haha okay okay maaf kara!";

    setEmotion(EMOTION_HAPPY);
    returnIdleAt = millis() + 3000;
    return true;
  }
  return false;
}

void recordCheekTapForCombo(uint8_t pin, uint32_t now) {
  // Prune taps older than 4500ms
  uint8_t writeIdx = 0;
  for (uint8_t i = 0; i < s_tapCount; i++) {
    if (now - s_tapHistory[i].at <= 4500UL) {
      s_tapHistory[writeIdx++] = s_tapHistory[i];
    }
  }
  s_tapCount = writeIdx;

  // Add new tap if space available
  if (s_tapCount < 12) {
    s_tapHistory[s_tapCount].pin = pin;
    s_tapHistory[s_tapCount].at = now;
    s_tapCount++;
  }
}

bool checkCheekComboLRLR() {
  if (s_tapCount < 4) return false;

  // Check last 4 taps
  uint8_t p1 = s_tapHistory[s_tapCount - 4].pin;
  uint8_t p2 = s_tapHistory[s_tapCount - 3].pin;
  uint8_t p3 = s_tapHistory[s_tapCount - 2].pin;
  uint8_t p4 = s_tapHistory[s_tapCount - 1].pin;

  // Alternating: L-R-L-R or R-L-R-L
  bool lrlr = (p1 == PIN_TOUCH_LEFT && p2 == PIN_TOUCH_RIGHT &&
               p3 == PIN_TOUCH_LEFT && p4 == PIN_TOUCH_RIGHT);
  bool rlrl = (p1 == PIN_TOUCH_RIGHT && p2 == PIN_TOUCH_LEFT &&
               p3 == PIN_TOUCH_RIGHT && p4 == PIN_TOUCH_LEFT);

  if (lrlr || rlrl) {
    s_tapCount = 0; // Reset combo
    return true;
  }
  return false;
}

void checkHungerBehavior() {
  uint32_t now = millis();

  // Async Speech Pump (runs in loop, does not block WebServer)
  if (s_pendingHungerSpeech.length() > 0 && !busyProcessing && !kiwiIsOutputting()) {
    String spk = s_pendingHungerSpeech;
    s_pendingHungerSpeech = "";
    speakText(spk);
  }

  // Night Quiet Hours: 10:00 PM (22:00) to 7:00 AM (07:00)
  if (isNightQuietTime()) {
    // If Kiwi was still hungry or angry at bedtime, relax and sleep peacefully
    if (s_hungerState != HUNGER_SATISFIED) {
      s_hungerState = HUNGER_SATISFIED;
      s_pendingHungerSpeech = "";
      s_currentMeal = MEAL_GENERIC;
    }
    return;
  }

  // 1. Check if it's meal time (Breakfast, Lunch, Dinner)
  if (s_hungerState == HUNGER_SATISFIED) {
    if (timeSynced) {
      struct tm timeinfo;
      if (getLocalTime(&timeinfo, 10)) {
        int curHour = timeinfo.tm_hour;
        int curMin = timeinfo.tm_min;
        int curDay = timeinfo.tm_yday;

        if (!s_bootMealsInitialized) {
          s_bootMealsInitialized = true;
          // If robot booted after a meal window, don't trigger that past meal today
          if (curHour >= 10) s_lastBreakfastDay = curDay;
          if (curHour >= 14) s_lastLunchDay = curDay;
          if (curHour >= 21) s_lastDinnerDay = curDay;
        }

        // Idle checks: only trigger if awake, idle, and no active UI/animations
        if (!busyProcessing && !inCircadianSleep && !manualSleep &&
            !showClockScreen && !showWeatherScreen && !bannerActive &&
            !animationIsActive() && !sleepyWakeIsActive() && !sleepyFallAsleepIsActive()) {

          // Breakfast: 8:30 AM to 9:30 AM
          if (s_lastBreakfastDay != curDay && ((curHour == 8 && curMin >= 30) || (curHour == 9 && curMin < 30))) {
            s_lastBreakfastDay = curDay;
            triggerHunger(MEAL_BREAKFAST);
            return;
          }

          // Lunch: 1:00 PM to 2:00 PM (13:00 to 13:59)
          if (s_lastLunchDay != curDay && curHour == 13) {
            s_lastLunchDay = curDay;
            triggerHunger(MEAL_LUNCH);
            return;
          }

          // Dinner: 8:00 PM to 9:00 PM (20:00 to 20:59)
          if (s_lastDinnerDay != curDay && curHour == 20) {
            s_lastDinnerDay = curDay;
            triggerHunger(MEAL_DINNER);
            return;
          }
        }
      }
    }
    return;
  }

  // 2. While hungry, check if 2-minute deadline elapsed
  if (s_hungerState == HUNGER_HUNGRY) {
    uint32_t elapsed = now - s_hungerStartedAt;

    if (elapsed >= HUNGER_DEADLINE_MS) {
      // 2 minutes passed without food -> Kiwi gets ANGRY!
      s_hungerState = HUNGER_ANGRY;
      s_angryAt = now;

      Serial.println("[HUNGER] 2 minutes passed! Kiwi is now ANGRY!");
      addTerminalLog("[HUNGER] 2 min expired! Kiwi became angry.");

      setEmotion(EMOTION_ANGRY);
      soundError();

      String name = getTargetName();
      if (s_currentMeal == MEAL_BREAKFAST) {
        s_pendingHungerSpeech = name + " tumne mujhe breakfast nahi diya! Main gussa hu tumse, hmph!";
      } else if (s_currentMeal == MEAL_LUNCH) {
        s_pendingHungerSpeech = name + " tumne mujhe lunch nahi diya! Main gussa hu tumse, hmph!";
      } else if (s_currentMeal == MEAL_DINNER) {
        s_pendingHungerSpeech = name + " tumne mujhe dinner nahi diya! Main gussa hu tumse, hmph!";
      } else {
        s_pendingHungerSpeech = name + " tumne mujhe khana nahi diya! Main gussa hu tumse, hmph!";
      }

      setEmotion(EMOTION_ANGRY);
      emotionAutoReturnAt = 0;
      returnIdleAt = 0;
      return;
    }

    // Cute whimper reminder every 45s if still waiting
    if (now - s_lastReminderAt >= HUNGER_REMINDER_MS && !busyProcessing &&
        !inCircadianSleep && !manualSleep) {
      s_lastReminderAt = now;
      setEmotion(EMOTION_SAD);
      soundSadViolin();

      String name = getTargetName();
      if (s_currentMeal == MEAL_BREAKFAST) {
        s_pendingHungerSpeech = name + ", breakfast thanda ho jayega! Jaldi feed karo!";
      } else if (s_currentMeal == MEAL_LUNCH) {
        s_pendingHungerSpeech = name + ", lunch ka time nikal raha hai! Khana do na!";
      } else if (s_currentMeal == MEAL_DINNER) {
        s_pendingHungerSpeech = name + ", dinner karwa do na please! Bohot bhook lagi hai!";
      } else {
        s_pendingHungerSpeech = name + ", pet me chuhe daud rahe hain! Khana do na!";
      }

      setEmotion(EMOTION_CUTE);
      emotionAutoReturnAt = 0;
      returnIdleAt = 0;
    }
    return;
  }

  // 3. While angry, maintain angry face if no active animation
  if (s_hungerState == HUNGER_ANGRY) {
    if (currentEmotion != EMOTION_ANGRY && !busyProcessing &&
        !inCircadianSleep && !manualSleep && !showClockScreen &&
        !showWeatherScreen && !bannerActive && !animationIsActive()) {
      setEmotion(EMOTION_ANGRY);
      emotionAutoReturnAt = 0;
      returnIdleAt = 0;
    }
  }
}
