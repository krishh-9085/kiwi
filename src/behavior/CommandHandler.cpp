#include "behavior/CommandHandler.h"
#include "behavior/ActionExecutor.h"
#include "behavior/HungerEngine.h"
#include "behavior/InteractionManager.h"
#include "config/AppConfig.h"
#include "config/ConfigManager.h"
#include "core/Timing.h"
#include "face/FaceEngine.h"
#include "hardware/AudioEngine.h"
#include "hardware/Battery.h"
#include "hardware/RgbLed.h"
#include "hardware/TouchController.h"
#include "hardware/WakeWordEngine.h"
#include "services/AiService.h"
#include "services/AiTextTools.h"
#include "services/HyperHdrService.h"
#include "services/MemoryService.h"
#include "services/WeatherService.h"
#include "services/WledService.h"
#include "web/WebPortal.h"
#include <Preferences.h>
#include <WiFi.h>

// ============================================================
// LOCAL VOICE COMMAND PARSER - WORD BOUNDARY HELPERS
// ============================================================

static inline bool isBoundaryChar(char c) {
  if (c == '\0' || c == ' ' || c == '\t' || c == '\r' || c == '\n')
    return true;
  unsigned char uc = (unsigned char)c;
  if (uc < 0x20 || (ispunct(uc) != 0))
    return true;
  return false;
}

bool matchWord(const String &s, const char *word) {
  if (!word || !word[0])
    return false;
  int wlen = strlen(word);
  int slen = s.length();
  if (wlen > slen)
    return false;

  int idx = 0;
  while ((idx = s.indexOf(word, idx)) >= 0) {
    bool leftBoundary = (idx == 0) || isBoundaryChar(s[idx - 1]);
    bool rightBoundary = (idx + wlen >= slen) || isBoundaryChar(s[idx + wlen]);
    if (leftBoundary && rightBoundary) {
      return true;
    }
    idx += 1;
  }
  return false;
}

bool hasWord(const String &s, std::initializer_list<const char *> words) {
  for (const char *w : words) {
    if (matchWord(s, w))
      return true;
  }
  return false;
}

bool hasAny(const String &s, std::initializer_list<const char *> words) {
  for (const char *w : words)
    if (s.indexOf(w) >= 0)
      return true;
  return false;
}

// ============================================================
// COMMAND: TIME / CLOCK
// ============================================================
bool handleTimeCommand(const String &qIn) {
  String q = qIn;
  q.toLowerCase();
  q.trim();

  // If query is about a timer or alarm, do NOT intercept as clock!
  if (hasAny(q, {"timer", "countdown", "alarm", "alaram", "टाइमर", "अलार्म",
                 "काउन्टडाउन", "काउंटडाउन"}))
    return false;

  // Negative filter: Prevent non-clock general questions and conversational
  // queries from being hijacked
  if (hasAny(q,
             {"how much time", "kitna time",    "kitna samay",  "time lagta",
              "time lagega",   "time travel",   "first time",   "all time",
              "every time",    "long time",     "take time",    "waste time",
              "pass time",     "speed of",      "history of",   "meaning of",
              "define",        "explain",       "why do",       "why does",
              "why is",        "tell me about", "watch this",   "watch movie",
              "watch video",   "can you watch", "please watch", "smart watch",
              "wrist watch"})) {
    return false;
  }

  // Positive explicit time query phrases
  bool isTime = hasAny(q, {"what time",
                           "what's the time",
                           "whats the time",
                           "what is the time",
                           "what time is it",
                           "tell me time",
                           "tell the time",
                           "show time",
                           "show clock",
                           "current time",
                           "time now",
                           "time right now",
                           "time kya",
                           "kya time",
                           "time kya hua",
                           "time kya hua hai",
                           "kya time hua",
                           "kya time hua hai",
                           "kitna baja",
                           "kitne baje",
                           "kitne baje hain",
                           "kitna baj raha",
                           "kitna baj raha hai",
                           "abhi kitna",
                           "abhi time",
                           "abhi kya time",
                           "samay kya",
                           "kya samay",
                           "samay batao",
                           "time batao",
                           "time bolo",
                           "ghadi dikhao",
                           "time check",
                           "समय क्या",
                           "क्या समय",
                           "समय बताओ",
                           "टाइम बताओ",
                           "टाइम क्या",
                           "कितना बजा",
                           "कितने बजे",
                           "अभी समय"});

  // Short direct queries (e.g. "time", "clock", "samay", "ghadi")
  if (!isTime && q.length() <= 20) {
    isTime = hasWord(q, {"time", "clock", "samay", "samey", "taim", "ghadi"}) ||
             hasAny(q, {"समय", "टाइम", "घड़ी", "घडी"});
  }

  if (!isTime)
    return false;

  Serial.println("[CMD] Fast-path: time -> show clock & speak");
  if (!timeSynced && WiFi.status() == WL_CONNECTED)
    syncTimeFromNTP();

  showClockScreen = true;
  soundClock();

  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 500)) {
    int hour12 = timeinfo.tm_hour % 12;
    if (hour12 == 0)
      hour12 = 12;
    const char *ampm = (timeinfo.tm_hour >= 12) ? "PM" : "AM";
    String timeSpeech;
    if (sttLang == "hi" || containsDevanagari(qIn) ||
        hasAny(q, {"samay", "kya", "baja", "baje", "hua", "समय", "टाइम"})) {
      timeSpeech = "Abhi " + String(hour12) + " bajke " +
                   String(timeinfo.tm_min) + " minute ho rahe hain.";
    } else {
      timeSpeech = "It is " + String(hour12) + ":" +
                   ((timeinfo.tm_min < 10) ? "0" : "") +
                   String(timeinfo.tm_min) + " " + String(ampm) + ".";
    }
    setEmotion(EMOTION_SPEAKING);
    speakText(timeSpeech);
    setEmotion(EMOTION_IDLE);
  } else {
    speakText("Time sync nahi hua hai.");
    setEmotion(EMOTION_IDLE);
  }

  uint32_t t0 = millis();
  while (millis() - t0 < 4000) {
    if (cancelRequest || touchT.state || touchL.state || touchR.state ||
        !showClockScreen) {
      showClockScreen = false;
      resyncTouch();
      break;
    }
    pumpWeb();
    maintainWiFi();
    serviceUi();
    yield();
  }
  showClockScreen = false;
  setEmotion(EMOTION_IDLE);
  returnIdleAt = 0;
  return true;
}

