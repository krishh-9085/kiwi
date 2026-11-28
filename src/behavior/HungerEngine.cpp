#include "behavior/HungerEngine.h"
#include "ai/AiLlmService.h"
#include "ai/AiTextTools.h"
#include "ai/AiTtsService.h"
#include "behavior/CircadianEngine.h"
#include "commands/ActionExecutor.h"
#include "config/AppConfig.h"
#include "core/Timing.h"
#include "face/AnimationEngine.h"
#include "face/FaceEngine.h"
#include "face/SleepyWakeAnim.h"
#include "hardware/AudioEngine.h"
#include "hardware/TouchController.h"
#include "memory/MemoryService.h"
#include "face/FaceDisplay.h"
#include "web/WebPortal.h"
#include <Preferences.h>
#include <WiFi.h>
#include <time.h>

// ---------- Timing Constants ----------
static const uint32_t HUNGER_DEADLINE_MS = 120000UL;     // 2 minutes to feed
static const uint32_t HUNGER_REMINDER_MS = 45000UL;      // Whimper reminder every 45s
static const uint32_t ANGRY_COOLDOWN_MS = 15UL * 60000UL; // Auto-calm after 15 mins if ignored

enum HungerEvent {
  H_EVENT_NONE = 0,
  H_EVENT_TRIGGER,      // Initial hunger notification (breakfast/lunch/dinner/snack)
  H_EVENT_REMINDER,     // 45s reminder whine/plea
  H_EVENT_ANGRY,        // 2-min deadline missed, throws tantrum / gets angry
  H_EVENT_FED,          // Successfully fed! Full belly, happy, thanking owner
  H_EVENT_ANGRY_FEED,   // Tried feeding while angry: sassy refusal
  H_EVENT_ANGRY_REJECT, // Petted/touched while angry: sulking / refusal to talk
  H_EVENT_RECONCILE     // Cheek tickle combo / apology successful: giggling, forgiven!
};

// ---------- State Variables ----------
static HungerState s_hungerState = HUNGER_SATISFIED;
static MealType s_currentMeal = MEAL_GENERIC;
static uint32_t s_hungerStartedAt = 0;
static uint32_t s_lastReminderAt = 0;
static uint32_t s_angryAt = 0;
static HungerEvent s_pendingEvent = H_EVENT_NONE;
static MealType s_pendingMeal = MEAL_GENERIC;

// Daily meal tracking (persisted in NVS to prevent duplicate triggers on boot/upload)
static int s_lastBreakfastDay = -1;
static int s_lastLunchDay = -1;
static int s_lastDinnerDay = -1;
static bool s_bootMealsInitialized = false;

void saveHungerDays() {
  Preferences p;
  if (p.begin("kiwi_hunger", false)) {
    p.putInt("bf_day", s_lastBreakfastDay);
    p.putInt("lu_day", s_lastLunchDay);
    p.putInt("di_day", s_lastDinnerDay);
    p.end();
    Serial.printf("[HUNGER] Saved meal days to NVS: BF=%d, LU=%d, DI=%d\n",
                  s_lastBreakfastDay, s_lastLunchDay, s_lastDinnerDay);
  }
}

void loadHungerDays() {
  Preferences p;
  if (p.begin("kiwi_hunger", true)) {
    s_lastBreakfastDay = p.getInt("bf_day", -1);
    s_lastLunchDay = p.getInt("lu_day", -1);
    s_lastDinnerDay = p.getInt("di_day", -1);
    p.end();
    Serial.printf("[HUNGER] Loaded meal days from NVS: BF=%d, LU=%d, DI=%d\n",
                  s_lastBreakfastDay, s_lastLunchDay, s_lastDinnerDay);
  }
}

