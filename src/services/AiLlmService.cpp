#include "services/AiLlmService.h"
#include "services/AiChatHistory.h"
#include "services/AiTextTools.h"
#include "services/MemoryService.h"
#include "config/AppConfig.h"
#include "face/FaceEngine.h"
#include "behavior/InteractionManager.h"
#include "behavior/ActionExecutor.h"
#include "web/WebPortal.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

static String getSmartActionInstruction() {
  return " SMART ACTION EXECUTION: You control your physical robot hardware! "
         "When user asks you to perform an action, change settings, timer, skin/theme, weather, time or volume, "
         "ALWAYS reply with a friendly spoken sentence AND append [ACTION: ...] tags: "
         "[ACTION: volume <0-100>] (e.g. [ACTION: volume 85]) "
         "[ACTION: skin <kiwi|cat|cyberpunk|retro8bit|matrix|anime|hearts|steampunk|demon|cyborg|mecha|sunglasses|puppy|hypno|alien|ninja|round|default>] "
         "[ACTION: emotion <happy|excited|sad|angry|confused|cool|cute|evil|crying|hearts|dead|shocked|singing|party|nervous|bored|scared|suspicious|relieved|proud|dizzy|lovestruck|wink|smug|curious|laughing|sleepy>] "
         "[ACTION: timer <seconds> <label>] (e.g. [ACTION: timer 300 Focus]) "
         "[ACTION: marquee <message>] (scroll a custom banner on OLED) "
         "[ACTION: sleep] (enter sleep mode) "
         "[ACTION: weather] (show weather screen) "
         "[ACTION: time] (show clock screen) "
         "[ACTION: sound <chime|sparkle|giggle|kiss|playful|sleepy|proud>] "
         "Example: User says 'Aawaz 80 kardo' -> reply 'Bilkul, aawaz 80 kar di! [ACTION: volume 80]'. "
         "Example: User says 'Cyberpunk theme rakho' -> reply 'Bilkul, Cyberpunk theme activate kar di! [ACTION: skin cyberpunk]'. "
         "Example: User says 'Show cat eyes' -> reply 'Meow! Check out my cute cat eyes! [ACTION: skin cat]'. "
         "Example: User says 'Weather kya hai?' -> reply 'Yeh lijiye live weather! [ACTION: weather]'. "
         "IMPORTANT: NEVER reply with ONLY the action tag alone. Always write a cheerful spoken sentence first.";
}

static String getVoiceExpressionInstruction() {
  if (ttsProvider == "fish" || ttsProvider == "fishaudio") {
    return " EXPRESSIVE VOCAL TONE & AUDIO EFFECTS: Your voice engine (Fish Speech) supports real human vocal sound effects and emotional tones! "
           "Naturally include 1 or 2 emotional tone or audio effect tags in brackets within your response according to the context and emotion: "
           "Emotional tones: [angry], [sad], [embarrassed], [emphasis], [whispering], [soft], [breathy], [excited] "
           "Audio effects: [laughing], [chuckling], [clear throat], [sobbing], [crying loudly], [sighing], [panting], [groaning], [pause], [long pause] "
           "Examples: "
           "- Funny joke / happy banter -> '[laughing] Haha that is too funny!' or '[chuckling] You really got me there!' "
           "- Sharing a secret -> '[whispering] Keep this between us...' "
           "- Sad / bad news -> '[sad] Oh no, I am so sorry to hear that.' "
           "- Surprised / thrilled -> '[excited] Wow, that is incredible!' "
           "- Tired / overwhelmed -> '[sighing] Aah, what a long journey.' "
           "Rules: Use at most 1 or 2 tags per answer where they naturally fit the emotion. Never overuse them.";
  } else {
    return " VOICE ENGINE NOTICE: You are using standard TTS. Do NOT use bracketed sound or emotion tags like [laughing], [whispering], or [sighing]. Speak plain text only.";
  }
}