// ============================================================
// COMMAND: BATTERY STATUS FAST-PATH (<50ms)
// ============================================================
bool handleBatteryCommand(const String &qIn) {
  String q = qIn;
  q.toLowerCase();
  q.trim();

  // Negative filter: Prevent science/technology discussions
  if (hasAny(q, {"what is a battery", "history of battery",
                 "how does a battery", "battery car", "battery technology",
                 "battery chemistry", "inverter"})) {
    return false;
  }

  bool isBattery =
      hasAny(q, {"battery", "battery kitni", "battery kitna", "battery status",
                 "battery percent", "battery percentage", "battery level",
                 "charge kitna", "kitna charge", "charging", "battery check",
                 "battery batao", "battery btao", "battery low", "बैटरी", "चार्ज",
                 "बैटरी कितनी", "बैटरी लेवल"});

  if (!isBattery) {
    if (hasWord(q, {"charge", "charging", "battery"}) &&
        (hasWord(q, {"status", "level", "percent", "percentage", "check",
                     "kitni", "kitna", "batao", "btao"}) ||
         q.length() <= 20)) {
      isBattery = true;
    }
  }

  if (!isBattery)
    return false;

  Serial.println("[CMD] Fast-path: battery requested");
  int bp = batteryPercent();
  soundCoin();
  setEmotion(EMOTION_SPEAKING);

  String reply;
  if (bp >= 0) {
    if (sttLang == "hi" || containsDevanagari(qIn) ||
        hasAny(q, {"kitni", "kitna", "batao", "btao", "बैटरी", "चार्ज"})) {
      reply = "Battery abhi " + String(bp) + " percent hai!";
    } else {
      reply = "Battery is at " + String(bp) + " percent!";
    }
  } else {
    // Direct USB power (powered via Type-C cable)
    if (sttLang == "hi" || containsDevanagari(qIn) ||
        hasAny(q, {"kitni", "kitna", "batao", "btao", "बैटरी", "चार्ज"})) {
      reply = "Main direct USB power par chal raha hoon! Full charge hoon!";
    } else {
      reply = "I am running on direct USB power! Fully powered up!";
    }
  }

  setEmotion(EMOTION_SPEAKING);
  speakText(reply);
  setEmotion(EMOTION_IDLE);
  returnIdleAt = 0;
  return true;
}

// ============================================================
// COMMAND: WEATHER
// ============================================================
bool handleWeatherCommand(const String &qIn) {
  String q = qIn;
  q.toLowerCase();
  q.trim();

  // Negative filter: Prevent science, educational, or biology questions from
  // being hijacked
  if (hasAny(q, {"why does it",
                 "why do",
                 "what causes",
                 "how does",
                 "how do",
                 "boiling",
                 "body temperature",
                 "human body",
                 "fever",
                 "sun",
                 "solar",
                 "water boils",
                 "acid rain",
                 "rainforest",
                 "rain water",
                 "process of",
                 "explain",
                 "definition",
                 "thermodynamics",
                 "difference between",
                 "cpu temp",
                 "gpu temp"})) {
    return false;
  }

  // Strong weather triggers
  bool isWeather = hasAny(
      q, {"weather", "mausam", "mosam", "मौसम", "forecast", "aaj ka mausam",
          "how is the weather", "current weather", "outside weather",
          "weather update", "weather report", "weather forecast", "वेदर",
          "आज का वेदर", "वेदर क्या", "मौसम कैसा", "weather kya", "kya mausam"});

  // Conditional triggers: temp, temperature, barish, rain, garmi, thand
  if (!isWeather) {
    if (hasWord(q, {"temp", "temperature", "taapmaan", "barish", "rain",
                    "garmi", "thand"}) ||
        hasAny(q, {"तापमान"})) {
      if (hasWord(q, {"outside", "today", "aaj", "bahar", "current", "now",
                      "city", "forecast", "degree", "here"}) ||
          q.length() <= 25) {
        isWeather = true;
      }
    }
  }

  if (!isWeather)
    return false;

  Serial.println("[CMD] weather requested");
  setEmotion(EMOTION_THINKING);
  if (millis() - lastWeatherFetchAt > 15 * 60 * 1000 || weatherCode < 0) {
    fetchWeather();
  }
  showWeatherScreen = true;
  soundWeather();

  String voiceReply;
  if (sttLang == "hi" || containsDevanagari(qIn) ||
      hasAny(q, {"mausam", "मौसम", "वेदर"})) {
    voiceReply = weatherCity + " mein mausam " + weatherDesc +
                 " hai, aur taapmaan " + String((int)round(weatherTemp)) +
                 " degree Celsius hai.";
  } else {
    voiceReply = "The weather in " + weatherCity + " is " + weatherDesc +
                 " and " + String((int)round(weatherTemp)) +
                 " degrees Celsius.";
  }
  setEmotion(EMOTION_SPEAKING);
  speakText(voiceReply);
  setEmotion(EMOTION_IDLE);

  uint32_t t0 = millis();
  while (millis() - t0 < 5000) {
    if (cancelRequest || touchT.state || touchL.state || touchR.state ||
        !showWeatherScreen) {
      showWeatherScreen = false;
      resyncTouch();
      break;
    }
    pumpWeb();
    maintainWiFi();
    serviceUi();
    yield();
  }
  showWeatherScreen = false;
  setEmotion(EMOTION_IDLE);
  returnIdleAt = 0;
  return true;
}

