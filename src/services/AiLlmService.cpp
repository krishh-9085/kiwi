#include "services/AiLlmService.h"

#include "services/AiTransport.h"

#include "services/AiChatHistory.h"

#include "core/Timing.h"

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
  return " ACTIONS: Append [ACTION: tag] after answer if needed: "
         "[ACTION: emotion happy/sad/angry/cool/surprised/sleepy/wink/laughing/excited] "
         "[ACTION: skin kiwi/vector/pixel/mecha] [ACTION: volume 0-100] [ACTION: timer SECS] "
         "[ACTION: sleep] (if user says so jao/sleep). Reply first, tag at end.";
}

static String getVoiceExpressionInstruction() {
  if (ttsProvider == "fish" || ttsProvider == "fishaudio") {
    return " VOCAL RULES: "
           "1. For normal answers, speak naturally in plain text with ZERO vocal tags. "
           "2. CRITICAL: If the user asks you to laugh in ANY way (e.g. 'has ke dikhao', 'haso', 'laugh', 'hasi sunao', 'thoda haso'): your ENTIRE reply MUST BE ONLY '[laughing] Hahahahaha! Ahahaha, hahahaha! Hahahahaha!' with ABSOLUTELY ZERO EXTRA WORDS, NO TALKING, AND NO CONVERSATIONAL FILLER. "
           "3. ONLY use emotion tags if user explicitly asks (e.g. 'gusse me bolo' -> [angry]; 'ro ke bolo' -> [sobbing]; 'has ke bolo' -> [laughing]; 'dheere bolo' -> [whisper]). "
           "Allowed emotion tags: [laughing] [giggle] [sobbing] [angry] [sad] [whisper] [excited].";
  } else {
    return " VOICE: Plain text only. No bracketed tone or sound tags. If asked to laugh, reply ONLY with 'Hahahahaha! Ahahaha, hahahaha!' without any talking.";
  }
}





// ============================================================

// STREAMING TOKEN EXTRACTION & SENTENCE PIPELINING

// ============================================================