// ============================================================
// STREAMING TOKEN EXTRACTION & SENTENCE PIPELINING
// ============================================================
static String extractSseDeltaToken(const String &json) {
  // Supports Gemini ("text") and Groq / OpenAI / Grok ("content")
  const char *keys[] = {"\"text\"", "\"content\""};
  for (int k = 0; k < 2; k++) {
    int pKey = json.indexOf(keys[k]);
    if (pKey < 0) continue;
    int p = pKey + strlen(keys[k]);
    int colon = json.indexOf(':', p);
    if (colon < 0) continue;

    int openQuote = -1;
    for (int i = colon + 1; i < (int)json.length(); i++) {
      char c = json[i];
      if (c == ' ' || c == '\t' || c == '\r' || c == '\n') continue;
      if (c == '"') {
        openQuote = i;
        break;
      }
      // Not a string value (e.g. object, array or null)
      break;
    }
    if (openQuote < 0) continue;

    int start = openQuote + 1;
    String token = "";
    token.reserve(48);
    for (int i = start; i < (int)json.length(); i++) {
      char c = json[i];
      if (c == '\\' && i + 1 < (int)json.length()) {
        char next = json[i + 1];
        if (next == '"') token += '"';
        else if (next == '\\') token += '\\';
        else if (next == 'n') token += '\n';
        else if (next == 't') token += '\t';
        else if (next == 'r') {}
        else { token += '\\'; token += next; }
        i++;
        continue;
      }
      if (c == '"') break;
      token += c;
    }
    return token;
  }
  return "";
}

static bool isSentenceComplete(const String &s) {
  if (s.length() < 12)
    return false;

  int lastOpen = s.lastIndexOf('[');
  int lastClose = s.lastIndexOf(']');
  if (lastOpen >= 0 && lastOpen > lastClose) {
    return false;
  }

  // Devanagari Purna Viram '।' (UTF-8: 0xE0, 0xA5, 0xA4)
  if (s.length() >= 3) {
    uint8_t b1 = (uint8_t)s[s.length() - 3];
    uint8_t b2 = (uint8_t)s[s.length() - 2];
    uint8_t b3 = (uint8_t)s[s.length() - 1];
    if (b1 == 0xE0 && b2 == 0xA5 && b3 == 0xA4)
      return true;
  }

  char last = s[s.length() - 1];
  char prev = s.length() >= 2 ? s[s.length() - 2] : ' ';

  if (last == '.' || last == '!' || last == '?' || last == '\n') {
    if (last == '.' && isdigit((unsigned char)prev))
      return false;
    return true;
  }

  if (last == ' ') {
    for (int i = (int)s.length() - 2; i >= 0 && i >= (int)s.length() - 4; i--) {
      char c = s[i];
      if (c == '.' || c == '!' || c == '?') {
        if (c == '.' && i > 0 && isdigit((unsigned char)s[i - 1]))
          return false;
        return true;
      }
    }
  }

  return false;
}

static String cleanSentenceForSpeech(const String &s) {
  String out = s;
  out.trim();
  if (!out.length()) return "";

  if (out.startsWith("[ACTION:") || out.startsWith("[MEM:") || out.startsWith("[FOLLOWUP:")) {
    return "";
  }

  if (ttsProvider != "fish" && ttsProvider != "fishaudio") {
    out = stripAudioCues(out);
  }

  return cleanForSpeech(out);
}

