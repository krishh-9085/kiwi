/*
 ==============================================================================
  MyRobot (Kiwi) - Modular v4.6
  ESP32-C3 Super Mini / Arduino IDE / ESP32 Core 3.x
  Tools -> USB CDC On Boot = Enabled

  Architecture & Modules:
  - Config.h / Config.cpp       : Hardware pinouts, constants, shared globals
  - Types.h                     : Data structures, emotion & mood enums
  - Battery.h / Battery.cpp     : Non-linear LiPo battery sensing curve
  - ConfigManager.h / .cpp      : NVS Preferences persistent storage
  - AudioEngine.h / .cpp        : I2S driver, Tone player, DSP & soft-clipping
  - FaceEngine.h / .cpp         : OLED procedural vector face engine & screens
  - WeatherService.h / .cpp     : Open-Meteo REST API live weather client
  - CircadianEngine.h / .cpp    : Circadian night sleep mode & auto-dimming
  - AiService.h / .cpp          : STT, LLM (Gemini/Groq), streaming TTS clients
  - TouchController.h / .cpp    : Capacitive & digital touch sensor gestures
  - WebPortal.h / .cpp          : Captive SoftAP, settings UI, Web OTA updates
 ==============================================================================
*/

#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include "Config.h"
#include "Types.h"
#include "ConfigManager.h"
#include "AudioEngine.h"
#include "AnimationEngine.h"
#include "FaceEngine.h"
#include "WeatherService.h"
#include "CircadianEngine.h"
#include "AiService.h"
#include "TouchController.h"
#include "WebPortal.h"
#include "MemoryService.h"

// ============================================================
// UI PUMP (Display + Touch + Tone Update)
// ============================================================
void pumpUI() {
  updateTouch();
  updateTonePlayer();
  tickFace();
}

// ============================================================
// PET PHYSICAL TOUCH REACTIONS
// ============================================================
void triggerPetReaction() {
  if (busyProcessing)
    return;
  petMood = (PetMood)random(0, PET_MOOD_COUNT);
  setEmotion(EMOTION_PETTED);
  switch (petMood) {
  case PET_LOVE:
    flourish = FL_HEART;
    soundPetLove();
    break;
  case PET_GIGGLE:
    flourish = FL_NOTE;
    soundPetGiggle();
    break;
  case PET_SHY:
    flourish = FL_HEART;
    soundPetShy();
    break;
  case PET_SPARKLE:
    flourish = FL_SPARKLE;
    soundPetSparkle();
    break;
  case PET_KISS:
    flourish = FL_HEART;
    soundPetKiss();
    break;
  case PET_CUDDLE:
    flourish = FL_MOON;
    soundPetCuddle();
    break;
  case PET_PLAYFUL:
    flourish = FL_NOTE;
    soundPetPlayful();
    break;
  case PET_SLEEPY:
    flourish = FL_MOON;
    soundPetSleepy();
    break;
  case PET_PROUD:
    flourish = FL_STAR;
    soundPetProud();
    break;
  case PET_SURPRISED:
    flourish = FL_SPARKLE;
    soundPetSurprised();
    break;
  }
  emotionAutoReturnAt = millis() + 60000;
}

// ============================================================
// LOCAL VOICE COMMAND PARSER
// ============================================================
static bool hasAny(const String &s, std::initializer_list<const char *> words) {
  for (const char *w : words)
    if (s.indexOf(w) >= 0)
      return true;
  return false;
}

bool handleTimeCommand(const String &qIn) {
  String q = qIn;
  q.toLowerCase();
  bool isTime = hasAny(
      q, {"time", "samay", "samey", "taim", "clock", "watch", "what time",
          "show time", "tell me time", "time kya", "kya time", "kitna baja",
          "kitne baje", "abhi kitna", "abhi time", "समय", "टाइम", "घड़ी",
          "घडी", "कितना बजा", "कितने बजे", "अभी समय", "क्या समय", "समय बताओ",
          "टाइम बताओ"});
  if (!isTime)
    return false;
  Serial.println("[CMD] time -> show clock");
  showClockScreen = true;
  soundClock();
  if (!timeSynced && WiFi.status() == WL_CONNECTED)
    syncTimeFromNTP();
  uint32_t t0 = millis();
  while (millis() - t0 < 6000) {
    if (cancelRequest || touchT.state)
      break;
    pumpWeb();
    maintainWiFi();
    pumpUI();
    delay(10);
  }
  showClockScreen = false;
  return true;
}