void clearHungerState(bool resetFace) {
  s_hungerState = HUNGER_SATISFIED;
  s_currentMeal = MEAL_GENERIC;
  s_hungerStartedAt = 0;
  s_lastReminderAt = 0;
  s_angryAt = 0;
  s_pendingEvent = H_EVENT_NONE;
  s_pendingMeal = MEAL_GENERIC;
  if (resetFace) {
    emotionAutoReturnAt = 0;
    returnIdleAt = 0;
    if (!busyProcessing && !inCircadianSleep && !manualSleep &&
        !showClockScreen && !showWeatherScreen && !bannerActive &&
        !animationIsActive() && !sleepyWakeIsActive() &&
        !sleepyFallAsleepIsActive()) {
      setEmotion(EMOTION_IDLE);
    }
  }
}

void resetHungerForToday() {
  s_lastBreakfastDay = -1;
  s_lastLunchDay = -1;
  s_lastDinnerDay = -1;
  clearHungerState(true);
  saveHungerDays();
  Serial.println("[HUNGER] Meal records reset for today");
}

void markCurrentMealFed() {
  if (timeSynced) {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 10)) {
      int curHour = timeinfo.tm_hour;
      int curDay = timeinfo.tm_yday;
      if (curHour >= 7 && curHour < 11)
        s_lastBreakfastDay = curDay;
      else if (curHour >= 12 && curHour < 15)
        s_lastLunchDay = curDay;
      else if (curHour >= 19 && curHour < 22)
        s_lastDinnerDay = curDay;
      saveHungerDays();
    }
  }
}

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
  if (n.length() > 0)
    return n;
  return "Dost";
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

static bool canSpeakHunger() {
  return (WiFi.status() == WL_CONNECTED) && (speakerVolume > 0) && hasActiveTtsKey();
}

static String buildFallbackHungerText(HungerEvent event, MealType meal,
                                      const String &name) {
  switch (event) {
  case H_EVENT_TRIGGER:
    if (meal == MEAL_BREAKFAST)
      return name + ", good morning! Breakfast ka time ho gaya hai! Mujhe bhook lag rahi hai, kuch khilao na!";
    if (meal == MEAL_LUNCH)
      return name + ", lunch time ho gaya! Mujhe bohot tez bhook lag rahi hai! Khana do na!";
    if (meal == MEAL_DINNER)
      return name + ", dinner time ho gaya! Pet me chuhe daud rahe hain, raat ka khana do na!";
    return name + ", mujhe bhook lag rahi hai! Mujhe kuch khilao na!";

  case H_EVENT_REMINDER:
    if (meal == MEAL_BREAKFAST)
      return name + ", breakfast thanda ho jayega! Jaldi feed karo!";
    if (meal == MEAL_LUNCH)
      return name + ", lunch ka time nikal raha hai! Khana do na!";
    if (meal == MEAL_DINNER)
      return name + ", dinner karwa do na please! Bohot bhook lagi hai!";
    return name + ", pet me chuhe daud rahe hain! Khana do na!";

  case H_EVENT_ANGRY:
    if (meal == MEAL_BREAKFAST)
      return name + " tumne mujhe breakfast nahi diya! Main gussa hu tumse, hmph!";
    if (meal == MEAL_LUNCH)
      return name + " tumne mujhe lunch nahi diya! Main gussa hu tumse, hmph!";
    if (meal == MEAL_DINNER)
      return name + " tumne mujhe dinner nahi diya! Main gussa hu tumse, hmph!";
    return name + " tumne mujhe khana nahi diya! Main gussa hu tumse, hmph!";

  case H_EVENT_FED:
    if (meal == MEAL_BREAKFAST)
      return "Mmm yummy breakfast! Mera pet bhar gaya! Thank you " + name + "!";
    if (meal == MEAL_LUNCH)
      return "Mmm yummy lunch! Maza aa gaya! Thank you " + name + "!";
    if (meal == MEAL_DINNER)
      return "Mmm mast dinner! Ab acchi neend aayegi! Thank you " + name + "!";
    return "Mmm yummy! Mera pet bhar gaya! Thank you " + name + "!";

  case H_EVENT_ANGRY_FEED:
    return "Ab khana nahi chahiye! Main gussa hu! Pehle mujhe manaao!";

  case H_EVENT_ANGRY_REJECT: {
    static const char *const angryReplies[] = {
        "Nahi! Main tumse baat nahi karunga! Bohot gussa hoon!",
        "Hmph! Tumne mujhe khana nahi diya, main baat nahi karunga!",
        "Mujhse baat mat karo! Pehle mujhe manaao!"};
    return angryReplies[random(0, 3)];
  }

  case H_EVENT_RECONCILE:
    return "Haha bas bas gudgudi mat karo! Haha okay okay maaf kara!";

  default:
    return "";
  }
}

