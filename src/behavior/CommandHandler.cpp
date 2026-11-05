#include "behavior/CommandHandler.h"
#include "config/AppConfig.h"
#include "core/Timing.h"
#include "config/ConfigManager.h"
#include "face/FaceEngine.h"
#include "hardware/AudioEngine.h"
#include "hardware/TouchController.h"
#include "services/AiService.h"
#include "services/WeatherService.h"
#include "services/WledService.h"
#include "web/WebPortal.h"
#include "behavior/InteractionManager.h"
#include "behavior/ActionExecutor.h"
#include "services/AiTextTools.h"
#include <WiFi.h>

// ============================================================
// LOCAL VOICE COMMAND PARSER
// ============================================================
bool hasAny(const String &s, std::initializer_list<const char *> words) {
  for (const char *w : words)
    if (s.indexOf(w) >= 0)
      return true;
  return false;
}

bool handleTimeCommand(const String &qIn) {
  String q = qIn;
  q.toLowerCase();

  // If query is about a timer or alarm, do NOT intercept as clock!
  if (hasAny(q, {"timer", "countdown", "alarm", "alaram", "टाइमर", "अलार्म", "काउन्टडाउन", "काउंटडाउन"}))
    return false;

  bool isTime = hasAny(
      q, {"time", "samay", "samey", "taim", "clock", "watch", "what time",
          "show time", "tell me time", "time kya", "kya time", "kitna baja",
          "kitne baje", "abhi kitna", "abhi time", "समय", "टाइम", "घड़ी",
          "घडी", "कितना बजा", "कितने बजे", "अभी समय", "क्या समय", "समय बताओ",
          "टाइम बताओ", "टाइम क्या"});
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
    if (hour12 == 0) hour12 = 12;
    const char *ampm = (timeinfo.tm_hour >= 12) ? "PM" : "AM";
    String timeSpeech;
    if (sttLang == "hi" || containsDevanagari(qIn) || hasAny(q, {"samay", "kya", "baja", "baje", "समय", "टाइम"})) {
      timeSpeech = "Abhi " + String(hour12) + " bajke " + String(timeinfo.tm_min) + " minute ho rahe hain.";
    } else {
      timeSpeech = "It is " + String(hour12) + ":" + ((timeinfo.tm_min < 10) ? "0" : "") + String(timeinfo.tm_min) + " " + String(ampm) + ".";
    }
    setEmotion(EMOTION_SPEAKING);
    speakText(timeSpeech);
  } else {
    speakText("Time sync nahi hua hai.");
  }

  uint32_t t0 = millis();
  while (millis() - t0 < 4000) {
    if (cancelRequest || touchT.state)
      break;
    pumpWeb();
    maintainWiFi();
    serviceUi();
    yield();
  }
  showClockScreen = false;
  setEmotion(EMOTION_HAPPY);
  returnIdleAt = millis() + 1500;
  return true;
}

