#include "behavior/OfflineBrain.h"
#include "config/AppConfig.h"
#include "core/Timing.h"
#include "core/CoreTypes.h"
#include "face/FaceEngine.h"
#include "face/FaceExpressions.h"
#include "face/FaceScreens.h"
#include "hardware/AudioEngine.h"
#include "hardware/TouchController.h"
#include "behavior/InteractionManager.h"
#include <WiFi.h>

static bool offlineModeActive = false;
static bool wasOnline = false;
static uint32_t lastConnectionCheckAt = 0;

bool isOfflineBrainActive() {
  return offlineModeActive;
}

void initOfflineBrain() {
  bool online = (WiFi.status() == WL_CONNECTED);
  wasOnline = online;
  if (!online) {
    offlineModeActive = true;
    Serial.println("[OFFLINE BRAIN] Wi-Fi offline - Offline Brain Active");
    addTerminalLog("[OFFLINE] Offline Brain Active");
  } else {
    offlineModeActive = false;
  }
}

void updateOfflineBrain() {
  uint32_t now = millis();
  if (now - lastConnectionCheckAt < 2000)
    return;
  lastConnectionCheckAt = now;

  bool online = (WiFi.status() == WL_CONNECTED);

  // Transition from ONLINE to OFFLINE
  if (wasOnline && !online) {
    wasOnline = false;
    offlineModeActive = true;
    Serial.println("[OFFLINE BRAIN] Connection dropped -> Offline Mode Activated");
    addTerminalLog("[OFFLINE] Wi-Fi lost -> Offline Mode Active");
    soundPetSparkle();
  }
  // Transition from OFFLINE to ONLINE
  else if (!wasOnline && online) {
    wasOnline = true;
    offlineModeActive = false;
    Serial.println("[OFFLINE BRAIN] Wi-Fi reconnected!");
    addTerminalLog("[NET] Wi-Fi reconnected!");
    soundPetSparkle();
  }
}

void handleOfflineBuddyTap() {
  Serial.println("[OFFLINE BRAIN] Top tap received - triggering cute Desk Buddy reaction!");
  addTerminalLog("[OFFLINE] Desk Buddy Tap Reaction");

  offlineModeActive = true;

  // Pick from 6 delightful, responsive offline reactions
  int reaction = random(0, 6);
  switch (reaction) {
    case 0:
      // Loving Wink & Kiss
      setEmotion(EMOTION_WINK);
      flourish = FL_HEART;
      soundPetKiss();
      break;

    case 1:
      // Kawaii Puppy Eyes & Sparkle
      setEmotion(EMOTION_CUTE);
      flourish = FL_SPARKLE;
      soundPetSparkle();
      break;

    case 2:
      // Musical Giggle & Laugh
      setEmotion(EMOTION_LAUGHING);
      flourish = FL_NOTE;
      soundPetGiggle();
      break;

    case 3:
      // Lovestruck Hearts
      setEmotion(EMOTION_HEARTS);
      flourish = FL_HEART;
      soundPetLove();
      break;

    case 4:
      // Cheerful Desk Party
      setEmotion(EMOTION_PARTY);
      flourish = FL_STAR;
      soundPetPlayful();
      break;

    case 5:
    default:
      // Cute Desk Buddy Toast Notice (non-intrusive, friendly)
      drawMessage("Kiwi Desk Buddy", "Offline Mode <3");
      soundPetSparkle();
      waitMs(1200, []() -> bool { return (bool)cancelRequest; });
      setEmotion(EMOTION_HAPPY);
      break;
  }

  returnIdleAt = millis() + 2500;
  resyncTouch();
  busyProcessing = false;
}
