#include "memory/MemoryService.h"
#include "config/AppConfig.h"
#include <Preferences.h>
#include <time.h>

static Preferences memPrefs;
MemoryProfile kiwiMemory = {"", "", {}, 0, "", 0, false};

void initMemory() {
  memPrefs.begin("kiwimem", true);
  kiwiMemory.userName = memPrefs.getString("name", "");
  kiwiMemory.friendsFamily = memPrefs.getString("friends", "");
  kiwiMemory.memoryCount = memPrefs.getInt("count", 0);
  if (kiwiMemory.memoryCount < 0)
    kiwiMemory.memoryCount = 0;
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
  for (int i = 0; i < MAX_PERSISTENT_MEMORIES; i++) {
    String key = "m" + String(i);
    if (i < kiwiMemory.memoryCount) {
      memPrefs.putString(key.c_str(), kiwiMemory.memories[i]);
    } else if (memPrefs.isKey(key.c_str())) {
      memPrefs.remove(key.c_str());
    }
  }
  memPrefs.putString("followup", kiwiMemory.pendingFollowUp);
  memPrefs.putBool("f_done", kiwiMemory.followUpTriggered);
  memPrefs.end();
  Serial.println("[MEMORY] Saved to Flash NVS");
}

static bool isAsciiOnlyText(const String &s) {
  bool hasPrintable = false;
  for (size_t i = 0; i < s.length(); i++) {
    unsigned char c = (unsigned char)s[i];
    // Reject control characters and non-ASCII UTF-8 bytes (e.g. Hindi/Devanagari)
    if (c < 0x20 || c >= 0x7F)
      return false;
    if (c > 0x20)
      hasPrintable = true;
  }
  return hasPrintable;
}

