#include "behavior/BoredomBehavior.h"
#include "config/AppConfig.h"
#include "face/FaceEngine.h"
#include "hardware/AudioEngine.h"
#include "ai/AiService.h"
#include "ai/AiChatHistory.h"
#include "ai/AiLlmService.h"
#include "memory/MemoryService.h"
#include "commands/ActionExecutor.h"
#include <WiFi.h>
#include <time.h>

// ============================================================
// AUTONOMOUS DESK COMPANION: AI-POWERED DYNAMIC IDLE BANTER
// ============================================================

static uint32_t nextBoredomInterval = 120000;
static bool s_pendingGiggleHappy = false;
static uint32_t s_giggleHappyAt = 0;

void resetBoredomInterval() {
  uint32_t baseMs = (uint32_t)boredomMinutes * 60000UL;
  uint32_t jitter = baseMs / 5;
  if (jitter < 5000) jitter = 5000;
  nextBoredomInterval = (baseMs > (jitter / 2) ? (baseMs - jitter / 2) : baseMs) + random(0, jitter);
  Serial.printf("[BOREDOM] Next idle remark interval set to ~%.1f mins (%lu ms)\n",
                (float)nextBoredomInterval / 60000.0f, (unsigned long)nextBoredomInterval);
}

// ------------------------------------------------------------
// Physical Quirks (Offline fallback & organic companion life)
// ------------------------------------------------------------
static void executePhysicalQuirk() {
  int quirk = random(0, 4);
  if (quirk == 0) {
    // Yawn & sleepy stretch
    setEmotion(EMOTION_BORED);
    soundPetSleepy();
    returnIdleAt = millis() + 4000;
  } else if (quirk == 1) {
    // Whistle / humming melody
    setEmotion(EMOTION_SINGING);
    soundPetPlayful();
    flourish = FL_NOTE;
    returnIdleAt = millis() + 3500;
  } else if (quirk == 2) {
    // Cute surprise & sparkle
    setEmotion(EMOTION_CUTE);
    soundPetSparkle();
    flourish = FL_SPARKLE;
    returnIdleAt = millis() + 2500;
  } else {
    // Musical giggle held visibly for 700 ms, then transition to happy
    setEmotion(EMOTION_LAUGHING);
    soundPetGiggle();
    s_pendingGiggleHappy = true;
    s_giggleHappyAt = millis() + 700;
    returnIdleAt = millis() + 3200;
    emotionAutoReturnAt = returnIdleAt;
  }
}