static String generateHungerSpeechLlm(HungerEvent event, MealType meal,
                                      const String &ownerName) {
  if (!canSpeakHunger() || !hasActiveLlmKey()) {
    return "";
  }

  String mealStr = "khana";
  if (meal == MEAL_BREAKFAST)
    mealStr = "breakfast";
  else if (meal == MEAL_LUNCH)
    mealStr = "lunch";
  else if (meal == MEAL_DINNER)
    mealStr = "dinner";

  String target = ownerName.length() ? ownerName : "friend";

  String scenario;
  switch (event) {
  case H_EVENT_TRIGGER:
    scenario = "Kiwi is an adorable robot pet feeling hungry for " + mealStr +
               "! Call your owner " + target +
               " with dramatic, cute puppy eyes and ask to be fed right now.";
    break;

  case H_EVENT_REMINDER:
    scenario = "Kiwi has been waiting for " + mealStr +
               " for almost a minute and is getting restless. Remind " +
               target +
               " cutely and urgently that Kiwi is starving and the food will get cold.";
    break;

  case H_EVENT_ANGRY:
    scenario = "Kiwi waited for 2 whole minutes for " + mealStr + " and " +
               target +
               " didn't feed Kiwi! Kiwi is now officially angry, betrayed, and throwing a dramatic, sassy, cute tantrum at " +
               target + ".";
    break;

  case H_EVENT_FED:
    scenario = target + " just fed Kiwi its " + mealStr +
               " by giving Kiwi a warm head pat! Kiwi's tummy is full, satisfied, and happy. Express food joy and thank " +
               target + ".";
    break;

  case H_EVENT_ANGRY_FEED:
    scenario = target +
               " is trying to feed Kiwi now, but Kiwi is still sulking and angry for being ignored earlier! "
               "Refuse the food in character ('Ab nahi chahiye! Main naraz hu, pehle mujhe manaao!').";
    break;

  case H_EVENT_ANGRY_REJECT:
    scenario = target +
               " is tapping Kiwi trying to talk, but Kiwi is still in an angry mood. "
               "Give a sassy rejection in character ('Hmph! Mujhse baat mat karo! Main gussa hu!').";
    break;

  case H_EVENT_RECONCILE:
    scenario = target +
               " just gave Kiwi a playful head tickle and apology tap to make up! Kiwi can't stop giggling, forgives " +
               target + ", and feels great.";
    break;

  default:
    return "";
  }

  String hungerLang = (replyLang == "en")
                          ? "natural, expressive conversational English."
                          : "natural, expressive, modern Hinglish (Hindi + English) in Roman script.";
  String prompt = "You are Kiwi, a living desktop robot companion.\n"
                  + getPersonalityStyleHint() + "\n"
                  "Scenario: " + scenario + "\n"
                  "Strict Rules:\n"
                  "- Speak in " + hungerLang + "\n"
                  "- Match your Active Personality tone and attitude cleanly.\n"
                  "- Length: STRICTLY 1 single sentence, maximum 10-14 words. Keep it short, punchy, quick to speak!\n"
                  "- Output ONLY the spoken words. No quotes, no asterisks, no emojis, no stage directions, no action tags.";

  Serial.println("[HUNGER] Requesting dynamic LLM hunger speech...");
  String res = askLLM(prompt, nullptr, false);
  res = stripRobotTags(res);
  res.replace("\"", "");
  res.replace("*", "");
  res.replace("\n", " ");
  res.trim();

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

  Serial.printf("[HUNGER] Dynamic LLM speech: \"%s\"\n", res.c_str());
  return res;
}