// ============================================================
// COMMAND: EMOTIONS
// ============================================================
bool handleEmotionCommand(const String &qIn) {
  String q = qIn;
  q.toLowerCase();
  q.trim();

  // ============================================================
  // SPECIAL HIGH-PRIORITY ACTION COMMANDS: CRY & LAUGH
  // Checked FIRST before any negative filter so "tell me a laugh" or "can you
  // laugh" always trigger the pure laughing action without talking!
  // ============================================================
  auto executeCryAction = [&]() -> bool {
    Serial.println("[CMD] Crying action requested ('cry / ro kr dikhao')");
    wakeWordSuppressUntil(millis() + 6500UL);
    bargeInTriggered = false;
    cancelRequest = false;
    startRecording = false;

    setEmotion(EMOTION_CRYING);
    uint32_t t0 = millis();
    const uint32_t CRY_TARGET_MS = 4800UL;
    returnIdleAt = t0 + CRY_TARGET_MS;
    emotionAutoReturnAt = returnIdleAt;
    soundCrying();

    while (millis() - t0 < CRY_TARGET_MS) {
      serviceUi();
      bargeInTriggered = false;
      cancelRequest = false;
      startRecording = false;
      yield();
    }

    toneStop();
    spkAbortSpeech();
    spkEndSpeech();

    setEmotion(EMOTION_IDLE);
    returnIdleAt = 0;
    emotionAutoReturnAt = 0;

    bargeInTriggered = false;
    cancelRequest = false;
    startRecording = false;
    wakeWordReset();
    preWakeReset();
    wakeWordSuppressUntil(millis() + 1500UL);
    return true;
  };

  auto executeLaughAction = [&]() -> bool {
    Serial.println("[CMD] Laughing action requested ('laugh / haske dikhao') "
                   "-> 5s clean vocal laugh");

    // Suppress wake word and barge-in for full 5s laugh duration + 1.5s post-laugh echo guard
    wakeWordSuppressUntil(millis() + 6500UL);
    bargeInTriggered = false;
    cancelRequest = false;
    startRecording = false;

    setEmotion(EMOTION_LAUGHING);
    uint32_t t0 = millis();
    const uint32_t LAUGH_TARGET_MS = 5000UL; // Strictly 5 seconds!
    returnIdleAt = t0 + LAUGH_TARGET_MS;
    emotionAutoReturnAt = returnIdleAt;

    bool spoke = false;
    // Only attempt cloud TTS if Fish Audio is specifically configured with an API key
    if ((ttsProvider == "fish" || ttsProvider == "fishaudio") &&
        fishKey.length() > 0 && WiFi.status() == WL_CONNECTED) {
      // Short, natural laughing cue without repetitive syllables that cause Fish Audio runaway loops
      spoke = speakText("[laughing] Hahaha! Haha!");
    }

    if (!spoke) {
      // Primary / Offline high-fidelity robot synthesizer laughter: 4-phase baby/robot giggle
      soundLaughing();
    }

    // Keep laughing face animated until exactly 5000ms is reached
    bool secondGigglePlayed = false;
    while (millis() - t0 < LAUGH_TARGET_MS) {
      // Periodic UI and animation pump so the squinting/smiling face bounces smoothly
      serviceUi();

      // If playing on-device synth, trigger a second playful giggle burst at ~2.2s for a rich 5s laugh
      if (!spoke && !secondGigglePlayed && (millis() - t0 >= 2200UL)) {
        soundPetGiggle();
        secondGigglePlayed = true;
      }

      // Neutralize any spurious barge-in / cancel flags from mic during speaker playback
      bargeInTriggered = false;
      cancelRequest = false;
      startRecording = false;

      yield();
    }

    // Clean stop at exactly 5 seconds
    toneStop();
    spkAbortSpeech();
    spkEndSpeech();

    setEmotion(EMOTION_IDLE);
    returnIdleAt = 0;
    emotionAutoReturnAt = 0;

    // Reset audio queues and protect against echo
    bargeInTriggered = false;
    cancelRequest = false;
    startRecording = false;
    wakeWordReset();
    preWakeReset();
    wakeWordSuppressUntil(millis() + 1500UL);

    Serial.println("[CMD] Laughing action completed cleanly after 5s.");
    return true;
  };

  bool isCryReq =
      hasAny(q, {"ro kr",      "ro kar",    "ro ke",  "ro k ",   "rokar",
                 "roke",       "rone ki",   "ro do",  "ro lo",   "rona dikha",
                 "rona shuru", "rone lago", "ro pad", "ro pado", "cry",
                 "crying",     "rona",      "रोना",   "रो के",    "रो कर",
                 "रोकर",       "रोने की",    "रो दो",  "रो पड़",   "रो पड़ो",
                 "रो"}) ||
      hasWord(q, {"cry", "crying", "sob", "sobbing", "weep", "tears", "rona",
                  "ro"});

  if (isCryReq) {
    return executeCryAction();
  }

  bool isLaughReq =
      hasAny(q, {"haske",       "has ke",      "hasske",      "hass ke",
                 "has kr",      "has k ",      "hass kr",     "hass k ",
                 "haskar",      "has kar",     "hasskar",     "hass kar",
                 "has karke",   "hass karke",  "hanske",      "hans ke",
                 "hans kr",     "hans k ",     "hanskar",     "hans kar",
                 "hans karke",
                 "has ke dikhao", "hass ke dikhao", "haske dikhao", "hans ke dikhao", "hanske dikhao",
                 "has ke dikha",  "hass ke dikha",  "haske dikha",  "hans ke dikha",  "hanske dikha",
                 "has kar dikhao","hass kar dikhao","haskar dikhao",
                 "haste",       "hanste",      "haste hue",   "hanste hue",
                 "haste raho",  "hasne",       "hansne",      "hasne ki",
                 "hansne ki",   "hasne lago",  "hasne wala",  "hasna",
                 "hansna",      "hasna dikhao","hassna dikhao","hansna dikhao",
                 "hasna sunao", "haso",        "hanso",       "hasso",
                 "haso na",     "hanso na",    "hasso na",    "thoda haso",
                 "thoda hasso", "thoda hanso", "zara haso",   "zara hasso",
                 "has do",      "hans do",     "hass do",     "has do na",
                 "has lo",      "hans lo",     "hasao",       "hansao",
                 "mujhe hasao", "hasi",        "hansi",       "hasi sunao",
                 "hansi sunao", "hasi dikhao", "hansi dikhao",
                 "thoda has",   "thoda hass",  "thoda hans",
                 "zara has",    "zara hass",   "zara hans",
                 "ek baar has", "ek baar hass","ek baar hans",
                 "laugh for me","can you laugh","please laugh","make me laugh",
                 "laugh",       "laughing",    "laughter",    "giggle",
                 "giggling",    "chuckle",
                 "हंस के दिखाओ",  "हंस के दिखा",   "हंसकर दिखाओ",
                 "हस के दिखाओ",   "हस के दिखा",    "हसकर दिखाओ",
                 "हंस के",        "हंस कर",
                 "हंसकर",        "हंसते",
                 "हंसने",         "हंसना",
                 "हंसी",         "हंस दो",
                 "हंसो",         "हंस",
                 "हस के",        "हस कर",
                 "हसकर",        "हसते",
                 "हसने",         "हसना",
                 "हसी",         "हस दो",
                 "हसो",         "हस"}) ||
      hasWord(q, {"laugh", "laughing", "laughter", "giggle", "giggling",
                  "chuckle", "haso", "hanso", "hasso", "hasna", "hansna",
                  "hasi", "hansi", "hass", "hans", "haste", "hanste"});

  if (isLaughReq) {
    return executeLaughAction();
  }

  // Negative filter: Reject questions, stories, recipes, informational requests
  // for other emotions
  if (hasAny(q, {"why", "how", "who", "what", "explain", "tell me", "recipe",
                 "make a ", "make an ", "story", "difference"}))
    return false;

  bool isCommand = hasWord(q, {"face", "expression", "bana", "banao", "dikha",
                               "dikhao", "show", "make", "look", "ho ja",
                               "ho jao", "kar do", "karo"}) ||
                   hasAny(q, {"फेस", "चेहरा", "चेहरे", "एक्सप्रेशन", "बना", "दिखा",
                              "हो जा", "हो जाओ", "कर दो", "करो"});

  if (!isCommand)
    return false;

  if (hasWord(q, {"happy", "smile"}) ||
      hasAny(q, {"हैप्पी", "खुश", "khush", "स्माइल"})) {
    showEmotionFor(EMOTION_HAPPY);
    return true;
  }
  if (hasWord(q, {"sad"}) ||
      hasAny(q, {"सैड", "उदास", "udaas", "दुखी", "dukhi"})) {
    showEmotionFor(EMOTION_SAD);
    return true;
  }
  if (hasWord(q, {"angry"}) || hasAny(q, {"एंग्री", "गुस्सा", "gussa", "naraz"})) {
    showEmotionFor(EMOTION_ANGRY);
    return true;
  }
  if (hasWord(q, {"excited"}) || hasAny(q, {"एक्साइटेड", "उत्साहित", "utsahit"})) {
    showEmotionFor(EMOTION_EXCITED);
    return true;
  }
  if (hasWord(q, {"confused", "confuse"}) ||
      hasAny(q, {"confus", "कन्फ्यूज", "उलझ", "ulajh"})) {
    showEmotionFor(EMOTION_CONFUSED);
    return true;
  }
  if (hasWord(q, {"sleep", "sleeping"}) ||
      hasAny(q, {"स्लीप", "सो जा", "so ja", "नींद", "neend"})) {
    showEmotionFor(EMOTION_SLEEPING, 6000);
    return true;
  }
  if (hasWord(q, {"think", "thinking"}) || hasAny(q, {"थिंक", "सोच", "soch"})) {
    showEmotionFor(EMOTION_THINKING);
    return true;
  }
  if (hasWord(q, {"love"}) ||
      hasAny(q, {"लव", "प्यार", "pyar", "pyaar", "dil"})) {
    showPetMood(PET_LOVE);
    return true;
  }
  if (hasWord(q, {"laugh", "laughing", "giggle", "haso", "hanso", "hasso", "hans", "hass"}) ||
      hasAny(q, {"लाफ", "हंस", "hans", "हस", "हसो", "हंसो"})) {
    return executeLaughAction();
  }
  if (hasWord(q, {"shy"}) || hasAny(q, {"शाई", "शर्मा", "sharma", "sharam"})) {
    showPetMood(PET_SHY);
    return true;
  }
  if (hasWord(q, {"sparkle", "shine"}) ||
      hasAny(q, {"स्पार्कल", "चमक", "chamak"})) {
    showPetMood(PET_SPARKLE);
    return true;
  }
  if (hasWord(q, {"kiss", "wink"}) || hasAny(q, {"किस", "चुम्मा", "chumma"})) {
    showPetMood(PET_KISS);
    return true;
  }
  if (hasWord(q, {"cool", "shades", "sunglass"}) ||
      hasAny(q, {"कूल", "chashma", "चश्मा"})) {
    showEmotionFor(EMOTION_COOL);
    return true;
  }
  if (hasWord(q, {"evil", "devil"}) ||
      hasAny(q, {"शैतान", "shaitan", "rakshas"})) {
    showEmotionFor(EMOTION_EVIL);
    return true;
  }
  if (hasWord(q, {"cry", "crying", "sob", "ro", "rona"}) ||
      hasAny(q, {"रोना", "rona", "ro mat", "aansu", "ro"})) {
    setEmotion(EMOTION_CRYING);
    returnIdleAt = millis() + 5200;
    emotionAutoReturnAt = returnIdleAt;
    soundCrying();
    holdFace(5200);
    setEmotion(EMOTION_IDLE);
    returnIdleAt = 0;
    emotionAutoReturnAt = 0;
    return true;
  }
  if (hasWord(q, {"heart", "hearts"}) || hasAny(q, {"dil", "दिल"})) {
    showEmotionFor(EMOTION_HEARTS);
    return true;
  }
  if (hasWord(q, {"dead"}) || hasAny(q, {"मर", "mar gaya", "khatam"})) {
    showEmotionFor(EMOTION_DEAD);
    return true;
  }
  if (hasWord(q, {"party", "celebrate", "celebrating"}) ||
      hasAny(q, {"पार्टी", "celebrat", "jashn"})) {
    showEmotionFor(EMOTION_PARTY);
    return true;
  }
  if (hasWord(q, {"sing", "singing", "music"}) ||
      hasAny(q, {"गाना", "gana", "गीत"})) {
    showEmotionFor(EMOTION_SINGING);
    return true;
  }
  if (hasWord(q, {"shock", "shocked"}) ||
      hasAny(q, {"शॉक", "hairan", "hairat"})) {
    showEmotionFor(EMOTION_SHOCKED);
    return true;
  }
  if (hasWord(q, {"nervous"}) || hasAny(q, {"नर्वस", "ghabra"})) {
    showEmotionFor(EMOTION_NERVOUS);
    return true;
  }
  if (hasWord(q, {"cute", "puppy"}) || hasAny(q, {"क्यूट", "pyara", "प्यारा"})) {
    showEmotionFor(EMOTION_CUTE);
    return true;
  }
  return false;
}