bool handleWeatherCommand(const String &qIn) {
  String q = qIn;
  q.toLowerCase();
  bool isWeather = hasAny(
      q, {"weather", "temperature", "temp", "mausam", "mosam", "मौसम",
          "तापमान", "barish", "rain", "forecast", "taapmaan", "weather kya",
          "kya mausam", "aaj ka mausam", "kitni garmi", "kitni thand",
          "how is the weather", "current weather"});
  if (!isWeather)
    return false;
  Serial.println("[CMD] weather requested");
  setEmotion(EMOTION_THINKING);
  if (millis() - lastWeatherFetchAt > 15 * 60 * 1000 || weatherCode < 0) {
    fetchWeather();
  }
  showWeatherScreen = true;
  soundPetSparkle();

  String voiceReply;
  if (sttLang == "hi" || q.indexOf("mausam") >= 0 || q.indexOf("मौसम") >= 0) {
    voiceReply = weatherCity + " mein mausam " + weatherDesc + " hai, aur taapmaan " +
                 String((int)round(weatherTemp)) + " degree Celsius hai.";
  } else {
    voiceReply = "The weather in " + weatherCity + " is " + weatherDesc +
                 " and " + String((int)round(weatherTemp)) + " degrees Celsius.";
  }
  setEmotion(EMOTION_SPEAKING);
  speakText(voiceReply);

  uint32_t t0 = millis();
  while (millis() - t0 < 5000) {
    if (cancelRequest || touchT.state)
      break;
    pumpWeb();
    maintainWiFi();
    pumpUI();
    delay(10);
  }
  showWeatherScreen = false;
  setEmotion(EMOTION_HAPPY);
  returnIdleAt = millis() + 1500;
  return true;
}

bool handleEmotionCommand(const String &qIn) {
  String q = qIn;
  q.toLowerCase();
  bool isCommand = hasAny(
      q, {"face", "फेस", "चेहरा", "चेहरे", "एक्सप्रेशन", "expression", "बना",
          "bana", "दिखा", "dikha", "show", "make", "look", "हो जा", "ho ja",
          "ho jao", "हो जाओ", "कर दो", "kar do"});
  if (!isCommand)
    return false;
  if (hasAny(q, {"happy", "हैप्पी", "खुश", "khush", "smile", "स्माइल"})) {
    showEmotionFor(EMOTION_HAPPY);
    return true;
  }
  if (hasAny(q, {"sad", "सैड", "उदास", "udaas", "दुखी", "dukhi"})) {
    showEmotionFor(EMOTION_SAD);
    return true;
  }
  if (hasAny(q, {"angry", "एंग्री", "गुस्सा", "gussa", "naraz"})) {
    showEmotionFor(EMOTION_ANGRY);
    return true;
  }
  if (hasAny(q, {"excited", "एक्साइटेड", "उत्साहित", "utsahit"})) {
    showEmotionFor(EMOTION_EXCITED);
    return true;
  }
  if (hasAny(q, {"confus", "कन्फ्यूज", "उलझ", "ulajh"})) {
    showEmotionFor(EMOTION_CONFUSED);
    return true;
  }
  if (hasAny(q, {"sleep", "स्लीप", "सो जा", "so ja", "नींद", "neend"})) {
    showEmotionFor(EMOTION_SLEEPING, 6000);
    return true;
  }
  if (hasAny(q, {"think", "थिंक", "सोच", "soch"})) {
    showEmotionFor(EMOTION_THINKING);
    return true;
  }
  if (hasAny(q, {"love", "लव", "प्यार", "pyar", "pyaar", "dil"})) {
    showPetMood(PET_LOVE);
    return true;
  }
  if (hasAny(q, {"laugh", "लाफ", "हंस", "hans", "giggle"})) {
    showPetMood(PET_GIGGLE);
    return true;
  }
  if (hasAny(q, {"shy", "शाई", "शर्मा", "sharma", "sharam"})) {
    showPetMood(PET_SHY);
    return true;
  }
  if (hasAny(q, {"sparkle", "स्पार्कल", "चमक", "chamak", "shine"})) {
    showPetMood(PET_SPARKLE);
    return true;
  }
  if (hasAny(q, {"kiss", "किस", "चुम्मा", "chumma", "wink"})) {
    showPetMood(PET_KISS);
    return true;
  }
  if (hasAny(q, {"cool", "कूल", "chashma", "चश्मा", "sunglass", "shades"})) {
    showEmotionFor(EMOTION_COOL);
    return true;
  }
  if (hasAny(q, {"evil", "devil", "शैतान", "shaitan", "rakshas"})) {
    showEmotionFor(EMOTION_EVIL);
    return true;
  }
  if (hasAny(q, {"cry", "crying", "रो", "rona", "ro mat", "aansu"})) {
    showEmotionFor(EMOTION_CRYING);
    return true;
  }
  if (hasAny(q, {"heart", "hearts", "dil", "दिल"})) {
    showEmotionFor(EMOTION_HEARTS);
    return true;
  }
  if (hasAny(q, {"dead", "मर", "mar gaya", "khatam"})) {
    showEmotionFor(EMOTION_DEAD);
    return true;
  }
  if (hasAny(q, {"party", "पार्टी", "celebrat", "jashn"})) {
    showEmotionFor(EMOTION_PARTY);
    return true;
  }
  if (hasAny(q, {"sing", "गाना", "gana", "music", "गीत"})) {
    showEmotionFor(EMOTION_SINGING);
    return true;
  }
  if (hasAny(q, {"shock", "शॉक", "hairan", "hairat"})) {
    showEmotionFor(EMOTION_SHOCKED);
    return true;
  }
  if (hasAny(q, {"nervous", "नर्वस", "ghabra"})) {
    showEmotionFor(EMOTION_NERVOUS);
    return true;
  }
  if (hasAny(q, {"cute", "क्यूट", "pyara", "प्यारा", "puppy"})) {
    showEmotionFor(EMOTION_CUTE);
    return true;
  }
  return false;
}

