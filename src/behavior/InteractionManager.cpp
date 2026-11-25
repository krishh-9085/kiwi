#include "behavior/InteractionManager.h"
#include "behavior/ActionExecutor.h"
#include "behavior/CommandHandler.h"
#include "config/AppConfig.h"
#include "config/ConfigManager.h"
#include "core/CoreTypes.h"
#include "face/BootWakeAnim.h"
#include "face/FaceEngine.h"
#include "hardware/AudioEngine.h"
#include "hardware/RgbLed.h"
#include "hardware/TouchController.h"
#include "power/PowerManager.h"
#include "services/AiService.h"
#include "web/WebPortal.h"

// ============================================================
// UI PUMP (Display + Touch + Tone Update)
// ============================================================
void pumpUI() {
  powerNotifyActivity();
  bootAnimPump();
  if (!uiTaskRunning) {
    rgbLedUpdate();
    tickFace();
  }
  updateTouch();
  updateTonePlayer();
  serviceConfigManager();
  pumpWeb();
}

// ============================================================
// PET PHYSICAL TOUCH REACTIONS
// ============================================================
void triggerPetReaction() {
  if (busyProcessing)
    return;
  petMood = (PetMood)random(0, PET_MOOD_COUNT - 1); // Exclude PET_SURPRISED
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
  default:
    flourish = FL_SPARKLE;
    soundPetSparkle();
    break;
  }
  emotionAutoReturnAt = millis() + 3500;
}

// ============================================================
// QUESTION & AI INTERACTION PIPELINE
// ============================================================
// ============================================================
// FISH AUDIO & LLM VOCAL EMOTION DETECTOR
// Maps Fish Audio tags ([happy], [laugh], [whisper], etc.) to Kiwi emotions
// ============================================================
Emotion detectVocalEmotion(const String &text) {
  String lower = text;
  lower.toLowerCase();

  auto hasTag = [&](const char *tag) -> bool {
    String t = tag;
    return (lower.indexOf("[" + t + "]") >= 0 ||
            lower.indexOf("(" + t + ")") >= 0);
  };

  // 1. Laughing / Amused -> Show laughing expression
  if (hasTag("laugh") || hasTag("laughs") || hasTag("laughing") ||
      hasTag("chuckle") || hasTag("chuckles") || hasTag("chuckling") ||
      hasTag("giggle") || hasTag("giggles") || hasTag("giggling")) {
    return EMOTION_LAUGHING;
  }

  // 2. Crying / Sobbing / Heartbroken
  if (hasTag("crying") || hasTag("cry") || hasTag("sob") || hasTag("sobbing") ||
      hasTag("crying loudly") || hasTag("tears")) {
    return EMOTION_CRYING;
  }

  // 3. Anger / Frustration
  if (hasTag("angry") || hasTag("furious") || hasTag("frustrated") ||
      hasTag("annoyed") || hasTag("mad")) {
    return EMOTION_ANGRY;
  }

  // 4. Sadness / Melancholy
  if (hasTag("sad") || hasTag("melancholic") || hasTag("gloomy") ||
      hasTag("heartbroken")) {
    return EMOTION_SAD;
  }

  // 5. Excited / Energetic / Celebration
  if (hasTag("excited") || hasTag("enthusiastic") || hasTag("celebration") ||
      hasTag("energetic")) {
    return EMOTION_EXCITED;
  }

  // 6. Happy / Joy / Delighted / Satisfied -> Keep natural speaking/idle face
  // (no ugly squinch)
  if (hasTag("happy") || hasTag("delighted") || hasTag("satisfied") ||
      hasTag("cheerful") || hasTag("joyful") || hasTag("glad")) {
    return EMOTION_SPEAKING;
  }

  // 7. Surprised / Shocked / Gasp
  if (hasTag("surprised") || hasTag("shocked") || hasTag("amazed") ||
      hasTag("astonished") || hasTag("gasp") || hasTag("gasps")) {
    return EMOTION_SURPRISED;
  }

  // 8. Scared / Fearful / Worried
  if (hasTag("scared") || hasTag("fearful") || hasTag("terrified") ||
      hasTag("worried") || hasTag("frightened")) {
    return EMOTION_SCARED;
  }

  // 9. Nervous / Anxious / Embarrassed / Hesitant
  if (hasTag("nervous") || hasTag("anxious") || hasTag("embarrassed") ||
      hasTag("hesitant") || hasTag("shy")) {
    return EMOTION_NERVOUS;
  }

  // 10. Whispering / Soft / Breathy (Cute face)
  if (hasTag("whisper") || hasTag("whispers") || hasTag("whispering") ||
      hasTag("soft") || hasTag("breathy") || hasTag("gentle")) {
    return EMOTION_CUTE;
  }

  // 11. Relieved / Calm / Sigh / Groan
  if (hasTag("calm") || hasTag("relieved") || hasTag("peaceful") ||
      hasTag("relaxed") || hasTag("sigh") || hasTag("sighs") ||
      hasTag("sighing") || hasTag("groan") || hasTag("groaning") ||
      hasTag("pant") || hasTag("panting")) {
    return EMOTION_RELIEVED;
  }

  // 12. Proud / Confident / Assertive
  if (hasTag("proud") || hasTag("confident") || hasTag("assertive")) {
    return EMOTION_PROUD;
  }

  // 13. Bored / Tired / Sleepy
  if (hasTag("bored") || hasTag("tired") || hasTag("exhausted") ||
      hasTag("sleepy") || hasTag("yawn")) {
    return EMOTION_BORED;
  }

  // 14. Curious / Confused / Inquisitive
  if (hasTag("curious") || hasTag("confused") || hasTag("puzzled")) {
    return EMOTION_CURIOUS;
  }

  return EMOTION_SPEAKING;
}

