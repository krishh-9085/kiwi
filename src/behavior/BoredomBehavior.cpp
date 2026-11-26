#include "behavior/BoredomBehavior.h"
#include "ai/AiChatHistory.h"
#include "ai/AiLlmService.h"
#include "ai/AiService.h"
#include "ai/AiTextTools.h"
#include "behavior/HungerEngine.h"
#include "behavior/InteractionManager.h"
#include "commands/ActionExecutor.h"
#include "config/AppConfig.h"
#include "face/AnimationEngine.h"
#include "face/FaceEngine.h"
#include "hardware/AudioEngine.h"
#include "memory/MemoryService.h"
#include <WiFi.h>
#include <time.h>

// ============================================================
// AUTONOMOUS DESK COMPANION: AI-POWERED DYNAMIC IDLE BANTER
// ============================================================

static constexpr int MAX_RECENT_IDLE_THOUGHTS = 4;
static constexpr uint32_t MAX_BACKOFF_INTERVAL_MS = 30UL * 60000UL;

static uint32_t nextBoredomInterval = 120000;
static uint32_t s_lastBoredomTriggerAt = 0;
static uint32_t s_lastSeenUserInteractionAt = 0;
static uint8_t s_unansweredIdleCount = 0;

static String s_recentIdleThoughts[MAX_RECENT_IDLE_THOUGHTS];
static int s_recentIdleWrite = 0;
static int s_recentIdleCount = 0;

static bool s_pendingGiggleHappy = false;
static uint32_t s_giggleHappyAt = 0;

static uint32_t computeBoredomIntervalMs(uint8_t unansweredCount) {
  uint32_t baseMs = (uint32_t)boredomMinutes * 60000UL;
  // Gently stretch intervals when nobody responds so Kiwi avoids repeating
  // itself or wasting API calls in an empty room (1x -> 1.5x -> 2x -> 3x).
  uint32_t scaledMs = baseMs;
  if (unansweredCount == 1) {
    scaledMs = baseMs + (baseMs / 2);
  } else if (unansweredCount == 2) {
    scaledMs = baseMs * 2UL;
  } else if (unansweredCount >= 3) {
    scaledMs = baseMs * 3UL;
  }
  if (scaledMs > MAX_BACKOFF_INTERVAL_MS) {
    scaledMs = MAX_BACKOFF_INTERVAL_MS;
  }
  uint32_t jitter = scaledMs / 5;
  if (jitter < 5000) {
    jitter = 5000;
  }
  uint32_t lo = (scaledMs > jitter / 2) ? (scaledMs - jitter / 2) : scaledMs;
  uint32_t computed = lo + (uint32_t)random(0, (long)jitter);
  if (computed > MAX_BACKOFF_INTERVAL_MS) {
    computed = MAX_BACKOFF_INTERVAL_MS;
  }
  return computed;
}

static void recordRecentIdleThought(const String &raw) {
  String clean = stripRobotTags(raw);
  clean.trim();
  if (clean.length() == 0) {
    return;
  }
  s_recentIdleThoughts[s_recentIdleWrite] = clean;
  s_recentIdleWrite = (s_recentIdleWrite + 1) % MAX_RECENT_IDLE_THOUGHTS;
  if (s_recentIdleCount < MAX_RECENT_IDLE_THOUGHTS) {
    s_recentIdleCount++;
  }
}

void resetBoredomInterval() {
  s_unansweredIdleCount = 0;
  nextBoredomInterval = computeBoredomIntervalMs(0);
  Serial.printf("[BOREDOM] Next idle remark interval set to ~%.1f mins (%lu ms)\n",
                (float)nextBoredomInterval / 60000.0f,
                (unsigned long)nextBoredomInterval);
}

// ------------------------------------------------------------
// Physical Quirks (Offline fallback, muted mode, & companion life)
// ------------------------------------------------------------
static void executePhysicalQuirk(bool withSound) {
  int quirk = random(0, 4);
  uint32_t now = millis();
  if (quirk == 0) {
    setEmotion(EMOTION_BORED);
    if (withSound) soundPetSleepy();
    returnIdleAt = now + 4000;
  } else if (quirk == 1) {
    setEmotion(EMOTION_SINGING);
    if (withSound) soundPetPlayful();
    flourish = FL_NOTE;
    flourishT0 = now;
    returnIdleAt = now + 3500;
  } else if (quirk == 2) {
    setEmotion(EMOTION_CUTE);
    if (withSound) soundPetSparkle();
    flourish = FL_SPARKLE;
    flourishT0 = now;
    returnIdleAt = now + 2500;
  } else {
    setEmotion(EMOTION_LAUGHING);
    if (withSound) soundPetGiggle();
    s_pendingGiggleHappy = true;
    s_giggleHappyAt = now + 700;
    returnIdleAt = now + 3200;
    emotionAutoReturnAt = returnIdleAt;
  }
}