bool handleWeatherCommand(const String &qIn) {
  String q = qIn;
  q.toLowerCase();
  bool isWeather = hasAny(
      q, {"weather", "temperature", "temp", "mausam", "mosam", "मौसम",
          "तापमान", "barish", "rain", "forecast", "taapmaan", "weather kya",
          "kya mausam", "aaj ka mausam", "kitni garmi", "kitni thand",
          "how is the weather", "current weather", "वेदर", "आज का वेदर",
          "वेदर क्या", "मौसम कैसा"});
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
  if (sttLang == "hi" || containsDevanagari(qIn) || q.indexOf("mausam") >= 0 || q.indexOf("मौसम") >= 0 || q.indexOf("वेदर") >= 0) {
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
    serviceUi();
    yield();
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
  q.trim();

  // Support all phrases: "theme change to cyberpunk", "change theme to desk buddy", "desk buddy kardo", "cyberpunk theme", etc.
  bool isThemeIntent = hasAny(q, {
      "skin", "eyes", "eye skin", "looks", "chashma", "theme", "थीम", "स्किन",
      "आंखें", "चश्मा", "चेहरा", "look", "rakho", "lagao", "karo", "रखो", "लगाओ",
      "करो", "बदलो", "change", "switch", "set", "mode", "bana", "banao", "kardo"
  });

  String name = "";
  EyeSkin targetSkin = currentEyeSkin;

  if (hasAny(q, {"cyberpunk", "cyber", "hud", "sci-fi", "साइबरपंक", "साइबर"})) {
    targetSkin = SKIN_CYBERPUNK; name = "Cyberpunk";
  } else if (hasAny(q, {"desk buddy", "deskbuddy", "desk body", "that body", "kiwi", "buddy", "कीवी"})) {
    targetSkin = SKIN_KIWI; name = "Desk Buddy";
  } else if (hasAny(q, {"cat", "neko", "billi", "kitty", "कैट", "बिल्ली"})) {
    targetSkin = SKIN_CAT; name = "Cat";
  } else if (hasAny(q, {"retro", "8bit", "8-bit", "pixel", "arcade", "रेट्रो"})) {
    targetSkin = SKIN_RETRO8BIT; name = "Retro 8-Bit";
  } else if (hasAny(q, {"anime", "star", "shounen", "starry", "एनिमे", "एनीमे"})) {
    targetSkin = SKIN_ANIME; name = "Anime";
  } else if (hasAny(q, {"heart", "hearts", "dil", "love", "हार्ट", "दिल"})) {
    targetSkin = SKIN_HEARTS; name = "Hearts";
  } else if (hasAny(q, {"steampunk", "goggles", "aviator", "स्टीमपंक"})) {
    targetSkin = SKIN_STEAMPUNK; name = "Steampunk";
  } else if (hasAny(q, {"demon", "devil", "horns", "shaitan", "डेमन", "शैतान"})) {
    targetSkin = SKIN_DEMON; name = "Demon";
  } else if (hasAny(q, {"matrix", "terminal", "code", "hacker", "मैट्रिक्स"})) {
    targetSkin = SKIN_MATRIX; name = "Matrix";
  } else if (hasAny(q, {"cyborg", "terminator", "t800", "t-800", "साइबोर्ग"})) {
    targetSkin = SKIN_CYBORG; name = "Cyborg";
  } else if (hasAny(q, {"mecha", "gundam", "robot eyes", "v-fin", "मेका"})) {
    targetSkin = SKIN_MECHA; name = "Mecha";
  } else if (hasAny(q, {"sunglass", "sunglasses", "shades", "thug life", "boss", "काला चश्मा"})) {
    targetSkin = SKIN_SUNGLASSES; name = "Sunglasses";
  } else if (hasAny(q, {"puppy", "dog", "doggy", "kutta", "पपी", "कुत्ता"})) {
    targetSkin = SKIN_PUPPY; name = "Puppy";
  } else if (hasAny(q, {"hypno", "spiral", "vortex", "dizzy", "हिप्नो"})) {
    targetSkin = SKIN_HYPNO; name = "Hypno";
  } else if (hasAny(q, {"alien", "ufo", "cosmic", "galaxy", "एलियन"})) {
    targetSkin = SKIN_ALIEN; name = "Alien";
  } else if (hasAny(q, {"ninja", "shinobi", "naruto", "headband", "निंजा"})) {
    targetSkin = SKIN_NINJA; name = "Ninja";
  } else if (hasAny(q, {"round", "circle", "gol", "rounded", "गोल"})) {
    targetSkin = SKIN_ROUND; name = "Round";
  } else if (hasAny(q, {"default", "normal", "clean", "reset skin", "डिफ़ॉल्ट", "डिफॉल्ट"})) {
    targetSkin = SKIN_DEFAULT; name = "Default";
  } else if (hasAny(q, {"squircle outline", "outline squircle"})) {
    targetSkin = SKIN_SQUIRCLE_OUTLINE; name = "Squircle Outline";
  } else if (hasAny(q, {"squircle glint", "glint squircle"})) {
    targetSkin = SKIN_SQUIRCLE_GLINT; name = "Squircle Glint";
  } else if (hasAny(q, {"squircle", "squircle solid", "स्क्वार्कल"})) {
    targetSkin = SKIN_SQUIRCLE_SOLID; name = "Squircle Solid";
  }

  if (name.length() == 0)
    return false;

  // If a theme name was matched, only trigger if it's a theme intent OR a short direct command (<= 25 chars)
  if (!isThemeIntent && q.length() > 25)
    return false;

  currentEyeSkin = targetSkin;
  saveConfig();
  soundPetSparkle();
  if (sttLang == "hi" || containsDevanagari(qIn) || hasAny(q, {"rakho", "karo", "lagao", "थीम", "बदलो", "रखो", "लगाओ", "kardo"})) {
    speakText("Theme " + name + " set kar di hai!");
  } else {
    speakText("Theme set to " + name + "!");
  }
  setEmotion(EMOTION_HAPPY);
  returnIdleAt = millis() + 1500;
  return true;
}

bool handleVolumeCommand(const String &qIn) {
  String q = qIn;
  q.toLowerCase();
  if (!hasAny(q, {"volume", "vol", "aawaz", "awaz", "sound", "loudness", "mute"}))
    return false;

  int targetVol = -1;
  if (hasAny(q, {"mute", "chup", "silent"})) {
    targetVol = 0;
  } else if (hasAny(q, {"badhao", "badha", "up", "high", "tez", "increase", "louder"})) {
    targetVol = min(100, speakerVolume + 20);
  } else if (hasAny(q, {"kam", "ghatao", "down", "low", "dheere", "decrease", "softer"})) {
    targetVol = max(10, speakerVolume - 20);
  } else if (hasAny(q, {"max", "full", "pura"})) {
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
    setEmotion(EMOTION_HAPPY);
    String reply = (sttLang == "hi" || q.indexOf("aawaz") >= 0 || q.indexOf("awaz") >= 0)
                       ? "Aawaz " + String(speakerVolume) + " percent kar di!"
                       : "Volume set to " + String(speakerVolume) + " percent!";
    speakText(reply);
    returnIdleAt = millis() + 1500;
    return true;
  }
  return false;
}

bool handleTimerCommand(const String &qIn) {
  String q = qIn;
  q.toLowerCase();
  if (!hasAny(q, {"timer", "countdown", "alarm", "alaram", "टाइमर", "अलार्म", "काउन्टडाउन", "काउंटडाउन"}))
    return false;

  // 1. Cancel / Stop Timer or Alarm
  if (hasAny(q, {"stop", "cancel", "band", "off", "hatao", "rok", "dismiss",
                 "बंद", "रोको", "हटाओ", "कैंसिल", "स्टॉप"})) {
    if (isRobotTimerRinging()) {
      stopRobotTimerAlarm();
      speakText((sttLang == "hi") ? "Alarm band kar diya!" : "Alarm stopped!");
      return true;
    }
    if (isRobotTimerActive()) {
      cancelRobotTimer();
      speakText((sttLang == "hi") ? "Timer cancel kar diya!" : "Timer cancelled!");
      return true;
    }
    speakText((sttLang == "hi") ? "Koi timer ya alarm chal nahi raha hai." : "No timer or alarm is active.");
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
        if (hasAny(q, {"hour", "hours", "ghanta", "ghante", "hr", "hrs", "घंटा", "घंटे"})) {
          totalSec = (uint32_t)val * 3600;
        } else if (hasAny(q, {"minute", "minutes", "min", "mins", "minto", "minut", "minuite", "मिनट", "मिन्ट"})) {
          totalSec = (uint32_t)val * 60;
        } else if (hasAny(q, {"second", "seconds", "sec", "secs", "सेकंड", "सेकेंड"})) {
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
    if (hasAny(q, {"aadha ghanta", "half an hour", "half hour", "आधा घंटा"})) totalSec = 1800;
    else if (hasAny(q, {"ek minute", "one minute", "a minute", "1 minute", "एक मिनट", "1 मिनट"})) totalSec = 60;
    else if (hasAny(q, {"do minute", "two minute", "two minutes", "2 minute", "दो मिनट", "2 मिनट"})) totalSec = 120;
    else if (hasAny(q, {"teen minute", "three minute", "three minutes", "3 minute", "तीन मिनट", "3 मिनट"})) totalSec = 180;
    else if (hasAny(q, {"chaar minute", "char minute", "four minute", "four minutes", "4 minute", "चार मिनट", "4 मिनट"})) totalSec = 240;
    else if (hasAny(q, {"paanch minute", "panch minute", "five minute", "five minutes", "5 minute", "पांच मिनट", "पाँच मिनट", "5 मिनट"})) totalSec = 300;
    else if (hasAny(q, {"das minute", "ten minute", "ten minutes", "10 minute", "दस मिनट", "10 मिनट"})) totalSec = 600;
    else if (hasAny(q, {"pandrah minute", "fifteen minute", "fifteen minutes", "15 minute", "पंद्रह मिनट", "15 मिनट"})) totalSec = 900;
    else if (hasAny(q, {"bees minute", "twenty minute", "20 minute", "बीस मिनट", "20 मिनट"})) totalSec = 1200;
    else if (hasAny(q, {"tees minute", "thirty minute", "thirty minutes", "30 minute", "तीस मिनट", "30 मिनट"})) totalSec = 1800;
    else if (hasAny(q, {"ek ghanta", "one hour", "1 hour", "एक घंटा", "1 घंटा"})) totalSec = 3600;
    else if (hasAny(q, {"tees second", "thirty second", "thirty seconds", "30 second", "तीस सेकंड", "30 सेकंड"})) totalSec = 30;
    else if (hasAny(q, {"pandrah second", "fifteen second", "fifteen seconds", "15 second", "पंद्रह सेकंड", "15 सेकंड"})) totalSec = 15;
    else if (hasAny(q, {"das second", "ten second", "ten seconds", "10 second", "दस सेकंड", "10 सेकंड"})) totalSec = 10;
  }

  if (totalSec > 0) {
    // Determine label if user specified e.g. "for tea" or "study ke liye"
    String label = "Timer";
    int forIdx = q.indexOf("for ");
    if (forIdx >= 0) {
      label = q.substring(forIdx + 4);
      int endP = label.indexOf(" timer");
      if (endP >= 0) label = label.substring(0, endP);
      label.trim();
    }
    if (label == "Timer" || label.length() == 0) {
      int keLiyeIdx = q.indexOf(" ke liye");
      if (keLiyeIdx > 0) {
        label = q.substring(0, keLiyeIdx);
        int sp = label.lastIndexOf(' ');
        if (sp >= 0) label = label.substring(sp + 1);
        label.trim();
      }
    }
    if (label.length() == 0 || label.length() > 20) label = "Timer";

    startRobotTimer(totalSec, label);

    String durationDesc;
    if (totalSec >= 60) {
      uint32_t m = totalSec / 60;
      uint32_t s = totalSec % 60;
      if (s > 0) {
        durationDesc = String(m) + (sttLang == "hi" ? " minute " : " minute ") + String(s) + (sttLang == "hi" ? " second" : " seconds");
      } else {
        durationDesc = String(m) + (sttLang == "hi" ? " minute" : (m == 1 ? " minute" : " minutes"));
      }
    } else {
      durationDesc = String(totalSec) + (sttLang == "hi" ? " second" : (totalSec == 1 ? " second" : " seconds"));
    }

    String reply = (sttLang == "hi")
                       ? durationDesc + " ka timer shuru kar diya hai!"
                       : "Timer started for " + durationDesc + "!";
    speakText(reply);
    return true;
  }
  return false;
}

bool handleLightCommand(const String &qIn) {
  String q = qIn;
  q.toLowerCase();
  q.trim();

  // Check if query is talking about light / lights / batti / bulb / lamp / wled / brightness / roshni
  bool hasLightTrigger = hasAny(q, {"light", "lights", "batti", "bulb", "lamp", "roshni", "wled", "brightness"});
  int directFxId = -1;
  String directFxName = "";

  if (!hasLightTrigger) {
    // If "light" is not explicitly mentioned, only match if query has "mode" or "effect" AND matches a valid WLED effect
    if (hasAny(q, {"mode", "effect"})) {
      directFxId = findWledEffectId(q, &directFxName);
      if (directFxId < 0)
        return false;
    } else {
      return false;
    }
  }

  // 1. Turn OFF
  if (hasAny(q, {"band", "off", "bujha", "turn off", "switch off", "close light"})) {
    if (executeWledAction("off")) {
      showEmotionFor(EMOTION_HAPPY, 1800);
      soundPetSparkle();
      speakText((sttLang == "hi") ? "Light band kar di!" : "Lights turned off!");
      return true;
    }
  }

  // 2. Turn ON
  if (hasAny(q, {"chalu", "turn on", "switch on", "jalao", "on kar", "open light"})) {
    if (executeWledAction("on")) {
      showEmotionFor(EMOTION_HAPPY, 1800);
      soundPetSparkle();
      speakText((sttLang == "hi") ? "Light on kar di!" : "Lights turned on!");
      return true;
    }
  }

  // 3. Brightness Control
  // 3a. Explicit percentage or numeric brightness (e.g. "brightness 50", "roshni 80 percent", "30%")
  if (hasAny(q, {"brightness", "bri", "roshni", "percent", "%"}) ||
      (hasAny(q, {"light", "lights", "bulb", "batti"}) && hasAny(q, {"percent", "%"}))) {
    int val = -1;
    for (int i = 0; i < (int)q.length(); i++) {
      if (isdigit(q[i])) {
        int v = 0;
        while (i < (int)q.length() && isdigit(q[i])) {
          v = v * 10 + (q[i] - '0');
          i++;
        }
        if (v >= 0 && v <= 100) {
          val = v;
          break;
        }
      }
    }
    if (val >= 0) {
      uint8_t bri = (uint8_t)((val * 255) / 100);
      if (setWledBrightness(bri)) {
        showEmotionFor(EMOTION_HAPPY, 1800);
        soundPetSparkle();
        String reply = (sttLang == "hi")
                           ? "Light brightness " + String(val) + " percent kar di!"
                           : "Light brightness set to " + String(val) + " percent!";
        speakText(reply);
        return true;
      }
    }
  }

  // 3b. Relative / Preset Brightness
  if (hasAny(q, {"dim", "kam", "ghatao", "low", "dheemi", "decrease"})) {
    if (executeWledAction("dim")) {
      showEmotionFor(EMOTION_RELIEVED, 1800);
      soundPetSparkle();
      speakText((sttLang == "hi") ? "Light dim kar di!" : "Lights dimmed!");
      return true;
    }
  }
  if (hasAny(q, {"tez", "full", "max", "bright", "badhao", "increase", "poori", "puri"})) {
    if (executeWledAction("full")) {
      showEmotionFor(EMOTION_EXCITED, 1800);
      soundPetSparkle();
      speakText((sttLang == "hi") ? "Light full bright kar di!" : "Lights set to full brightness!");
      return true;
    }
  }
  if (hasAny(q, {"medium", "half", "aadhi", "adha"})) {
    if (executeWledAction("medium")) {
      showEmotionFor(EMOTION_HAPPY, 1800);
      soundPetSparkle();
      speakText((sttLang == "hi") ? "Light 50 percent brightness kar di!" : "Light set to 50 percent brightness!");
      return true;
    }
  }

  // 4. Effects / Modes
  // 4a. Next / Change Mode
  if (hasAny(q, {"mode badlo", "mode badal", "effect badlo", "effect badal", "change mode", "next mode",
                 "switch mode", "koi aur mode", "aur koi mode", "dusra mode", "doosra mode", "agla mode",
                 "next effect", "aur effect", "koi aur effect", "dusra effect", "doosra effect", "agla effect",
                 "random mode", "random effect", "kuch aur dikhao", "aur mode", "mode change", "effect change"})) {
    if (setWledNextEffect()) {
      showEmotionFor(EMOTION_PARTY, 2000);
      soundPetSparkle();
      speakText((sttLang == "hi") ? "Light ka agla mode laga diya!" : "Switched to next light mode!");
      return true;
    }
  }

  // 4b. Find ANY named effect (from all 160+ WLED effects!)
  String effectQuery = q;
  if (hasAny(q, {"aag", "mombatti"})) effectQuery += " fire candle";
  if (hasAny(q, {"rang birang", "disco", "party"})) effectQuery += " rainbow";
  if (hasAny(q, {"jugnu", "taare", "tare"})) effectQuery += " twinkle";
  if (hasAny(q, {"samundar", "lahar", "pani"})) effectQuery += " pacifica waves";
  if (hasAny(q, {"bijli", "tufan"})) effectQuery += " lightning";
  if (hasAny(q, {"barish"})) effectQuery += " rain";
  if (hasAny(q, {"patakha", "diwali"})) effectQuery += " fireworks";
  if (hasAny(q, {"suraj", "subah", "dhoop"})) effectQuery += " sunrise";
  if (hasAny(q, {"dhadkan"})) effectQuery += " heartbeat";
  if (hasAny(q, {"bhoot"})) effectQuery += " halloween eyes";
  if (hasAny(q, {"siren"})) effectQuery += " police";

  String matchedFx = directFxName;
  int fxId = (directFxId >= 0) ? directFxId : findWledEffectId(effectQuery, &matchedFx);
  if (fxId >= 0) {
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
  } colors[] = {
      {{"red", "laal", nullptr}, "red", "Light red kar di!", "Light set to red!"},
      {{"green", "hara", "haree", nullptr}, "green", "Light green kar di!", "Light set to green!"},
      {{"blue", "neela", "neeli", nullptr}, "blue", "Light blue kar di!", "Light set to blue!"},
      {{"yellow", "peela", "peeli", nullptr}, "yellow", "Light yellow kar di!", "Light set to yellow!"},
      {{"pink", "gulabi", nullptr}, "pink", "Light pink kar di!", "Light set to pink!"},
      {{"purple", "violet", "baingani", nullptr}, "purple", "Light purple kar di!", "Light set to purple!"},
      {{"orange", "narangi", nullptr}, "orange", "Light orange kar di!", "Light set to orange!"},
      {{"cyan", "aqua", "firozi", nullptr}, "cyan", "Light cyan kar di!", "Light set to cyan!"},
      {{"white", "safed", nullptr}, "white", "Light white kar di!", "Light set to white!"},
      {{"warm", "night", nullptr}, "warm", "Warm light on kar di!", "Warm light activated!"},
      {{"cool", nullptr}, "cool", "Cool white light on kar di!", "Cool white light activated!"},
      {{nullptr}, nullptr, nullptr, nullptr}
  };

  for (int i = 0; colors[i].action != nullptr; i++) {
    for (int k = 0; colors[i].names[k] != nullptr; k++) {
      if (q.indexOf(colors[i].names[k]) >= 0) {
        if (executeWledAction(colors[i].action)) {
          showEmotionFor(EMOTION_HAPPY, 1800);
          soundPetSparkle();
          speakText((sttLang == "hi") ? colors[i].hiReply : colors[i].enReply);
          return true;
        }
      }
    }
  }

  // 6. Status / Discover query
  if (hasAny(q, {"status", "connected", "kahan hai", "dhundo", "scan"})) {
    if (discoverWled(true)) {
      showEmotionFor(EMOTION_HAPPY, 1800);
      speakText("WLED light connected at " + getWledIp());
      return true;
    } else {
      showEmotionFor(EMOTION_CONFUSED, 1800);
      speakText("Mujhe network par WLED light nahi mili. Check karo ki light on hai!");
      return true;
    }
  }

  return false;
}
