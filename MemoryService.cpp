#include "MemoryService.h"
#include <Preferences.h>
#include "Config.h"

static Preferences memPrefs;
MemoryProfile kiwiMemory = {"", {}, 0, "", 0, false};

void initMemory() {
  memPrefs.begin("kiwimem", true);
  kiwiMemory.userName = memPrefs.getString("name", "");
  kiwiMemory.memoryCount = memPrefs.getInt("count", 0);
  if (kiwiMemory.memoryCount > MAX_PERSISTENT_MEMORIES)
    kiwiMemory.memoryCount = MAX_PERSISTENT_MEMORIES;

  for (int i = 0; i < kiwiMemory.memoryCount; i++) {
    String key = "m" + String(i);
    kiwiMemory.memories[i] = memPrefs.getString(key.c_str(), "");
  }
  kiwiMemory.pendingFollowUp = memPrefs.getString("followup", "");
  kiwiMemory.followUpTriggered = memPrefs.getBool("f_done", false);
  memPrefs.end();

  Serial.printf("[MEMORY] Loaded: Owner='%s', %d facts stored, pending followup='%s'\n",
                kiwiMemory.userName.c_str(), kiwiMemory.memoryCount,
                kiwiMemory.pendingFollowUp.c_str());
}

void saveMemory() {
  memPrefs.begin("kiwimem", false);
  memPrefs.putString("name", kiwiMemory.userName);
  memPrefs.putInt("count", kiwiMemory.memoryCount);
  for (int i = 0; i < kiwiMemory.memoryCount; i++) {
    String key = "m" + String(i);
    memPrefs.putString(key.c_str(), kiwiMemory.memories[i]);
  }
  memPrefs.putString("followup", kiwiMemory.pendingFollowUp);
  memPrefs.putBool("f_done", kiwiMemory.followUpTriggered);
  memPrefs.end();
  Serial.println("[MEMORY] Saved to Flash NVS");
}

void setOwnerNameNoSave(const String &name) {
  String n = name;
  n.trim();
  if (!n.length())
    return;
  kiwiMemory.userName = n;
  Serial.printf("[MEMORY] Owner name set to: %s\n", n.c_str());
}

void setOwnerName(const String &name) {
  setOwnerNameNoSave(name);
  saveMemory();
}

String getOwnerName() {
  return kiwiMemory.userName;
}

bool addMemoryFactNoSave(const String &factIn) {
  String fact = factIn;
  fact.trim();
  if (!fact.length())
    return false;

  // Check if fact is already present
  for (int i = 0; i < kiwiMemory.memoryCount; i++) {
    if (kiwiMemory.memories[i].equalsIgnoreCase(fact))
      return false;
  }

  // If memory is full, shift older memories out (FIFO)
  if (kiwiMemory.memoryCount >= MAX_PERSISTENT_MEMORIES) {
    for (int i = 0; i < MAX_PERSISTENT_MEMORIES - 1; i++) {
      kiwiMemory.memories[i] = kiwiMemory.memories[i + 1];
    }
    kiwiMemory.memoryCount = MAX_PERSISTENT_MEMORIES - 1;
  }

  kiwiMemory.memories[kiwiMemory.memoryCount++] = fact;
  Serial.printf("[MEMORY] Learned new fact: %s\n", fact.c_str());
  return true;
}

bool addMemoryFact(const String &factIn) {
  if (addMemoryFactNoSave(factIn)) {
    saveMemory();
    return true;
  }
  return false;
}

void removeMemoryFact(int index) {
  if (index < 0 || index >= kiwiMemory.memoryCount)
    return;
  for (int i = index; i < kiwiMemory.memoryCount - 1; i++) {
    kiwiMemory.memories[i] = kiwiMemory.memories[i + 1];
  }
  kiwiMemory.memoryCount--;
  saveMemory();
}

void clearAllMemory() {
  memPrefs.begin("kiwimem", false);
  memPrefs.clear();
  memPrefs.end();

  kiwiMemory.userName = "";
  kiwiMemory.memoryCount = 0;
  kiwiMemory.pendingFollowUp = "";
  kiwiMemory.followUpTriggered = false;
  Serial.println("[MEMORY] All personal memories cleared!");
}

String getFormattedMemoryPrompt() {
  String p = "OWNER PROFILE & REMEMBERED MEMORIES: ";
  if (kiwiMemory.userName.length() > 0) {
    p += "The user's name is " + kiwiMemory.userName +
         ". Remember it, but do not repeatedly say it or use it as a greeting; "
         "only use the name when it is genuinely natural or specifically relevant. ";
  }
  if (kiwiMemory.memoryCount > 0) {
    p += "Facts you remember about the user: ";
    for (int i = 0; i < kiwiMemory.memoryCount; i++) {
      p += "(" + String(i + 1) + ") " + kiwiMemory.memories[i] + ". ";
    }
    p += "Use these details only when relevant to the current question. Never force a memory into a reply. ";
  }
  return p;
}

bool hasPendingFollowUp() {
  return (kiwiMemory.pendingFollowUp.length() > 0 && !kiwiMemory.followUpTriggered);
}

String popPendingFollowUp() {
  String q = kiwiMemory.pendingFollowUp;
  kiwiMemory.followUpTriggered = true;
  saveMemory();
  return q;
}

void setPendingFollowUpNoSave(const String &q) {
  String query = q;
  query.trim();
  if (!query.length())
    return;
  kiwiMemory.pendingFollowUp = query;
  kiwiMemory.followUpTriggered = false;
  kiwiMemory.followUpSetTime = millis();
  Serial.printf("[MEMORY] Set pending follow-up question: %s\n", query.c_str());
}

void setPendingFollowUp(const String &q) {
  setPendingFollowUpNoSave(q);
  saveMemory();
}

void parseAndSaveMemoriesFromLlm(String &resp) {
  bool modified = false;

  // Extract all [MEM: ...] tags
  while (true) {
    int start = resp.indexOf("[MEM:");
    if (start < 0)
      break;
    int end = resp.indexOf(']', start);
    if (end < 0)
      break;

    String fact = resp.substring(start + 5, end);
    fact.trim();
    if (fact.length() > 0) {
      if (fact.startsWith("User name is ") || fact.startsWith("Name is ")) {
        int idx = fact.lastIndexOf(' ');
        if (idx > 0) {
          String name = fact.substring(idx + 1);
          name.trim();
          setOwnerNameNoSave(name);
          modified = true;
        }
      } else {
        if (addMemoryFactNoSave(fact)) {
          modified = true;
        }
      }
    }
    resp.remove(start, (end - start) + 1);
  }

  // Extract [FOLLOWUP: ...] tag
  int fStart = resp.indexOf("[FOLLOWUP:");
  if (fStart >= 0) {
    int fEnd = resp.indexOf(']', fStart);
    if (fEnd >= 0) {
      String q = resp.substring(fStart + 10, fEnd);
      q.trim();
      if (q.length() > 0) {
        setPendingFollowUpNoSave(q);
        modified = true;
      }
      resp.remove(fStart, (fEnd - fStart) + 1);
    }
  }

  if (modified) {
    resp.trim();
    saveMemory();
  }
}