// ------------------------------------------------------------
// Build Dynamic LLM Prompt with Time, Day, Weather, Memory & Anti-Repetition
// ------------------------------------------------------------
static String buildDynamicIdlePrompt() {
  struct tm timeinfo;
  bool gotTime = timeSynced && getLocalTime(&timeinfo, 10);
  String timeLine;
  if (gotTime) {
    int curHour = timeinfo.tm_hour;
    int curWday = timeinfo.tm_wday;

    String timePhase;
    if (curHour >= 5 && curHour < 12) {
      timePhase = "Morning (fresh start, chai/coffee mood)";
    } else if (curHour >= 12 && curHour < 16) {
      timePhase = "Afternoon (lunch / midday focus)";
    } else if (curHour >= 16 && curHour < 20) {
      timePhase = "Evening (winding down / evening break)";
    } else if (curHour >= 20 && curHour < 24) {
      timePhase = "Night (dinner / late evening desk work)";
    } else {
      timePhase = "Late Midnight (quiet night-owl hours, rest reminder)";
    }

    const char *days[] = {
        "Sunday", "Monday", "Tuesday", "Wednesday",
        "Thursday", "Friday", "Saturday"};
    String dayDesc = (curWday >= 0 && curWday < 7) ? days[curWday] : "Today";
    timeLine = "Time: " + timePhase + " | Day: " + dayDesc + "\n";
  } else {
    timeLine = "Time: unavailable\n";
  }

  String weatherInfo = "";
  bool hasWeather = (weatherDesc.length() > 0);
  if (hasWeather) {
    weatherInfo = weatherDesc + ", " + String((int)weatherTemp) + "C";
    if (weatherCity.length() > 0) {
      weatherInfo += " in " + weatherCity;
    }
  }

  String owner = getOwnerName();
  owner.trim();
  String exName = (owner.length() > 0) ? owner : "Dost";
  String friends = getFriendsFamily();
  friends.trim();

  bool hasMemories = (kiwiMemory.memoryCount > 0);

  // Rotating Theme Seed (Grounded desk topics + weather & personal memory)
  static const char *const THEMES[] = {
      "Topic: Quick posture, eyes-rest, or stretch check-in on the desk.",
      "Topic: Playful focus/work/study teasing or cheering the owner on.",
      "Topic: Hydration, water, or chai/coffee break reminder.",
      "Topic: Natural remark matched to the current time of day or weekday/weekend vibe.",
      "Topic: Playfully breaking the room silence and asking what the owner is up to.",
      "Topic: Cute desk pet moment (asking for a gentle head tap or flashing a smile).",
      "Topic: Casual weather-aware desk comment tied to outside conditions.",
      "Topic: Warm personal check-in referencing something you remember about the owner."
  };

  const int numThemes = (int)(sizeof(THEMES) / sizeof(THEMES[0]));
  static int lastThemeIdx = -1;
  int themeIdx = random(0, numThemes);
  for (int attempt = 0; attempt < numThemes; attempt++) {
    bool allowed = true;
    if (themeIdx == 3 && !gotTime) allowed = false;
    if (themeIdx == 6 && !hasWeather) allowed = false;
    if (themeIdx == 7 && !hasMemories) allowed = false;
    if (allowed && themeIdx != lastThemeIdx) {
      break;
    }
    themeIdx = (themeIdx + 1) % numThemes;
  }
  lastThemeIdx = themeIdx;

  String langRule;
  if (replyLang == "en") {
    langRule = "natural conversational English";
  } else if (replyLang == "hi") {
    langRule = "natural conversational Hindi or Hinglish";
  } else {
    langRule = "natural conversational Hinglish (light Hindi + everyday English)";
  }

  String prompt;
  prompt.reserve(1100);
  prompt = "[AUTONOMOUS DESK BUDDY BANTER]\n";
  prompt += "You are Kiwi, a witty, warm robotic desk companion sitting on your owner's desk.\n";
  if (owner.length() > 0) {
    prompt += "Owner Name: " + owner + "\n";
  }
  if (friends.length() > 0) {
    prompt += "Owner's Friends/Family: " + friends + "\n";
  }
  if (hasMemories && themeIdx == 7) {
    int mIdx = random(0, kiwiMemory.memoryCount);
    prompt += "Remembered Fact: " + kiwiMemory.memories[mIdx] + "\n";
  }
  prompt += timeLine;
  if (hasWeather) {
    prompt += "Weather: " + weatherInfo + "\n";
  }
  prompt += String(THEMES[themeIdx]) + "\n";

  if (s_recentIdleCount > 0) {
    prompt += "\nDO NOT REPEAT OR PARAPHRASE THESE RECENT LINES:\n";
    for (int i = 0; i < s_recentIdleCount; i++) {
      int idx = (s_recentIdleWrite - 1 - i + MAX_RECENT_IDLE_THOUGHTS) %
                MAX_RECENT_IDLE_THOUGHTS;
      prompt += "- " + s_recentIdleThoughts[idx] + "\n";
    }
  }

  prompt += "\nSTYLE REFERENCE (match tone only, invent a fresh line):\n";
  if (replyLang == "en") {
    prompt += "- 'Quick posture check " + exName + ", your shoulders are inching toward your ears! [ACTION: emotion smug]'\n";
    prompt += "- 'So quiet at the desk, " + exName + "—winning or fighting a tricky task? [ACTION: emotion curious]'\n";
  } else {
    prompt += "- 'Chota sa posture check " + exName + ", thoda seedhe baith ke paani pi lo! [ACTION: emotion cute]'\n";
    prompt += "- 'Itna sannata kyu hai " + exName + ", kaam smoothly chal raha hai kya? [ACTION: emotion curious]'\n";
  }

  prompt += "\nSTRICT RULES:\n";
  prompt += "1. Keep it grounded, fresh, and friendly—no sci-fi nonsense.\n";
  if (!gotTime) {
    prompt += "2. Time is unavailable—do NOT guess or mention time of day or day of the week.\n";
  } else {
    prompt += "2. Stay consistent with the provided time/weather context.\n";
  }
  prompt += "3. Write 1 short, complete sentence (8-15 words) in " + langRule + ".\n";
  prompt += "4. End with exactly 1 emotion action tag: [ACTION: emotion smug], [ACTION: emotion wink], [ACTION: emotion curious], [ACTION: emotion cute], [ACTION: emotion happy], or [ACTION: emotion laughing].";

  return prompt;
}