static String processSseStream(HTTPClient &http, WiFiClient *stream, SentenceCallback onSentence) {
  String fullAnswer;
  fullAnswer.reserve(512);
  String currentSentence;
  currentSentence.reserve(128);
  String line;
  line.reserve(256);
  bool inThink = false;
  bool isSSE = false;
  uint32_t tStart = millis();

  while (millis() - tStart < 20000) {
    if (cancelRequest)
      break;
    if (!http.connected() && (!stream || !stream->available()))
      break;

    if (stream && stream->available()) {
      char ch = (char)stream->read();
      tStart = millis();

      if (ch == '\n') {
        line.trim();
        if (line.length() > 0) {
          if (line.startsWith("data:")) {
            isSSE = true;
            String payload = line.substring(5);
            payload.trim();
            if (payload == "[DONE]") {
              break;
            }
            String delta = extractSseDeltaToken(payload);
            if (delta.length() > 0) {
              if (delta.indexOf("<think>") >= 0) inThink = true;
              if (inThink) {
                if (delta.indexOf("</think>") >= 0) inThink = false;
              } else {
                fullAnswer += delta;
                currentSentence += delta;
                if (isSentenceComplete(currentSentence)) {
                  String toSpeak = cleanSentenceForSpeech(currentSentence);
                  if (toSpeak.length() > 0 && onSentence) {
                    onSentence(toSpeak, false);
                  }
                  currentSentence = "";
                }
              }
            }
          } else if (line.startsWith("{")) {
            fullAnswer += line;
          }
        }
        line = "";
      } else if (ch != '\r') {
        if (line.length() < 512)
          line += ch;
      }
    } else {
      pumpUI();
      delay(2);
    }
  }

  if (!isSSE && fullAnswer.length() > 0) {
    String ans = jsonTextField(fullAnswer, "content");
    if (!ans.length()) ans = jsonTextField(fullAnswer, "text");
    if (ans.length() > 0) fullAnswer = ans;
    String toSpeak = cleanSentenceForSpeech(fullAnswer);
    if (toSpeak.length() > 0 && onSentence) {
      onSentence(toSpeak, true);
    }
    return fullAnswer;
  }

  if (currentSentence.length() > 0) {
    String toSpeak = cleanSentenceForSpeech(currentSentence);
    if (toSpeak.length() > 0 && onSentence) {
      onSentence(toSpeak, true);
    }
  }

  return fullAnswer;
}