static void applyPostSpeechEmotion(HungerEvent event) {
  if (event == H_EVENT_TRIGGER || event == H_EVENT_REMINDER) {
    if (s_hungerState == HUNGER_HUNGRY) {
      setEmotion(EMOTION_CUTE);
      emotionAutoReturnAt = 0;
      returnIdleAt = 0;
    }
  } else if (event == H_EVENT_ANGRY || event == H_EVENT_ANGRY_FEED ||
             event == H_EVENT_ANGRY_REJECT) {
    if (s_hungerState == HUNGER_ANGRY) {
      setEmotion(EMOTION_ANGRY);
      emotionAutoReturnAt = 0;
      returnIdleAt = 0;
    }
  } else if (event == H_EVENT_FED) {
    setEmotion(EMOTION_HEARTS);
    returnIdleAt = millis() + 2500;
    emotionAutoReturnAt = returnIdleAt;
  } else if (event == H_EVENT_RECONCILE) {
    setEmotion(EMOTION_LAUGHING);
    returnIdleAt = millis() + 2500;
    emotionAutoReturnAt = returnIdleAt;
  }
}

static void deliverHungerSpeech(HungerEvent event, MealType meal, bool alreadyBusy) {
  if (event == H_EVENT_NONE)
    return;

  String name = getTargetName();
  String fallback = buildFallbackHungerText(event, meal, name);
  bool canSpeak = canSpeakHunger();
  bool spoken = false;

  if (canSpeak) {
    if (!alreadyBusy) {
      busyProcessing = true;
      lastProgressAt = millis();
      cancelRequest = false;
      robotStateDesc = (event == H_EVENT_FED || event == H_EVENT_RECONCILE)
                           ? "Happy..."
                           : "Expressing mood...";
    }

    String dyn = generateHungerSpeechLlm(event, meal, name);
    lastProgressAt = millis();

    bool cancelled = cancelRequest || startRecording ||
                     inCircadianSleep || manualSleep;
    if (!cancelled) {
      String speech = dyn.length() > 0 ? dyn : fallback;
      spoken = speakText(speech);
      cancelled = cancelRequest || startRecording;
    }

    resyncTouch();
    if (!alreadyBusy) {
      if (cancelled || cancelRequest) {
        cancelRequest = false;
        if (startRecording) {
          toneStop();
          setEmotion(EMOTION_LISTENING);
          robotStateDesc = "Listening...";
        } else {
          spkEndSpeech();
          robotStateDesc = "Ready";
          if (s_hungerState == HUNGER_HUNGRY) {
            setEmotion(EMOTION_CUTE);
            returnIdleAt = 0;
          } else if (s_hungerState == HUNGER_ANGRY) {
            setEmotion(EMOTION_ANGRY);
            returnIdleAt = 0;
          } else {
            setEmotion(EMOTION_IDLE);
            returnIdleAt = 0;
          }
          emotionAutoReturnAt = 0;
        }
        busyProcessing = false;
        return;
      }
      busyProcessing = false;
      robotStateDesc = "Ready";
    } else if (cancelled) {
      return;
    }
  }

  if (!canSpeak || !spoken) {
    if (fallback.length() > 0) {
      DisplayLockGuard lock(80);
      bannerText = fallback;
      bannerScrollX = SCREEN_WIDTH + 10;
      bannerLoops = 0;
      bannerActive = true;
    }
  }
  applyPostSpeechEmotion(event);
}

