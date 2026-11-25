#pragma once
#include <Arduino.h>

// ============================================================
// LLM TOOL / SMART ACTION EXECUTOR
// ============================================================

extern int lastActionsExecuted;
extern bool pendingSleepAction;
void triggerPendingSleepAction();

// Scans text for [ACTION: ...] tags, executes all requested robot actions,
// and removes the tags so speech/TTS and chat logs remain clean and natural.
// Returns count of executed actions.
int parseAndExecuteActions(String &text);

// Executes a single action command string, e.g. "volume 80", "skin cat", "timer 60 Study"
bool executeRobotAction(const String &actionCmd);

// Maps friendly skin name to EyeSkin enum
bool setEyeSkinByName(const String &nameIn);

// Plays named sound effect
void triggerSoundByName(const String &nameIn);

// Countdown timer subsystem
void checkRobotTimer();
void startRobotTimer(uint32_t seconds, const String &label = "Timer");
void cancelRobotTimer();
void stopRobotTimerAlarm();
bool isRobotTimerActive();
bool isRobotTimerRinging();
uint32_t getRobotTimerRingingDurationMs();
uint32_t getRobotTimerRemainingSec();