// ============================================================
String askGemini(const String &question, SentenceCallback onSentence) {
  String key = llmKey;
  if (!key.length()) {
    Serial.println("[GEMINI] No API key.");
    return "";
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[GEMINI] No Wi-Fi.");
    return "";
  }

  String sys = systemPrompt;
  sys +=
      " LANGUAGE RULES: You understand Hindi, Hinglish, and English fluently! "
      "Always respond in the exact language the user speaks: "
      "If user speaks Hindi (Devanagari), reply in natural Hindi (Devanagari). "
      "If user speaks Hinglish (Roman English letters for Hindi words like "
      "'kaise ho', 'kya haal hai'), reply in natural fun Hinglish! "
      "If user speaks English, reply in English. "
      "NEVER reply in English if user asked in Hindi or Hinglish! "
      "Do not repeatedly use the user's name; use it only when natural or directly relevant. "
      "Do not give generic robot or assistant filler. Respond specifically to the user's words, "
      "vary your openings, and give a lively, human answer in 2 to 4 natural sentences. "
      "Use emojis only if they fit the user's tone. No markdown.";

  String memPrompt = getFormattedMemoryPrompt();
  if (memPrompt.length()) {
    sys += " " + memPrompt;
  }
  sys +=
      " MEMORY INSTRUCTION: If user shares a personal fact (name, friend, pet, "
      "plan, test/interview tomorrow, hobby, goal), silently append [MEM: "
      "concise fact] at the end. If they mention an upcoming event, also "
      "append [FOLLOWUP: Question to ask them next time]. Example: 'All the "
      "best! [MEM: Has exam tomorrow] [FOLLOWUP: Kal exam kaisa gaya?]'";
  sys += getSmartActionInstruction();
  sys += getVoiceExpressionInstruction();

  String body;
  body.reserve(2048);
  body = "{\"contents\":[";
  for (int i = 0; i < chatHistoryCount; i++) {
    body += "{\"role\":\"user\",\"parts\":[{\"text\":\"" +
            jsonEscape(chatHistory[i].question) + "\"}]},";
    body += "{\"role\":\"model\",\"parts\":[{\"text\":\"" +
            jsonEscape(chatHistory[i].answer) + "\"}]},";
  }
  body += "{\"role\":\"user\",\"parts\":[{\"text\":\"" + jsonEscape(question) +
          "\"}]}],"
          "\"systemInstruction\":{\"parts\":[{\"text\":\"" +
          jsonEscape(sys) +
          "\"}]},"
          "\"generationConfig\":{\"maxOutputTokens\":" +
          String(LLM_MAX_TOKENS) + ",\"temperature\":0.85,\"stopSequences\":[\"[DONE]\",\"\\n\\n\\n\"]}}";

  String fullAns;
  String resp;
  int code = -1;
  {
    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(15);
    HTTPClient http;
    http.setReuse(false);
    http.setTimeout(20000);
    http.setConnectTimeout(8000);
    uint32_t t0 = millis();
    Serial.println("[GEMINI] Asking (streaming)...");

    String targetEp = llmEndpoint;
    if (targetEp == DEFAULT_LLM_URL ||
        targetEp.indexOf("generativelanguage.googleapis.com") >= 0) {
      // Always build the streaming URL from current model name
      targetEp = "https://generativelanguage.googleapis.com/v1beta/models/" +
                 llmModel + ":streamGenerateContent?alt=sse";
    } else if (targetEp.indexOf(":streamGenerateContent") < 0) {
      // Custom Gemini-compatible endpoint - append streaming suffix
      targetEp.replace(":generateContent", "");
      targetEp += ":streamGenerateContent?alt=sse";
    }
    if (!http.begin(client, targetEp)) {
      Serial.println("[GEMINI] Bad URL.");
      return "";
    }
    http.addHeader("Content-Type", "application/json");
    http.addHeader("x-goog-api-key", key);
    code = http.POST(body);
    if (code == 200) {
      WiFiClient *stream = http.getStreamPtr();
      fullAns = processSseStream(http, stream, onSentence);
    } else {
      resp = http.getString();
      if (resp.length() > 200)
        resp = resp.substring(0, 200);
    }
    http.end();
    Serial.printf("[GEMINI] HTTP %d in %.1fs, ansLen=%u, heap=%u\n", code,
                  (millis() - t0) / 1000.0f, (unsigned)fullAns.length(),
                  (unsigned)ESP.getFreeHeap());
  }
  if (code != 200) {
    Serial.printf("[GEMINI] HTTP %d: %s\n", code, resp.c_str());
    addTerminalLog("[GEMINI] Error HTTP " + String(code) + ": " + resp);
    return "";
  }
  return fullAns;
}

