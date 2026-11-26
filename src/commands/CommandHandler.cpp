#include "commands/CommandHandler.h"
#include "commands/ActionExecutor.h"
#include "behavior/HungerEngine.h"
#include "behavior/InteractionManager.h"
#include "config/AppConfig.h"
#include "config/ConfigManager.h"
#include "core/Timing.h"
#include "face/FaceEngine.h"
#include "hardware/AudioEngine.h"
#include "hardware/TouchController.h"
#include "ai/AiService.h"
#include "ai/AiTextTools.h"
#include "integrations/WeatherService.h"
#include "net/WifiManager.h"
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
                    hasAny(q, {"volume", "aawaz", "awaz", "awaaz",
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
