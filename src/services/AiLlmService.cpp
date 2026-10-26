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

// ============================================================
String askGemini(const String &question) {
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
          String(LLM_MAX_TOKENS) + ",\"temperature\":0.8}}";

  String resp;
  int code = -1;
  {
    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(15);
    HTTPClient http;
    http.setReuse(false);
    http.setTimeout(15000);
    http.setConnectTimeout(6000);
    uint32_t t0 = millis();
    Serial.println("[GEMINI] Asking...");

    String targetEp = llmEndpoint;
    if (targetEp == DEFAULT_LLM_URL ||
        targetEp.indexOf("generativelanguage.googleapis.com") >= 0) {
      targetEp = "https://generativelanguage.googleapis.com/v1beta/models/" +
                 llmModel + ":generateContent";
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
      resp.reserve(2048);
      uint32_t tr = millis();
      while (millis() - tr < 12000 && resp.length() < 4096) {
        if (!http.connected() && (!stream || !stream->available()))
          break;
        if (stream) {
          while (stream->available() && resp.length() < 4096) {
            resp += (char)stream->read();
          }
        }
        if (cancelRequest)
          break;
        pumpUI();
        delay(1);
      }
    } else {
      resp = http.getString();
      if (resp.length() > 200)
        resp = resp.substring(0, 200);
    }
    http.end();
    Serial.printf("[GEMINI] HTTP %d in %.1fs, respLen=%u, heap=%u\n", code,
                  (millis() - t0) / 1000.0f, (unsigned)resp.length(),
                  (unsigned)ESP.getFreeHeap());
  }
  if (code != 200) {
    Serial.printf("[GEMINI] HTTP %d: %s\n", code, resp.c_str());
    return "";
  }
  String ans = jsonTextField(resp, "text");
  if (!ans.length()) ans = jsonTextField(resp, "content");
  if (!ans.length()) {
    Serial.printf("[GEMINI] Warning: empty parsed. Raw: %s\n", resp.substring(0, 300).c_str());
  }
  return cleanForSpeech(ans);
}

String askGroq(const String &question) {
  String key = groqKey;
  if (!key.length()) {
    Serial.println("[GROQ] No API key.");
    return "";
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[GROQ] No Wi-Fi.");
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

  int reqTokens =
      (groqModel.indexOf("gpt-oss") >= 0 || groqModel.indexOf("qwen") >= 0)
          ? 512
          : LLM_MAX_TOKENS;

  String body;
  body.reserve(2048);
  body = "{\"model\":\"" + jsonEscape(groqModel) +
         "\","
         "\"messages\":["
         "{\"role\":\"system\",\"content\":\"" +
         jsonEscape(sys) + "\"}";
  for (int i = 0; i < chatHistoryCount; i++) {
    body += ",{\"role\":\"user\",\"content\":\"" +
            jsonEscape(chatHistory[i].question) + "\"}";
    body += ",{\"role\":\"assistant\",\"content\":\"" +
            jsonEscape(chatHistory[i].answer) + "\"}";
  }
  body += ",{\"role\":\"user\",\"content\":\"" + jsonEscape(question) +
          "\"}],"
          "\"max_tokens\":" +
          String(reqTokens) + ",\"temperature\":0.7";
  if (groqModel.indexOf("gpt-oss") >= 0) {
    body += ",\"reasoning_effort\":\"low\",\"reasoning_format\":\"hidden\"";
  }
  body += "}";

  String resp;
  int code = -1;
  {
    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(15);
    HTTPClient http;
    http.setReuse(false);
    http.setTimeout(18000);
    http.setConnectTimeout(6000);
    uint32_t t0 = millis();
    Serial.println("[GROQ] Asking...");
    if (!http.begin(client, groqEndpoint)) {
      Serial.println("[GROQ] Bad URL.");
      return "";
    }
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", "Bearer " + key);
    code = http.POST(body);
    if (code == 200) {
      WiFiClient *stream = http.getStreamPtr();
      resp.reserve(2048);
      uint32_t tr = millis();
      while (millis() - tr < 15000 && resp.length() < 4096) {
        if (!http.connected() && (!stream || !stream->available()))
          break;
        if (stream) {
          while (stream->available() && resp.length() < 4096) {
            resp += (char)stream->read();
          }
        }
        if (cancelRequest)
          break;
        pumpUI();
        delay(1);
      }
    } else {
      resp = http.getString();
      if (resp.length() > 300)
        resp = resp.substring(0, 300);
    }
    http.end();
    Serial.printf("[GROQ] HTTP %d in %.1fs, respLen=%u, heap=%u\n", code,
                  (millis() - t0) / 1000.0f, (unsigned)resp.length(),
                  (unsigned)ESP.getFreeHeap());
  }
  if (code != 200) {
    Serial.printf("[GROQ] HTTP %d: %s\n", code, resp.c_str());
    return "";
  }
  String ans = jsonTextField(resp, "content");
  if (!ans.length()) ans = jsonTextField(resp, "text");
  if (!ans.length()) {
    Serial.printf("[GROQ] Warning: empty 'content' parsed. Raw: %s\n", resp.substring(0, 300).c_str());
  }
  return cleanForSpeech(ans);
}

String askOpenAI(const String &question) {
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
          ",\"temperature\":0.8}";

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
  String resp = http.getString();
  http.end();
  if (resp.length() > 4096)
    resp = resp.substring(0, 4096);
  Serial.printf("[OPENAI] HTTP %d in %.1fs, respLen=%u, heap=%u\n", code,
                (millis() - t0) / 1000.0f, (unsigned)resp.length(),
                (unsigned)ESP.getFreeHeap());
  if (code != 200) {
    Serial.printf("[OPENAI] HTTP %d: %s\n", code, resp.substring(0, 200).c_str());
    return "";
  }
  String ans = jsonTextField(resp, "content");
  if (!ans.length()) ans = jsonTextField(resp, "text");
  if (!ans.length()) {
    Serial.printf("[OPENAI] Warning: empty 'content' parsed. Raw: %s\n", resp.substring(0, 300).c_str());
  }
  return cleanForSpeech(ans);
}

String askLLM(const String &question) {
  checkChatHistoryTimeout();
  bool apWasUp = suspendLocalNet();
  String ans;
  if (llmProvider == "groq") {
    Serial.printf("[LLM] Provider = Groq | Memory turns = %d\n",
                  chatHistoryCount);
    ans = askGroq(question);
  } else if (llmProvider == "openai") {
    Serial.printf("[LLM] Provider = OpenAI | Memory turns = %d\n",
                  chatHistoryCount);
    ans = askOpenAI(question);
  } else {
    Serial.printf("[LLM] Provider = Gemini | Memory turns = %d\n",
                  chatHistoryCount);
    ans = askGemini(question);
  }
  restoreWebAndAP(apWasUp);
  if (ans.length() > 0) {
    parseAndExecuteActions(ans);
    parseAndSaveMemoriesFromLlm(ans);
    addChatHistory(question, ans);
  }
  return ans;
}