#if LLM_STREAM_TTS
struct StreamSpeakerState {
  bool firstSpoken = false;
  bool ttsFailed = false;
  bool streamActive = false;
  String pending = "";
  uint32_t questionStartedAt = 0;
  bool anySpoken = false;
  bool heapDecided = false;
  bool heapOk = false;
  bool ttsInProgress = false;

  void reset(uint32_t startedAt) {
    firstSpoken = false;
    ttsFailed = false;
    streamActive = true;
    pending = "";
    questionStartedAt = startedAt;
    anySpoken = false;
    heapDecided = false;
    heapOk = false;
    ttsInProgress = false;
  }
};

static StreamSpeakerState streamSpk;

static void streamSentenceCallback(const String &sentence, bool isFinal) {
  // a. If cancelRequest is true -> clear pending, return.
  if (cancelRequest) {
    streamSpk.pending = "";
    return;
  }

  // b. If sentence is empty -> return.
  if (sentence.length() == 0) {
    return;
  }

  // When isFinal arrives, LLM stream is finishing right now.
  // On PSRAM with dual-TLS support, if first sentence hasn't spoken yet (e.g.
  // 1-sentence answer), speak it immediately without waiting for LLM connection
  // teardown!
  if (isFinal) {
    if (psramFound() && !streamSpk.firstSpoken) {
      if (streamSpk.pending.length() > 0) {
        streamSpk.pending += " ";
      }
      streamSpk.pending += sentence;
      Emotion vocalEmotion = detectVocalEmotion(streamSpk.pending);
      if (vocalEmotion != EMOTION_SPEAKING) {
        setEmotion(vocalEmotion);
      } else {
        setEmotion(EMOTION_SPEAKING);
      }
      robotStateDesc = "Speaking...";
      Serial.printf(
          "[LATENCY] question->first_audio=%lu ms (PSRAM 1-turn direct)\n",
          (unsigned long)(millis() - streamSpk.questionStartedAt));
      streamSpk.firstSpoken = true;
      streamSpk.ttsInProgress = true;
      bool ok = speakText(streamSpk.pending);
      streamSpk.ttsInProgress = false;
      if (ok) {
        streamSpk.anySpoken = true;
      } else {
        streamSpk.ttsFailed = true;
      }
      streamSpk.pending = "";
      return;
    }

    if (streamSpk.pending.length() > 0) {
      streamSpk.pending += " ";
    }
    streamSpk.pending += sentence;
    return;
  }

  // c. Decide heap mode ONCE per question at the first sentence:
  //      heapOk = ESP.getMaxAllocHeap() >= STREAM_TTS_MIN_MAX_ALLOC (or PSRAM
  //      available)
  //    Log: "[STREAM] maxAlloc=%u psram=%u heapOk=%d" and, at first audio,
  //    "[LATENCY] question->first_audio=%lu ms".
  if (!streamSpk.heapDecided) {
    uint32_t maxAlloc = ESP.getMaxAllocHeap();
    streamSpk.heapOk = psramFound() ? (ESP.getFreePsram() >= 65536)
                                    : (maxAlloc >= STREAM_TTS_MIN_MAX_ALLOC);
    streamSpk.heapDecided = true;
    Serial.printf("[STREAM] maxAlloc=%u psram=%u heapOk=%d\n",
                  (unsigned)maxAlloc, (unsigned)ESP.getFreePsram(),
                  streamSpk.heapOk ? 1 : 0);
  }

  // d. If heapOk == false -> just append to pending (no TTS during stream).
  if (!streamSpk.heapOk) {
    if (streamSpk.pending.length() > 0) {
      streamSpk.pending += " ";
    }
    streamSpk.pending += sentence;
    return;
  }

  // f. Never call speakText while another speakText is in progress.
  if (streamSpk.ttsInProgress) {
    if (streamSpk.pending.length() > 0) {
      streamSpk.pending += " ";
    }
    streamSpk.pending += sentence;
    return;
  }

  // e. If heapOk == true:
  //      - First sentence (firstSpoken == false): call speakText(sentence)
  //        immediately if contiguous heap is safe. Set firstSpoken = true. If
  //        it returns false, set ttsFailed = true and append the sentence to
  //        pending.
  if (!streamSpk.firstSpoken) {
    // Contiguous heap protection: Ensure at least STREAM_TTS_MIN_MAX_ALLOC
    // contiguous bytes before dual-TLS (when no PSRAM)
    if (!psramFound() && ESP.getMaxAllocHeap() < STREAM_TTS_MIN_MAX_ALLOC) {
      Serial.printf("[STREAM] Contiguous heap low for first sentence (%u < "
                    "%u). Deferring.\n",
                    (unsigned)ESP.getMaxAllocHeap(),
                    (unsigned)STREAM_TTS_MIN_MAX_ALLOC);
      if (streamSpk.pending.length() > 0) {
        streamSpk.pending += " ";
      }
      streamSpk.pending += sentence;
      return;
    }

    Emotion vocalEmotion = detectVocalEmotion(sentence);
    if (vocalEmotion != EMOTION_SPEAKING) {
      setEmotion(vocalEmotion);
    } else {
      setEmotion(EMOTION_SPEAKING);
    }
    robotStateDesc = "Speaking...";

    Serial.printf(
        "[LATENCY] question->first_audio=%lu ms (PSRAM dual-TLS parallel)\n",
        (unsigned long)(millis() - streamSpk.questionStartedAt));

    streamSpk.firstSpoken = true;
    streamSpk.ttsInProgress = true;
    bool ok = speakText(sentence);
    streamSpk.ttsInProgress = false;

    if (ok) {
      streamSpk.anySpoken = true;
    } else {
      streamSpk.ttsFailed = true;
      if (streamSpk.pending.length() > 0) {
        streamSpk.pending += " ";
      }
      streamSpk.pending += sentence;
    }
    return;
  }

  //      - Later sentences: append to pending. Flush pending with speakText()
  //        only when pending.length() >= threshold AND heap is safe.
  //        Do not call speakText on every sentence (each call is a new TLS
  //        handshake).
  if (streamSpk.pending.length() > 0) {
    streamSpk.pending += " ";
  }
  streamSpk.pending += sentence;

  size_t flushThreshold = psramFound() ? 60 : 120;
  if (!streamSpk.ttsFailed && streamSpk.pending.length() >= flushThreshold) {
    if (cancelRequest) {
      streamSpk.pending = "";
      return;
    }
    // Contiguous heap protection: Check heap before initiating secondary TLS
    // handshake
    if (!psramFound() && ESP.getMaxAllocHeap() < STREAM_TTS_MIN_MAX_ALLOC) {
      Serial.printf("[STREAM] Dual-TLS heap tight (maxAlloc=%u < %u). "
                    "Deferring to final flush.\n",
                    (unsigned)ESP.getMaxAllocHeap(),
                    (unsigned)STREAM_TTS_MIN_MAX_ALLOC);
      return;
    }
    Emotion vocalEmotion = detectVocalEmotion(streamSpk.pending);
    if (vocalEmotion != EMOTION_SPEAKING) {
      setEmotion(vocalEmotion);
    }
    streamSpk.ttsInProgress = true;
    bool ok = speakText(streamSpk.pending);
    streamSpk.ttsInProgress = false;

    if (ok) {
      streamSpk.anySpoken = true;
      streamSpk.pending = "";
    } else {
      streamSpk.ttsFailed = true;
    }
  }
}
#endif