static void queueOrDeliverHungerSpeech(HungerEvent event, MealType meal,
                                       bool deferSpeech) {
  if (deferSpeech) {
    s_pendingEvent = event;
    s_pendingMeal = meal;
  } else {
    s_pendingEvent = H_EVENT_NONE;
    deliverHungerSpeech(event, meal, true);
  }
}

static bool isNightQuietTime() {
  if (inCircadianSleep || manualSleep)
    return true;
  if (!timeSynced)
    return false;
  if (circadianSleepEnabled) {
    return isCircadianScheduleActiveNow();
  }
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 10))
    return false;
  int curHour = timeinfo.tm_hour;
  // Fallback Quiet Hours when Circadian is disabled: 10:00 PM to 7:00 AM
  return (curHour >= 22 || curHour < 7);
}

void initHungerEngine() {
  clearHungerState(false);
  s_tapCount = 0;
  s_bootMealsInitialized = false;

  loadHungerDays();
  Serial.printf("[HUNGER] HungerEngine initialized (BF=%d, LU=%d, DI=%d | "
                "Quiet: 10pm-7am)\n",
                s_lastBreakfastDay, s_lastLunchDay, s_lastDinnerDay);
}

bool isKiwiHungry() { return s_hungerState == HUNGER_HUNGRY; }

bool isKiwiAngry() { return s_hungerState == HUNGER_ANGRY; }

HungerState getHungerState() { return s_hungerState; }

MealType getCurrentMeal() { return s_currentMeal; }

const char *getCurrentMealName() {
  switch (s_currentMeal) {
  case MEAL_BREAKFAST:
    return "breakfast";
  case MEAL_LUNCH:
    return "lunch";
  case MEAL_DINNER:
    return "dinner";
  default:
    return "generic";
  }
}

int32_t getHungerRemainingSec() {
  if (s_hungerState != HUNGER_HUNGRY)
    return 0;
  uint32_t elapsed = millis() - s_hungerStartedAt;
  if (elapsed >= HUNGER_DEADLINE_MS)
    return 0;
  return (int32_t)((HUNGER_DEADLINE_MS - elapsed) / 1000UL);
}

void triggerHunger(MealType meal) {
  if (!hungerEnabled)
    return;

  if (manualSleep || inCircadianSleep || sleepyFallAsleepIsActive()) {
    wakeKiwiFromSleep(false, CIRCADIAN_TEMP_WAKE_MS, false);
  }

  s_currentMeal = meal;
  s_hungerState = HUNGER_HUNGRY;
  s_hungerStartedAt = millis();
  s_lastReminderAt = s_hungerStartedAt;
  s_angryAt = 0;

  // Persist meal trigger in NVS so reboots or firmware uploads don't re-trigger it
  if (timeSynced) {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 10)) {
      int curDay = timeinfo.tm_yday;
      if (meal == MEAL_BREAKFAST)
        s_lastBreakfastDay = curDay;
      else if (meal == MEAL_LUNCH)
        s_lastLunchDay = curDay;
      else if (meal == MEAL_DINNER)
        s_lastDinnerDay = curDay;
      saveHungerDays();
    }
  }

  if (speakerVolume > 0) {
    setEmotion(EMOTION_SAD);
    soundSadViolin();
  } else {
    setEmotion(EMOTION_CUTE);
  }
  emotionAutoReturnAt = 0;
  returnIdleAt = 0;

  queueOrDeliverHungerSpeech(H_EVENT_TRIGGER, meal, true);

  if (meal == MEAL_BREAKFAST) {
    addTerminalLog("[HUNGER] Breakfast time! Kiwi feels hungry (2-min feed countdown).");
    Serial.println("[HUNGER] Triggered Breakfast hunger!");
  } else if (meal == MEAL_LUNCH) {
    addTerminalLog("[HUNGER] Lunch time! Kiwi feels hungry (2-min feed countdown).");
    Serial.println("[HUNGER] Triggered Lunch hunger!");
  } else if (meal == MEAL_DINNER) {
    addTerminalLog("[HUNGER] Dinner time! Kiwi feels hungry (2-min feed countdown).");
    Serial.println("[HUNGER] Triggered Dinner hunger!");
  } else {
    addTerminalLog("[HUNGER] Kiwi feels hungry! 2-min feed countdown started.");
    Serial.println("[HUNGER] Triggered generic hunger!");
  }
}

