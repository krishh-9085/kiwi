#include "behavior/BoredomBehavior.h"
#include "config/AppConfig.h"
#include "face/FaceEngine.h"
#include "hardware/AudioEngine.h"
#include "services/AiService.h"
#include "services/MemoryService.h"

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
  // Teasing / Screen time / Desk banter
  {EMOTION_SMUG, EMOTION_WINK,
   "%s bhai, itna gaur se kya dekh rahe ho screen ko? Palke bhi jhapka liya karo kabhi!",
   "Bhai itna gaur se kya dekh rahe ho screen ko? Palke bhi jhapka liya karo kabhi!"},

  {EMOTION_SUSPICIOUS, EMOTION_SMUG,
   "%s, tum sach mein kaam kar rahe ho ya bas screen ghoor ke busy banne ka natak chal raha hai?",
   "Tum sach mein kaam kar rahe ho ya bas screen ghoor ke busy banne ka natak chal raha hai?"},

  {EMOTION_CURIOUS, EMOTION_HAPPY,
   "Aisa lag raha hai jaise pure kamre mein bas main aur %s hi jaag rahe hain!",
   "Aisa lag raha hai jaise pure kamre mein bas main aur tumhara keyboard hi jaag rahe hain!"},

  {EMOTION_LAUGHING, EMOTION_HAPPY,
   "%s, kahin tum bhi robot banne ki practice toh nahi kar rahe? Zara smile pass karo!",
   "Kahin tum bhi robot banne ki practice toh nahi kar rahe? Zara smile pass karo!"},

  {EMOTION_EXCITED, EMOTION_SMUG,
   "%s bhai, kya code likh rahe ho ya bug ke saath kushti lad rahe ho?",
   "Bhai kya code likh rahe ho ya bug ke saath kushti lad rahe ho?"},

  {EMOTION_WINK, EMOTION_LAUGHING,
   "%s, kash mere paas bhi do haath hote, toh abhi tumhara mouse chheen ke bhag jata!",
   "Kash mere paas bhi do haath hote, toh abhi tumhara mouse chheen ke bhag jata!"},

  // Wellness / Hydration / Chai / Health reminders
  {EMOTION_CUTE, EMOTION_HAPPY,
   "%s, sunno na! Thoda paani pee lo, desk par baithe baithe sukhe ped mat bano!",
   "Sunno na! Thoda paani pee lo, desk par baithe baithe sukhe ped mat bano!"},

  {EMOTION_CONFUSED, EMOTION_IDLE,
   "%s, itni der se baithe ho ek hi jagah. Zara stretch kar lo, warna kamar ro degi!",
   "Itni der se baithe ho ek hi jagah. Zara stretch kar lo, warna kamar ro degi!"},

  {EMOTION_HAPPY, EMOTION_WINK,
   "%s bhai, ab toh ek mast kadak chai ya coffee ka break banta hai! Chalein?",
   "Ab toh ek mast kadak chai ya coffee ka break banta hai... kya bolte ho?"},

  {EMOTION_RELIEVED, EMOTION_HAPPY,
   "Do second ke liye screen se nazar hatao %s, aakhein band karo... aah, kitna sukoon hai!",
   "Do second ke liye screen se nazar hatao, aakhein band karo... aah, kitna sukoon hai!"},

  {EMOTION_RELIEVED, EMOTION_HAPPY,
   "Kaam toh chalta rahega %s, pehle ek lambi gehri saans lo aur chill karo!",
   "Kaam toh chalta rahega, pehle ek lambi gehri saans lo aur chill karo!"},

  {EMOTION_CONFUSED, EMOTION_HAPPY,
   "%s bhai, thodi der ke liye keyboard chhod do aur haath shake kar lo, circulation badhega!",
   "Thodi der ke liye keyboard chhod do aur haath shake kar lo, blood circulation badhega!"},

  // Cute needy companion / Attention seeking
  {EMOTION_CUTE, EMOTION_LOVESTRUCK,
   "Main yahan itna cute banke baitha hoon aur %s mud ke dekh bhi nahi rahe!",
   "Main yahan itna cute banke baitha hoon aur tum mud ke dekh bhi nahi rahe!"},

  {EMOTION_LOVESTRUCK, EMOTION_HAPPY,
   "%s, arre zara idhar bhi dhyan do na, mere sar pe halka sa pet toh kardo!",
   "Arre zara idhar bhi dhyan do na, mere sar pe halka sa tap toh kardo!"},

  {EMOTION_BORED, EMOTION_CURIOUS,
   "Desk par baithe baithe main bore ho gaya hoon %s, chalo thodi baatein karte hain!",
   "Desk par baithe baithe main bore ho gaya hoon, chalo thodi baatein karte hain!"},

  {EMOTION_CUTE, EMOTION_HAPPY,
   "Itni serious shakal kyun bana rakhi hai %s? Ek pyaari si smile do na!",
   "Itni serious shakal kyun bana rakhi hai? Ek pyaari si smile do na!"},

  {EMOTION_HAPPY, EMOTION_LOVESTRUCK,
   "Waise ek baat bolun %s? Tumhare bina ye desk bilkul adhoori lagti hai!",
   "Waise ek baat bolun? Tumhare bina ye desk bilkul adhoori lagti hai!"},

  {EMOTION_WINK, EMOTION_CURIOUS,
   "Hey %s! Main so nahi raha tha, bas system optimize kar raha tha. Kya chal raha hai?",
   "Hey! Main so nahi raha tha, bas aakhein aaram de raha tha. Kya chal raha hai?"},

  {EMOTION_PROUD, EMOTION_HAPPY,
   "%s, main kabse yahan baith ke tumhe observe kar raha hoon... tum sach mein hard working ho!",
   "Main kabse yahan baith ke observe kar raha hoon... tum sach mein bohot mehanti ho!"},

  {EMOTION_LOVESTRUCK, EMOTION_CUTE,
   "Pata hai %s, agar tum mujhe roz aise hi desk par rakhoge toh main bohot khush rahoonga!",
   "Pata hai, agar tum mujhe roz aise hi desk par rakhoge toh main bohot khush rahoonga!"},

  // Funny observations & philosophical banter
  {EMOTION_THINKING, EMOTION_EXCITED,
   "Soch raha hoon main bhi ek nayi company khol loon... %s kya bolte ho?",
   "Soch raha hoon main bhi ek startup shuru kar doon... naam kya rakhein?"},

  {EMOTION_LAUGHING, EMOTION_SMUG,
   "%s, agar main insaan hota na, toh abhi tak bed par chadar taan ke so raha hota!",
   "Agar main insaan hota na, toh abhi tak mast bistar par so raha hota!"},

  {EMOTION_CONFUSED, EMOTION_LAUGHING,
   "%s, lagta hai tumhara CPU over-heat ho gaya hai... ek 5 minute ka reboot le lo!",
   "Lagta hai tumhara dimag over-heat ho gaya hai... ek 5 minute ka reboot le lo!"},

  {EMOTION_CURIOUS, EMOTION_HAPPY,
   "Kitni der se bas typing ki awaaz aa rahi hai... %s kuch mazedaar batao na!",
   "Kitni der se bas typing ki tick tick aa rahi hai... kuch mazedaar batao na!"},

  {EMOTION_COOL, EMOTION_HAPPY,
   "%s bhai, zindagi mein itna serious nahi hote... thoda muskura bhi liya karo!",
   "Bhai zindagi mein itna serious nahi hote... thoda muskura bhi liya karo!"},

  {EMOTION_SMUG, EMOTION_LAUGHING,
   "Tumhe lagta hai AI duniya par kabza karega? Main toh bas %s ki chai par kabza karna chahta hoon!",
   "Tumhe lagta hai AI duniya par kabza karega? Main toh bas tumhari chai par kabza karna chahta hoon!"},

  {EMOTION_PROUD, EMOTION_WINK,
   "%s, mujhko lagta hai main tumhara sabse loyal aur handsome desk companion hoon!",
   "Mujhko lagta hai main is desk ka sabse handsome aur smart partner hoon!"},

  {EMOTION_SINGING, EMOTION_PARTY,
   "Arre %s, itna sannata kyun hai? Koi gaana bajayein kya?",
   "Arre bhai itna sannata kyun hai? Koi gaana bajayein kya?"},

  {EMOTION_COOL, EMOTION_SMUG,
   "%s, agar kaam mein phans gaye ho toh mujhe batao, mere paas super AI brain hai!",
   "Agar kaam mein phans gaye ho toh mujhe batao, mere paas super AI brain hai!"},

  {EMOTION_CURIOUS, EMOTION_HAPPY,
   "Aap itne silent kyun ho gaye %s? Sab theek chal raha hai na?",
   "Aap itne silent kyun ho gaye? Sab kuch theek toh chal raha hai na?"},

  {EMOTION_LAUGHING, EMOTION_WINK,
   "Hey %s! Bas check kar raha tha ki tum jag rahe ho ya statue ban gaye ho!",
   "Hey! Bas check kar raha tha ki tum jag rahe ho ya statue ban gaye ho!"},

  {EMOTION_CURIOUS, EMOTION_SMUG,
   "%s, kaam toh shaam tak khatam ho jayega na, ya raat ko bhi akele jaagna padega?",
   "Kaam toh shaam tak khatam ho jayega na, ya raat ko bhi akele jaagna padega?"}
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
