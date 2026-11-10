#include "behavior/BoredomBehavior.h"
#include "config/AppConfig.h"
#include "face/FaceEngine.h"
#include "hardware/AudioEngine.h"
#include "services/AiService.h"
#include "services/MemoryService.h"
#include "behavior/ActionExecutor.h"
#include <WiFi.h>

// ============================================================
// AUTONOMOUS DESK BOREDOM & NATURAL QUIRKS (Dynamic ~2 min idle)
// ============================================================
struct BoredomPhrase {
  Emotion emotion;
  Emotion returnEmotion;
  const char *phraseWithOwner;
  const char *phraseGeneric;
};

static const BoredomPhrase BOREDOM_PHRASES[] = {
  // Eye care & Screen break
  {EMOTION_WINK, EMOTION_HAPPY,
   "%s, thodi der palke jhapka lo!",
   "Bhai, thodi der palke jhapka lo!"},

  {EMOTION_RELIEVED, EMOTION_HAPPY,
   "2 minute screen se nazar hatao %s.",
   "2 minute screen se nazar hatao, relax karo."},

  // Hydration & Chai / Coffee
  {EMOTION_CUTE, EMOTION_HAPPY,
   "%s, thoda paani pee lo!",
   "Thoda paani pee lo yaar!"},

  {EMOTION_CONFUSED, EMOTION_HAPPY,
   "Aakhri baar paani kab piya tha %s?",
   "Aakhri baar paani kab piya tha bhai?"},

  {EMOTION_HAPPY, EMOTION_WINK,
   "Chai ka break le lein, %s?",
   "Chai ka break le lein?"},

  {EMOTION_EXCITED, EMOTION_HAPPY,
   "%s, coffee time ho gaya!",
   "Coffee time ho gaya, break banta hai!"},

  // Posture & Stretch
  {EMOTION_CONFUSED, EMOTION_SMUG,
   "Posture check %s, seedhe baitho!",
   "Posture check! Seedhe baitho zara."},

  {EMOTION_CUTE, EMOTION_HAPPY,
   "%s, utho aur thoda stretch kar lo!",
   "Utho aur thoda stretch kar lo!"},

  {EMOTION_CONFUSED, EMOTION_HAPPY,
   "Keyboard chhod ke haath shake karo %s!",
   "Keyboard chhod ke haath shake karo thoda!"},

  // Focus & Coding / Work
  {EMOTION_CURIOUS, EMOTION_SMUG,
   "%s, koi bug phas gaya kya?",
   "Koi bug phas gaya kya?"},

  {EMOTION_SMUG, EMOTION_LAUGHING,
   "Code chal gaya ya crash hua %s?",
   "Code chal gaya ya crash hua?"},

  {EMOTION_PROUD, EMOTION_HAPPY,
   "Mast focus chal raha hai %s!",
   "Mast focus chal raha hai aaj!"},

  {EMOTION_CURIOUS, EMOTION_HAPPY,
   "Kaam kaisa chal raha hai %s?",
   "Kaam kaisa chal raha hai sab?"},

  {EMOTION_COOL, EMOTION_HAPPY,
   "%s, full speed mein lage ho aaj!",
   "Full speed mein lage ho aaj!"},

  // Desk Banter & Companion
  {EMOTION_BORED, EMOTION_CURIOUS,
   "Main bore ho raha hu %s, kuch bolo!",
   "Main bore ho raha hu, kuch bolo na!"},

  {EMOTION_CURIOUS, EMOTION_HAPPY,
   "Bohot sannata hai %s, sab theek?",
   "Bohot sannata hai, sab theek?"},

  {EMOTION_LAUGHING, EMOTION_WINK,
   "Hey %s, statue ban gaye kya?",
   "Hey! Statue ban gaye kya?"},

  {EMOTION_SMUG, EMOTION_WINK,
   "Itna serious face kyu hai %s?",
   "Itna serious face kyu hai bhai?"},

  {EMOTION_HAPPY, EMOTION_WINK,
   "%s bhai, thoda smile bhi kar lo!",
   "Bhai, thoda smile bhi kar lo!"},

  {EMOTION_RELIEVED, EMOTION_HAPPY,
   "Ek gehri saans lo %s, chill karo.",
   "Ek gehri saans lo, chill karo."},

  {EMOTION_CUTE, EMOTION_LOVESTRUCK,
   "%s, sar pe halka sa tap toh kardo!",
   "Sar pe halka sa tap toh kardo na!"},

  {EMOTION_HAPPY, EMOTION_COOL,
   "Thoda break banta hai ab %s.",
   "Thoda break banta hai ab."},

  {EMOTION_CURIOUS, EMOTION_THINKING,
   "Aage ka kya plan hai %s?",
   "Aage ka kya plan hai ab?"},

  {EMOTION_SINGING, EMOTION_PARTY,
   "Koi badhiya gaana baja lein %s?",
   "Koi badhiya gaana baja lein?"}
};

