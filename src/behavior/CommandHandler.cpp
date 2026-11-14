#include "behavior/CommandHandler.h"
#include "behavior/ActionExecutor.h"
#include "behavior/HungerEngine.h"
#include "behavior/InteractionManager.h"
#include "config/AppConfig.h"
#include "config/ConfigManager.h"
#include "core/Timing.h"
#include "face/FaceEngine.h"
#include "hardware/AudioEngine.h"
#include "hardware/TouchController.h"
#include "services/AiService.h"
#include "services/AiTextTools.h"
#include "services/HyperHdrService.h"
#include "services/MemoryService.h"
#include "services/WeatherService.h"
#include "services/WledService.h"
#include "web/WebPortal.h"
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
  bool isTime = hasAny(
      q, {"what time",     "what's the time", "whats the time", "tell me time",
          "tell the time", "show time",       "show clock",     "current time",
          "time now",      "time right now",  "time kya",       "kya time",
          "kitna baja",    "kitne baje",      "abhi kitna",     "abhi time",
          "abhi kya time", "samay kya",       "kya samay",      "samay batao",
          "time batao",    "time bolo",       "ghadi dikhao",   "time check",
          "समय क्या",       "क्या समय",         "समय बताओ",       "टाइम बताओ",
          "टाइम क्या",      "कितना बजा",       "कितने बजे",        "अभी समय"});

  // Short direct queries (e.g. "time", "clock", "samay", "ghadi")
  if (!isTime && q.length() <= 20) {
    isTime = hasWord(q, {"time", "clock", "samay", "samey", "taim", "ghadi"}) ||
             hasAny(q, {"समय", "टाइम", "घड़ी", "घडी"});
  }

  if (!isTime)
    return false;

  Serial.println("[CMD] time -> show clock & speak");
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
        hasAny(q, {"samay", "kya", "baja", "baje", "समय", "टाइम"})) {
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
  soundPetSparkle();

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

  // Negative filter: Reject questions, stories, recipes, informational requests
  if (hasAny(q, {"why", "how", "who", "what", "explain", "tell me", "recipe",
                 "make a ", "make an ", "story", "difference"}))
    return false;

  bool isCommand = hasWord(q, {"face", "expression", "bana", "dikha", "show",
                               "make", "look", "ho ja", "ho jao", "kar do"}) ||
                   hasAny(q, {"फेस", "चेहरा", "चेहरे", "एक्सप्रेशन", "बना", "दिखा",
                              "हो जा", "हो जाओ", "कर दो"});

  if (!isCommand && q.length() > 25)
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
  if (hasWord(q, {"laugh", "laughing", "giggle"}) ||
      hasAny(q, {"लाफ", "हंस", "hans"})) {
    showPetMood(PET_GIGGLE);
    return true;
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
  if (hasWord(q, {"cry", "crying"}) ||
      hasAny(q, {"रोना", "rona", "ro mat", "aansu"})) {
    showEmotionFor(EMOTION_CRYING);
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

  bool isThemeIntent =
      hasWord(q, {"skin", "eyes", "eye skin", "looks", "theme", "look", "mode",
                  "change", "switch", "set"}) ||
      hasAny(q, {"थीम", "स्किन", "आंखें", "चश्मा", "चेहरा", "रखो", "लगाओ", "करो",
                 "बदलो", "rakho", "lagao", "karo", "bana", "banao", "kardo"});

  String name = "";
  EyeSkin targetSkin = currentEyeSkin;

  if (hasWord(q, {"kiwi", "desk buddy", "deskbuddy", "buddy", "cute"}) ||
      hasAny(q, {"कीवी"})) {
    targetSkin = SKIN_KIWI;
    name = "Kiwi";
  } else if (hasWord(q, {"vector", "vector classic", "anki", "default",
                         "classic", "glow"}) ||
             hasAny(q, {"वेक्टर"})) {
    targetSkin = SKIN_VECTOR;
    name = "Vector Classic";
  } else if (hasWord(q,
                     {"capsule", "neon capsule", "pill", "stadium", "neon"}) ||
             hasAny(q, {"कैप्सूल"})) {
    targetSkin = SKIN_CAPSULE;
    name = "Neon Capsule";
  } else if (hasWord(q, {"pixel", "pixel 8-bit", "arcade", "retro", "8bit",
                         "8-bit"}) ||
             hasAny(q, {"रेट्रो", "पिक्सेल"})) {
    targetSkin = SKIN_PIXEL;
    name = "Pixel 8-Bit";
  } else if (hasAny(q, {"squircle bold", "squircle solid", "bold"})) {
    targetSkin = SKIN_SQUIRCLE_BOLD;
    name = "Squircle Bold";
  } else if (hasAny(q,
                    {"neon frame", "squircle outline", "frame", "outline"})) {
    targetSkin = SKIN_NEON_FRAME;
    name = "Neon Frame";
  } else if (hasAny(q, {"lumina spark", "squircle glint", "spark", "lumina",
                        "glint"})) {
    targetSkin = SKIN_LUMINA_SPARK;
    name = "Lumina Spark";
  } else if (hasWord(q, {"squircle"}) || hasAny(q, {"स्क्वार्कल"})) {
    targetSkin = SKIN_SQUIRCLE_BOLD;
    name = "Squircle Bold";
  }

  if (name.length() == 0)
    return false;

  // If a theme name was matched, only trigger if it's a theme intent OR a short
  // direct command (<= 25 chars)
  if (!isThemeIntent && q.length() > 25)
    return false;

  currentEyeSkin = targetSkin;
  saveConfig();
  soundPetSparkle();
  if (sttLang == "hi" || containsDevanagari(qIn) ||
      hasAny(q, {"rakho", "karo", "lagao", "थीम", "बदलो", "रखो", "लगाओ",
                 "kardo"})) {
    speakText("Theme " + name + " set kar di hai!");
  } else {
    speakText("Theme set to " + name + "!");
  }
  setEmotion(EMOTION_IDLE);
  returnIdleAt = 0;
  return true;
}

// ============================================================
// COMMAND: VOLUME
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
                                "loudness", "mute"}) ||
                    hasAny(q, {"आवाज़", "आवाज", "वॉल्यूम", "म्यूट"});

  if (!hasVolWord)
    return false;

  int targetVol = -1;
  if (hasWord(q, {"mute", "silent"}) || hasAny(q, {"chup", "म्यूट", "शांत"})) {
    targetVol = 0;
  } else if (hasWord(q, {"up", "high", "increase", "louder"}) ||
             hasAny(q, {"badhao", "badha", "tez", "बढ़ाओ", "तेज़"})) {
    targetVol = min(100, speakerVolume + 20);
  } else if (hasWord(q, {"down", "low", "decrease", "softer"}) ||
             hasAny(q, {"kam", "ghatao", "dheere", "कम", "घटाओ"})) {
    targetVol = max(10, speakerVolume - 20);
  } else if (hasWord(q, {"max", "full"}) || hasAny(q, {"pura", "पूरा"})) {
    targetVol = 100;
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
    soundPetSparkle();
    String reply = (sttLang == "hi" || q.indexOf("aawaz") >= 0 ||
                    q.indexOf("awaz") >= 0 || containsDevanagari(qIn))
                       ? "Aawaz " + String(speakerVolume) + " percent kar di!"
                       : "Volume set to " + String(speakerVolume) + " percent!";
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
      hasWord(q, {"light", "lights", "bulb", "lamp", "wled", "brightness"}) ||
      hasAny(q, {"batti", "roshni", "बत्ती", "रोशनी", "लाइट"}) ||
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
        showEmotionFor(EMOTION_HAPPY, 2000);
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
    if (executeUnifiedLightAction("off")) {
      setEmotion(EMOTION_IDLE);
      soundPetSparkle();
      speakText((sttLang == "hi") ? "Light band kar di!"
                                  : "Lights turned off!");
      return true;
    }
  }

  // 2. Turn ON
  if (hasWord(q, {"on", "open"}) ||
      hasAny(q, {"chalu", "turn on", "switch on", "jalao", "on kar",
                 "open light", "चालू", "जलाओ"})) {
    if (executeUnifiedLightAction("on")) {
      setEmotion(EMOTION_IDLE);
      soundPetSparkle();
      speakText((sttLang == "hi") ? "Light on kar di!" : "Lights turned on!");
      return true;
    }
  }

  // 3. Brightness presets
  if (hasWord(q, {"dim", "low", "night", "slight"}) ||
      hasAny(q, {"kam", "dheemi", "kam karo", "कम"})) {
    if (executeUnifiedLightAction("dim")) {
      setEmotion(EMOTION_IDLE);
      soundPetSparkle();
      speakText((sttLang == "hi") ? "Light dim kar di!" : "Light dimmed!");
      return true;
    }
  }
  if (hasWord(q, {"bright", "full", "max", "increase"}) ||
      hasAny(q, {"tez", "badhao", "poori", "puri", "तेज़"})) {
    if (executeUnifiedLightAction("full")) {
      setEmotion(EMOTION_IDLE);
      soundPetSparkle();
      speakText((sttLang == "hi") ? "Light full bright kar di!"
                                  : "Lights set to full brightness!");
      return true;
    }
  }
  if (hasWord(q, {"medium", "half"}) || hasAny(q, {"aadhi", "adha", "आधी"})) {
    if (executeUnifiedLightAction("medium")) {
      setEmotion(EMOTION_IDLE);
      soundPetSparkle();
      speakText((sttLang == "hi") ? "Light 50 percent brightness kar di!"
                                  : "Light set to 50 percent brightness!");
      return true;
    }
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

  // 5. Colors (Leaves brightness untouched!)
  struct ColorMatch {
    const char *names[5];
    const char *action;
    const char *hiReply;
    const char *enReply;
  } colors[] = {{{"red", "laal", nullptr},
                 "red",
                 "Light red kar di!",
                 "Light set to red!"},
                {{"green", "hara", "haree", nullptr},
                 "green",
                 "Light green kar di!",
                 "Light set to green!"},
                {{"blue", "neela", "neeli", nullptr},
                 "blue",
                 "Light blue kar di!",
                 "Light set to blue!"},
                {{"yellow", "peela", "peeli", nullptr},
                 "yellow",
                 "Light yellow kar di!",
                 "Light set to yellow!"},
                {{"pink", "gulabi", nullptr},
                 "pink",
                 "Light pink kar di!",
                 "Light set to pink!"},
                {{"purple", "violet", "baingani", nullptr},
                 "purple",
                 "Light purple kar di!",
                 "Light set to purple!"},
                {{"orange", "narangi", nullptr},
                 "orange",
                 "Light orange kar di!",
                 "Light set to orange!"},
                {{"cyan", "aqua", "firozi", nullptr},
                 "cyan",
                 "Light cyan kar di!",
                 "Light set to cyan!"},
                {{"white", "safed", nullptr},
                 "white",
                 "Light white kar di!",
                 "Light set to white!"},
                {{"warm", "night", nullptr},
                 "warm",
                 "Warm light on kar di!",
                 "Warm light activated!"},
                {{"cool", nullptr},
                 "cool",
                 "Cool white light on kar di!",
                 "Cool white light activated!"},
                {{nullptr}, nullptr, nullptr, nullptr}};

  for (int i = 0; colors[i].action != nullptr; i++) {
    for (int k = 0; colors[i].names[k] != nullptr; k++) {
      if (matchWord(q, colors[i].names[k])) {
        if (executeUnifiedLightAction(colors[i].action)) {
          setEmotion(EMOTION_IDLE);
          soundPetSparkle();
          speakText((sttLang == "hi") ? colors[i].hiReply : colors[i].enReply);
          return true;
        }
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
// COMMAND: HUNGER & PET FEEDING / APOLOGY
// ============================================================
bool handleHungerCommand(const String &qIn) {
  String q = qIn;
  q.toLowerCase();

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
      String n = getOwnerName();
      if (n.length() == 0)
        n = "Krishna";
      speakText("Mera pet pehle se bhara hua hai " + n + "!");
      setEmotion(EMOTION_HAPPY);
      return true;
    }
  }

  if (hasAny(q, {"maaf kardo", "maaf kar do", "sorry kiwi", "maan jao",
                 "man jao", "gussa mat ho", "gussa thook do"})) {
    if (isKiwiAngry()) {
      reconcileKiwi();
      return true;
    } else {
      speakText("Arey main to gussa hoon hi nahi!");
      setEmotion(EMOTION_HAPPY);
      return true;
    }
  }

  return false;
}
