#pragma once
#include <Arduino.h>

// ============================================================
// OFFLINE FALLBACK BRAIN (DESK BUDDY CUTE EYE THEME)
// ============================================================

// Initializes offline brain and activates Desk Buddy cute eyes if Wi-Fi is offline at boot
void initOfflineBrain();

// Monitors connection state; activates Desk Buddy cute eyes when disconnected
void updateOfflineBrain();

// Handles top tap when offline (cute sounds, winks, hearts, giggles instead of error screens)
void handleOfflineBuddyTap();

// Returns true if robot is running in offline Desk Buddy fallback mode
bool isOfflineBrainActive();
