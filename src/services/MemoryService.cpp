#include "services/MemoryService.h"
#include <Preferences.h>
#include "config/AppConfig.h"

static Preferences memPrefs;
MemoryProfile kiwiMemory = {"", "", {}, 0, "", 0, false};

void initMemory() {
  memPrefs.begin("kiwimem", true);
  kiwiMemory.userName = memPrefs.getString("name", "");
  kiwiMemory.friendsFamily = memPrefs.getString("friends", "");
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

  Serial.printf("[MEMORY] Loaded: Owner='%s', Friends='%s', %d facts stored, pending followup='%s'\n",
                kiwiMemory.userName.c_str(), kiwiMemory.friendsFamily.c_str(),
                kiwiMemory.memoryCount, kiwiMemory.pendingFollowUp.c_str());
}

void saveMemory() {
  memPrefs.begin("kiwimem", false);
  memPrefs.putString("name", kiwiMemory.userName);
  memPrefs.putString("friends", kiwiMemory.friendsFamily);
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

void setFriendsFamilyNoSave(const String &ff) {
  String f = ff;
  f.trim();
  kiwiMemory.friendsFamily = f;
  Serial.printf("[MEMORY] Friends & family set to: %s\n", f.c_str());
}

void setFriendsFamily(const String &ff) {
  setFriendsFamilyNoSave(ff);
  saveMemory();
}

void addFriendOrFamilyNoSave(const String &entryIn) {
  String entry = entryIn;
  entry.trim();
  if (!entry.length())
    return;

  while (entry.endsWith(".") || entry.endsWith("!") || entry.endsWith(",")) {
    entry.remove(entry.length() - 1);
    entry.trim();
  }

  String lowerEntry = entry;
  lowerEntry.toLowerCase();
  String lowerExisting = kiwiMemory.friendsFamily;
  lowerExisting.toLowerCase();

  // Avoid exact or substring duplicate in the list
  if (lowerExisting.indexOf(lowerEntry) >= 0) {
    return;
  }

  if (kiwiMemory.friendsFamily.length() > 0) {
    kiwiMemory.friendsFamily += ", " + entry;
  } else {
    kiwiMemory.friendsFamily = entry;
  }
  Serial.printf("[MEMORY] Added to Friends & Family: %s (Now: '%s')\n",
                entry.c_str(), kiwiMemory.friendsFamily.c_str());
}

void addFriendOrFamily(const String &entry) {
  addFriendOrFamilyNoSave(entry);
  saveMemory();
}

String getFriendsFamily() {
  return kiwiMemory.friendsFamily;
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
  kiwiMemory.friendsFamily = "";
  kiwiMemory.memoryCount = 0;
  kiwiMemory.pendingFollowUp = "";
  kiwiMemory.followUpTriggered = false;
  Serial.println("[MEMORY] All personal memories cleared!");
}

String getFormattedMemoryPrompt() {
  String p = " MEMORY CONTEXT: ";
  if (kiwiMemory.userName.length() > 0) {
    p += "The user's name is " + kiwiMemory.userName +
         ". If they ask ('mera naam kya hai', 'who am I'), answer directly: Tumhara naam " + kiwiMemory.userName + " hai. ";
  } else {
    p += "You do not know the user's name yet. If they ask ('mera naam kya hai', 'who am I'), politely say you don't know yet and ask their name. ";
  }
  if (kiwiMemory.friendsFamily.length() > 0) {
    p += "Friends & Family members (from web settings): " + kiwiMemory.friendsFamily +
         ". You recognize and know all these people and their relation to the user. ";
  }
  if (kiwiMemory.memoryCount > 0) {
    p += "Known facts about user: ";
    for (int i = 0; i < kiwiMemory.memoryCount; i++) {
      p += "(" + String(i + 1) + ") " + kiwiMemory.memories[i] + ". ";
    }
  }
  p += "LEARNING RULES: "
       "1. If user shares a family member or friend's name (e.g. bua, chacha, mama, dadi, dada, mummy, papa, sister, brother, dost, etc.), reply warmly AND append '[MEM: FAMILY: <relation> <name>]' at the very end so it is added directly to their web dashboard list (e.g. 'dadi raj, bua Sunita')! "
       "Example: User says 'Meri bua ka naam Sunita hai' -> reply warmly AND append '[MEM: FAMILY: bua Sunita]'. "
       "2. If user shares their own name, append '[MEM: Name is <Name>]'. "
       "3. If user shares any other general fact or says 'yaad rakhna', append '[MEM: <concise fact>]'. ";
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

  // Extract all [MEM: ...] or [mem: ...] tags
  while (true) {
    int start = resp.indexOf("[MEM:");
    if (start < 0) start = resp.indexOf("[mem:");
    if (start < 0) start = resp.indexOf("[Mem:");
    if (start < 0)
      break;
    int end = resp.indexOf(']', start);
    if (end < 0)
      break;

    String fact = resp.substring(start + 5, end);
    fact.trim();
    if (fact.length() > 0) {
      if (fact.startsWith("User name is ") || fact.startsWith("Name is ") ||
          fact.startsWith("User's name is ") || fact.startsWith("name is ")) {
        String name = "";
        int isIdx = fact.lastIndexOf(" is ");
        if (isIdx >= 0) {
          name = fact.substring(isIdx + 4);
        } else {
          int idx = fact.lastIndexOf(' ');
          if (idx > 0) name = fact.substring(idx + 1);
        }
        name.trim();
        if (name.length() > 0) {
          setOwnerNameNoSave(name);
          modified = true;
        }
      } else if (fact.startsWith("FAMILY:") || fact.startsWith("family:") ||
                 fact.startsWith("RELATION:") || fact.startsWith("FRIEND:")) {
        int col = fact.indexOf(':');
        String rel = fact.substring(col + 1);
        rel.trim();
        addFriendOrFamilyNoSave(rel);
        addMemoryFactNoSave("Family: " + rel);
        modified = true;
      } else {
        String lowerF = fact;
        lowerF.toLowerCase();
        if (lowerF.indexOf("bua") >= 0 || lowerF.indexOf("chacha") >= 0 ||
            lowerF.indexOf("mama") >= 0 || lowerF.indexOf("mami") >= 0 ||
            lowerF.indexOf("dadi") >= 0 || lowerF.indexOf("dada") >= 0 ||
            lowerF.indexOf("nana") >= 0 || lowerF.indexOf("nani") >= 0 ||
            lowerF.indexOf("mummy") >= 0 || lowerF.indexOf("papa") >= 0 ||
            lowerF.indexOf("mother") >= 0 || lowerF.indexOf("father") >= 0 ||
            lowerF.indexOf("brother") >= 0 || lowerF.indexOf("sister") >= 0 ||
            lowerF.indexOf("bhai") >= 0 || lowerF.indexOf("behen") >= 0 ||
            lowerF.indexOf("friend") >= 0 || lowerF.indexOf("dost") >= 0) {
          addFriendOrFamilyNoSave(fact);
        }
        if (addMemoryFactNoSave(fact)) {
          modified = true;
        }
      }
    }
    resp.remove(start, (end - start) + 1);
  }

  // Extract [FOLLOWUP: ...] tag
  int fStart = resp.indexOf("[FOLLOWUP:");
  if (fStart < 0) fStart = resp.indexOf("[followup:");
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
