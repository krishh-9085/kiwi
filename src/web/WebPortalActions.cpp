#include "animation/AnimationEngine.h"
#include "behavior/ActionExecutor.h"
#include "behavior/HungerEngine.h"
#include "config/AppConfig.h"
#include "config/ConfigManager.h"
#include "face/FaceDisplay.h"
#include "face/FaceEngine.h"
#include "face/FaceExpressions.h"
#include "face/SleepyWakeAnim.h"
#include "hardware/AudioEngine.h"
#include "hardware/RgbLed.h"
#include "hardware/WakeWordEngine.h"
#include "services/AiService.h"
#include "services/MemoryService.h"
#include "services/WeatherService.h"
#include "web/WebPortal.h"
#include "web/WebPortalApiInternal.h"
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>

void handleDoAction() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "*");

  if (server.method() == HTTP_OPTIONS) {
    server.send(204);
    return;
  }

  notifyWebRequest();
  lastUserInteractionAt = millis();

  // Web Mic Audio Record Trigger
  if (server.hasArg("record")) {
    String rVal = server.arg("record");
    rVal.trim();
    if (rVal == "0" || rVal.equalsIgnoreCase("stop") ||
        rVal.equalsIgnoreCase("cancel")) {
      startRecording = false;
      cancelRequest = true;
      busyProcessing = false;
      setEmotion(EMOTION_IDLE);
      robotStateDesc = "Ready";
      toneStop();
      spkAbortSpeech();
      spkEndSpeech();
      invalidateStatusCache();
      addTerminalLog("[WEB] Mic stop/cancel requested");
      server.send(200, "application/json", "{\"ok\":true,\"cancelled\":true}");
      return;
    }
    if (busyProcessing) {
      startRecording = false;
      cancelRequest = true;
      busyProcessing = false;
      setEmotion(EMOTION_IDLE);
      robotStateDesc = "Ready";
      toneStop();
      spkAbortSpeech();
      spkEndSpeech();
      invalidateStatusCache();
      addTerminalLog(
          "[WEB] Mic clicked while busy -> Cancel request / Interrupt");
      server.send(200, "application/json", "{\"ok\":true,\"cancelled\":true}");
      return;
    }
    preWakeReset();
    startRecording = true;
    invalidateStatusCache();
    addTerminalLog("[WEB] Mic record button clicked -> start listening");
    server.send(200, "application/json", "{\"ok\":true,\"recording\":true}");
    return;
  }

  // Web Text Prompt Trigger
  if (server.hasArg("ask")) {
    String q = server.arg("ask");
    q.trim();
    if (q.length() == 0) {
      server.send(200, "application/json",
                  "{\"ok\":false,\"reason\":\"empty\"}");
      return;
    }
    if (busyProcessing) {
      cancelRequest = true;
      addTerminalLog(
          "[WEB] Mic clicked while busy -> Cancel request / Interrupt");
      server.send(200, "application/json", "{\"ok\":true,\"cancelled\":true}");
      return;
    }
    webQueuedQuestion = q;
    addTerminalLog("[WEB] Queued prompt: \"" + q + "\"");
    server.send(200, "application/json", "{\"ok\":true,\"queued\":true}");
    return;
  }

  // Clear Terminal Output
  if (server.hasArg("clearlog")) {
    clearTerminalLog();
    server.send(200, "application/json", "{\"ok\":true,\"cleared\":true}");
    return;
  }

  // Hunger & Mood System Triggers
  if (server.hasArg("hunger")) {
    String mArg = server.arg("hunger");
    MealType m = MEAL_GENERIC;
    if (mArg == "breakfast")
      m = MEAL_BREAKFAST;
    else if (mArg == "lunch")
      m = MEAL_LUNCH;
    else if (mArg == "dinner")
      m = MEAL_DINNER;
    triggerHunger(m);
    server.send(200, "application/json", "{\"ok\":true,\"hunger\":true}");
    return;
  }
  if (server.hasArg("feed")) {
    bool ok = feedKiwi();
    server.send(200, "application/json",
                ok ? "{\"ok\":true,\"fed\":true}"
                   : "{\"ok\":false,\"reason\":\"not_hungry\"}");
    return;
  }
  if (server.hasArg("apologize") || server.hasArg("reconcile")) {
    bool ok = reconcileKiwi();
    server.send(200, "application/json",
                ok ? "{\"ok\":true,\"reconciled\":true}"
                   : "{\"ok\":false,\"reason\":\"not_angry\"}");
    return;
  }

  // Timer Control (cancel or start)
  if (server.hasArg("timer")) {
    String tCmd = server.arg("timer");
    if (tCmd == "cancel" || tCmd == "stop") {
      if (isRobotTimerRinging()) {
        stopRobotTimerAlarm();
      } else {
        cancelRobotTimer();
      }
      server.send(200, "application/json", "{\"ok\":true,\"timer\":false}");
      return;
    }
    int sec = tCmd.toInt();
    if (sec > 0) {
      startRobotTimer(sec, "Portal Timer");
      server.send(200, "application/json",
                  "{\"ok\":true,\"timer\":true,\"sec\":" + String(sec) + "}");
      return;
    }
  }

  // Soundboard FX
  if (server.hasArg("sound")) {
    String s = server.arg("sound");
    if (s == "fart") {
      setEmotion(EMOTION_SMUG);
      soundFart();
    } else if (s == "laser") {
      setEmotion(EMOTION_COOL);
      soundLaser();
    } else if (s == "fanfare") {
      setEmotion(EMOTION_PARTY);
      soundFanfare();
    } else if (s == "coin") {
      setEmotion(EMOTION_EXCITED);
      soundCoin();
    } else if (s == "giggle") {
      setEmotion(EMOTION_LAUGHING);
      soundPetGiggle();
    } else if (s == "violin") {
      setEmotion(EMOTION_CRYING);
      soundSadViolin();
    }
    returnIdleAt = millis() + 3000;
    server.send(200, "application/json", "{\"ok\":true}");
    return;
  }

  // Custom OLED Marquee Banner & Animations
  if (server.hasArg("animation")) {
    bannerActive = false;
    playAnimation((uint8_t)server.arg("animation").toInt());
    server.send(200, "application/json", "{\"ok\":true}");
    return;
  }
  if (server.hasArg("stopanimation")) {
    stopAnimation();
    setEmotion(EMOTION_IDLE);
    server.send(200, "application/json", "{\"ok\":true}");
    return;
  }
  if (server.hasArg("animations")) {
    server.send(200, "application/json", animationListJson());
    return;
  }
  if (server.hasArg("banner")) {
    stopAnimation();
    bannerText = server.arg("banner");
    bannerActive = true;
    bannerScrollX = SCREEN_WIDTH + 10;
    bannerLoops = 0;
    showClockScreen = false;
    showWeatherScreen = false;
    soundPetSparkle();
    server.send(200, "application/json", "{\"ok\":true}");
    return;
  }
  if (server.hasArg("stopbanner")) {
    bannerActive = false;
    setEmotion(EMOTION_IDLE);
    server.send(200, "application/json", "{\"ok\":true}");
    return;
  }

  // Autonomous Boredom Live Toggle
  if (server.hasArg("boredom")) {
    boredomEnabled = parsePortalBool(server.arg("boredom"));
    Preferences p;
    p.begin("myrobot", false);
    p.putBool("boredom", boredomEnabled);
    p.end();
    Serial.printf("[PORTAL] Boredom toggle live set to: %d\n", boredomEnabled);
    server.send(200, "application/json",
                boredomEnabled ? "{\"ok\":true,\"boredom\":true}"
                               : "{\"ok\":true,\"boredom\":false}");
    return;
  }

  // Autonomous Boredom Interval Live Action
  if (server.hasArg("boredom_mins")) {
    int m = server.arg("boredom_mins").toInt();
    if (m >= 1 && m <= 60) {
      boredomMinutes = m;
      Preferences p;
      p.begin("myrobot", false);
      p.putInt("boredom_mins", boredomMinutes);
      p.end();
      resetBoredomInterval();
      Serial.printf("[PORTAL] Boredom interval live set to: %d minutes\n",
                    boredomMinutes);
    }
    server.send(200, "application/json",
                "{\"ok\":true,\"boredom_mins\":" + String(boredomMinutes) +
                    "}");
    return;
  }

  // Custom OLED Eye Skin Action
  if (server.hasArg("skin")) {
    String s = server.arg("skin");
    currentEyeSkin = parseEyeSkinArg(s);
    s.toLowerCase();

    Preferences p;
    p.begin("myrobot", false);
    p.putInt("eyeskin", (int)currentEyeSkin);
    p.end();

    soundPetSparkle();
    Serial.printf("[PORTAL] Eye skin switched to: %d\n", (int)currentEyeSkin);
    server.send(200, "application/json",
                "{\"ok\":true,\"skin\":\"" + s + "\"}");
    return;
  }

  // Live Toggle / Set Sleep Mode
  if (server.hasArg("sleep")) {
    String slp = server.arg("sleep");
    if (slp == "toggle") {
      manualSleep = !manualSleep;
    } else {
      manualSleep = parsePortalBool(slp);
    }
    inCircadianSleep = manualSleep;
    if (manualSleep) {
      showClockScreen = false;
      showWeatherScreen = false;
      sleepyWakeCancel();
      sleepyFallAsleepStart();
    } else {
      sleepyFallAsleepCancel();
      setOledDim(false);
      updateDisplayBrightnessMode();
      sleepyWakeStart();
    }
    Serial.printf("[PORTAL] Manual sleep mode set to: %d\n", manualSleep);
    server.send(200, "application/json",
                "{\"ok\":true,\"sleeping\":" +
                    String(manualSleep ? "true" : "false") + "}");
    return;
  }

  // Live Circadian Night Sleep Schedule
  if (server.hasArg("circ_en") || server.hasArg("circ_start") ||
      server.hasArg("circ_end")) {
    Preferences p;
    p.begin("myrobot", false);
    if (server.hasArg("circ_en")) {
      circadianSleepEnabled = parsePortalBool(server.arg("circ_en"));
      p.putBool("circ_en", circadianSleepEnabled);
      Serial.printf("[PORTAL] Circadian sleep toggle live set to: %d\n",
                    circadianSleepEnabled);
    }
    if (server.hasArg("circ_start")) {
      int s = server.arg("circ_start").toInt();
      if (s >= 0 && s <= 23) {
        circadianSleepStart = s;
        p.putInt("circ_start", circadianSleepStart);
        Serial.printf("[PORTAL] Circadian start hour set to: %d\n",
                      circadianSleepStart);
      }
    }
    if (server.hasArg("circ_end")) {
      int e = server.arg("circ_end").toInt();
      if (e >= 0 && e <= 23) {
        circadianSleepEnd = e;
        p.putInt("circ_end", circadianSleepEnd);
        Serial.printf("[PORTAL] Circadian end hour set to: %d\n",
                      circadianSleepEnd);
      }
    }
    p.end();
    server.send(200, "application/json",
                "{\"ok\":true,\"circ_en\":" +
                    String(circadianSleepEnabled ? "true" : "false") +
                    ",\"circ_start\":" + String(circadianSleepStart) +
                    ",\"circ_end\":" + String(circadianSleepEnd) + "}");
    return;
  }

  // Live Greetings Toggle
  if (server.hasArg("greet_en")) {
    greetingsEnabled = parsePortalBool(server.arg("greet_en"));
    Preferences p;
    p.begin("myrobot", false);
    p.putBool("greet_en", greetingsEnabled);
    p.end();
    Serial.printf("[PORTAL] Greetings toggle live set to: %d\n",
                  greetingsEnabled);
    server.send(200, "application/json",
                "{\"ok\":true,\"greet_en\":" +
                    String(greetingsEnabled ? "true" : "false") + "}");
    return;
  }

  // Live Hunger Routine Toggle
  if (server.hasArg("hunger_en")) {
    hungerEnabled = parsePortalBool(server.arg("hunger_en"));
    Preferences p;
    p.begin("myrobot", false);
    p.putBool("hunger_en", hungerEnabled);
    p.end();
    if (!hungerEnabled && (isKiwiHungry() || isKiwiAngry())) {
      reconcileKiwi();
    }
    Serial.printf("[PORTAL] Hunger toggle live set to: %d\n", hungerEnabled);
    server.send(200, "application/json",
                "{\"ok\":true,\"hunger_en\":" +
                    String(hungerEnabled ? "true" : "false") + "}");
    return;
  }

  // Live Sound-Reactive Face Toggle
  if (server.hasArg("snd_react")) {
    soundReactiveEnabled = parsePortalBool(server.arg("snd_react"));
    Preferences p;
    p.begin("myrobot", false);
    p.putBool("snd_react", soundReactiveEnabled);
    p.end();
    Serial.printf("[PORTAL] Sound reactive face live set to: %d\n",
                  soundReactiveEnabled);
    server.send(200, "application/json",
                "{\"ok\":true,\"snd_react\":" +
                    String(soundReactiveEnabled ? "true" : "false") + "}");
    return;
  }

  // Live Wake-Word Toggle & Cutoff Tuning
  if (server.hasArg("ww_en")) {
    wakeWordEnabled = parsePortalBool(server.arg("ww_en"));
    wakeWordSetEnabled(wakeWordEnabled);
    Preferences p;
    p.begin("myrobot", false);
    p.putBool("ww_en", wakeWordEnabled);
    p.end();
    Serial.printf("[PORTAL] Wake-word live set to: %d\n", wakeWordEnabled);
    server.send(200, "application/json",
                "{\"ok\":true,\"ww_en\":" +
                    String(wakeWordEnabled ? "true" : "false") + "}");
    return;
  }

  if (server.hasArg("ww_cutoff")) {
    wakeWordCutoff = server.arg("ww_cutoff").toFloat();
    wakeWordSetSensitivity(wakeWordCutoff);
    Preferences p;
    p.begin("myrobot", false);
    p.putFloat("ww_cutoff", wakeWordCutoff);
    p.end();
    Serial.printf("[PORTAL] Wake-word cutoff set to: %.2f\n", wakeWordCutoff);
    server.send(200, "application/json",
                "{\"ok\":true,\"ww_cutoff\":" + String(wakeWordCutoff, 2) +
                    "}");
    return;
  }

  // Live Wake-Word Model Switch (0=Alexa)
  if (server.hasArg("ww_model")) {
    uint8_t mIdx = 0; // Alexa
    bool ok = wakeWordSetModel(mIdx);
    if (!ok) {
      server.send(200, "application/json",
                  "{\"ok\":false,\"error\":\"Model switch failed\"}");
      return;
    }
    Preferences p;
    p.begin("myrobot", false);
    p.putUChar("ww_model", wakeWordModel);
    p.putFloat("ww_cutoff", wakeWordCutoff);
    p.end();
    Serial.printf(
        "[PORTAL] Wake-word model switched to: %d (%s), cutoff: %.2f\n",
        wakeWordModel, wakeWordGetModelName(), wakeWordCutoff);
    server.send(200, "application/json",
                "{\"ok\":true,\"ww_model\":" + String(wakeWordModel) +
                    ",\"name\":\"" + String(wakeWordGetModelName()) +
                    "\",\"cutoff\":" + String(wakeWordCutoff, 2) + "}");
    return;
  }

  // Live Wake-Word Gain Tuning (0.5 to 20.0, default 8.0)
  if (server.hasArg("ww_gain")) {
    float g = server.arg("ww_gain").toFloat();
    wakeWordSetGain(g);
    Preferences p;
    p.begin("myrobot", false);
    p.putFloat("ww_gain", wakeWordGain);
    p.end();
    server.send(200, "application/json",
                "{\"ok\":true,\"ww_gain\":" + String(wakeWordGain, 2) + "}");
    return;
  }

  // Live Wake-Word Secondary Spike Clause Toggle (default false)
  if (server.hasArg("ww_spike")) {
    bool sp = parsePortalBool(server.arg("ww_spike"));
    wakeWordSetSpikeTrigger(sp);
    Preferences p;
    p.begin("myrobot", false);
    p.putBool("ww_spike", wakeWordSpikeTrigger);
    p.end();
    server.send(200, "application/json",
                "{\"ok\":true,\"ww_spike\":" +
                    String(wakeWordSpikeTrigger ? "true" : "false") + "}");
    return;
  }

  // Live STT Adaptive Endpointer Parameters Tuning
  if (server.hasArg("stt_trail_ms") || server.hasArg("stt_noise_k") ||
      server.hasArg("stt_rel_k") || server.hasArg("stt_max_sil")) {
    if (server.hasArg("stt_trail_ms")) {
      int v = server.arg("stt_trail_ms").toInt();
      if (v >= 100 && v <= 3000)
        sttTrailingMs = v;
    }
    if (server.hasArg("stt_noise_k")) {
      float v = server.arg("stt_noise_k").toFloat();
      if (v >= 1.1f && v <= 10.0f)
        sttNoiseK = v;
    }
    if (server.hasArg("stt_rel_k")) {
      float v = server.arg("stt_rel_k").toFloat();
      if (v >= 0.0f && v <= 1.0f)
        sttRelK = v;
    }
    if (server.hasArg("stt_max_sil")) {
      int v = server.arg("stt_max_sil").toInt();
      if (v >= 300 && v <= 5000)
        sttMaxSilenceMs = v;
    }
    saveConfig();
    server.send(200, "application/json", "{\"ok\":true}");
    return;
  }

  // Live Microphone Slot Selection (0=Left, 1=Right)
  if (server.hasArg("micslot")) {
    int s = server.arg("micslot").toInt();
    setMicSlot(s, true, true);
    server.send(200, "application/json",
                "{\"ok\":true,\"micslot\":" + String(getMicSlot()) + "}");
    return;
  }

  // Live Microphone Slot Lock Toggle
  if (server.hasArg("micslot_lock")) {
    bool lk = parsePortalBool(server.arg("micslot_lock"));
    setMicSlotLocked(lk);
    Preferences p;
    p.begin("myrobot", false);
    p.putBool("micslot_lock", lk);
    p.end();
    server.send(200, "application/json",
                "{\"ok\":true,\"micslot_lock\":" +
                    String(isMicSlotLocked() ? "true" : "false") + "}");
    return;
  }

  // Live RGB LED (Pin 48) Toggle
  if (server.hasArg("rgb_en")) {
    rgbLedEnabled = parsePortalBool(server.arg("rgb_en"));
    rgbLedSetEnabled(rgbLedEnabled);
    Preferences p;
    p.begin("myrobot", false);
    p.putBool("rgb_en", rgbLedEnabled);
    p.end();
    Serial.printf("[PORTAL] Pin 48 RGB LED live set to: %d\n", rgbLedEnabled);
    server.send(200, "application/json",
                "{\"ok\":true,\"rgb_en\":" +
                    String(rgbLedEnabled ? "true" : "false") + "}");
    return;
  }

  // Live RGB LED (Pin 48) Brightness (0-100%)
  if (server.hasArg("rgb_bright")) {
    int bPct = server.arg("rgb_bright").toInt();
    if (bPct < 0)
      bPct = 0;
    if (bPct > 100)
      bPct = 100;
    rgbLedBrightness = (uint8_t)((bPct * 255) / 100);
    rgbLedSetBrightness(rgbLedBrightness);
    Preferences p;
    p.begin("myrobot", false);
    p.putUChar("rgb_bright", rgbLedBrightness);
    p.end();
    Serial.printf("[PORTAL] Pin 48 RGB LED brightness set to: %d%% (%d/255)\n",
                  bPct, rgbLedBrightness);
    server.send(200, "application/json",
                "{\"ok\":true,\"rgb_bright\":" + String(bPct) + "}");
    return;
  }

  // Live RGB LED (Pin 48) Custom Color (Hex "#RRGGBB", "RRGGBB", or
  // "%23RRGGBB")
  if (server.hasArg("rgb_color")) {
    String c = server.arg("rgb_color");
    c.trim();
    if (c.startsWith("%23"))
      c = c.substring(3);
    else if (c.startsWith("#"))
      c = c.substring(1);
    long hexVal = strtol(c.c_str(), NULL, 16);
    uint8_t r = (hexVal >> 16) & 0xFF;
    uint8_t g = (hexVal >> 8) & 0xFF;
    uint8_t b = hexVal & 0xFF;
    rgbLedSetOverride(r, g, b);
    Serial.printf(
        "[PORTAL] Pin 48 RGB LED color set to: #%06X (R:%d, G:%d, B:%d)\n",
        (unsigned int)hexVal, r, g, b);
    server.send(200, "application/json",
                "{\"ok\":true,\"rgb_color\":\"#" + c + "\"}");
    return;
  }

  // Live RGB LED (Pin 48) Clear Custom Override (Restore auto mood sync)
  if (server.hasArg("rgb_clear")) {
    rgbLedClearOverride();
    Serial.println(
        "[PORTAL] Pin 48 RGB LED override cleared (auto mood sync restored)");
    server.send(200, "application/json", "{\"ok\":true,\"rgb_clear\":true}");
    return;
  }

  // Live Toggle Mouth & Lips
  if (server.hasArg("mouth")) {
    showMouth = parsePortalBool(server.arg("mouth"));
    Preferences p;
    p.begin("myrobot", false);
    p.putBool("showmouth", showMouth);
    p.end();
    server.send(
        200, "application/json",
        "{\"ok\":true,\"mouth\":" + String(showMouth ? "true" : "false") + "}");
    return;
  }

  // Live Display Day Brightness Action
  if (server.hasArg("brightness")) {
    int b = server.arg("brightness").toInt();
    if (b < 0)
      b = 0;
    if (b > 100)
      b = 100;
    displayBrightness = b;
    Preferences p;
    p.begin("myrobot", false);
    p.putInt("disp_bright", displayBrightness);
    p.end();
    updateDisplayBrightnessMode();
    Serial.printf("[PORTAL] Day Brightness live set to: %d%%\n",
                  displayBrightness);
    server.send(200, "application/json",
                "{\"ok\":true,\"brightness\":" + String(displayBrightness) +
                    "}");
    return;
  }

  // Live Display Night Brightness Action
  if (server.hasArg("night_bright")) {
    int nb = server.arg("night_bright").toInt();
    if (nb < 0)
      nb = 0;
    if (nb > 100)
      nb = 100;
    nightBrightness = nb;
    Preferences p;
    p.begin("myrobot", false);
    p.putInt("night_bright", nightBrightness);
    p.end();
    updateDisplayBrightnessMode();
    Serial.printf("[PORTAL] Night Brightness live set to: %d%%\n",
                  nightBrightness);
    server.send(200, "application/json",
                "{\"ok\":true,\"night_bright\":" + String(nightBrightness) +
                    "}");
    return;
  }

  // Live Night Mode Action (0: Same as day, 1: Dim, 2: Screen off)
  if (server.hasArg("night_mode")) {
    int nm = server.arg("night_mode").toInt();
    if (nm >= 0 && nm <= 2) {
      nightMode = nm;
      Preferences p;
      p.begin("myrobot", false);
      p.putInt("night_mode", nightMode);
      p.end();
      updateDisplayBrightnessMode();
      Serial.printf("[PORTAL] Night Mode live set to: %d\n", nightMode);
      server.send(200, "application/json",
                  "{\"ok\":true,\"night_mode\":" + String(nightMode) + "}");
      return;
    }
  }

  // Live Speaker Volume Action
  if (server.hasArg("vol") || server.hasArg("volume")) {
    int v = server.hasArg("vol") ? server.arg("vol").toInt()
                                 : server.arg("volume").toInt();
    if (v < 0)
      v = 0;
    if (v > 100)
      v = 100;
    speakerVolume = v;
    Preferences p;
    p.begin("myrobot", false);
    p.putInt("volume", speakerVolume);
    p.end();
    Serial.printf("[PORTAL] Speaker volume live set to: %d%%\n", speakerVolume);
    server.send(200, "application/json",
                "{\"ok\":true,\"volume\":" + String(speakerVolume) + "}");
    return;
  }

  // Memory & Diary Actions
  if (server.hasArg("clearmem")) {
    clearAllMemory();
    invalidateStatusCache();
    server.send(200, "application/json", "{\"ok\":true,\"cleared\":true}");
    return;
  }
  if (server.hasArg("delmem")) {
    int idx = server.arg("delmem").toInt();
    removeMemoryFact(idx);
    invalidateStatusCache();
    server.send(200, "application/json", "{\"ok\":true,\"deleted\":true}");
    return;
  }
  if (server.hasArg("addmem")) {
    bool added = addMemoryFact(server.arg("addmem"));
    invalidateStatusCache();
    server.send(200, "application/json",
                added ? "{\"ok\":true}" : "{\"ok\":false,\"duplicate\":true}");
    return;
  }
  if (server.hasArg("setname")) {
    setOwnerName(server.arg("setname"));
    invalidateStatusCache();
    server.send(200, "application/json", "{\"ok\":true}");
    return;
  }
  if (server.hasArg("setfriends")) {
    setFriendsFamily(server.arg("setfriends"));
    invalidateStatusCache();
    server.send(200, "application/json", "{\"ok\":true}");
    return;
  }
  if (server.hasArg("clearfollowup")) {
    clearPendingFollowUp();
    invalidateStatusCache();
    server.send(200, "application/json", "{\"ok\":true,\"cleared\":true}");
    return;
  }

  String e = server.hasArg("e") ? server.arg("e") : "";
  uint32_t ms =
      server.hasArg("ms") ? (uint32_t)server.arg("ms").toInt() : 60000;
  bool snd = server.hasArg("s") ? (server.arg("s").toInt() != 0) : true;
  if (busyProcessing) {
    server.send(200, "application/json", "{\"ok\":false,\"reason\":\"busy\"}");
    return;
  }
  if (e == "sleep" || e == "sleeping") {
    manualSleep = true;
    inCircadianSleep = true;
    showClockScreen = false;
    showWeatherScreen = false;
    sleepyWakeCancel();
    sleepyFallAsleepStart();
    server.send(200, "application/json", "{\"ok\":true,\"sleeping\":true}");
    return;
  }
  if (e == "wake") {
    manualSleep = false;
    inCircadianSleep = false;
    sleepyFallAsleepCancel();
    setOledDim(false);
    updateDisplayBrightnessMode();
    sleepyWakeStart();
    server.send(200, "application/json", "{\"ok\":true,\"sleeping\":false}");
    return;
  }
  if (e == "clock") {
    manualSleep = false;
    inCircadianSleep = false;
    setOledDim(false);
    showClockScreen = true;
    showWeatherScreen = false;
    if (snd)
      soundClock();
    if (!timeSynced && WiFi.status() == WL_CONNECTED)
      syncTimeFromNTP();
    server.send(200, "application/json", "{\"ok\":true}");
    return;
  }
  if (e == "weather") {
    manualSleep = false;
    inCircadianSleep = false;
    setOledDim(false);
    showWeatherScreen = true;
    showClockScreen = false;
    if (snd)
      soundWeather();
    if (lastWeatherFetchAt == 0 ||
        millis() - lastWeatherFetchAt > 10 * 60 * 1000 || weatherCode < 0) {
      fetchWeather();
    }
    server.send(200, "application/json", "{\"ok\":true}");
    return;
  }
  if (e == "clear" || e == "forget") {
    clearConversationHistory();
    server.send(200, "application/json", "{\"ok\":true,\"cleared\":true}");
    return;
  }
  bool ok = triggerEmotionByName(e, ms, snd);
  server.send(200, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false}");
}