// ============================================================
// COMMAND: SKINS / THEMES
// ============================================================
bool handleSkinCommand(const String &qIn) {
  String q = qIn;
  q.toLowerCase();
  q.trim();

  // Negative filter: Reject stories, general questions, movie/animal
  // discussions
  if (hasAny(q, {"tell me", "story", "what is", "who is", "why", "explain",
                 "how to", "movie", "video", "breed", "animal"}))
    return false;

  // Strict Negative filter: Never hijack light, LED, volume, timer, or battery
  // commands!
  if (hasWord(q, {"led", "leds", "light", "lights", "bulb", "lamp", "volume",
                  "timer", "alarm", "battery"}) ||
      hasAny(q, {"बत्ती", "लाइट", "एलईडी", "आवाज़", "वॉल्यूम"})) {
    return false;
  }

  bool isThemeIntent = hasWord(q, {"skin", "eyes", "eye skin", "looks", "theme",
                                   "look", "face"}) ||
                       hasAny(q, {"थीम", "स्किन", "आंखें", "चश्मा", "चेहरा"});

  String name = "";
  EyeSkin targetSkin = currentEyeSkin;

  if (hasWord(q, {"kiwi", "desk buddy", "deskbuddy", "buddy", "cute"}) ||
      hasAny(q, {"कीवी"})) {
    targetSkin = SKIN_KIWI;
    name = "Kiwi";
  } else if (hasWord(q, {"pixel", "pixel 8-bit", "arcade", "retro", "8bit",
                         "8-bit"}) ||
             hasAny(q, {"रेट्रो", "पिक्सेल"})) {
    targetSkin = SKIN_PIXEL;
    name = "Pixel 8-Bit";
  } else if (hasAny(q, {"lumina spark", "squircle glint", "spark", "lumina",
                        "glint"})) {
    targetSkin = SKIN_LUMINA_SPARK;
    name = "Lumina Spark";
  } else if (hasWord(q, {"mecha", "circtangle", "circrect"}) ||
             hasAny(q, {"मेका", "मैका"})) {
    targetSkin = SKIN_MECHA;
    name = "Mecha";
  } else if (hasWord(q, {"emo", "emo-style", "emostyle"}) ||
             hasAny(q, {"emo style", "ईमो", "इमो"})) {
    targetSkin = SKIN_EMO;
    name = "EMO Style";
  }

  // Check for generic "cycle / next / change" eyes command
  bool isCycleCommand =
      isThemeIntent &&
      (hasWord(q, {"change", "next", "switch", "cycle", "toggle", "badlo"}) ||
       hasAny(q, {"eyes change", "eye change", "change eye", "change eyes",
                  "change skin", "skin change", "next skin", "next eyes",
                  "next eye", "aankhein badlo", "aankh badlo",
                  "aankhein change", "aankh change", "dusri skin",
                  "dusri aankhein", "dusra look", "naya look", "theme badlo"}));

  // If user asked to cycle or change eyes without specifying a theme name:
  if (name.length() == 0 && isCycleCommand) {
    targetSkin = (EyeSkin)(((uint8_t)currentEyeSkin + 1) % SKIN_COUNT);
    const char *skinNames[SKIN_COUNT] = {"Kiwi", "Pixel 8-Bit", "Lumina Spark",
                                         "Mecha", "EMO Style"};
    name = skinNames[targetSkin];
  }

  if (name.length() == 0)
    return false;

  // Apply new skin immediately (<10ms!)
  Serial.printf("[CMD] Fast-path: eye skin -> %s\n", name.c_str());
  currentEyeSkin = targetSkin;
  saveConfig();
  soundPetSparkle();
  if (sttLang == "hi" || containsDevanagari(qIn) ||
      hasAny(q, {"rakho", "karo", "lagao", "थीम", "बदलो", "रखो", "लगाओ",
                 "kardo", "aankhein", "aankh"})) {
    speakText("Aankhein badal di! Yeh hai " + name + " skin!");
  } else {
    speakText("Theme set to " + name + "!");
  }
  setEmotion(EMOTION_IDLE);
  returnIdleAt = 0;
  return true;
}