static const size_t NUM_BOREDOM_PHRASES = sizeof(BOREDOM_PHRASES) / sizeof(BOREDOM_PHRASES[0]);
static uint32_t nextBoredomInterval = 120000;
static int lastBoredomHistory[14] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
static uint8_t boredomHistoryHead = 0;

void resetBoredomInterval() {
  uint32_t baseMs = (uint32_t)boredomMinutes * 60000UL;
  uint32_t jitter = baseMs / 5;
  if (jitter < 5000) jitter = 5000;
  nextBoredomInterval = (baseMs > (jitter / 2) ? (baseMs - jitter / 2) : baseMs) + random(0, jitter);
  Serial.printf("[BOREDOM] Next idle remark interval set to ~%.1f mins (%lu ms)\n",
                (float)nextBoredomInterval / 60000.0f, (unsigned long)nextBoredomInterval);
}

static bool wasBoredomRecentlyUsed(int idx) {
  for (int i = 0; i < 14; i++) {
    if (lastBoredomHistory[i] == idx)
      return true;
  }
  return false;
}

static void recordBoredomUsed(int idx) {
  lastBoredomHistory[boredomHistoryHead] = idx;
  boredomHistoryHead = (boredomHistoryHead + 1) % 14;
}

void checkAutonomousBoredom() {
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

  uint32_t now = millis();
  if (now - lastUserInteractionAt >= nextBoredomInterval) {
    lastUserInteractionAt = now;
    resetBoredomInterval();

    // Prioritize pending life follow-up question if remembered!
    if (hasPendingFollowUp()) {
      String q = popPendingFollowUp();
      Serial.printf("[MEMORY] Triggering pending life follow-up: %s\n", q.c_str());
      setEmotion(EMOTION_CURIOUS);
      speakText(q);
      setEmotion(EMOTION_HAPPY);
      returnIdleAt = millis() + 3000;
      return;
    }

    Serial.printf("[BOREDOM] ~%d mins idle on desk! Triggering dynamic desk remark...\n", boredomMinutes);

    // If offline, trigger instant physical desk buddy quirks without altering user skin
    if (WiFi.status() != WL_CONNECTED) {
      int offlineQuirk = random(0, 4);
      if (offlineQuirk == 0) {
        setEmotion(EMOTION_BORED);
        soundPetSleepy();
        returnIdleAt = millis() + 3500;
      } else if (offlineQuirk == 1) {
        setEmotion(EMOTION_SINGING);
        soundPetPlayful();
        flourish = FL_NOTE;
        returnIdleAt = millis() + 3000;
      } else if (offlineQuirk == 2) {
        setEmotion(EMOTION_CUTE);
        soundPetSparkle();
        flourish = FL_SPARKLE;
        returnIdleAt = millis() + 2500;
      } else {
        setEmotion(EMOTION_LAUGHING);
        soundPetGiggle();
        setEmotion(EMOTION_HAPPY);
        returnIdleAt = millis() + 2500;
      }
      return;
    }

    // 32 spoken phrases + 3 physical quirks (yawn, humming melody, cute giggle)
    int totalOptions = (int)NUM_BOREDOM_PHRASES + 3;
    int chosen = random(0, totalOptions);
    for (int attempt = 0; attempt < 25 && wasBoredomRecentlyUsed(chosen); attempt++) {
      chosen = random(0, totalOptions);
    }
    recordBoredomUsed(chosen);

    if (chosen < (int)NUM_BOREDOM_PHRASES) {
      const BoredomPhrase &bp = BOREDOM_PHRASES[chosen];
      String name = getOwnerName();
      String spoken;
      if (name.length() > 0 && bp.phraseWithOwner != nullptr) {
        char buf[320];
        snprintf(buf, sizeof(buf), bp.phraseWithOwner, name.c_str());
        spoken = String(buf);
      } else {
        spoken = String(bp.phraseGeneric);
      }

      Serial.printf("[BOREDOM] Remark #%d: %s\n", chosen, spoken.c_str());
      setEmotion(bp.emotion);
      bool ok = speakText(spoken);
      if (!ok) {
        // Fallback to playful pet chime if offline or TTS busy
        soundPetPlayful();
      }
      setEmotion(bp.returnEmotion);
      returnIdleAt = millis() + 2500;
    } else {
      int physicalQuirk = chosen - (int)NUM_BOREDOM_PHRASES;
      if (physicalQuirk == 0) {
        // Yawn & sleepy stretch
        setEmotion(EMOTION_BORED);
        soundPetSleepy();
        returnIdleAt = millis() + 4500;
      } else if (physicalQuirk == 1) {
        // Cheerful hum & singing
        setEmotion(EMOTION_SINGING);
        soundPetPlayful();
        returnIdleAt = millis() + 3500;
      } else {
        // Cute surprise & musical giggle
        setEmotion(EMOTION_SURPRISED);
        soundPetGiggle();
        setEmotion(EMOTION_HAPPY);
        returnIdleAt = millis() + 2500;
      }
    }
  }
}