bool handleSkinCommand(const String &qIn) {
  String q = qIn;
  q.toLowerCase();
  if (!hasAny(q, {"skin", "eyes", "eye skin", "looks", "chashma", "theme"}))
    return false;

  String name = "";
  if (hasAny(q, {"cyberpunk", "cyber", "hud", "sci-fi"})) {
    currentEyeSkin = SKIN_CYBERPUNK; name = "Cyberpunk Visor";
  } else if (hasAny(q, {"cat", "neko", "billi", "kitty"})) {
    currentEyeSkin = SKIN_CAT; name = "Kawaii Cat";
  } else if (hasAny(q, {"retro", "8bit", "8-bit", "pixel", "arcade"})) {
    currentEyeSkin = SKIN_RETRO8BIT; name = "Retro 8-Bit";
  } else if (hasAny(q, {"anime", "star", "shounen", "starry"})) {
    currentEyeSkin = SKIN_ANIME; name = "Anime Star";
  } else if (hasAny(q, {"heart", "hearts", "dil", "love"})) {
    currentEyeSkin = SKIN_HEARTS; name = "Heart Eyes";
  } else if (hasAny(q, {"steampunk", "goggles", "aviator"})) {
    currentEyeSkin = SKIN_STEAMPUNK; name = "Steampunk Goggles";
  } else if (hasAny(q, {"demon", "devil", "horns", "shaitan"})) {
    currentEyeSkin = SKIN_DEMON; name = "Demon Horns";
  } else if (hasAny(q, {"matrix", "terminal", "code", "hacker"})) {
    currentEyeSkin = SKIN_MATRIX; name = "Matrix Code";
  } else if (hasAny(q, {"cyborg", "terminator", "t800", "t-800"})) {
    currentEyeSkin = SKIN_CYBORG; name = "Cyborg T-800";
  } else if (hasAny(q, {"mecha", "gundam", "robot eyes", "v-fin"})) {
    currentEyeSkin = SKIN_MECHA; name = "Mecha Gundam";
  } else if (hasAny(q, {"sunglass", "sunglasses", "shades", "thug life", "boss"})) {
    currentEyeSkin = SKIN_SUNGLASSES; name = "Boss Shades";
  } else if (hasAny(q, {"puppy", "dog", "doggy", "kutta"})) {
    currentEyeSkin = SKIN_PUPPY; name = "Kawaii Puppy";
  } else if (hasAny(q, {"hypno", "spiral", "vortex", "dizzy"})) {
    currentEyeSkin = SKIN_HYPNO; name = "Hypno Spiral";
  } else if (hasAny(q, {"alien", "ufo", "cosmic", "galaxy"})) {
    currentEyeSkin = SKIN_ALIEN; name = "Cosmic Alien";
  } else if (hasAny(q, {"ninja", "shinobi", "naruto", "headband"})) {
    currentEyeSkin = SKIN_NINJA; name = "Shinobi Ninja";
  } else if (hasAny(q, {"round", "circle", "gol", "rounded"})) {
    currentEyeSkin = SKIN_ROUND; name = "Round Vector";
  } else if (hasAny(q, {"kiwi", "desk buddy", "buddy", "relaxed eyes", "bored eyes", "block eyes"})) {
    currentEyeSkin = SKIN_KIWI; name = "Desk Buddy (Cute Block)";
  } else if (hasAny(q, {"default", "normal", "clean", "reset skin"})) {
    currentEyeSkin = SKIN_DEFAULT; name = "Default Eyes";
  } else {
    return false;
  }

  saveConfig();
  soundPetSparkle();
  speakText("Eye skin set to " + name + "!");
  setEmotion(EMOTION_HAPPY);
  returnIdleAt = millis() + 1500;
  return true;
}

