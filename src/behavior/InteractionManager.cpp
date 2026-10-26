#include "behavior/InteractionManager.h"
#include "config/AppConfig.h"
#include "core/CoreTypes.h"
#include "face/FaceEngine.h"
#include "hardware/AudioEngine.h"
#include "hardware/TouchController.h"
#include "services/AiService.h"
#include "behavior/CommandHandler.h"

// ============================================================
// UI PUMP (Display + Touch + Tone Update)
// ============================================================
void pumpUI() {
  updateTouch();
  updateTonePlayer();
  tickFace();
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
  if (hasAny(q, {"new topic", "naya topic", "naya sawal", "reset chat", "clear memory", "bhool jao", "forget that", "forget everything"})) {
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
  addTerminalLog("[LLM] Sending to " + (llmProvider == "groq" ? groqModel : llmModel) + ": \"" + q + "\"");
  String answer = askLLM(q);
  if (cancelRequest)
    return;
  if (!answer.length()) {
    robotStateDesc = "LLM Empty / Error";
    addTerminalLog("[LLM] Empty response or error received");
    Serial.println("[ROBOT] LLM empty");
    showStatus("LLM error", "check key/model", 2000);
    returnIdleAt = millis() + 500;
    return;
  }

  Serial.print("[ROBOT] A: ");
  Serial.println(answer);
  lastLlmAnswer = answer;
  robotStateDesc = "Speaking...";
  addTerminalLog("[LLM] Answer: \"" + answer + "\"");
  setEmotion(EMOTION_SPEAKING);
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
  robotStateDesc = "Ready";
  addTerminalLog("[ROBOT] Turn completed. Ready.");
  setEmotion(EMOTION_HAPPY);
  returnIdleAt = millis() + 1200;
}