static String extractSseDeltaToken(const String &json) {

  // Supports Gemini ("text") and Groq / Grok ("content")

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

  if (s.length() < 40)

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

  // Fast sub-clause split on natural speech pause (commas/semicolons) when 55+ chars accumulated
  // Do not split on commas/semicolons mid-sentence to avoid choppy fragmented audio
  if (false && (last == ',' || last == ';' || last == ':') && s.length() >= 55) {
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



  if (out.startsWith("[ACTION:") || out.startsWith("[action:") ||
      out.startsWith("[MEM:") || out.startsWith("[mem:") ||
      out.startsWith("[FOLLOWUP:") || out.startsWith("[followup:")) {
    return "";
  }



  if (ttsProvider != "fish" && ttsProvider != "fishaudio") {

    out = stripAudioCues(out);

  }



  return cleanForSpeech(out);

}



static String processSseStream(HTTPClient &http, WiFiClient *stream, SentenceCallback onSentence) {

  String fullAnswer;

  fullAnswer.reserve(1024);

  String currentSentence;

  currentSentence.reserve(256);

  String line;

  line.reserve(1024);

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
                  if (cancelRequest)
                    break;
                  String toSpeak = cleanSentenceForSpeech(currentSentence);

                  if (toSpeak.length() > 0 && onSentence) {

                    onSentence(toSpeak, false);
                    tStart = millis();
                    if (cancelRequest)
                      break;

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
        if (line.length() < 2048)
          line += ch;
      }
      lastProgressAt = millis();
    } else {
      serviceUi();
      yield();
    }

  }



  if (!cancelRequest && !isSSE && fullAnswer.length() > 0) {

    String ans = jsonTextField(fullAnswer, "content");

    if (!ans.length()) ans = jsonTextField(fullAnswer, "text");

    if (ans.length() > 0) fullAnswer = ans;

    String toSpeak = cleanSentenceForSpeech(fullAnswer);

    if (toSpeak.length() > 0 && onSentence) {

      onSentence(toSpeak, true);

    }

    return fullAnswer;

  }



  if (!cancelRequest && currentSentence.length() > 0) {

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

      " LANG: Reply in the same language as user (Hindi/Hinglish/English). "

      "Never switch to English if user speaks Hindi/Hinglish. "

      "2-4 natural sentences, lively, specific. No markdown.";



  sys += getFormattedMemoryPrompt();
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

  bool isGeminiThinkingModel = (llmModel.indexOf("2.5") >= 0 || llmModel.indexOf("2.0") >= 0 || llmModel.indexOf("thinking") >= 0);

  body += "{\"role\":\"user\",\"parts\":[{\"text\":\"" + jsonEscape(question) +

          "\"}]}],"

          "\"systemInstruction\":{\"parts\":[{\"text\":\"" +

          jsonEscape(sys) +

          "\"}]},"

          "\"generationConfig\":{\"maxOutputTokens\":" +

          String(LLM_MAX_TOKENS) + ",\"temperature\":0.85" +
          (isGeminiThinkingModel ? ",\"thinkingConfig\":{\"thinkingBudget\":0}" : "") +
          "}}";



  String fullAns;

  String resp;

  int code = -1;

  {

    static WiFiClientSecure client;
    static HTTPClient http;
    static bool inited = false;
    if (!inited) {
      client.setInsecure();
      client.setTimeout(8000);
      inited = true;
    }
    http.setReuse(psramFound());
    http.setTimeout(8000);
    http.setConnectTimeout(6000);

    // DNS pre-check before TLS (fixes HTTP -1 on first boot / after AP mode)

    ensureDns("generativelanguage.googleapis.com");

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

      " LANG: Reply in the same language as user (Hindi/Hinglish/English). "

      "Never switch to English if user speaks Hindi/Hinglish. "

      "2-4 natural sentences, lively, specific. No markdown.";



  sys += getFormattedMemoryPrompt();
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

          String(reqTokens) + ",\"temperature\":0.85,\"top_p\":0.95";

  if (model.indexOf("gpt-oss") >= 0) {

    body += ",\"reasoning_effort\":\"low\",\"reasoning_format\":\"hidden\"";

  }

  body += ",\"stream\":true}";



  // DNS pre-check: ensures api.groq.com resolves before TLS (fixes HTTP -1)

  ensureDns("api.groq.com");



  String fullAns;

  String resp;

  int code = -1;

  {

    static WiFiClientSecure client;
    static HTTPClient http;
    static bool inited = false;
    if (!inited) {
      client.setInsecure();
      client.setTimeout(8000);
      inited = true;
    }
    http.setReuse(psramFound());
    http.setTimeout(8000);
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



String askLLM(const String &question, SentenceCallback onSentence, bool saveChatHistory) {

  checkChatHistoryTimeout();

  bool apWasUp = suspendLocalNet();

  String ans;

  if (llmProvider == "groq") {

    Serial.printf("[LLM] Provider = Groq | Memory turns = %d (streaming)\n",

                  chatHistoryCount);

    ans = askGroq(question, onSentence);

  } else {

    Serial.printf("[LLM] Provider = Gemini | Memory turns = %d (streaming)\n",

                  chatHistoryCount);

    ans = askGemini(question, onSentence);

  }

  restoreWebAndAP(apWasUp);

  if (ans.length() > 0) {

    // NOTE: With streaming TTS enabled, ACTION tags are executed after the full LLM
    // reply has completed streaming. Therefore, actions (volume, eye skins, timer, etc.)
    // fire after the first sentence (and any streaming sentences) has already been spoken.
    parseAndExecuteActions(ans);
    parseAndSaveMemoriesFromLlm(ans);
    if (saveChatHistory) {
      addChatHistory(question, ans);
    }

  }

  return ans;

}