// ============================================================
// QUESTION & AI INTERACTION PIPELINE
// ============================================================
void handleQuestion(const String &q) {
  lastUserInteractionAt = millis();
  Serial.print("[ROBOT] Q: ");
  Serial.println(q);
  if (cancelRequest)
    return;
  if (handleTimeCommand(q))
    return;
  if (handleWeatherCommand(q))
    return;
  if (handleSkinCommand(q))
    return;
  if (handleEmotionCommand(q))
    return;
  if (hasAny(q, {"new topic", "naya topic", "naya sawal", "reset chat", "clear memory", "bhool jao", "forget that", "forget everything"})) {
    clearConversationHistory();
    showEmotionFor(EMOTION_CONFUSED);
    speakText("Theek hai, sab bhool gaya. Naya sawal poocho!");
    return;
  }

  setEmotion(EMOTION_THINKING);
  if (cancelRequest)
    return;
  lastTranscribedQuestion = q;
  robotStateDesc = "Thinking...";
  addTerminalLog("[LLM] Sending to " + (llmProvider == "groq" ? groqModel : llmModel) + ": \"" + q + "\"");
  String answer = askLLM(q);
  if (cancelRequest)
    return;
  if (!answer.length()) {
    robotStateDesc = "LLM Empty / Error";
    addTerminalLog("[LLM] Empty response or error received");
    Serial.println("[ROBOT] LLM empty");
    showStatus("LLM error", "check key/model", 2000);
    returnIdleAt = millis() + 500;
    return;
  }

  Serial.print("[ROBOT] A: ");
  Serial.println(answer);
  lastLlmAnswer = answer;
  robotStateDesc = "Speaking...";
  addTerminalLog("[LLM] Answer: \"" + answer + "\"");
  setEmotion(EMOTION_SPEAKING);
  if (cancelRequest)
    return;

  bool ok = speakText(answer);
  if (cancelRequest)
    return;
  if (!ok) {
    robotStateDesc = "TTS Failed";
    addTerminalLog("[TTS] Playback error or offline");
    Serial.println("[ROBOT] TTS failed");
    showStatus("TTS error", "see Serial log", 2000);
    returnIdleAt = millis() + 500;
    return;
  }
  robotStateDesc = "Ready";
  addTerminalLog("[ROBOT] Turn completed. Ready.");
  setEmotion(EMOTION_HAPPY);
  returnIdleAt = millis() + 1200;
}

// ============================================================
// MAIN SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println();
  Serial.println("==========================================");
  Serial.println("  MYROBOT (KIWI) - MODULAR ARCHITECTURE   ");
  Serial.println("==========================================");

  randomSeed(esp_random());
  initBase64Lookup();



  if (!initDisplay()) {
    Serial.println("[OLED] FAILED to initialize");
  } else {
    Serial.println("[OLED] Display ready");
  }
  initAnimations();

  initTouch();
  loadConfig();
  resetBoredomInterval();
  initMemory();

  Serial.printf("[LLM] %s / %s\n", llmProvider.c_str(),
                llmProvider == "groq" ? groqModel.c_str() : llmModel.c_str());
  Serial.printf("[STT] %s / %s\n", sttProvider.c_str(), sttLang.c_str());
  Serial.printf("[TTS] %s\n", ttsProvider.c_str());
  Serial.printf("[VOL] %d%%\n", speakerVolume);

  eyesInit(millis());
  setEmotion(EMOTION_IDLE);
  drawConnectingFrame(wifiSSID.c_str(), 0);

  bool connected = false;
  if (wifiSSID.length())
    connected = connectHome();
  if (connected) {
    startAPParallel();
    if (syncTimeFromNTP())
      Serial.println("[NTP] Time Synced");
    fetchWeather();
    if (MDNS.begin("myrobot")) {
      MDNS.addService("http", "tcp", 80);
      Serial.println("[MDNS] http://myrobot.local");
    }
  } else {
    startAP();
  }

  startWeb();
  Serial.printf("[NET] Heap: %u\n", (unsigned)ESP.getFreeHeap());
  showBootInfo(4000);

  if (!startAudio()) {
    setEmotion(EMOTION_ERROR);
    Serial.println("[I2S] Audio initialization FAILED");
  } else {
    detectMicSlot();
    playBootChime();
    Serial.printf("[I2S] Audio OK, heap=%u\n", (unsigned)ESP.getFreeHeap());
  }
  lastUserInteractionAt = millis();
}

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

