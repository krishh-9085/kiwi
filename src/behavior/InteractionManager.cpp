#include "behavior/InteractionManager.h"
#include "config/AppConfig.h"
#include "core/CoreTypes.h"
#include "face/FaceEngine.h"
#include "hardware/AudioEngine.h"
#include "hardware/TouchController.h"
#include "services/AiService.h"
#include "behavior/CommandHandler.h"
#include "behavior/ActionExecutor.h"
#include "web/WebPortal.h"

// ============================================================
// UI PUMP (Display + Touch + Tone Update)
// ============================================================
void pumpUI() {
  updateTouch();
  updateTonePlayer();
  tickFace();
  pumpWeb();
}

// ============================================================
// PET PHYSICAL TOUCH REACTIONS
// ============================================================
void triggerPetReaction() {
  if (busyProcessing)
    return;
  petMood = (PetMood)random(0, PET_MOOD_COUNT);
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
    flourish = FL_SPARKLE;
    soundPetSurprised();
    break;
  }
  emotionAutoReturnAt = millis() + 60000;
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
    return (lower.indexOf("[" + t + "]") >= 0 || lower.indexOf("(" + t + ")") >= 0);
  };

  // 1. Laughing / Amused
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

  // 6. Happy / Joy / Delighted / Satisfied
  if (hasTag("happy") || hasTag("delighted") || hasTag("satisfied") ||
      hasTag("cheerful") || hasTag("joyful") || hasTag("glad")) {
    return EMOTION_HAPPY;
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

void handleQuestion(const String &q) {
  lastUserInteractionAt = millis();
  Serial.print("[ROBOT] Q: ");
  Serial.println(q);
  if (cancelRequest)
    return;
  if (handleTimeCommand(q))
    return;
  if (handleWeatherCommand(q))
    return;
  if (handleSkinCommand(q))
    return;
  if (handleEmotionCommand(q))
    return;
  if (handleVolumeCommand(q))
    return;
  if (handleTimerCommand(q))
    return;
  if (hasAny(q, {"new topic", "naya topic", "naya sawal", "reset chat", "clear memory", "bhool jao", "forget that", "forget everything"})) {
    clearConversationHistory();
    showEmotionFor(EMOTION_CONFUSED);
    speakText("Theek hai, sab bhool gaya. Naya sawal poocho!");
    return;
  }

  // Save the emotion Kiwi was displaying before answering
  Emotion prevEmotion = (currentEmotion != EMOTION_THINKING && currentEmotion != EMOTION_LISTENING && currentEmotion != EMOTION_SPEAKING)
                            ? currentEmotion
                            : EMOTION_IDLE;

  setEmotion(EMOTION_THINKING);
  if (cancelRequest)
    return;
  lastTranscribedQuestion = q;
  robotStateDesc = "Thinking...";
  addTerminalLog("[LLM] Sending to " + (llmProvider == "groq" ? groqModel : llmModel) + ": \"" + q + "\"");

  // Single-core ESP32-C3: Complete LLM cleanly first so max heap (~75KB) is
  // available for TLS TTS and no simultaneous TLS collisions or socket stalling occurs.
  String answer = askLLM(q, nullptr);
  if (cancelRequest)
    return;
  if (!answer.length()) {
    if (lastActionsExecuted > 0) {
      robotStateDesc = "Ready";
      addTerminalLog("[ROBOT] Action completed.");
      setEmotion(EMOTION_HAPPY);
      returnIdleAt = millis() + 1500;
      return;
    }
    robotStateDesc = "LLM Empty / Error";
    addTerminalLog("[LLM] Empty response or error received");
    Serial.println("[ROBOT] LLM empty");
    lastLlmAnswer = "Error: No response from AI. Please check Wi-Fi, model, or API key.";
    setEmotion(EMOTION_CONFUSED);
    showStatus("AI No Response", "Check key/model", 1800);
    speakText("Sorry, abhi answer nahi de pa raha hoon. Ek baar firse poocho!");
    returnIdleAt = millis() + 500;
    return;
  }

  Serial.print("[ROBOT] A: ");
  Serial.println(answer);
  lastLlmAnswer = answer;
  addTerminalLog("[LLM] Answer: \"" + answer + "\"");

  robotStateDesc = "Speaking...";
  {
    Emotion vocalEmotion = detectVocalEmotion(answer);
    setEmotion(vocalEmotion);

    if (cancelRequest)
      return;

    bool ok = speakText(answer);
    if (cancelRequest)
      return;
    if (!ok) {
      robotStateDesc = "TTS Failed";
      addTerminalLog("[TTS] Playback error or offline");
      Serial.println("[ROBOT] TTS failed");
      showStatus("TTS error", "see Serial log", 2000);
      returnIdleAt = millis() + 500;
      return;
    }
  }

  robotStateDesc = "Ready";
  addTerminalLog("[ROBOT] Turn completed. Ready.");
  // Revert immediately to previous emotion once speech is finished
  setEmotion(prevEmotion);
  returnIdleAt = 0;
  emotionAutoReturnAt = 0;
}