static bool isValidAsciiOwnerName(const String &s) {
  if (!isAsciiOnlyText(s))
    return false;
  for (size_t i = 0; i < s.length(); i++) {
    unsigned char c = (unsigned char)s[i];
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
      return true;
  }
  return false;
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

static bool isRelationWord(const String &w) {
  static const char *const rels[] = {
      "my",       "dost",    "friend",  "best",    "bhai",    "behen",
      "behan",    "didi",    "bhaiya",  "sister",  "brother", "mom",
      "dad",      "mummy",   "papa",    "mother",  "father",  "bhatija",
      "bhatiji",  "bhanja",  "bhanji",  "chacha",  "chachi",  "mama",
      "mami",     "dada",    "dadi",    "nana",    "nani",    "bua",
      "fufa",     "mausa",   "mausi",   "bhabhi",  "jija",    "cousin",
      "wife",     "husband", "biwi",    "pati",    "beta",    "beti",
      "son",      "daughter","saheli",  "yaar",    "mitra",   "roommate",
      "colleague","partner", "nephew",  "niece",   nullptr};
  for (int i = 0; rels[i] != nullptr; i++) {
    if (w == rels[i])
      return true;
  }
  return false;
}

static String extractPersonKey(const String &rawToken) {
  String s = rawToken;
  int p = s.indexOf('(');
  if (p >= 0) {
    s = s.substring(0, p);
  }
  s.toLowerCase();
  s.trim();
  String kept;
  int i = 0;
  while (i < (int)s.length()) {
    while (i < (int)s.length() && s[i] == ' ')
      i++;
    if (i >= (int)s.length())
      break;
    int sp = s.indexOf(' ', i);
    String word = (sp < 0) ? s.substring(i) : s.substring(i, sp);
    i = (sp < 0) ? (int)s.length() : (sp + 1);
    if (!isRelationWord(word)) {
      if (kept.length() > 0)
        kept += ' ';
      kept += word;
    }
  }
  if (kept.length() > 0)
    return kept;
  return s;
}

static String stripDatePrefix(const String &s) {
  if (s.startsWith("[")) {
    int close = s.indexOf("] ");
    if (close > 0 && close <= 10) {
      String rest = s.substring(close + 2);
      rest.trim();
      return rest;
    }
  }
  return s;
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
  if (!entry.length())
    return;

  String newKey = extractPersonKey(entry);
  String lowerNew = entry;
  lowerNew.toLowerCase();

  const int MAX_ENTRIES = 24;
  String items[MAX_ENTRIES];
  int count = 0;
  bool updated = false;

  int start = 0;
  while (start < (int)kiwiMemory.friendsFamily.length() && count < MAX_ENTRIES) {
    int comma = kiwiMemory.friendsFamily.indexOf(',', start);
    String tok = (comma < 0) ? kiwiMemory.friendsFamily.substring(start)
                             : kiwiMemory.friendsFamily.substring(start, comma);
    tok.trim();
    start = (comma < 0) ? (int)kiwiMemory.friendsFamily.length() : (comma + 1);
    if (!tok.length())
      continue;

    String lowerTok = tok;
    lowerTok.toLowerCase();
    String tokKey = extractPersonKey(tok);
    if (lowerTok == lowerNew) {
      return; // Exact match already present
    }
    if (newKey.length() > 0 && tokKey == newKey) {
      // Upgrade short entry with richer relation context if longer
      if (entry.length() > tok.length()) {
        items[count++] = entry;
        updated = true;
      } else {
        return;
      }
    } else {
      items[count++] = tok;
    }
  }

  if (!updated && count < MAX_ENTRIES) {
    items[count++] = entry;
  }

  String rebuilt;
  for (int i = 0; i < count; i++) {
    if (i > 0)
      rebuilt += ", ";
    rebuilt += items[i];
  }
  kiwiMemory.friendsFamily = rebuilt;
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

bool addMemoryFactNoSave(const String &factIn, bool stampDate) {
  String fact = factIn;
  fact.trim();
  if (!fact.length())
    return false;

  String coreNew = stripDatePrefix(fact);
  // Check if fact is already present (ignoring optional date prefix)
  for (int i = 0; i < kiwiMemory.memoryCount; i++) {
    if (stripDatePrefix(kiwiMemory.memories[i]).equalsIgnoreCase(coreNew))
      return false;
  }

  if (stampDate && timeSynced && !fact.startsWith("[")) {
    struct tm ti;
    if (getLocalTime(&ti, 5)) {
      char dateBuf[16];
      strftime(dateBuf, sizeof(dateBuf), "[%d %b] ", &ti);
      fact = String(dateBuf) + fact;
    }
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
  if (addMemoryFactNoSave(factIn, true)) {
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
  kiwiMemory.memories[kiwiMemory.memoryCount - 1] = "";
  kiwiMemory.memoryCount--;
  saveMemory();
}

void syncMemoryFacts(const String *facts, int count) {
  kiwiMemory.memoryCount = 0;
  for (int i = 0; i < MAX_PERSISTENT_MEMORIES; i++) {
    kiwiMemory.memories[i] = "";
  }
  if (facts && count > 0) {
    for (int i = 0; i < count; i++) {
      addMemoryFactNoSave(facts[i], false);
    }
  }
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
    p += "User's name is " + kiwiMemory.userName +
         " (only mention if asked 'who am I' / 'mera naam kya hai'; do NOT repeatedly append their name at the end of answers). ";
  } else {
    p += "You do not know the user's name yet. If they ask ('mera naam kya hai', 'who am I'), politely say you don't know yet and ask their name. ";
  }
  if (kiwiMemory.friendsFamily.length() > 0) {
    p += "User's Friends & Family List: [" + kiwiMemory.friendsFamily +
         "]. IMPORTANT RULES FOR FRIENDS & FAMILY: "
         "(A) You personally know each person in this list, their relationship to the user, AND any parent/spouse/family links noted inside '(relation - detail)' or in Known facts. Combine them to know who is whose parent, child, spouse, or sibling. "
         "(B) Whenever user mentions, asks about, or jokes about anyone in this list (e.g. 'Yunay kiska beta hai?', 'Rohan kaisa hai?'), ALWAYS answer using that person's exact family/friend connections! "
         "(C) NEVER confuse a family member's name with yourself (Kiwi). The user is talking about that person, NOT calling you that name or accusing you. Reply warmly, playfully, and affectionately like a close companion who knows their family. ";
  }
  if (kiwiMemory.memoryCount > 0) {
    p += "Known facts about user: ";
    for (int i = 0; i < kiwiMemory.memoryCount; i++) {
      p += "(" + String(i + 1) + ") " + kiwiMemory.memories[i] + ". ";
    }
  }
  p += "CRITICAL LEARNING & MEMORY RULES: "
       "1. NEVER save transient requests, jokes, or queries (e.g. NEVER output '[MEM: Story requested]', '[MEM: asked for joke]', etc.). "
       "2. If user shares a family member/friend OR clarifies who someone's parent/spouse/child is, reply warmly AND append '[MEM: FAMILY: <Name> (<relation>)]' written strictly in English/Roman letters (ASCII only, never Hindi/Devanagari script). If that person already exists in Friends & Family (e.g. 'Yunay (Bhatija)') and user adds a parent/spouse link, keep BOTH in one entry like '[MEM: FAMILY: Yunay (Bhatija - Yash & Aditi's son)]'. ";
  if (isValidAsciiOwnerName(kiwiMemory.userName)) {
    p += "3. User's name is ALREADY saved; NEVER emit '[MEM: Name is ...]' tags. ";
  } else {
    p += "3. If user shares their own name, append '[MEM: Name is <Name>]' written strictly in English/Roman letters (ASCII only, never Hindi/Devanagari script). ";
  }
  p += "4. ONLY save real, meaningful personal life facts (e.g. Papa likes ginger tea, upcoming exam tomorrow, pet name) or when user says 'yaad rakhna', append '[MEM: <concise fact>]' strictly in English/Roman letters (ASCII only, never Hindi/Devanagari script). "
       "5. If user mentions an upcoming event, test, interview, trip, or doctor visit, optionally append '[FOLLOWUP: <short warm check-in question in user's language>]' at the end so Kiwi checks in later. ";
  return p;
}

bool hasPendingFollowUp() {
  return (kiwiMemory.pendingFollowUp.length() > 0 && !kiwiMemory.followUpTriggered);
}

String popPendingFollowUp() {
  String q = kiwiMemory.pendingFollowUp;
  kiwiMemory.pendingFollowUp = "";
  kiwiMemory.followUpTriggered = true;
  saveMemory();
  return q;
}

void clearPendingFollowUp() {
  kiwiMemory.pendingFollowUp = "";
  kiwiMemory.followUpTriggered = false;
  saveMemory();
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

static bool isFactSentenceWord(const String &w) {
  static const char *const markers[] = {
      "is",      "was",     "are",     "am",      "likes",  "loves",
      "like",    "love",    "has",     "have",    "had",    "wants",
      "went",    "going",   "lives",   "works",   "pasand", "hai",
      "hain",    "tha",     "thi",     "ka",      "ki",     "ke",
      "aur",     "and",     "ko",      "exam",    "test",   "birthday",
      "tomorrow","kal",     "aaj",     nullptr};
  for (int i = 0; markers[i] != nullptr; i++) {
    if (w == markers[i])
      return true;
  }
  return w.endsWith("'s");
}

static bool looksLikeShortFamilyEntry(const String &lowerFact) {
  int lp = lowerFact.indexOf('(');
  int rp = lowerFact.indexOf(')');
  bool hasParens = (lp > 0 && rp > lp);

  int totalWords = 0;
  int wordsBeforeParen = 0;
  bool hasRel = false;
  bool hasSentenceBeforeParen = false;
  bool hasSentenceAny = false;

  int n = lowerFact.length();
  int i = 0;
  while (i < n) {
    while (i < n) {
      char c = lowerFact[i];
      if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '\'')
        break;
      i++;
    }
    if (i >= n)
      break;
    int start = i;
    while (i < n) {
      char c = lowerFact[i];
      if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '\''))
        break;
      i++;
    }
    String w = lowerFact.substring(start, i);
    totalWords++;
    if (hasParens && start < lp)
      wordsBeforeParen++;
    if (isRelationWord(w))
      hasRel = true;
    if (isFactSentenceWord(w)) {
      hasSentenceAny = true;
      if (hasParens && start < lp)
        hasSentenceBeforeParen = true;
    }
  }

  if (!hasRel)
    return false;
  if (hasParens) {
    return (wordsBeforeParen >= 1 && wordsBeforeParen <= 3 && !hasSentenceBeforeParen);
  }
  return (totalWords <= 4 && !hasSentenceAny);
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
      String lowerFact = fact;
      lowerFact.toLowerCase();
      if (lowerFact.startsWith("user name is ") ||
          lowerFact.startsWith("name is ") ||
          lowerFact.startsWith("user's name is ") ||
          lowerFact.startsWith("owner name is ") ||
          lowerFact.startsWith("owner's name is ") ||
          lowerFact.startsWith("my name is ")) {
        String name = "";
        int isIdx = lowerFact.lastIndexOf(" is ");
        if (isIdx >= 0) {
          name = fact.substring(isIdx + 4);
        } else {
          int idx = fact.lastIndexOf(' ');
          if (idx > 0) name = fact.substring(idx + 1);
        }
        name.trim();
        while (name.length() > 0 &&
               (name.endsWith(".") || name.endsWith("!") || name.endsWith(",") ||
                name.endsWith("\"") || name.endsWith("'"))) {
          name.remove(name.length() - 1);
          name.trim();
        }
        while (name.length() > 0 && (name.startsWith("\"") || name.startsWith("'"))) {
          name.remove(0, 1);
          name.trim();
        }
        // Only accept pure English/ASCII names from LLM tags, and never overwrite an
        // existing English/ASCII owner name (use Web UI Settings to intentionally rename).
        if (isValidAsciiOwnerName(name) && !isValidAsciiOwnerName(kiwiMemory.userName)) {
          setOwnerNameNoSave(name);
          modified = true;
        } else {
          Serial.printf("[MEMORY] Ignored LLM owner name tag ('%s', current='%s')\n",
                        name.c_str(), kiwiMemory.userName.c_str());
        }
      } else if (fact.startsWith("FAMILY:") || fact.startsWith("family:") ||
                 fact.startsWith("RELATION:") || fact.startsWith("FRIEND:")) {
        int col = fact.indexOf(':');
        String rel = fact.substring(col + 1);
        rel.trim();
        if (isAsciiOnlyText(rel)) {
          addFriendOrFamilyNoSave(rel);
          modified = true;
        } else {
          Serial.printf("[MEMORY] Ignored non-ASCII LLM family tag: '%s'\n",
                        rel.c_str());
        }
      } else {
        String lowerF = fact;
        lowerF.toLowerCase();

        // Guardrail: Skip temporary transient requests/commands (e.g. story, joke, weather, etc.)
        if (lowerF.indexOf("requested") >= 0 || lowerF.indexOf("request") >= 0 ||
            lowerF.indexOf("story") >= 0 || lowerF.indexOf("joke") >= 0 ||
            lowerF.indexOf("kahani") >= 0 || lowerF.indexOf("weather") >= 0 ||
            lowerF.indexOf("time") >= 0 || lowerF.indexOf("alarm") >= 0) {
          // Ignore transient command or request
          resp.remove(start, (end - start) + 1);
          continue;
        }

        if (!isAsciiOnlyText(fact)) {
          Serial.printf("[MEMORY] Ignored non-ASCII LLM memory tag: '%s'\n",
                        fact.c_str());
        } else if (looksLikeShortFamilyEntry(lowerF)) {
          addFriendOrFamilyNoSave(fact);
          modified = true;
        } else {
          if (addMemoryFactNoSave(fact, true)) {
            modified = true;
          }
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