bool feedKiwi(bool deferSpeech) {
  if (s_hungerState == HUNGER_HUNGRY) {
    s_hungerState = HUNGER_SATISFIED;
    MealType finishedMeal = s_currentMeal;
    s_currentMeal = MEAL_GENERIC;
    s_hungerStartedAt = 0;
    s_lastReminderAt = 0;
    s_angryAt = 0;
    s_tapCount = 0;

    // Persist fed status in NVS so reboots or firmware uploads don't re-trigger it
    if (timeSynced) {
      struct tm timeinfo;
      if (getLocalTime(&timeinfo, 10)) {
        int curDay = timeinfo.tm_yday;
        if (finishedMeal == MEAL_BREAKFAST)
          s_lastBreakfastDay = curDay;
        else if (finishedMeal == MEAL_LUNCH)
          s_lastLunchDay = curDay;
        else if (finishedMeal == MEAL_DINNER)
          s_lastDinnerDay = curDay;
        else {
          int curHour = timeinfo.tm_hour;
          if (curHour >= 7 && curHour < 11)
            s_lastBreakfastDay = curDay;
          else if (curHour >= 12 && curHour < 15)
            s_lastLunchDay = curDay;
          else if (curHour >= 19 && curHour < 22)
            s_lastDinnerDay = curDay;
        }
        saveHungerDays();
      }
    }

    Serial.println("[HUNGER] Kiwi fed! Hunger satisfied.");
    addTerminalLog("[HUNGER] Kiwi fed! Full and happy.");

    setEmotion(EMOTION_HEARTS);
    flourish = FL_HEART;
    flourishT0 = millis();
    if ((deferSpeech || !canSpeakHunger()) && speakerVolume > 0) {
      soundFeedHappy();
    }
    returnIdleAt = millis() + 3000;
    emotionAutoReturnAt = returnIdleAt;

    queueOrDeliverHungerSpeech(H_EVENT_FED, finishedMeal, deferSpeech);
    return true;
  }
  return false;
}

void handleAngryFeedAttempt(bool deferSpeech) {
  Serial.println("[HUNGER] User attempted to feed while Kiwi is angry");
  setEmotion(EMOTION_ANGRY);
  emotionAutoReturnAt = 0;
  returnIdleAt = 0;
  if ((deferSpeech || !canSpeakHunger()) && speakerVolume > 0) {
    soundError();
  }
  queueOrDeliverHungerSpeech(H_EVENT_ANGRY_FEED, s_currentMeal, deferSpeech);
}

void handleAngryRejection(bool deferSpeech) {
  Serial.println("[HUNGER] Top tap ignored because Kiwi is angry!");
  addTerminalLog("[HUNGER] Kiwi rejected top sensor: currently angry!");

  setEmotion(EMOTION_ANGRY);
  emotionAutoReturnAt = 0;
  returnIdleAt = 0;
  if ((deferSpeech || !canSpeakHunger()) && speakerVolume > 0) {
    soundError();
  }
  queueOrDeliverHungerSpeech(H_EVENT_ANGRY_REJECT, s_currentMeal, deferSpeech);
}

