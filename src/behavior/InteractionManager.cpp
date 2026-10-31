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
    if (ttsProvider == "fish" || ttsProvider == "fishaudio") {
      String lower = answer;
      lower.toLowerCase();
      if (lower.indexOf("[laughing]") >= 0 || lower.indexOf("[chuckling]") >= 0 || lower.indexOf("laugh") >= 0) {
        setEmotion(EMOTION_LAUGHING);
      } else if (lower.indexOf("[crying loudly]") >= 0 || lower.indexOf("[sobbing]") >= 0) {
        setEmotion(EMOTION_CRYING);
      } else if (lower.indexOf("[angry]") >= 0) {
        setEmotion(EMOTION_ANGRY);
      } else if (lower.indexOf("[sad]") >= 0) {
        setEmotion(EMOTION_SAD);
      } else if (lower.indexOf("[excited]") >= 0) {
        setEmotion(EMOTION_EXCITED);
      } else if (lower.indexOf("[whispering]") >= 0 || lower.indexOf("[soft]") >= 0 || lower.indexOf("[breathy]") >= 0) {
        setEmotion(EMOTION_CUTE);
      } else if (lower.indexOf("[sighing]") >= 0 || lower.indexOf("[groaning]") >= 0) {
        setEmotion(EMOTION_RELIEVED);
      } else if (lower.indexOf("[embarrassed]") >= 0) {
        setEmotion(EMOTION_CONFUSED);
      } else {
        setEmotion(EMOTION_SPEAKING);
      }
    } else {
      setEmotion(EMOTION_SPEAKING);
    }

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