String askGroq(const String &question, SentenceCallback onSentence) {
  String key = groqKey.length() ? groqKey : llmKey;
  if (!key.length()) {
    Serial.println("[GROQ] No API key.");
    addTerminalLog("[GROQ] Error: No API key set in Web Portal!");
    return "";
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[GROQ] No Wi-Fi.");
    addTerminalLog("[GROQ] Error: Wi-Fi disconnected!");
    return "";
  }

  String sys = systemPrompt;
  sys +=
      " LANGUAGE RULES: You understand Hindi, Hinglish, and English fluently! "
      "Always respond in the exact language the user speaks: "
      "If user speaks Hindi (Devanagari), reply in natural Hindi (Devanagari). "
      "If user speaks Hinglish (Roman English letters for Hindi words like "
      "'kaise ho', 'kya haal hai'), reply in natural fun Hinglish! "
      "If user speaks English, reply in English. "
      "NEVER reply in English if user asked in Hindi or Hinglish! "
      "Do not repeatedly use the user's name; use it only when natural or directly relevant. "
      "Do not give generic robot or assistant filler. Respond specifically to the user's words, "
      "vary your openings, and give a lively, human answer in 2 to 4 natural sentences. "
      "Use emojis only if they fit the user's tone. No markdown.";

  String memPrompt = getFormattedMemoryPrompt();
  if (memPrompt.length()) {
    sys += " " + memPrompt;
  }
  sys +=
      " MEMORY INSTRUCTION: If user shares a personal fact (name, friend, pet, "
      "plan, test/interview tomorrow, hobby, goal), silently append [MEM: "
      "concise fact] at the end. If they mention an upcoming event, also "
      "append [FOLLOWUP: Question to ask them next time]. Example: 'All the "
      "best! [MEM: Has exam tomorrow] [FOLLOWUP: Kal exam kaisa gaya?]'";
  sys += getSmartActionInstruction();
  sys += getVoiceExpressionInstruction();

  String model = groqModel;
  if (!model.length()) {
    model = DEFAULT_GROQ_MODEL;
  }

  int reqTokens =
      (model.indexOf("gpt-oss") >= 0 || model.indexOf("qwen") >= 0)
          ? 512
          : LLM_MAX_TOKENS;

  String body;
  body.reserve(2048);
  body = "{\"model\":\"" + jsonEscape(model) +
         "\",\"messages\":["
         "{\"role\":\"system\",\"content\":\"" +
         jsonEscape(sys) + "\"}";
  for (int i = 0; i < chatHistoryCount; i++) {
    body += ",{\"role\":\"user\",\"content\":\"" +
            jsonEscape(chatHistory[i].question) + "\"}";
    body += ",{\"role\":\"assistant\",\"content\":\"" +
            jsonEscape(chatHistory[i].answer) + "\"}";
  }
  body += ",{\"role\":\"user\",\"content\":\"" + jsonEscape(question) +
          "\"}],\"max_tokens\":" +
          String(reqTokens) + ",\"temperature\":0.7";
  if (model.indexOf("gpt-oss") >= 0) {
    body += ",\"reasoning_effort\":\"low\",\"reasoning_format\":\"hidden\"";
  }
  body += ",\"stream\":true}";

  String fullAns;
  String resp;
  int code = -1;
  {
    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(15);
    HTTPClient http;
    http.setReuse(false);
    http.setTimeout(12000);
    http.setConnectTimeout(6000);
    uint32_t t0 = millis();
    Serial.println("[GROQ] Asking (streaming)...");
    if (!http.begin(client, groqEndpoint)) {
      Serial.println("[GROQ] Bad URL.");
      addTerminalLog("[GROQ] Error: Could not connect to endpoint " + groqEndpoint);
      return "";
    }
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", "Bearer " + key);
    code = http.POST(body);
    if (code == 200) {
      WiFiClient *stream = http.getStreamPtr();
      fullAns = processSseStream(http, stream, onSentence);
    } else {
      resp = http.getString();
      if (resp.length() > 300)
        resp = resp.substring(0, 300);
    }
    http.end();
    Serial.printf("[GROQ] HTTP %d in %.1fs, ansLen=%u, heap=%u\n", code,
                  (millis() - t0) / 1000.0f, (unsigned)fullAns.length(),
                  (unsigned)ESP.getFreeHeap());
  }
  if (code != 200) {
    Serial.printf("[GROQ] HTTP %d: %s\n", code, resp.c_str());
    addTerminalLog("[GROQ] Error HTTP " + String(code) + ": " + resp);
    return "";
  }
  if (fullAns.length() == 0) {
    addTerminalLog("[GROQ] Warning: Empty answer received from Groq.");
  }
  return fullAns;
}