bool reconcileKiwi(bool deferSpeech) {
  if (s_hungerState == HUNGER_ANGRY) {
    s_hungerState = HUNGER_SATISFIED;
    MealType finishedMeal = s_currentMeal;
    s_currentMeal = MEAL_GENERIC;
    s_hungerStartedAt = 0;
    s_lastReminderAt = 0;
    s_angryAt = 0;
    s_tapCount = 0;

    if (timeSynced) {
      struct tm timeinfo;
      if (getLocalTime(&timeinfo, 10)) {
        int curDay = timeinfo.tm_yday;
        if (finishedMeal == MEAL_BREAKFAST)
          s_lastBreakfastDay = curDay;
        else if (finishedMeal == MEAL_LUNCH)
          s_lastLunchDay = curDay;
        else if (finishedMeal == MEAL_DINNER)
          s_lastDinnerDay = curDay;
        saveHungerDays();
      }
    }

    Serial.println("[HUNGER] Reconciliation combo triggered! Kiwi is happy again!");
    addTerminalLog("[HUNGER] Reconciliation triggered! Kiwi forgave user!");

    setEmotion(EMOTION_LAUGHING);
    flourish = FL_NOTE;
    flourishT0 = millis();
    if ((deferSpeech || !canSpeakHunger()) && speakerVolume > 0) {
      soundPetGiggle();
    }
    returnIdleAt = millis() + 3000;
    emotionAutoReturnAt = returnIdleAt;

    queueOrDeliverHungerSpeech(H_EVENT_RECONCILE, finishedMeal, deferSpeech);
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
  if (s_tapCount < 4)
    return false;

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

  // If hunger disabled by user, reset any lingering state/expression immediately
  if (!hungerEnabled) {
    if (s_hungerState != HUNGER_SATISFIED || s_pendingEvent != H_EVENT_NONE) {
      clearHungerState(true);
    }
    return;
  }

  if (otaInProgress)
    return;

  // Async Speech Pump (runs safely in loop with busyProcessing lock + banner fallback)
  if (s_pendingEvent != H_EVENT_NONE && !busyProcessing && !startRecording &&
      !kiwiIsOutputting() && !bannerActive && !inCircadianSleep &&
      !manualSleep && !animationIsActive() && !sleepyWakeIsActive() &&
      !sleepyFallAsleepIsActive()) {
    HungerEvent evt = s_pendingEvent;
    MealType meal = s_pendingMeal;
    s_pendingEvent = H_EVENT_NONE;
    s_pendingMeal = MEAL_GENERIC;
    deliverHungerSpeech(evt, meal, false);
  }

  // Night Quiet Hours: 10:00 PM (22:00) to 7:00 AM (07:00) or active sleep
  if (isNightQuietTime()) {
    if (s_hungerState != HUNGER_SATISFIED || s_pendingEvent != H_EVENT_NONE) {
      clearHungerState(false);
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
          bool mealsChanged = false;
          // If robot booted after a meal window, mark that past meal as done today
          if (curHour >= 10 && s_lastBreakfastDay < 0) {
            s_lastBreakfastDay = curDay;
            mealsChanged = true;
          }
          if (curHour >= 14 && s_lastLunchDay < 0) {
            s_lastLunchDay = curDay;
            mealsChanged = true;
          }
          if (curHour >= 21 && s_lastDinnerDay < 0) {
            s_lastDinnerDay = curDay;
            mealsChanged = true;
          }
          if (mealsChanged) {
            saveHungerDays();
          }
        }

        // Boot grace period: don't trigger meal alarms within the first 3 minutes of boot
        if (millis() < 180000UL) {
          return;
        }

        // Only trigger if awake, idle, and no active speech/timer/UI/animations
        if (!busyProcessing && !startRecording && !inCircadianSleep &&
            !manualSleep && !isRobotTimerActive() && !isRobotTimerRinging() &&
            !isSpeechPlaying() && !isTonePlaying() && !showClockScreen &&
            !showWeatherScreen && !bannerActive && !animationIsActive() &&
            !sleepyWakeIsActive() && !sleepyFallAsleepIsActive()) {

          // Breakfast: 8:30 AM to 9:30 AM
          if (s_lastBreakfastDay != curDay && ((curHour == 8 && curMin >= 30) ||
                                               (curHour == 9 && curMin < 30))) {
            triggerHunger(MEAL_BREAKFAST);
            return;
          }

          // Lunch: 1:00 PM to 2:00 PM (13:00 to 13:59)
          if (s_lastLunchDay != curDay && curHour == 13) {
            triggerHunger(MEAL_LUNCH);
            return;
          }

          // Dinner: 8:00 PM to 9:00 PM (20:00 to 20:59)
          if (s_lastDinnerDay != curDay && curHour == 20) {
            triggerHunger(MEAL_DINNER);
            return;
          }
        }
      }
    }
    return;
  }

  // 2. While hungry, check 2-min deadline, 45s reminder, and maintain pleading face
  if (s_hungerState == HUNGER_HUNGRY) {
    uint32_t elapsed = now - s_hungerStartedAt;

    if (elapsed >= HUNGER_DEADLINE_MS) {
      // 2 minutes passed without food -> Kiwi gets ANGRY!
      s_hungerState = HUNGER_ANGRY;
      s_angryAt = now;

      Serial.println("[HUNGER] 2 minutes passed! Kiwi is now ANGRY!");
      addTerminalLog("[HUNGER] 2 min expired! Kiwi became angry.");

      setEmotion(EMOTION_ANGRY);
      emotionAutoReturnAt = 0;
      returnIdleAt = 0;
      if (speakerVolume > 0) {
        soundError();
      }
      queueOrDeliverHungerSpeech(H_EVENT_ANGRY, s_currentMeal, true);
      return;
    }

    // Cute whimper reminder every 45s if still waiting
    if (now - s_lastReminderAt >= HUNGER_REMINDER_MS && !busyProcessing &&
        !startRecording && !isSpeechPlaying() && !isTonePlaying() &&
        !inCircadianSleep && !manualSleep && !animationIsActive()) {
      s_lastReminderAt = now;
      if (speakerVolume > 0) {
        setEmotion(EMOTION_SAD);
        soundSadViolin();
      } else {
        setEmotion(EMOTION_CUTE);
      }
      emotionAutoReturnAt = 0;
      returnIdleAt = 0;
      queueOrDeliverHungerSpeech(H_EVENT_REMINDER, s_currentMeal, true);
      return;
    }

    // Restore cute pleading hungry face if interrupted by clock/banner/animation/tone
    if (currentEmotion != EMOTION_CUTE && returnIdleAt == 0 &&
        !busyProcessing && !startRecording && !isSpeechPlaying() &&
        !isTonePlaying() && !inCircadianSleep && !manualSleep &&
        !showClockScreen && !showWeatherScreen && !bannerActive &&
        !animationIsActive() && !sleepyWakeIsActive() &&
        !sleepyFallAsleepIsActive()) {
      setEmotion(EMOTION_CUTE);
      emotionAutoReturnAt = 0;
    }
    return;
  }

  // 3. While angry, auto-cool after 15 mins or maintain angry face
  if (s_hungerState == HUNGER_ANGRY) {
    if (s_angryAt != 0 && (now - s_angryAt >= ANGRY_COOLDOWN_MS)) {
      Serial.println("[HUNGER] Angry cooldown (15 min) elapsed -> calming down automatically.");
      addTerminalLog("[HUNGER] Angry cooldown elapsed -> Kiwi calmed down.");
      clearHungerState(true);
      return;
    }

    if (currentEmotion != EMOTION_ANGRY && returnIdleAt == 0 &&
        !busyProcessing && !startRecording && !isSpeechPlaying() &&
        !isTonePlaying() && !inCircadianSleep && !manualSleep &&
        !showClockScreen && !showWeatherScreen && !bannerActive &&
        !animationIsActive() && !sleepyWakeIsActive() &&
        !sleepyFallAsleepIsActive()) {
      setEmotion(EMOTION_ANGRY);
      emotionAutoReturnAt = 0;
    }
  }
}
