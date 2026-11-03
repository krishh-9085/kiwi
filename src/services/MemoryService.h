#pragma once
#include <Arduino.h>

#define MAX_PERSISTENT_MEMORIES 8

struct MemoryProfile {
  String userName;
  String friendsFamily;
  String memories[MAX_PERSISTENT_MEMORIES];
  int memoryCount;
  String pendingFollowUp;
  uint32_t followUpSetTime;
  bool followUpTriggered;
};

extern MemoryProfile kiwiMemory;

void initMemory();
void saveMemory();
void setOwnerName(const String &name);
void setOwnerNameNoSave(const String &name);
String getOwnerName();
void setFriendsFamily(const String &ff);
void setFriendsFamilyNoSave(const String &ff);
String getFriendsFamily();
bool addMemoryFact(const String &fact);
bool addMemoryFactNoSave(const String &fact);
void removeMemoryFact(int index);
void clearAllMemory();

String getFormattedMemoryPrompt();
void parseAndSaveMemoriesFromLlm(String &response);

bool hasPendingFollowUp();
String popPendingFollowUp();
void setPendingFollowUp(const String &q);
void setPendingFollowUpNoSave(const String &q);