// ============================================================
// COMMAND: VOLUME FAST-PATH (<50ms)
// ============================================================
bool handleVolumeCommand(const String &qIn) {
  String q = qIn;
  q.toLowerCase();
  q.trim();

  // Negative filter: Prevent science/acoustics questions from triggering volume
  if (hasAny(q, {"speed of sound", "sound wave", "sound waves", "ultrasound",
                 "infrasound", "sound of music", "sound quality",
                 "how does sound", "what is sound"})) {
    return false;
  }

  bool hasVolWord = hasWord(q, {"volume", "vol", "aawaz", "awaz", "sound",
                                "loudness", "mute", "audio"}) ||
                    hasAny(q, {"volume", "vol", "aawaz", "awaz", "awaaz",
                               "avaz", "आवाज़", "आवाज", "वॉल्यूम", "म्यूट"});

  // Also support direct "tez bolo", "dheere bolo", "chup ho jao", "shant ho
  // jao"
  bool directIntent = hasAny(q, {"tez bolo", "zor se", "dheere bolo",
                                 "chup ho jao", "shant ho jao", "mute karo",
                                 "mute kar", "sound badhao", "sound kam"});

  if (!hasVolWord && !directIntent)
    return false;

  int targetVol = -1;
  bool isUp = false;
  bool isDown = false;

  if (hasWord(q, {"mute", "silent"}) ||
      hasAny(q, {"chup", "shant", "म्यूट", "शांत", "aawaz band", "sound off"})) {
    targetVol = 0;
  } else if (hasWord(q, {"up", "high", "increase", "louder"}) ||
             hasAny(q, {"badhao", "badha", "badha do", "tez", "zor se", "unche",
                        "बढ़ाओ", "तेज़", "ज्यादा"})) {
    targetVol = min(100, speakerVolume + 20);
    isUp = true;
  } else if (hasWord(q, {"down", "low", "decrease", "softer"}) ||
             hasAny(q, {"kam", "ghatao", "dheere", "dheemi", "dheema", "कम",
                        "घटाओ", "धीमे"})) {
    targetVol = max(10, speakerVolume - 20);
    isDown = true;
  } else if (hasWord(q, {"max", "full", "maximum"}) ||
             hasAny(q, {"pura", "poora", "100%", "100 percent", "पूरा", "फुल"})) {
    targetVol = 100;
    isUp = true;
  } else {
    // Scan for direct number in string (e.g. 80, 50, 75)
    for (int i = 0; i < (int)q.length(); i++) {
      if (isdigit(q[i])) {
        int val = 0;
        while (i < (int)q.length() && isdigit(q[i])) {
          val = val * 10 + (q[i] - '0');
          i++;
        }
        if (val >= 0 && val <= 100) {
          targetVol = val;
          break;
        }
      }
    }
  }

  if (targetVol >= 0) {
    speakerVolume = targetVol;
    saveConfig();

    // Instant acoustic feedback in new volume
    if (targetVol == 0) {
      soundTouchBlip();
    } else if (isUp) {
      soundVolUp();
    } else if (isDown) {
      soundVolDown();
    } else {
      soundPetSparkle();
    }

    String reply;
    if (targetVol == 0) {
      reply = (sttLang == "hi" || containsDevanagari(qIn) ||
               hasAny(q, {"chup", "aawaz", "awaz"}))
                  ? "Kiwi mute ho gaya hai!"
                  : "Muted!";
    } else {
      reply = (sttLang == "hi" || q.indexOf("aawaz") >= 0 ||
               q.indexOf("awaz") >= 0 || q.indexOf("awaaz") >= 0 ||
               containsDevanagari(qIn) ||
               hasAny(q, {"kar", "karo", "badha", "kam", "tez", "dheere"}))
                  ? "Aawaz " + String(speakerVolume) + " percent kar di!"
                  : "Volume set to " + String(speakerVolume) + " percent!";
    }

    setEmotion(EMOTION_SPEAKING);
    speakText(reply);
    setEmotion(EMOTION_IDLE);
    returnIdleAt = 0;
    return true;
  }
  return false;
}

