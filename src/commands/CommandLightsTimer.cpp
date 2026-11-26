#include "commands/CommandHandler.h"
#include "commands/ActionExecutor.h"
#include "config/AppConfig.h"
#include "face/FaceEngine.h"
#include "hardware/AudioEngine.h"
#include "hardware/RgbLed.h"
#include "ai/AiService.h"
#include "integrations/HyperHdrService.h"
#include "integrations/WledService.h"

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

  // Negative filter: Do not hijack explanatory / educational questions about timers
  if (hasAny(q, {"how does", "what is", "explain", "ic ", "555 timer",
                 "kya hota", "kaam kaise"})) {
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
        if (val < 86400) {
          val = val * 10 + (q[i] - '0');
          if (val > 86400)
            val = 86400;
        }
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

  // Onboard RGB LED specific controls (must run before generic on/off/color)
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

  bool hasExplicitColorWord = hasWord(
      q, {"red", "laal", "green", "hara", "haree", "blue", "neela", "neeli",
          "yellow", "peela", "peeli", "pink", "gulabi", "purple", "violet",
          "baingani", "orange", "narangi", "cyan", "aqua", "firozi", "white",
          "safed", "warm", "cool"});

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

  // 2. Turn ON (skip if caller asked for a specific color like "turn on red light")
  if (!hasExplicitColorWord &&
      (hasWord(q, {"on", "open"}) ||
       hasAny(q, {"chalu", "turn on", "switch on", "jalao", "on kar",
                  "open light", "चालू", "जलाओ"}))) {
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