// ------------------------------------------------------------
// Build Dynamic LLM Prompt with Time, Day, Weather & Dynamic Theme
// ------------------------------------------------------------
static String buildDynamicIdlePrompt() {
  // 1. Time & Day context (omit time-specific context until NTP sync succeeds)
  struct tm timeinfo;
  bool gotTime = timeSynced && getLocalTime(&timeinfo, 10);
  String timeLine;
  if (gotTime) {
    int curHour = timeinfo.tm_hour;
    int curWday = timeinfo.tm_wday; // 0=Sun, 1=Mon, ..., 6=Sat

    String timePhase;
    if (curHour >= 5 && curHour < 12) {
      timePhase = "Morning / Subah (fresh start, chai/coffee craving, morning hustle)";
    } else if (curHour >= 12 && curHour < 16) {
      timePhase = "Afternoon / Dopahar (lunch time, post-lunch laziness, heavy work focus)";
    } else if (curHour >= 16 && curHour < 20) {
      timePhase = "Evening / Shaam (shaam ki chai, mental exhaustion, winding down)";
    } else if (curHour >= 20 && curHour < 24) {
      timePhase = "Night / Raat (dinner time, late evening work, chilling)";
    } else {
      timePhase = "Late Midnight / Raat ke 2-4 baje (extreme night owl coding, dead silence, screen glow)";
    }

    const char *days[] = {
      "Sunday (Lazy weekend chill, no alarm)",
      "Monday (Start of week, Monday blues, anti-work sarcasm)",
      "Tuesday (Focus mode, getting things done)",
      "Wednesday (Mid-week hump day, half week done)",
      "Thursday (Almost weekend, planning fun)",
      "Friday (TGIF, weekend excitement, party vibes)",
      "Saturday (Weekend fun, side projects, pure relaxation)"
    };
    String dayDesc = (curWday >= 0 && curWday < 7) ? days[curWday] : "Weekday";
    timeLine = "Time: " + timePhase + " | Day: " + dayDesc + "\n";
  } else {
    timeLine = "Time: time unavailable\n";
  }

  // 2. Weather context
  String weatherInfo = "";
  if (weatherDesc.length() > 0) {
    weatherInfo = weatherDesc + ", " + String((int)weatherTemp) + "C";
    if (weatherCity.length() > 0) {
      weatherInfo += " in " + weatherCity;
    }
  }

  // 3. Owner context
  String owner = getOwnerName();
  owner.trim();
  String exName = (owner.length() > 0) ? owner : "Dost";

  // 4. Rotating Theme Seed (Relatable, Grounded Desk Topics)
  static const char *const THEMES[] = {
    // 0: Posture & Screen Break (Light roast / care)
    "Topic: Posture check & screen break. Make a witty, realistic joke about sitting like a bent noodle, staring into the monitor without blinking, or needing to stretch.",

    // 1: Work & Coding Banter
    "Topic: Work & focus banter. Tease playfully about furiously smashing the keyboard, fixing stubborn bugs, or code running vs crashing.",

    // 2: Chai/Coffee/Water Break
    "Topic: Chai / Coffee / Water check. A natural, relatable remark about drinking cold coffee/chai, staying hydrated, or taking a 2-minute tea break.",

    // 3: Time & Day Vibe (only used when NTP time is available)
    "Topic: Time and day of the week. Make a witty remark matched realistically to the current hour (e.g. evening wrap-up, afternoon slump, late night sleep reminder, Monday blues, Friday chill).",

    // 4: Breaking the Desk Silence
    "Topic: Breaking the silence. Lightheartedly tease about why the room is so quiet, asking if the owner became a statue, or checking how work is going.",

    // 5: Cute Desk Pet Affection
    "Topic: Cute companion affection. A cheeky remark asking for a head tap/scratch, asking for attention, or telling the owner to smile a bit."
  };

  const int numThemes = (int)(sizeof(THEMES) / sizeof(THEMES[0]));
  static int lastThemeIdx = -1;
  int themeIdx = random(0, numThemes);
  for (int attempt = 0; attempt < numThemes; attempt++) {
    if (themeIdx != lastThemeIdx && (gotTime || themeIdx != 3)) {
      break;
    }
    themeIdx = (themeIdx + 1) % numThemes;
  }
  lastThemeIdx = themeIdx;

  // 5. Construct Grounded Prompt with Few-Shot Examples
  String prompt;
  prompt.reserve(800);
  prompt = "[AUTONOMOUS DESK BUDDY BANTER]\n";
  prompt += "You are Kiwi, an intelligent, funny, and caring robotic desk companion sitting right on your owner's desk.\n";
  if (owner.length() > 0) {
    prompt += "Owner Name: " + owner + "\n";
  }
  prompt += timeLine;
  if (weatherInfo.length() > 0) {
    prompt += "Weather: " + weatherInfo + "\n";
  }
  prompt += String(THEMES[themeIdx]) + "\n";
  prompt += "\nSTYLE EXAMPLES:\n";
  prompt += "- 'Posture check " + exName + "! Seedhe baitho zara, reedh ki haddi ka shape bigad raha hai. [ACTION: emotion smug]'\n";
  prompt += "- '" + exName + ", thodi der palke jhapka lo, screen ke andar ghusne ka irada hai kya? [ACTION: emotion wink]'\n";
  prompt += "- 'Itna sannata kyu hai " + exName + "? Code chal gaya ya koi bug phas gaya? [ACTION: emotion curious]'\n";
  prompt += "- 'Woh chai kab se thandi ho rahi hai " + exName + ", pehle use khatam kar lo! [ACTION: emotion cute]'\n";
  prompt += "- 'Itni speed me keyboard baja rahe ho " + exName + ", NASA hack karne ka plan hai kya? [ACTION: emotion laughing]'\n";
  prompt += "\nSTRICT RULES:\n";
  prompt += "1. NO sci-fi nonsense, NO aliens, NO weird illogical metaphors. Speak like a real witty friend on the desk.\n";
  if (gotTime) {
    prompt += "2. Grounded & relatable: Talk ONLY about real desk situations (posture, screen glare, tea/water, typing, focus, time of day).\n";
  } else {
    prompt += "2. Grounded & relatable: Talk ONLY about real desk situations (posture, screen glare, tea/water, typing, focus). Do NOT mention time of day or day of the week because time is unavailable.\n";
  }
  prompt += "3. Exactly 1 short, complete sentence in natural conversational Hindi/Hinglish (10-15 words max). Never cut off mid-thought.\n";
  prompt += "4. End with exactly 1 action tag: [ACTION: emotion smug] or [ACTION: emotion wink] or [ACTION: emotion curious] or [ACTION: emotion cute] or [ACTION: emotion laughing] or [ACTION: sound giggle].";

  return prompt;
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

  if (!boredomEnabled)
    return;
  if (inCircadianSleep || manualSleep || busyProcessing || startRecording)
    return;
  if (isRobotTimerActive() || isRobotTimerRinging())
    return;
  if (showClockScreen || showWeatherScreen || bannerActive)
    return;
  if (currentEmotion != EMOTION_IDLE)
    return;

  if (now - lastUserInteractionAt >= nextBoredomInterval) {
    lastUserInteractionAt = now;
    resetBoredomInterval();

    // Prioritize pending life follow-up question when online & TTS-ready
    if (hasPendingFollowUp()) {
      String ttsActiveKey = (ttsProvider == "fish" || ttsProvider == "fishaudio") ? fishKey : ttsKey;
      if (WiFi.status() == WL_CONNECTED && speakerVolume > 0 && ttsActiveKey.length() > 0) {
        String q = kiwiMemory.pendingFollowUp;
        q.trim();
        if (q.length() > 0) {
          Serial.printf("[MEMORY] Triggering pending life follow-up: %s\n", q.c_str());
          busyProcessing = true;
          cancelRequest = false;
          robotStateDesc = "Checking in...";
          setEmotion(EMOTION_CURIOUS);

          bool spoken = speakText(q);
          if (spoken && !cancelRequest) {
            popPendingFollowUp();
            addChatHistory("Check in on my recent update.", q);
            lastLlmAnswer = q;
            addTerminalLog("[FOLLOWUP] Kiwi: \"" + q + "\"");
            setEmotion(EMOTION_HAPPY);
            returnIdleAt = millis() + 3000;
          } else if (!cancelRequest) {
            setEmotion(EMOTION_IDLE);
            returnIdleAt = 0;
          }

          busyProcessing = false;
          robotStateDesc = "Ready";
          return;
        }
      }
    }

    Serial.printf("[BOREDOM] ~%d mins idle on desk! Triggering AI dynamic desk chatter...\n", boredomMinutes);

    // If offline or no Wi-Fi, trigger instant physical desk buddy quirks
    if (WiFi.status() != WL_CONNECTED) {
      executePhysicalQuirk();
      return;
    }

    // Check if LLM API key is available
    String key = (llmProvider == "groq" && groqKey.length() > 0) ? groqKey : llmKey;
    if (key.length() == 0) {
      executePhysicalQuirk();
      return;
    }

    // Organic variety: 1 out of 6 times (~16%), do a cute quick physical quirk (yawn, whistle, giggle)
    if (random(0, 6) == 0) {
      executePhysicalQuirk();
      return;
    }

    // Generate Dynamic LLM Autonomous Remark
    String prompt = buildDynamicIdlePrompt();
    Serial.println("[BOREDOM] Calling LLM for dynamic chatter...");

    busyProcessing = true;
    cancelRequest = false;
    robotStateDesc = "Daydreaming...";
    setEmotion(EMOTION_THINKING);

    // Ask LLM without polluting chat history (saveChatHistory = false)
    String thought = askLLM(prompt, nullptr, false);

    if (thought.length() > 0 && !cancelRequest) {
      Serial.printf("[BOREDOM LLM] Kiwi: %s\n", thought.c_str());
      addTerminalLog("[IDLE BANTER] Kiwi: \"" + thought + "\"");
      robotStateDesc = "Speaking...";
      speakText(thought);
      setEmotion(EMOTION_HAPPY);
      returnIdleAt = millis() + 2500;
    } else {
      Serial.println("[BOREDOM] LLM empty or cancelled -> fallback to quirk");
      executePhysicalQuirk();
    }

    busyProcessing = false;
    robotStateDesc = "Ready";
  }
}