// ============================================================
// COMMAND: TIMER / COUNTDOWN
// ============================================================
bool handleTimerCommand(const String &qIn) {
  String q = qIn;
  q.toLowerCase();
  q.trim();

  if (!hasWord(q, {"timer", "countdown", "alarm", "alaram"}) &&
      !hasAny(q, {"टाइमर", "अलार्म", "काउन्टडाउन", "काउंटडाउन"})) {
    return false;
  }

  // 1. Cancel / Stop Timer or Alarm
  if (hasWord(q, {"stop", "cancel", "off", "dismiss"}) ||
      hasAny(q,
             {"band", "hatao", "rok", "बंद", "रोको", "हटाओ", "कैंसिल", "स्टॉप"})) {
    if (isRobotTimerRinging()) {
      stopRobotTimerAlarm();
      speakText((sttLang == "hi") ? "Alarm band kar diya!" : "Alarm stopped!");
      return true;
    }
    if (isRobotTimerActive()) {
      cancelRobotTimer();
      speakText((sttLang == "hi") ? "Timer cancel kar diya!"
                                  : "Timer cancelled!");
      return true;
    }
    speakText((sttLang == "hi") ? "Koi timer ya alarm chal nahi raha hai."
                                : "No timer or alarm is active.");
    return true;
  }

  // 2. Parse duration
  uint32_t totalSec = 0;
  for (int i = 0; i < (int)q.length(); i++) {
    if (isdigit(q[i])) {
      int val = 0;
      while (i < (int)q.length() && isdigit(q[i])) {
        val = val * 10 + (q[i] - '0');
        i++;
      }
      if (val > 0) {
        if (hasWord(q, {"hour", "hours", "hr", "hrs"}) ||
            hasAny(q, {"ghanta", "ghante", "घंटा", "घंटे"})) {
          totalSec = (uint32_t)val * 3600;
        } else if (hasWord(q, {"minute", "minutes", "min", "mins"}) ||
                   hasAny(q, {"minto", "minut", "minuite", "मिनट", "मिन्ट"})) {
          totalSec = (uint32_t)val * 60;
        } else if (hasWord(q, {"second", "seconds", "sec", "secs"}) ||
                   hasAny(q, {"सेकंड", "सेकेंड"})) {
          totalSec = (uint32_t)val;
        } else {
          // If no unit specified: <= 10 assume minutes, > 10 assume seconds
          totalSec = (val <= 10) ? ((uint32_t)val * 60) : (uint32_t)val;
        }
        break;
      }
    }
  }

  // Word-based numbers (Hindi & English, including Devanagari script)
  if (totalSec == 0) {
    if (hasAny(q, {"aadha ghanta", "half an hour", "half hour", "आधा घंटा"}))
      totalSec = 1800;
    else if (hasAny(q, {"ek minute", "one minute", "a minute", "1 minute",
                        "एक मिनट", "1 मिनट"}))
      totalSec = 60;
    else if (hasAny(q, {"do minute", "two minute", "two minutes", "2 minute",
                        "दो मिनट", "2 मिनट"}))
      totalSec = 120;
    else if (hasAny(q, {"teen minute", "three minute", "three minutes",
                        "3 minute", "तीन मिनट", "3 मिनट"}))
      totalSec = 180;
    else if (hasAny(q, {"chaar minute", "char minute", "four minute",
                        "four minutes", "4 minute", "चार मिनट", "4 मिनट"}))
      totalSec = 240;
    else if (hasAny(q, {"paanch minute", "panch minute", "five minute",
                        "five minutes", "5 minute", "पांच मिनट", "पाँच मिनट",
                        "5 मिनट"}))
      totalSec = 300;
    else if (hasAny(q, {"das minute", "ten minute", "ten minutes", "10 minute",
                        "दस मिनट", "10 मिनट"}))
      totalSec = 600;
    else if (hasAny(q, {"pandrah minute", "fifteen minute", "fifteen minutes",
                        "15 minute", "पंद्रह मिनट", "15 मिनट"}))
      totalSec = 900;
    else if (hasAny(q, {"bees minute", "twenty minute", "20 minute", "बीस मिनट",
                        "20 मिनट"}))
      totalSec = 1200;
    else if (hasAny(q, {"tees minute", "thirty minute", "thirty minutes",
                        "30 minute", "तीस मिनट", "30 मिनट"}))
      totalSec = 1800;
    else if (hasAny(q, {"ek ghanta", "one hour", "1 hour", "एक घंटा", "1 घंटा"}))
      totalSec = 3600;
    else if (hasAny(q, {"tees second", "thirty second", "thirty seconds",
                        "30 second", "तीस सेकंड", "30 सेकंड"}))
      totalSec = 30;
    else if (hasAny(q, {"pandrah second", "fifteen second", "fifteen seconds",
                        "15 second", "पंद्रह सेकंड", "15 सेकंड"}))
      totalSec = 15;
    else if (hasAny(q, {"das second", "ten second", "ten seconds", "10 second",
                        "दस सेकंड", "10 सेकंड"}))
      totalSec = 10;
  }

  if (totalSec > 0) {
    String label = "Timer";
    int forIdx = q.indexOf("for ");
    if (forIdx >= 0) {
      label = q.substring(forIdx + 4);
      int endP = label.indexOf(" timer");
      if (endP >= 0)
        label = label.substring(0, endP);
      label.trim();
    }
    if (label == "Timer" || label.length() == 0) {
      int keLiyeIdx = q.indexOf(" ke liye");
      if (keLiyeIdx > 0) {
        label = q.substring(0, keLiyeIdx);
        int sp = label.lastIndexOf(' ');
        if (sp >= 0)
          label = label.substring(sp + 1);
        label.trim();
      }
    }
    if (label.length() == 0 || label.length() > 20)
      label = "Timer";

    startRobotTimer(totalSec, label);

    String durationDesc;
    if (totalSec >= 60) {
      uint32_t m = totalSec / 60;
      uint32_t s = totalSec % 60;
      if (s > 0) {
        durationDesc = String(m) + (sttLang == "hi" ? " minute " : " minute ") +
                       String(s) + (sttLang == "hi" ? " second" : " seconds");
      } else {
        durationDesc =
            String(m) +
            (sttLang == "hi" ? " minute" : (m == 1 ? " minute" : " minutes"));
      }
    } else {
      durationDesc =
          String(totalSec) + (sttLang == "hi"
                                  ? " second"
                                  : (totalSec == 1 ? " second" : " seconds"));
    }

    String reply = (sttLang == "hi")
                       ? durationDesc + " ka timer shuru kar diya hai!"
                       : "Timer started for " + durationDesc + "!";
    speakText(reply);
    return true;
  }
  return false;
}