// ============================================================
// MAIN EVENT LOOP
// ============================================================
void loop() {
  pumpWeb();
  maintainWiFi();

  checkCircadianSleep();
  updateTouch();
  checkAutonomousBoredom();
  updateIdleVariety();
  tickFace();
  maybeChangeEmotion();
  updateTonePlayer();
  processPendingSound();

  if (webQueuedQuestion.length() > 0 && !busyProcessing) {
    String q = webQueuedQuestion;
    webQueuedQuestion = "";
    busyProcessing = true;
    cancelRequest = false;
    lastTranscribedQuestion = q;
    addTerminalLog("[WEB] Prompt received: \"" + q + "\"");
    setEmotion(EMOTION_THINKING);
    handleQuestion(q);
    bool wasCancelled = cancelRequest;
    cancelRequest = false;
    setEmotion(EMOTION_IDLE);
    resyncTouch();
    busyProcessing = false;
    if (wasCancelled) {
      startRecording = true;
    }
  }

  if (startRecording && !busyProcessing) {
    startRecording = false;
    busyProcessing = true;
    cancelRequest = false;
    robotStateDesc = "Listening...";
    addTerminalLog("[REC] Listening for up to " + String(RECORD_SECONDS) +
                   " seconds; stops after speech ends...");

    setEmotion(EMOTION_LISTENING);

    if (WiFi.status() != WL_CONNECTED) {
      robotStateDesc = "Wi-Fi Not Connected";
      addTerminalLog("[REC] Error: Wi-Fi not connected");
      Serial.println("[ROBOT] Top tap: Wi-Fi not connected!");
      setEmotion(EMOTION_CONFUSED);
      showStatus("No Wi-Fi!", "Connect via Portal", 2500);
      setEmotion(EMOTION_IDLE);
      resyncTouch();
      busyProcessing = false;
      return;
    }

    SttCfg cfg = resolveStt();
    if (!cfg.key.length()) {
      robotStateDesc = "No STT API Key";
      addTerminalLog("[REC] Error: No STT API Key configured");
      Serial.println("[ROBOT] Top tap: No STT API Key configured!");
      setEmotion(EMOTION_CONFUSED);
      showStatus("No STT API Key", "Add in Web Portal", 2500);
      setEmotion(EMOTION_IDLE);
      resyncTouch();
      busyProcessing = false;
      return;
    }

    robotStateDesc = "Transcribing...";
    addTerminalLog("[STT] Transcribing captured audio with " + sttProvider + " (" + cfg.model + ")...");
    String question = recordAndTranscribe(RECORD_SECONDS);
    if (!cancelRequest && question.length()) {
      lastTranscribedQuestion = question;
      addTerminalLog("[STT] User: \"" + question + "\"");
      handleQuestion(question);
    } else if (!cancelRequest) {
      robotStateDesc = "STT: " + sttFail;
      addTerminalLog("[STT] Speech not recognized: " + sttFail);
      Serial.printf("[ROBOT] STT failed: %s\n", sttFail.c_str());
      if (sttFail == "quiet" || sttFail == "empty" || sttFail == "") {
        showStatus("Didn't hear you", "try again", 1300);
      } else {
        showStatus("STT error", sttFail.c_str(), 2500);
      }
    }
    bool wasCancelled = cancelRequest;
    cancelRequest = false;
    setEmotion(EMOTION_IDLE);
    resyncTouch();
    busyProcessing = false;
    if (wasCancelled) {
      Serial.println("[ROBOT] Cancelled - restarting listen");
      startRecording = true;
    }
  }

  if (returnIdleAt && millis() >= returnIdleAt && !busyProcessing) {
    returnIdleAt = 0;
    if (!showClockScreen && !showWeatherScreen)
      setEmotion(EMOTION_IDLE);
  }

  delay(1);
}
