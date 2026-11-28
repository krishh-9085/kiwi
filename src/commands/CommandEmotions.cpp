#include "commands/CommandHandler.h"
#include "commands/ActionExecutor.h"
#include "behavior/InteractionManager.h"
#include "config/AppConfig.h"
#include "config/ConfigManager.h"
#include "core/Timing.h"
#include "face/FaceDisplay.h"
#include "face/FaceEngine.h"
#include "hardware/AudioEngine.h"
#include "hardware/WakeWordEngine.h"
#include "ai/AiService.h"
#include "ai/AiTextTools.h"
#include <WiFi.h>

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
      if (cancelRequest)
        break;
      yield();
    }

    toneStop();
    spkAbortSpeech();
    spkEndSpeech();

    bool interrupted = cancelRequest;
    bool bargeInListen = interrupted && startRecording;
    cancelRequest = false;
    if (!bargeInListen) {
      startRecording = false;
      setEmotion(EMOTION_IDLE);
    } else {
      setEmotion(EMOTION_LISTENING);
    }
    returnIdleAt = 0;
    emotionAutoReturnAt = 0;

    wakeWordReset();
    preWakeReset();
    wakeWordSuppressUntil(millis() + 1500UL);
    return true;
  };

  auto executeLaughAction = [&]() -> bool {
    const uint32_t LAUGH_TARGET_MS = 5000UL; // Strictly 5 seconds
    Serial.printf("[CMD] Laughing action requested ('laugh / haske dikhao') "
                  "[tts=%s, fishKey=%d, wifi=%d]\n",
                  ttsProvider.c_str(), (int)fishKey.length(),
                  (int)(WiFi.status() == WL_CONNECTED));

    // Suppress wake word for full laugh + network margin + post-laugh echo guard
    wakeWordSuppressUntil(millis() + LAUGH_TARGET_MS + 3500UL);
    cancelRequest = false;
    startRecording = false;

    setEmotion(EMOTION_LAUGHING);
    uint32_t t0 = millis();
    returnIdleAt = t0 + LAUGH_TARGET_MS + 3000UL;
    emotionAutoReturnAt = returnIdleAt;

    bool spoke = false;
    // Generate vocal laugh via Fish Audio whenever a Fish API key & Wi-Fi exist
    // (works even when general chat TTS provider is set to Sarvam)
    if (fishKey.length() > 0 && WiFi.status() == WL_CONNECTED) {
      spoke = speakFishAudio(
          "[laughing] Hahahahaha! Ahahaha, hahahaha! Hehehe, hahaha!",
          LAUGH_TARGET_MS);
    } else if ((ttsProvider == "sarvam") && ttsKey.length() > 0 &&
               fishKey.length() == 0 && WiFi.status() == WL_CONNECTED) {
      Serial.println("[CMD/Laugh] No Fish Audio key configured -> fallback synth");
    }

    if (!spoke && !cancelRequest) {
      // Fallback high-fidelity robot synthesizer laughter paired with laughing -> happy face
      spkEndSpeech();
      setEmotion(EMOTION_LAUGHING);
      t0 = millis();
      returnIdleAt = t0 + LAUGH_TARGET_MS;
      emotionAutoReturnAt = returnIdleAt;
      soundLaughing();

      bool secondGigglePlayed = false;
      bool switchedToHappy = false;
      while (millis() - t0 < LAUGH_TARGET_MS) {
        serviceUi();
        if (cancelRequest)
          break;

        uint32_t elapsed = millis() - t0;
        if (!secondGigglePlayed && elapsed >= 2000UL) {
          soundPetGiggle();
          secondGigglePlayed = true;
        }
        if (!switchedToHappy && elapsed >= 3400UL) {
          setEmotion(EMOTION_HAPPY);
          switchedToHappy = true;
        }

        yield();
      }
    } else {
      // Hold a warm happy smile briefly after Fish Audio laugh finishes
      wakeWordSuppressUntil(millis() + 2000UL);
      setEmotion(EMOTION_HAPPY);
      uint32_t settleStart = millis();
      while (millis() - settleStart < 450UL) {
        serviceUi();
        if (cancelRequest)
          break;
        yield();
      }
    }

    // Clean stop and return to neutral idle (or barge-in listening if requested)
    toneStop();
    spkAbortSpeech();
    spkEndSpeech();

    bool interrupted = cancelRequest;
    bool bargeInListen = interrupted && startRecording;
    cancelRequest = false;
    if (!bargeInListen) {
      startRecording = false;
      setEmotion(EMOTION_IDLE);
    } else {
      setEmotion(EMOTION_LISTENING);
    }
    returnIdleAt = 0;
    emotionAutoReturnAt = 0;

    // Reset audio queues and protect against echo
    wakeWordReset();
    preWakeReset();
    wakeWordSuppressUntil(millis() + 1500UL);

    Serial.println("[CMD] Laughing action completed cleanly after 5s.");
    return true;
  };

  bool isCryReq =
      hasAny(q, {"ro kr",      "ro kar",    "ro ke",  "ro k ",   "rokar",
                 "rone ki",    "ro do",     "ro lo",  "rona dikha",
                 "rona shuru", "rone lago", "ro pad", "ro pado",
                 "रो के",       "रो कर",     "रोकर",   "रोने की",  "रो दो",
                 "रो पड़",      "रो पड़ो"}) ||
      hasWord(q, {"cry", "crying", "sob", "sobbing", "weep", "tears", "rona",
                  "roke", "ro", "रो", "रोना"});

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
                 "haste hue",   "hanste hue",
                 "haste raho",  "hasne ki",
                 "hansne ki",   "hasne lago",  "hasne wala",
                 "hasna dikhao","hassna dikhao","hansna dikhao",
                 "hasna sunao",
                 "haso na",     "hanso na",    "hasso na",    "thoda haso",
                 "thoda hasso", "thoda hanso", "zara haso",   "zara hasso",
                 "has do",      "hans do",     "hass do",     "has do na",
                 "has lo",      "hans lo",
                 "mujhe hasao", "hasi sunao",
                 "hansi sunao", "hasi dikhao", "hansi dikhao",
                 "thoda has",   "thoda hass",  "thoda hans",
                 "zara has",    "zara hass",   "zara hans",
                 "ek baar has", "ek baar hass","ek baar hans",
                 "laugh for me","can you laugh","please laugh","make me laugh",
                 "हंस के दिखाओ",  "हंस के दिखा",   "हंसकर दिखाओ",  "हंसके दिखाओ",
                 "हस के दिखाओ",   "हस के दिखा",    "हसकर दिखाओ",   "हसके दिखाओ",
                 "हँस के दिखाओ",  "हँस के दिखा",   "हँसकर दिखाओ",  "हँसके दिखाओ",
                 "हंस के",        "हंस कर",
                 "हंसकर",        "हंसके",
                 "हंसने",         "हंसना",
                 "हंसी",         "हंस दो",
                 "हस के",        "हस कर",
                 "हसकर",        "हसके",
                 "हसने",         "हसना",
                 "हसी",         "हस दो",
                 "हँस के",       "हँस कर",
                 "हँसकर",       "हँसके",
                 "हँसने",        "हँसना",
                 "हँसी",        "हँस दो"}) ||
      hasWord(q, {"laugh", "laughing", "laughter", "giggle", "giggling",
                  "chuckle", "haso", "hanso", "hasso", "hasna", "hansna",
                  "hasne", "hansne", "hasao", "hansao", "hasi", "hansi",
                  "hass", "hans", "haste", "hanste", "हंस", "हंसो", "हंसते",
                  "हस", "हसो", "हसते", "हँस", "हँसो", "हँसते"});

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
    return executeCryAction();
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

  // Strict Negative filter: Never hijack light, LED, volume, or timer commands!
  if (hasWord(q, {"led", "leds", "light", "lights", "bulb", "lamp", "volume",
                  "timer", "alarm"}) ||
      hasAny(q, {"बत्ती", "लाइट", "एलईडी", "आवाज़", "वॉल्यूम"})) {
    return false;
  }

  bool isThemeIntent = hasWord(q, {"skin", "eyes", "eye skin", "looks", "theme",
                                   "look", "face"}) ||
                       hasAny(q, {"थीम", "स्किन", "आंखें", "चश्मा", "चेहरा"});
  bool isExplicitSkinWord = hasWord(q, {"skin", "eyes", "eye", "eye skin", "theme"}) ||
                            hasAny(q, {"थीम", "स्किन", "आंखें"});

  String name = "";
  EyeSkin targetSkin = currentEyeSkin;

  if (hasWord(q, {"desk buddy", "deskbuddy"}) ||
      hasAny(q, {"kiwi skin", "kiwi eyes", "kiwi theme", "cute skin", "cute eyes",
                 "default skin", "default eyes", "normal skin", "normal eyes",
                 "कीवी स्किन", "कीवी थीम"}) ||
      (isExplicitSkinWord && (hasWord(q, {"kiwi", "buddy", "cute", "default"}) ||
                              hasAny(q, {"कीवी"})))) {
    targetSkin = SKIN_KIWI;
    name = "Kiwi";
  } else if (!hasWord(q, {"phone", "camera", "photo", "picture"}) &&
             (hasWord(q, {"pixel", "pixel 8-bit", "arcade", "retro", "8bit",
                          "8-bit"}) ||
              hasAny(q, {"रेट्रो", "पिक्सेल"}))) {
    targetSkin = SKIN_PIXEL;
    name = "Pixel 8-Bit";
  } else if (hasAny(q, {"lumina spark", "squircle glint", "lumina"}) ||
             (isExplicitSkinWord && hasAny(q, {"spark", "glint"}))) {
    targetSkin = SKIN_LUMINA_SPARK;
    name = "Lumina Spark";
  } else if (hasWord(q, {"mecha", "circtangle", "circrect"}) ||
             hasAny(q, {"मेका", "मैका"})) {
    targetSkin = SKIN_MECHA;
    name = "Mecha";
  } else if (hasWord(q, {"emo-style", "emostyle"}) ||
             hasAny(q, {"emo style"}) ||
             (isExplicitSkinWord && (hasWord(q, {"emo"}) ||
                                     hasAny(q, {"ईमो", "इमो"})))) {
    targetSkin = SKIN_EMO;
    name = "EMO Style";
  } else if (hasWord(q, {"kiwi+", "kiwi_plus", "nova"}) ||
             hasAny(q, {"kiwi plus", "kiwi pro", "kiwi glow", "कीवी प्लस"}) ||
             (isExplicitSkinWord && hasWord(q, {"plus", "pro", "glow"}))) {
    targetSkin = SKIN_KIWI_PLUS;
    name = "Kiwi+";
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
    DisplayLockGuard lock(80);
    targetSkin = (EyeSkin)(((uint8_t)currentEyeSkin + 1) % SKIN_COUNT);
    const char *skinNames[SKIN_COUNT] = {"Kiwi", "Pixel 8-Bit", "Lumina Spark",
                                         "Mecha", "EMO Style", "Kiwi+"};
    name = skinNames[targetSkin];
  }

  if (name.length() == 0)
    return false;

  // Apply new skin immediately (<10ms!)
  Serial.printf("[CMD] Fast-path: eye skin -> %s\n", name.c_str());
  {
    DisplayLockGuard lock(80);
    currentEyeSkin = targetSkin;
  }
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