// ============================================================
// COMMAND: WLED & HYPERHDR LIGHTS
// ============================================================
bool handleLightCommand(const String &qIn) {
  String q = qIn;
  q.toLowerCase();
  q.trim();

  // Negative filter: Prevent science/optics questions from hijacking light
  // control
  if (hasAny(q, {"speed of light", "traffic light", "traffic lights",
                 "light year", "light years", "sunlight", "tubelight",
                 "what is light", "quantum", "optics"})) {
    return false;
  }

  // ambilight / hyperhdr / monitor / batti / bulb
  bool hasScreenTrigger =
      hasWord(q, {"sync", "ambilight", "hyperhdr", "monitor"}) ||
      hasAny(q, {"screen sync", "pc sync", "screen light", "monitor light"});
  bool hasLightTrigger =
      hasWord(q, {"light", "lights", "led", "leds", "rgb", "bulb", "lamp",
                  "wled", "brightness", "torch", "color", "colour"}) ||
      hasAny(q, {"batti", "roshni", "बत्ती", "रोशनी", "लाइट", "एलईडी", "color",
                 "colour", "onboard"}) ||
      hasScreenTrigger;
  int directFxId = -1;
  String directFxName = "";

  if (!hasLightTrigger) {
    // If "light" is not explicitly mentioned, only match if query has "mode" or
    // "effect" AND matches a valid WLED effect
    if (hasWord(q, {"mode", "effect"})) {
      directFxId = findWledEffectId(q, &directFxName);
      if (directFxId < 0)
        return false;
    } else {
      return false;
    }
  }

  // 0. Screen Sync / Ambilight Controls
  if (hasScreenTrigger) {
    if (hasWord(q, {"off", "close", "stop", "pause"}) ||
        hasAny(q, {"band", "bujha", "turn off", "switch off", "pause"})) {
      if (executeUnifiedLightAction("screensyncoff")) {
        setEmotion(EMOTION_IDLE);
        soundPetSparkle();
        speakText((sttLang == "hi") ? "Screen sync band kar diya!"
                                    : "Screen sync paused!");
        return true;
      }
    } else {
      if (executeUnifiedLightAction("screensync")) {
        setEmotion(EMOTION_SPEAKING);
        soundPetSparkle();
        speakText((sttLang == "hi") ? "Screen sync chalu kar diya!"
                                    : "Screen sync activated!");
        return true;
      }
    }
  }

  // HDR Mode Toggle
  if (hasWord(q, {"hdr"})) {
    bool enableHdr =
        !(hasWord(q, {"off", "close"}) || hasAny(q, {"band", "turn off"}));
    if (executeUnifiedLightAction(enableHdr ? "hdr on" : "hdr off")) {
      soundPetSparkle();
      speakText(enableHdr ? "HDR mode on kar diya!" : "HDR mode off kar diya!");
      return true;
    }
  }

  // 1. Turn OFF
  if (hasWord(q, {"off", "close"}) ||
      hasAny(q, {"band", "bujha", "turn off", "switch off", "close light",
                 "बंद"})) {
    executeUnifiedLightAction("off");
    rgbLedClearOverride();
    rgbLedShow(0, 0, 0);
    setEmotion(EMOTION_IDLE);
    soundPetSparkle();
    speakText((sttLang == "hi" || hasAny(q, {"band", "bujha", "बंद"}))
                  ? "Light band kar di!"
                  : "Lights turned off!");
    return true;
  }

  // 2. Turn ON
  if (hasWord(q, {"on", "open"}) ||
      hasAny(q, {"chalu", "turn on", "switch on", "jalao", "on kar",
                 "open light", "चालू", "जलाओ"})) {
    executeUnifiedLightAction("on");
    rgbLedSetOverride(255, 240, 220);
    setEmotion(EMOTION_IDLE);
    soundPetSparkle();
    speakText((sttLang == "hi" || hasAny(q, {"chalu", "jalao", "चालू", "जलाओ"}))
                  ? "Light on kar di!"
                  : "Lights turned on!");
    return true;
  }

  // 3. Brightness presets
  if (hasWord(q, {"dim", "low", "night", "slight"}) ||
      hasAny(q, {"kam", "dheemi", "kam karo", "कम"})) {
    executeUnifiedLightAction("dim");
    rgbLedSetOverride(60, 40, 20);
    setEmotion(EMOTION_IDLE);
    soundPetSparkle();
    speakText((sttLang == "hi") ? "Light dim kar di!" : "Light dimmed!");
    return true;
  }
  if (hasWord(q, {"bright", "full", "max", "increase"}) ||
      hasAny(q, {"tez", "badhao", "poori", "puri", "तेज़"})) {
    executeUnifiedLightAction("full");
    rgbLedSetOverride(255, 255, 255);
    setEmotion(EMOTION_IDLE);
    soundPetSparkle();
    speakText((sttLang == "hi") ? "Light full bright kar di!"
                                : "Lights set to full brightness!");
    return true;
  }
  if (hasWord(q, {"medium", "half"}) || hasAny(q, {"aadhi", "adha", "आधी"})) {
    executeUnifiedLightAction("medium");
    rgbLedSetOverride(160, 140, 100);
    setEmotion(EMOTION_IDLE);
    soundPetSparkle();
    speakText((sttLang == "hi") ? "Light 50 percent brightness kar di!"
                                : "Light set to 50 percent brightness!");
    return true;
  }

  // 4. Effects / Modes
  // 4a. Next / Change Mode
  if (hasAny(q, {"mode badlo",   "mode badal",    "effect badlo",
                 "effect badal", "change mode",   "next mode",
                 "switch mode",  "koi aur mode",  "aur koi mode",
                 "dusra mode",   "doosra mode",   "agla mode",
                 "next effect",  "aur effect",    "koi aur effect",
                 "dusra effect", "doosra effect", "agla effect",
                 "random mode",  "random effect", "kuch aur dikhao",
                 "aur mode",     "mode change",   "effect change"})) {
    if (isHyperHdrDiscovered()) {
      setHyperHdrLedDevice(false);
    }
    if (setWledNextEffect()) {
      showEmotionFor(EMOTION_PARTY, 2000);
      soundPetSparkle();
      speakText((sttLang == "hi") ? "Light ka agla mode laga diya!"
                                  : "Switched to next light mode!");
      return true;
    }
  }

  // 4b. Find ANY named effect (from all 160+ WLED effects!)
  String effectQuery = q;
  if (hasAny(q, {"aag", "mombatti"}))
    effectQuery += " fire candle";
  if (hasAny(q, {"rang birang", "disco", "party"}))
    effectQuery += " rainbow";
  if (hasAny(q, {"jugnu", "taare", "tare"}))
    effectQuery += " twinkle";
  if (hasAny(q, {"samundar", "lahar", "pani"}))
    effectQuery += " pacifica waves";
  if (hasAny(q, {"bijli", "tufan"}))
    effectQuery += " lightning";
  if (hasAny(q, {"barish"}))
    effectQuery += " rain";
  if (hasAny(q, {"patakha", "diwali"}))
    effectQuery += " fireworks";
  if (hasAny(q, {"suraj", "subah", "dhoop"}))
    effectQuery += " sunrise";
  if (hasAny(q, {"dhadkan"}))
    effectQuery += " heartbeat";
  if (hasAny(q, {"bhoot"}))
    effectQuery += " halloween eyes";
  if (hasAny(q, {"siren"}))
    effectQuery += " police";

  String matchedFx = directFxName;
  int fxId = (directFxId >= 0) ? directFxId
                               : findWledEffectId(effectQuery, &matchedFx);
  if (fxId >= 0) {
    if (isHyperHdrDiscovered()) {
      setHyperHdrLedDevice(false);
    }
    if (setWledEffectById(fxId, matchedFx)) {
      showEmotionFor(EMOTION_PARTY, 2200);
      soundPetSparkle();
      String reply = (sttLang == "hi")
                         ? "Light ka " + matchedFx + " mode laga diya!"
                         : "Light set to " + matchedFx + " mode!";
      speakText(reply);
      return true;
    }
  }

  // 4c. Onboard RGB LED specific controls
  if (hasAny(q, {"onboard", "choti light", "chhoti light", "robot light",
                 "robot ki light", "torch", "flashlight"})) {
    if (hasWord(q, {"off", "close"}) || hasAny(q, {"band", "bujha"})) {
      rgbLedClearOverride();
      rgbLedShow(0, 0, 0);
      soundPetSparkle();
      speakText("Onboard light band kar di!");
      return true;
    } else {
      rgbLedSetOverride(255, 255, 255);
      soundPetSparkle();
      speakText("Onboard light on kar di!");
      return true;
    }
  }

  // 5. Colors (Leaves brightness untouched!)
  struct ColorMatch {
    const char *names[5];
    const char *action;
    uint8_t r, g, b;
    const char *hiReply;
    const char *enReply;
  } colors[] = {{{"red", "laal", nullptr},
                 "red",
                 255,
                 0,
                 0,
                 "Light red kar di!",
                 "Light set to red!"},
                {{"green", "hara", "haree", nullptr},
                 "green",
                 0,
                 255,
                 0,
                 "Light green kar di!",
                 "Light set to green!"},
                {{"blue", "neela", "neeli", nullptr},
                 "blue",
                 0,
                 100,
                 255,
                 "Light blue kar di!",
                 "Light set to blue!"},
                {{"yellow", "peela", "peeli", nullptr},
                 "yellow",
                 255,
                 200,
                 0,
                 "Light yellow kar di!",
                 "Light set to yellow!"},
                {{"pink", "gulabi", nullptr},
                 "pink",
                 255,
                 50,
                 150,
                 "Light pink kar di!",
                 "Light set to pink!"},
                {{"purple", "violet", "baingani", nullptr},
                 "purple",
                 160,
                 0,
                 255,
                 "Light purple kar di!",
                 "Light set to purple!"},
                {{"orange", "narangi", nullptr},
                 "orange",
                 255,
                 100,
                 0,
                 "Light orange kar di!",
                 "Light set to orange!"},
                {{"cyan", "aqua", "firozi", nullptr},
                 "cyan",
                 0,
                 255,
                 255,
                 "Light cyan kar di!",
                 "Light set to cyan!"},
                {{"white", "safed", nullptr},
                 "white",
                 255,
                 255,
                 255,
                 "Light white kar di!",
                 "Light set to white!"},
                {{"warm", "night", nullptr},
                 "warm",
                 255,
                 180,
                 80,
                 "Warm light on kar di!",
                 "Warm light activated!"},
                {{"cool", nullptr},
                 "cool",
                 200,
                 220,
                 255,
                 "Cool white light on kar di!",
                 "Cool white light activated!"},
                {{nullptr}, nullptr, 0, 0, 0, nullptr, nullptr}};

  for (int i = 0; colors[i].action != nullptr; i++) {
    for (int k = 0; colors[i].names[k] != nullptr; k++) {
      if (matchWord(q, colors[i].names[k])) {
        executeUnifiedLightAction(colors[i].action);
        rgbLedSetOverride(colors[i].r, colors[i].g, colors[i].b);
        setEmotion(EMOTION_IDLE);
        soundPetSparkle();
        speakText((sttLang == "hi") ? colors[i].hiReply : colors[i].enReply);
        return true;
      }
    }
  }

  // 6. Status / Discover query
  if (hasWord(q, {"status", "connected", "scan"}) ||
      hasAny(q, {"kahan hai", "dhundo"})) {
    String reply = "";
    bool hhdrOk = discoverHyperHdr(true);
    bool wledOk = discoverWled(true);
    if (hhdrOk) {
      reply += "HyperHDR is connected at " + getHyperHdrIp() + ". ";
    }
    if (wledOk) {
      reply += "WLED light is connected at " + getWledIp() + ".";
    }
    if (!hhdrOk && !wledOk) {
      reply = (sttLang == "hi")
                  ? "Mujhe network par koi light ya HyperHDR nahi "
                    "mila."
                  : "No light or HyperHDR server found on network.";
      showEmotionFor(EMOTION_CONFUSED, 1800);
    } else {
      setEmotion(EMOTION_IDLE);
    }
    speakText(reply);
    return true;
  }

  return false;
}

