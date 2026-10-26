#include "behavior/CommandHandler.h"
#include "config/AppConfig.h"
#include "config/ConfigManager.h"
#include "face/FaceEngine.h"
#include "hardware/AudioEngine.h"
#include "hardware/TouchController.h"
#include "services/AiService.h"
#include "services/WeatherService.h"
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
    pumpUI();
    delay(10);
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
  if (!hasAny(q, {"skin", "eyes", "eye skin", "looks", "chashma", "theme", "थीम", "स्किन", "आंखें", "चश्मा", "चेहरा", "look", "rakho", "lagao", "karo", "रखो", "लगाओ", "करो", "बदलो"}))
    return false;

  String name = "";
  if (hasAny(q, {"cyberpunk", "cyber", "hud", "sci-fi", "साइबरपंक", "साइबर"})) {
    currentEyeSkin = SKIN_CYBERPUNK; name = "Cyberpunk Visor";
  } else if (hasAny(q, {"cat", "neko", "billi", "kitty", "कैट", "बिल्ली"})) {
    currentEyeSkin = SKIN_CAT; name = "Kawaii Cat";
  } else if (hasAny(q, {"retro", "8bit", "8-bit", "pixel", "arcade", "रेट्रो"})) {
    currentEyeSkin = SKIN_RETRO8BIT; name = "Retro 8-Bit";
  } else if (hasAny(q, {"anime", "star", "shounen", "starry", "एनिमे", "एनीमे"})) {
    currentEyeSkin = SKIN_ANIME; name = "Anime Star";
  } else if (hasAny(q, {"heart", "hearts", "dil", "love", "हार्ट", "दिल"})) {
    currentEyeSkin = SKIN_HEARTS; name = "Heart Eyes";
  } else if (hasAny(q, {"steampunk", "goggles", "aviator", "स्टीमपंक"})) {
    currentEyeSkin = SKIN_STEAMPUNK; name = "Steampunk Goggles";
  } else if (hasAny(q, {"demon", "devil", "horns", "shaitan", "डेमन", "शैतान"})) {
    currentEyeSkin = SKIN_DEMON; name = "Demon Horns";
  } else if (hasAny(q, {"matrix", "terminal", "code", "hacker", "मैट्रिक्स"})) {
    currentEyeSkin = SKIN_MATRIX; name = "Matrix Code";
  } else if (hasAny(q, {"cyborg", "terminator", "t800", "t-800", "साइबोर्ग"})) {
    currentEyeSkin = SKIN_CYBORG; name = "Cyborg T-800";
  } else if (hasAny(q, {"mecha", "gundam", "robot eyes", "v-fin", "मेका"})) {
    currentEyeSkin = SKIN_MECHA; name = "Mecha Gundam";
  } else if (hasAny(q, {"sunglass", "sunglasses", "shades", "thug life", "boss", "काला चश्मा"})) {
    currentEyeSkin = SKIN_SUNGLASSES; name = "Boss Shades";
  } else if (hasAny(q, {"puppy", "dog", "doggy", "kutta", "पपी", "कुत्ता"})) {
    currentEyeSkin = SKIN_PUPPY; name = "Kawaii Puppy";
  } else if (hasAny(q, {"hypno", "spiral", "vortex", "dizzy", "हिप्नो"})) {
    currentEyeSkin = SKIN_HYPNO; name = "Hypno Spiral";
  } else if (hasAny(q, {"alien", "ufo", "cosmic", "galaxy", "एलियन"})) {
    currentEyeSkin = SKIN_ALIEN; name = "Cosmic Alien";
  } else if (hasAny(q, {"ninja", "shinobi", "naruto", "headband", "निंजा"})) {
    currentEyeSkin = SKIN_NINJA; name = "Shinobi Ninja";
  } else if (hasAny(q, {"round", "circle", "gol", "rounded", "गोल"})) {
    currentEyeSkin = SKIN_ROUND; name = "Round Vector";
  } else if (hasAny(q, {"kiwi", "desk buddy", "buddy", "कीवी"})) {
    currentEyeSkin = SKIN_KIWI; name = "Desk Buddy (Cute Block)";
  } else if (hasAny(q, {"default", "normal", "clean", "reset skin", "डिफ़ॉल्ट", "डिफॉल्ट"})) {
    currentEyeSkin = SKIN_DEFAULT; name = "Default Eyes";
  } else {
    return false;
  }

  saveConfig();
  soundPetSparkle();
  if (sttLang == "hi" || containsDevanagari(qIn) || hasAny(q, {"rakho", "karo", "lagao", "थीम", "बदलो", "रखो", "लगाओ"})) {
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
  if (!hasAny(q, {"timer", "countdown", "alaram"}))
    return false;

  uint32_t totalSec = 0;
  for (int i = 0; i < (int)q.length(); i++) {
    if (isdigit(q[i])) {
      int val = 0;
      while (i < (int)q.length() && isdigit(q[i])) {
        val = val * 10 + (q[i] - '0');
        i++;
      }
      if (val > 0) {
        if (hasAny(q, {"minute", "min", "minto", "minut"})) {
          totalSec = (uint32_t)val * 60;
        } else {
          totalSec = (uint32_t)val;
        }
        break;
      }
    }
  }

  if (totalSec == 0 && hasAny(q, {"ek minute", "one minute"})) totalSec = 60;
  if (totalSec == 0 && hasAny(q, {"do minute", "two minutes"})) totalSec = 120;
  if (totalSec == 0 && hasAny(q, {"paanch minute", "five minutes"})) totalSec = 300;

  if (totalSec > 0) {
    startRobotTimer(totalSec, "Focus Timer");
    String reply = (sttLang == "hi")
                       ? String(totalSec >= 60 ? totalSec / 60 : totalSec) +
                             (totalSec >= 60 ? " minute" : " second") + " ka timer shuru!"
                       : "Timer started for " +
                             String(totalSec >= 60 ? totalSec / 60 : totalSec) +
                             (totalSec >= 60 ? " minutes!" : " seconds!");
    speakText(reply);
    return true;
  }
  return false;
}
