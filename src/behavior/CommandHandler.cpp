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