void handleQuestion(const String &qIn) {
  String q = cleanTranscribedQuestion(qIn);
  uint32_t questionStartedAt = millis();
  lastUserInteractionAt = questionStartedAt;
  Serial.print("[ROBOT] Q: ");
  Serial.println(q);
  if (cancelRequest)
    return;

  // ============================================================
  // ZERO-LATENCY OFFLINE LOCAL INTENT FAST-PATH (<50ms)
  // Executes locally on ESP32 without touching cloud LLM
  // ============================================================
  if (handleTimerCommand(q))
    return;
  if (handleTimeCommand(q))
    return;
  if (handleVolumeCommand(q))
    return;
  if (handleLightCommand(q))
    return;
  if (handleSkinCommand(q))
    return;
  if (handleSleepCommand(q))
    return;
  if (handleWeatherCommand(q))
    return;
  if (handleEmotionCommand(q))
    return;
  if (handleHungerCommand(q))
    return;
  if (hasAny(q, {"new topic", "naya topic", "naya sawal", "reset chat",
                 "clear memory", "bhool jao", "forget that",
                 "forget everything"})) {
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
  addTerminalLog("[LLM] Sending to " +
                 (llmProvider == "groq" ? groqModel : llmModel) + ": \"" + q +
                 "\"");

#if LLM_STREAM_TTS
  streamSpk.reset(questionStartedAt);
  SentenceCallback streamCb = streamSentenceCallback;
#else
  // Single-core ESP32-C3: Complete LLM cleanly first so max heap (~75KB) is
  // available for TLS TTS and no simultaneous TLS collisions or socket stalling
  // occurs.
  SentenceCallback streamCb = nullptr;
#endif

  String answer = askLLM(q, streamCb);
#if LLM_STREAM_TTS
  streamSpk.streamActive = false;
#endif
  if (cancelRequest)
    return;
  if (!answer.length()) {
#if LLM_STREAM_TTS
    if (!cancelRequest && streamSpk.pending.length() > 0) {
      Emotion vocalEmotion = detectVocalEmotion(streamSpk.pending);
      if (vocalEmotion != EMOTION_SPEAKING) {
        setEmotion(vocalEmotion);
      }
      streamSpk.ttsInProgress = true;
      bool ok = speakText(streamSpk.pending);
      streamSpk.ttsInProgress = false;
      if (ok) {
        streamSpk.anySpoken = true;
      }
      streamSpk.pending = "";
    }
    if (streamSpk.anySpoken) {
      robotStateDesc = "Ready";
      addTerminalLog("[ROBOT] Turn completed. Ready.");
      setEmotion(EMOTION_IDLE);
      returnIdleAt = 0;
      emotionAutoReturnAt = 0;
      return;
    }
#endif
    if (lastActionsExecuted > 0) {
      robotStateDesc = "Ready";
      addTerminalLog("[ROBOT] Action completed.");
      // Preserve timed emotions triggered by [ACTION: emotion/sound ...]
      if (returnIdleAt == 0 && emotionAutoReturnAt == 0) {
        setEmotion(EMOTION_IDLE);
      }
      return;
    }
    robotStateDesc = "LLM Empty / Error";
    addTerminalLog("[LLM] Empty response or error received");
    Serial.println("[ROBOT] LLM empty");
    lastLlmAnswer =
        "Error: No response from AI. Please check Wi-Fi, model, or API key.";
    setEmotion(EMOTION_CONFUSED);
    speakText("Sorry, abhi answer nahi de pa raha hoon. Ek baar firse poocho!");
    returnIdleAt = millis() + 2000;
    return;
  }

  Serial.print("[ROBOT] A: ");
  Serial.println(answer);
  lastLlmAnswer = answer;
  addTerminalLog("[LLM] Answer: \"" + answer + "\"");

  robotStateDesc = "Speaking...";
  {
    bool actionHeldEmotion =
        (returnIdleAt > millis() || emotionAutoReturnAt > millis()) &&
        currentEmotion != EMOTION_THINKING &&
        currentEmotion != EMOTION_LISTENING && currentEmotion != EMOTION_IDLE;
    Emotion vocalEmotion = detectVocalEmotion(answer);
    if (actionHeldEmotion) {
      vocalEmotion = currentEmotion; // Retain timed emotion set by [ACTION: emotion/sound ...]
    } else if (vocalEmotion == EMOTION_HAPPY || vocalEmotion == EMOTION_LAUGHING) {
      vocalEmotion = EMOTION_SPEAKING;
    }
    setEmotion(vocalEmotion);

    // Bedtime / sleep fallback: If user asked to sleep or answer says good
    // night / shubh ratri, queue sleep action so Kiwi falls asleep as soon as
    // speaking completes!
    if (!pendingSleepAction) {
      String qLower = q;
      qLower.toLowerCase();
      String aLower = answer;
      aLower.toLowerCase();
      if (hasAny(qLower,
                 {"so jao", "sone jao", "sone chalo", "soja", "so ja",
                  "sleep mode", "go to sleep", "good night", "goodnight",
                  "shubh ratri", "सोने जाओ", "सोने जा", "सो जाओ", "सो जा",
                  "सोने चलो", "शुभ रात्रि", "गुड नाईट"}) ||
          ((hasAny(aLower,
                   {"शुभ रात्रि", "shubh ratri", "good night", "goodnight"}) ||
            hasAny(aLower, {"so raha hoon", "so ja", "sone ja"})) &&
           hasAny(qLower,
                  {"so", "sleep", "night", "sone", "सो", "रात", "नींद"}))) {
        pendingSleepAction = true;
      }
    }

    if (cancelRequest)
      return;

#if LLM_STREAM_TTS
    // Allow LwIP to finalize and free the LLM TCP socket buffers before
    // starting TTS TLS (only needed when no PSRAM)
    if (!psramFound()) {
      yield();
      delay(40);
    }

    // 3. Final flush: after askLLM() returns in handleQuestion():
    //    - If cancelRequest -> do nothing more for speech.
    //    - If pending is non-empty and !cancelRequest -> speakText(pending)
    //    once.
    if (!cancelRequest && streamSpk.pending.length() > 0) {
      Emotion vocalEmotion = detectVocalEmotion(streamSpk.pending);
      if (vocalEmotion != EMOTION_SPEAKING) {
        setEmotion(vocalEmotion);
      }
      streamSpk.ttsInProgress = true;
      bool ok = speakText(streamSpk.pending);
      streamSpk.ttsInProgress = false;
      if (ok) {
        streamSpk.anySpoken = true;
      }
      streamSpk.pending = "";
    }

    if (cancelRequest)
      return;

    //    - Do NOT speakText(answer) again if anything was already spoken
    //      (anySpoken == true). Only fall back to speaking the whole answer if
    //      nothing was spoken at all and answer is non-empty (keeps the old
    //      path).
    if (!streamSpk.anySpoken && !cancelRequest && answer.length() > 0) {
      bool ok = speakText(answer);
      if (cancelRequest)
        return;
      if (!ok) {
        robotStateDesc = "TTS Failed";
        addTerminalLog("[TTS] Playback error or offline");
        Serial.println("[ROBOT] TTS failed");
        setEmotion(EMOTION_CONFUSED);
        returnIdleAt = millis() + 1500;
        return;
      }
    }
#else
    cancelRequest = false;
    bool ok = speakText(answer);
    if (cancelRequest)
      return;
    if (!ok) {
      robotStateDesc = "TTS Failed";
      addTerminalLog("[TTS] Playback error or offline");
      Serial.println("[ROBOT] TTS failed");
      setEmotion(EMOTION_CONFUSED);
      returnIdleAt = millis() + 1500;
      return;
    }
#endif

    // If sleep action is pending, trigger fall-asleep animation immediately
    // after speech ends!
    if (pendingSleepAction) {
      triggerPendingSleepAction();
      returnIdleAt = 0;
      emotionAutoReturnAt = 0;
      return;
    }

    // Turn completed: return to natural IDLE unless an action timer holds an expression
    uint32_t endNow = millis();
    if ((returnIdleAt == 0 || (int32_t)(endNow - returnIdleAt) >= 0) &&
        (emotionAutoReturnAt == 0 || (int32_t)(endNow - emotionAutoReturnAt) >= 0)) {
      setEmotion(EMOTION_IDLE);
      returnIdleAt = 0;
      emotionAutoReturnAt = 0;
    }
  }

  robotStateDesc = "Ready";
  addTerminalLog("[ROBOT] Turn completed. Ready.");
}