static bool finalizeIdleSpeechExit(bool cancelled) {
  if (cancelled) {
    cancelRequest = false;
    if (startRecording) {
      setEmotion(EMOTION_LISTENING);
      robotStateDesc = "Listening...";
    } else {
      setEmotion(EMOTION_IDLE);
      robotStateDesc = "Ready";
      returnIdleAt = 0;
    }
    busyProcessing = false;
    return true;
  }
  busyProcessing = false;
  robotStateDesc = "Ready";
  return false;
}

// ------------------------------------------------------------
// Autonomous Boredom Checker
// ------------------------------------------------------------
void checkAutonomousBoredom() {
  uint32_t now = millis();
  if (s_pendingGiggleHappy) {
    if (!boredomEnabled || inCircadianSleep || manualSleep || busyProcessing ||
        startRecording || currentEmotion != EMOTION_LAUGHING) {
      s_pendingGiggleHappy = false;
    } else if ((int32_t)(now - s_giggleHappyAt) >= 0) {
      s_pendingGiggleHappy = false;
      setEmotion(EMOTION_HAPPY);
      returnIdleAt = now + 2500;
      emotionAutoReturnAt = returnIdleAt;
      return;
    }
  }

  // Detect fresh real user interaction (touch/voice/web) and restore 1x cadence
  if (lastUserInteractionAt != s_lastBoredomTriggerAt &&
      lastUserInteractionAt != s_lastSeenUserInteractionAt) {
    s_lastSeenUserInteractionAt = lastUserInteractionAt;
    if (s_unansweredIdleCount > 0) {
      resetBoredomInterval();
    }
  }

  if (!boredomEnabled)
    return;
  if (inCircadianSleep || manualSleep || busyProcessing || startRecording)
    return;
  if (isTonePlaying() || isSpeechPlaying())
    return;
  if (animationIsActive() || isRobotTimerActive() || isRobotTimerRinging())
    return;
  if (showClockScreen || showWeatherScreen || bannerActive)
    return;
  if (isKiwiHungry() || isKiwiAngry())
    return;
  if (currentEmotion != EMOTION_IDLE)
    return;

  if (now - lastUserInteractionAt >= nextBoredomInterval) {
    bool userWasActiveBeforeThis =
        (lastUserInteractionAt != s_lastBoredomTriggerAt);
    if (userWasActiveBeforeThis) {
      s_unansweredIdleCount = 0;
    }

    s_lastBoredomTriggerAt = now;
    lastUserInteractionAt = now;

    // Schedule next interval with smooth backoff if user stays away
    if (s_unansweredIdleCount < 4) {
      s_unansweredIdleCount++;
    }
    nextBoredomInterval = computeBoredomIntervalMs(s_unansweredIdleCount);

    // Prioritize pending life follow-up question when online & TTS-ready
    bool useFish = (ttsProvider == "fish" || ttsProvider == "fishaudio");
    bool ttsReady = useFish
                        ? (fishKey.length() > 0)
                        : (ttsKey.length() > 0 || sttKey.length() > 0 ||
                           llmKey.length() > 0);
    if (hasPendingFollowUp()) {
      if (WiFi.status() == WL_CONNECTED && speakerVolume > 0 && ttsReady) {
        String q = kiwiMemory.pendingFollowUp;
        q.trim();
        if (q.length() > 0) {
          Serial.printf("[MEMORY] Triggering pending life follow-up: %s\n", q.c_str());
          busyProcessing = true;
          cancelRequest = false;
          robotStateDesc = "Checking in...";
          setEmotion(detectVocalEmotion(q));

          bool spoken = speakText(q);
          bool wasCancelled = cancelRequest;
          if (spoken && !wasCancelled) {
            popPendingFollowUp();
            addChatHistory("Check in on my recent update.", q);
            lastLlmAnswer = q;
            addTerminalLog("[FOLLOWUP] Kiwi: \"" + q + "\"");
            setEmotion(EMOTION_HAPPY);
            returnIdleAt = millis() + 3000;
          } else if (!wasCancelled) {
            setEmotion(EMOTION_IDLE);
            returnIdleAt = 0;
          }
          finalizeIdleSpeechExit(wasCancelled);
          return;
        }
      }
    }

    Serial.printf("[BOREDOM] ~%d mins idle on desk! (unanswered=%u, next ~%.1f min)\n",
                  boredomMinutes, (unsigned)s_unansweredIdleCount,
                  (float)nextBoredomInterval / 60000.0f);

    // When muted, keep idle life purely visual (no wasted LLM/TTS calls)
    if (speakerVolume <= 0) {
      executePhysicalQuirk(false);
      return;
    }

    // Offline or missing LLM/TTS keys -> instant physical desk buddy quirks
    String activeLlmKey =
        (llmProvider == "groq" && groqKey.length() > 0) ? groqKey : llmKey;
    if (WiFi.status() != WL_CONNECTED || activeLlmKey.length() == 0 || !ttsReady) {
      executePhysicalQuirk(true);
      return;
    }

    // Quirk mix: ~17% when active, ~50% when user is away for multiple cycles
    int quirkChance = (s_unansweredIdleCount >= 2) ? 2 : 6;
    if (random(0, quirkChance) == 0) {
      executePhysicalQuirk(true);
      return;
    }

    // Generate Dynamic LLM Autonomous Remark
    String prompt = buildDynamicIdlePrompt();
    Serial.println("[BOREDOM] Calling LLM for dynamic chatter...");

    busyProcessing = true;
    cancelRequest = false;
    robotStateDesc = "Daydreaming...";
    setEmotion(EMOTION_THINKING);

    String thought = askLLM(prompt, nullptr, false);

    if (cancelRequest) {
      Serial.println("[BOREDOM] Cancelled / interrupted while thinking");
      finalizeIdleSpeechExit(true);
      return;
    }

    if (thought.length() > 0) {
      Serial.printf("[BOREDOM LLM] Kiwi: %s\n", thought.c_str());
      addTerminalLog("[IDLE BANTER] Kiwi: \"" + thought + "\"");

      parseAndExecuteActions(thought);
      String cleanThought = stripRobotTags(thought);
      cleanThought.trim();
      if (cleanThought.length() == 0) {
        finalizeIdleSpeechExit(false);
        return;
      }

      recordRecentIdleThought(cleanThought);
      lastLlmAnswer = cleanThought;

      if (returnIdleAt == 0 || (int32_t)(millis() - returnIdleAt) >= 0 ||
          currentEmotion == EMOTION_THINKING) {
        setEmotion(detectVocalEmotion(cleanThought));
      }

      robotStateDesc = "Speaking...";
      speakText(cleanThought);
      if (cancelRequest) {
        Serial.println("[BOREDOM] Interrupted mid-speech");
        finalizeIdleSpeechExit(true);
        return;
      }
      if (returnIdleAt == 0 || (int32_t)(millis() - returnIdleAt) >= 0) {
        setEmotion(EMOTION_HAPPY);
        returnIdleAt = millis() + 2500;
      }
    } else {
      Serial.println("[BOREDOM] LLM empty -> fallback to quirk");
      executePhysicalQuirk(true);
    }

    finalizeIdleSpeechExit(false);
  }
}