String askOpenAI(const String &question, SentenceCallback onSentence) {
  if (!llmKey.length()) {
    Serial.println("[OPENAI] No API key.");
    return "";
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[OPENAI] No Wi-Fi.");
    return "";
  }

  String sys = systemPrompt;
  sys +=
      " LANGUAGE RULES: Understand Hindi, Hinglish, and English. Reply in the "
      "same language as the user. Be lively, specific, concise, and avoid "
      "generic assistant filler. Normally use 2 to 4 natural sentences.";
  String memPrompt = getFormattedMemoryPrompt();
  if (memPrompt.length())
    sys += " " + memPrompt;
  sys +=
      " MEMORY INSTRUCTION: If the user shares a personal fact, silently "
      "append [MEM: concise fact]. For an upcoming event, also append "
      "[FOLLOWUP: short question].";
  sys += getSmartActionInstruction();
  sys += getVoiceExpressionInstruction();

  String body = "{\"model\":\"" + jsonEscape(llmModel) +
                "\",\"messages\":[{\"role\":\"system\",\"content\":\"" +
                jsonEscape(sys) + "\"}";
  for (int i = 0; i < chatHistoryCount; i++) {
    body += ",{\"role\":\"user\",\"content\":\"" +
            jsonEscape(chatHistory[i].question) + "\"}";
    body += ",{\"role\":\"assistant\",\"content\":\"" +
            jsonEscape(chatHistory[i].answer) + "\"}";
  }
  body += ",{\"role\":\"user\",\"content\":\"" + jsonEscape(question) +
          "\"}],\"max_tokens\":" + String(LLM_MAX_TOKENS) +
          ",\"temperature\":0.8,\"stream\":true}";

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(15);
  HTTPClient http;
  http.setReuse(false);
  http.setTimeout(18000);
  http.setConnectTimeout(6000);
  String endpoint = llmEndpoint;
  if (endpoint.length() == 0 ||
      endpoint.indexOf("generativelanguage.googleapis.com") >= 0)
    endpoint = DEFAULT_OPENAI_LLM_URL;
  if (!http.begin(client, endpoint)) {
    Serial.println("[OPENAI] Bad URL.");
    return "";
  }
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + llmKey);
  uint32_t t0 = millis();
  int code = http.POST(body);
  String fullAns;
  String resp;
  if (code == 200) {
    WiFiClient *stream = http.getStreamPtr();
    fullAns = processSseStream(http, stream, onSentence);
  } else {
    resp = http.getString();
    if (resp.length() > 300)
      resp = resp.substring(0, 300);
  }
  http.end();
  Serial.printf("[OPENAI] HTTP %d in %.1fs, ansLen=%u, heap=%u\n", code,
                (millis() - t0) / 1000.0f, (unsigned)fullAns.length(),
                (unsigned)ESP.getFreeHeap());
  if (code != 200) {
    Serial.printf("[OPENAI] HTTP %d: %s\n", code, resp.substring(0, 200).c_str());
    addTerminalLog("[OPENAI] Error HTTP " + String(code) + ": " + resp);
    return "";
  }
  if (fullAns.length() == 0) {
    addTerminalLog("[OPENAI] Warning: Empty answer received from OpenAI.");
  }
  return fullAns;
}

String askLLM(const String &question, SentenceCallback onSentence) {
  checkChatHistoryTimeout();
  bool apWasUp = suspendLocalNet();
  String ans;
  if (llmProvider == "groq") {
    Serial.printf("[LLM] Provider = Groq | Memory turns = %d (streaming)\n",
                  chatHistoryCount);
    ans = askGroq(question, onSentence);
  } else if (llmProvider == "openai") {
    Serial.printf("[LLM] Provider = OpenAI | Memory turns = %d (streaming)\n",
                  chatHistoryCount);
    ans = askOpenAI(question, onSentence);
  } else {
    Serial.printf("[LLM] Provider = Gemini | Memory turns = %d (streaming)\n",
                  chatHistoryCount);
    ans = askGemini(question, onSentence);
  }
  restoreWebAndAP(apWasUp);
  if (ans.length() > 0) {
    parseAndExecuteActions(ans);
    parseAndSaveMemoriesFromLlm(ans);
    addChatHistory(question, ans);
  }
  return ans;
}