// ============================================================
// COMMAND: SLEEP MODE FAST-PATH (<50ms)
// ============================================================
bool handleSleepCommand(const String &qIn) {
  String q = qIn;
  q.toLowerCase();
  q.trim();

  // Negative filter: Reject explanations or questions about sleep
  if (hasAny(q, {"why do", "how much", "what is", "explain", "story",
                 "sleeping beauty", "sleepless", "meaning of", "kyun sote",
                 "kyu sote", "mat so", "don't sleep"})) {
    return false;
  }

  bool isSleep = hasAny(
      q, {// Hindi Devanagari
          "सोने जाओ", "सोने जा", "सोने चलो", "सोने चल", "सो जाओ", "सो जा",
          "सो जाना", "अब सो जाओ", "अब सो जा", "तुम सो जाओ", "चलो सो जाओ",
          "नींद आ रही", "शुभ रात्रि", "गुड नाईट", "गुड नाइट", "जाके सो जाओ",
          "जाओ सो जाओ", "सो जाओ ना",
          // Roman / Hinglish
          "so jao", "so ja", "sone jao", "sone ja", "sone chalo", "sone chal",
          "soja", "so jaao", "so jana", "so jane", "ab so jao", "ab so ja",
          "tum so jao", "chalo so jao", "kuch der so", "neend aa rahi",
          "shubh ratri", "shubhratri", "good night", "goodnight", "gud night",
          "gudnight", "sleep mode", "go to sleep", "go to bed", "sleep now",
          "time to sleep", "fall asleep", "take a nap", "night mode"});

  if (!isSleep && q.length() <= 25) {
    isSleep = hasWord(q, {"sleep", "sleeping"}) &&
              (hasWord(q, {"mode", "now", "go", "please", "bed"}) ||
               hasAny(q, {"so ja", "jao", "sone"}));
  }

  if (!isSleep)
    return false;

  Serial.println("[CMD] Fast-path: sleep mode requested");
  soundPetSleepy();
  String reply = (sttLang == "hi" || containsDevanagari(qIn) ||
                  hasAny(q, {"so ja", "jao", "sone", "ratri"}))
                     ? "Theek hai, shubh ratri! Main sone ja raha hoon."
                     : "Alright, going to sleep. Good night!";
  speakText(reply);
  triggerPendingSleepAction();
  return true;
}

// ============================================================
// COMMAND: HUNGER & PET FEEDING / APOLOGY
// ============================================================
bool handleHungerCommand(const String &qIn) {
  String q = qIn;
  q.toLowerCase();

  // 1. Toggle / Disable Hunger Routine
  if (hasAny(q, {"bhook band", "bhookh band", "hunger band", "disable hunger",
                 "stop hunger", "bhook mat mango", "bhook mat maango",
                 "khana mat mango", "khana mat maango"})) {
    hungerEnabled = false;
    Preferences p;
    p.begin("myrobot", false);
    p.putBool("hunger_en", false);
    p.end();
    if (isKiwiHungry() || isKiwiAngry()) {
      reconcileKiwi();
    }
    speakText("Theek hai! Ab main khane ke liye pareshaan nahi karunga.");
    setEmotion(EMOTION_IDLE);
    return true;
  }

  // 2. Enable Hunger Routine
  if (hasAny(q, {"bhook chalu", "bhookh chalu", "hunger chalu", "enable hunger",
                 "start hunger", "khana maango", "khana mango"})) {
    hungerEnabled = true;
    Preferences p;
    p.begin("myrobot", false);
    p.putBool("hunger_en", true);
    p.end();
    speakText("Yay! Ab main time par apna khana maangunga!");
    setEmotion(EMOTION_IDLE);
    return true;
  }

  // 3. Feeding Kiwi
  if (hasAny(q,
             {"khana khao", "khana kha lo", "kuch khao", "feed kiwi",
              "khana khila", "pet bhar lo", "kuch khale", "bhook lagi hai"})) {
    if (isKiwiHungry()) {
      feedKiwi();
      return true;
    } else if (isKiwiAngry()) {
      handleAngryFeedAttempt();
      return true;
    } else {
      markCurrentMealFed();
      speakText("Mera pet pehle se bhara hua hai!");
      setEmotion(EMOTION_IDLE);
      return true;
    }
  }

  // 4. Apology / Reconciliation
  if (hasAny(q, {"maaf kardo", "maaf kar do", "sorry kiwi", "maan jao",
                 "man jao", "gussa mat ho", "gussa thook do"})) {
    if (isKiwiAngry()) {
      reconcileKiwi();
      return true;
    } else {
      speakText("Arey main to gussa hoon hi nahi!");
      setEmotion(EMOTION_IDLE);
      return true;
    }
  }

  return false;
}
