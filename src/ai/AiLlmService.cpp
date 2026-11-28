#include "ai/AiLlmService.h"

#include "ai/AiTransport.h"

#include "ai/AiChatHistory.h"

#include "core/Timing.h"

#include "ai/AiTextTools.h"

#include "memory/MemoryService.h"

#include "config/AppConfig.h"

#include "face/FaceEngine.h"

#include "behavior/InteractionManager.h"

#include "commands/ActionExecutor.h"

#include "net/WifiManager.h"

#include "web/WebPortal.h"

#include <WiFi.h>

#include <HTTPClient.h>

#include <WiFiClientSecure.h>
#include <time.h>

static String getLlmTimeContextPrompt() {
  struct tm ti;
  if (timeSynced && getLocalTime(&ti, 5)) {
    const char *DAYS[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    const char *dayStr = (ti.tm_wday >= 0 && ti.tm_wday < 7) ? DAYS[ti.tm_wday] : "";
    const char *period = "Night";
    if (ti.tm_hour >= 5 && ti.tm_hour < 12) period = "Morning";
    else if (ti.tm_hour >= 12 && ti.tm_hour < 17) period = "Afternoon";
    else if (ti.tm_hour >= 17 && ti.tm_hour < 21) period = "Evening";
    int h12 = ti.tm_hour % 12;
    if (h12 == 0) h12 = 12;
    char buf[48];
    snprintf(buf, sizeof(buf), "%02d:%02d %s (%s, %s)", h12, ti.tm_min,
             (ti.tm_hour >= 12) ? "PM" : "AM", period, dayStr);
    return String(" [CLOCK: Current local time is ") + buf +
           ". Use matching greetings (NEVER say Good morning outside Morning); only mention exact clock time if asked.]";
  }
  return " [CLOCK: Current local time is unknown; never guess time of day or say Good morning/afternoon/evening/night unless asked.]";
}

static const char *getReplyLangInstruction() {
  if (replyLang == "en") {
    return " LANG: Always reply in natural, crisp conversational English. "
           "2-4 natural sentences, lively, specific. No markdown.";
  }
  if (replyLang == "hi") {
    return " LANG: Reply in natural Hinglish or Hindi (never switch to pure English). "
           "2-4 natural sentences, lively, specific. No markdown.";
  }
  return " LANG: Reply in the same language as user (Hindi/Hinglish/English). "
         "Never switch to English if user speaks Hindi/Hinglish. "
         "2-4 natural sentences, lively, specific. No markdown.";
}

static String getSmartActionInstruction() {
  return " ACTIONS: Append [ACTION: tag] after answer if needed: "
         "[ACTION: emotion "
         "happy/sad/angry/cool/surprised/sleepy/wink/laughing/excited/curious/"
         "confused/proud/smug/scared/nervous/cute/crying/hearts/dizzy/singing/"
         "party/evil/shocked] "
         "[ACTION: skin kiwi/pixel/lumina/mecha/emo/kiwi_plus] [ACTION: volume 0-100] "
         "[ACTION: timer SECS] "
         "[ACTION: personality friendly/funny/serious/bakchod/cute] (only if user explicitly asks to switch mode) "
         "[ACTION: sleep] (if user says so jao/sleep). Reply first, tag at "
         "end.";
}

static String getVoiceExpressionInstruction() {
  if (ttsProvider == "fish" || ttsProvider == "fishaudio") {
    return " VOCAL RULES: "
           "1. CRITICAL: If the user asks you to laugh in ANY way (e.g. 'has "
           "ke dikhao', 'haso', 'laugh', 'hasi sunao', 'thoda haso'): your "
           "ENTIRE reply MUST BE ONLY '[ACTION: sound laugh]' with ABSOLUTELY "
           "ZERO EXTRA WORDS, NO TALKING, AND NO CONVERSATIONAL FILLER. "
           "2. Put an emotion tag at the START of EACH sentence matching its "
           "tone so Kiwi switches facial expressions dynamically while speaking "
           "(example: '[curious] Yeh sawal badhiya hai! [laughing] Par bada "
           "simple scene hai yaar. [proud] Main abhi explain karta hoon!'). "
           "Switch tags whenever the emotion shifts across sentences. "
           "3. Use ONLY these supported Fish Audio tags: [laughing] [giggle] "
           "[sobbing] [angry] [sad] [whisper] [excited] [curious] [shy] "
           "[proud] [surprised] [scared] [cute].";
  } else {
    return " VOICE & MOOD: Reply in plain spoken text (no markdown). If asked "
           "to laugh, reply ONLY with '[ACTION: sound laugh]' without any "
           "talking. Put a fitting mood tag at the START of EACH sentence or "
           "part so Kiwi switches facial emotions dynamically while speaking! "
           "Change tags across sentences as the tone shifts (example: "
           "'[curious] Arre yeh kya pooch liya? [surprised] Sach me bawal "
           "cheese hai! [laughing] Haha chal mazak side, main samjhata "
           "hoon. [wink] Bas dhyan se sun!'). Put tags only on full "
           "sentences, not single-word fragments. Allowed tags: [curious], "
           "[happy], [excited], [laughing], [smug], [proud], [surprised], "
           "[shocked], [wink], [cute], [confused], [sad], [angry], [shy], "
           "[relieved], [cool], [scared]. Kiwi hides tags from speech and uses "
           "them only for live face expressions.";
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

    if (pKey < 0)
      continue;

    int p = pKey + strlen(keys[k]);

    int colon = json.indexOf(':', p);

    if (colon < 0)
      continue;

    int openQuote = -1;

    for (int i = colon + 1; i < (int)json.length(); i++) {

      char c = json[i];

      if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
        continue;

      if (c == '"') {

        openQuote = i;

        break;
      }

      // Not a string value (e.g. object, array or null)

      break;
    }

    if (openQuote < 0)
      continue;

    int start = openQuote + 1;

    String token = "";

    token.reserve(48);

    for (int i = start; i < (int)json.length(); i++) {

      char c = json[i];

      if (c == '\\' && i + 1 < (int)json.length()) {

        char next = json[i + 1];

        if (next == '"')
          token += '"';

        else if (next == '\\')
          token += '\\';

        else if (next == 'n')
          token += '\n';

        else if (next == 't')
          token += '\t';

        else if (next == 'r') {
        }

        else if (next == 'u' && i + 5 < (int)json.length()) {
          long cp = strtol(json.substring(i + 2, i + 6).c_str(), NULL, 16);
          if (cp >= 0xD800 && cp <= 0xDBFF && i + 11 < (int)json.length() &&
              json[i + 6] == '\\' && json[i + 7] == 'u') {
            long lo = strtol(json.substring(i + 8, i + 12).c_str(), NULL, 16);
            if (lo >= 0xDC00 && lo <= 0xDFFF) {
              cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
              i += 6;
            }
          }
          if (cp < 0x80)
            token += (char)cp;
          else if (cp < 0x800) {
            token += (char)(0xC0 | (cp >> 6));
            token += (char)(0x80 | (cp & 0x3F));
          } else if (cp < 0x10000) {
            token += (char)(0xE0 | (cp >> 12));
            token += (char)(0x80 | ((cp >> 6) & 0x3F));
            token += (char)(0x80 | (cp & 0x3F));
          } else {
            token += (char)(0xF0 | (cp >> 18));
            token += (char)(0x80 | ((cp >> 12) & 0x3F));
            token += (char)(0x80 | ((cp >> 6) & 0x3F));
            token += (char)(0x80 | (cp & 0x3F));
          }
          i += 6;
          continue;
        }

        else {
          token += '\\';
          token += next;
        }

        i++;

        continue;
      }

      if (c == '"')
        break;

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

  // Fast sub-clause split on natural speech pause (commas/semicolons) when 55+
  // chars accumulated Do not split on commas/semicolons mid-sentence to avoid
  // choppy fragmented audio
  if (false && (last == ',' || last == ';' || last == ':') &&
      s.length() >= 55) {
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

  if (!out.length())
    return "";

  String upper = out;
  upper.toUpperCase();
  if (upper.startsWith("[ACTION:") || upper.startsWith("[MEM:") ||
      upper.startsWith("[FOLLOWUP:")) {
    return "";
  }

  // Preserve inline mood tags ([curious], [happy], [laughing], etc.) so
  // InteractionManager can dynamically animate each segment on the OLED.
  String cleaned = cleanForSpeech(out, true);
  String spokenOnly = stripAudioCues(cleaned);
  spokenOnly.trim();
  if (!spokenOnly.length())
    return "";

  return cleaned;
}

static String extractUrlHost(const String &url, const char *fallbackHost) {
  int start = url.indexOf("://");
  start = (start >= 0) ? (start + 3) : 0;
  int slash = url.indexOf('/', start);
  String host =
      (slash >= 0) ? url.substring(start, slash) : url.substring(start);
  int colon = host.indexOf(':');
  if (colon >= 0)
    host = host.substring(0, colon);
  host.trim();
  return host.length() ? host : String(fallbackHost);
}

static bool isHexChunkSizeLine(const String &s) {
  if (s.length() == 0 || s.length() > 12)
    return false;
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == ';')
      return i > 0;
    bool isHex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
                 (c >= 'A' && c <= 'F');
    if (!isHex)
      return false;
  }
  return true;
}

static String processSseStream(HTTPClient &http, WiFiClient *stream,
                               SentenceCallback onSentence,
                               bool preferTextKey) {
  String fullAnswer;
  fullAnswer.reserve(1024);
  String currentSentence;
  currentSentence.reserve(256);
  String line;
  line.reserve(1024);
  String chunkLenLine;
  chunkLenLine.reserve(16);

  bool inThink = false;
  bool isSSE = false;
  bool headerChunked =
      http.header("Transfer-Encoding").equalsIgnoreCase("chunked");
  bool isChunked = headerChunked;
  bool autoDetectPending = !headerChunked;
  long chunkLeft = 0;
  bool chunkSkipTrailerCrlf = false;
  bool sawFirstLine = false;
  bool cleanEof = false;

  bool sawDone = false;
  uint32_t sawDoneAt = 0;

  auto handleSseLine = [&](const String &rawLine) -> bool {
    if (sawDone)
      return isChunked;
    String ln = rawLine;
    ln.trim();
    if (ln.length() == 0)
      return true;
    if (ln.startsWith("data:")) {
      isSSE = true;
      String payload = ln.substring(5);
      payload.trim();
      if (payload == "[DONE]") {
        sawDone = true;
        sawDoneAt = millis();
        // Keep chunked loop alive briefly so the trailing "0\r\n\r\n" chunk
        // terminator can finish cleanly and preserve Groq TLS keep-alive.
        return isChunked;
      }
      String delta = extractSseDeltaToken(payload);
      if (delta.length() > 0) {
        if (!inThink) {
          int tStartIdx = delta.indexOf("<think>");
          if (tStartIdx >= 0) {
            String before = delta.substring(0, tStartIdx);
            String rest = delta.substring(tStartIdx + 7);
            int tEndIdx = rest.indexOf("</think>");
            if (tEndIdx >= 0) {
              delta = before + rest.substring(tEndIdx + 8);
            } else {
              delta = before;
              inThink = true;
            }
          }
        } else {
          int tEndIdx = delta.indexOf("</think>");
          if (tEndIdx >= 0) {
            delta = delta.substring(tEndIdx + 8);
            inThink = false;
          } else {
            delta = "";
          }
        }
        if (delta.length() > 0) {
          fullAnswer += delta;
          currentSentence += delta;
          if (isSentenceComplete(currentSentence)) {
            if (cancelRequest)
              return false;
            String toSpeak = cleanSentenceForSpeech(currentSentence);
            if (toSpeak.length() > 0 && onSentence) {
              onSentence(toSpeak, false);
              if (cancelRequest)
                return false;
            }
            currentSentence = "";
          }
        }
      }
    } else if (!isSSE && (fullAnswer.length() > 0 || ln.startsWith("{") ||
                          ln.startsWith("["))) {
      fullAnswer += ln;
    }
    return true;
  };

  uint32_t tStart = millis();
  uint8_t sseBuf[256];
  bool stopLoop = false;
  while (!stopLoop && millis() - tStart < 20000) {
    if (cancelRequest)
      break;
    if (sawDone && millis() - sawDoneAt >= 150)
      break;
    if (!http.connected() && (!stream || !stream->available())) {
      cleanEof = true;
      break;
    }

    int avail = stream ? stream->available() : 0;
    if (avail > 0) {
      int nr = stream->read(sseBuf, avail < (int)sizeof(sseBuf) ? avail
                                                                : (int)sizeof(sseBuf));
      if (nr > 0) {
        tStart = millis();
        lastProgressAt = tStart;
      }
      for (int bi = 0; bi < nr && !stopLoop; bi++) {
        char ch = (char)sseBuf[bi];

        if (isChunked) {
          if (chunkLeft == 0) {
            if (chunkSkipTrailerCrlf) {
              if (ch == '\r')
                continue;
              if (ch == '\n') {
                chunkSkipTrailerCrlf = false;
                continue;
              }
              chunkSkipTrailerCrlf = false;
            }
            if (ch == '\r')
              continue;
            if (ch == '\n') {
              chunkLenLine.trim();
              if (chunkLenLine.length() == 0)
                continue;
              long sz = strtol(chunkLenLine.c_str(), NULL, 16);
              chunkLenLine = "";
              if (sz <= 0) {
                cleanEof = true;
                stopLoop = true;
                break;
              }
              chunkLeft = sz;
            } else if (chunkLenLine.length() < 16) {
              chunkLenLine += ch;
            }
            continue;
          }
          chunkLeft--;
          if (chunkLeft == 0) {
            chunkSkipTrailerCrlf = true;
          }
        }

        if (ch == '\n') {
          if (autoDetectPending && !sawFirstLine) {
            String trimmedFirst = line;
            trimmedFirst.trim();
            if (trimmedFirst.length() > 0) {
              sawFirstLine = true;
              autoDetectPending = false;
              if (isHexChunkSizeLine(trimmedFirst)) {
                isChunked = true;
                long sz = strtol(trimmedFirst.c_str(), NULL, 16);
                line = "";
                if (sz <= 0) {
                  cleanEof = true;
                  stopLoop = true;
                  break;
                }
                chunkLeft = sz;
                continue;
              }
            }
          }
          bool cont = handleSseLine(line);
          line = "";
          if (!cont) {
            stopLoop = true;
            break;
          }
          tStart = millis();
        } else if (ch != '\r') {
          if (line.length() < 2048)
            line += ch;
        }
      }
    } else {
      serviceUi();
      yield();
    }
  }

  // Consume trailing "\r\n" after chunked "0\r\n" so keep-alive buffer is empty
  if (cleanEof && isChunked && stream) {
    uint32_t drainEnd = millis() + 20;
    while (millis() < drainEnd) {
      int av = stream->available();
      if (av > 0) {
        while (av-- > 0)
          stream->read();
        break;
      }
      if (!http.connected())
        break;
      yield();
    }
  }

  if (line.length() > 0 && !cancelRequest) {
    handleSseLine(line);
  }

  // Drain/abort leftover bytes if stopped before clean EOF so socket reuse
  // won't desync
  if (!cleanEof && stream) {
    stream->stop();
  }

  if (!cancelRequest && !isSSE && fullAnswer.length() > 0) {
    String ans = jsonTextField(fullAnswer, preferTextKey ? "text" : "content");
    if (!ans.length())
      ans = jsonTextField(fullAnswer, preferTextKey ? "content" : "text");
    if (ans.length() > 0)
      fullAnswer = ans;
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

String askGemini(const String &question, SentenceCallback onSentence, bool includeHistory) {

  String key = llmKey;

  if (!key.length()) {

    Serial.println("[GEMINI] No API key.");

    return "";
  }

  if (WiFi.status() != WL_CONNECTED) {

    Serial.println("[GEMINI] No Wi-Fi.");

    return "";
  }

  String sys = getEffectiveSystemPrompt();

  sys += getReplyLangInstruction();

  sys += getFormattedMemoryPrompt();
  sys += getLlmTimeContextPrompt();
  sys += getSmartActionInstruction();

  sys += getVoiceExpressionInstruction();

  String body;

  body.reserve(2048);

  body = "{\"contents\":[";

  if (includeHistory) {
    String activePersona = normalizePersonalityMode(personalityMode);
    for (int i = 0; i < chatHistoryCount; i++) {
      if (chatHistory[i].persona.length() > 0 &&
          chatHistory[i].persona != activePersona) {
        continue;
      }

      body += "{\"role\":\"user\",\"parts\":[{\"text\":\"" +

              jsonEscape(chatHistory[i].question) + "\"}]},";

      body += "{\"role\":\"model\",\"parts\":[{\"text\":\"" +

              jsonEscape(chatHistory[i].answer) + "\"}]},";
    }
  }

  bool isGeminiThinkingModel =
      (llmModel.indexOf("2.5") >= 0 || llmModel.indexOf("2.0") >= 0 ||
       llmModel.indexOf("thinking") >= 0);

  body += "{\"role\":\"user\",\"parts\":[{\"text\":\"" + jsonEscape(question) +

          "\"}]}],"

          "\"systemInstruction\":{\"parts\":[{\"text\":\"" +

          jsonEscape(sys) +

          "\"}]},"

          "\"generationConfig\":{\"maxOutputTokens\":" +

          String(LLM_MAX_TOKENS) + ",\"temperature\":0.85" +
          (isGeminiThinkingModel ? ",\"thinkingConfig\":{\"thinkingBudget\":0}"
                                 : "") +
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

    // DNS pre-check before TLS (fixes HTTP -1 on first boot / after AP mode)
    ensureDns(extractUrlHost(targetEp, "generativelanguage.googleapis.com"));

    uint32_t t0 = millis();

    Serial.println("[GEMINI] Asking (streaming)...");

    if (!http.begin(client, targetEp)) {

      Serial.println("[GEMINI] Bad URL.");

      return "";
    }

    const char *sseHdrs[] = {"Transfer-Encoding"};
    http.collectHeaders(sseHdrs, 1);
    http.addHeader("Content-Type", "application/json");

    http.addHeader("x-goog-api-key", key);

    code = http.POST(body);

    if (code == 200) {

      WiFiClient *stream = http.getStreamPtr();

      fullAns = processSseStream(http, stream, onSentence, true);

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

String askGroq(const String &question, SentenceCallback onSentence, bool includeHistory) {

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

  String sys = getEffectiveSystemPrompt();

  sys += getReplyLangInstruction();

  sys += getFormattedMemoryPrompt();
  sys += getLlmTimeContextPrompt();
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

  if (includeHistory) {
    String activePersona = normalizePersonalityMode(personalityMode);
    for (int i = 0; i < chatHistoryCount; i++) {
      if (chatHistory[i].persona.length() > 0 &&
          chatHistory[i].persona != activePersona) {
        continue;
      }

      body += ",{\"role\":\"user\",\"content\":\"" +

              jsonEscape(chatHistory[i].question) + "\"}";

      body += ",{\"role\":\"assistant\",\"content\":\"" +

              jsonEscape(chatHistory[i].answer) + "\"}";
    }
  }

  body += ",{\"role\":\"user\",\"content\":\"" + jsonEscape(question) +

          "\"}],\"max_tokens\":" +

          String(reqTokens) + ",\"temperature\":0.85,\"top_p\":0.95";

  if (model.indexOf("gpt-oss") >= 0) {

    body += ",\"reasoning_effort\":\"low\",\"reasoning_format\":\"hidden\"";
  }

  body += ",\"stream\":true}";

  // DNS pre-check: ensures endpoint resolves before TLS (fixes HTTP -1)

  ensureDns(extractUrlHost(groqEndpoint, "api.groq.com"));

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

      addTerminalLog("[GROQ] Error: Could not connect to endpoint " +
                     groqEndpoint);

      return "";
    }

    const char *sseHdrs[] = {"Transfer-Encoding"};
    http.collectHeaders(sseHdrs, 1);
    http.addHeader("Content-Type", "application/json");

    http.addHeader("Authorization", "Bearer " + key);

    code = http.POST(body);

    if (code == 200) {

      WiFiClient *stream = http.getStreamPtr();

      fullAns = processSseStream(http, stream, onSentence, false);

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

String askLLM(const String &question, SentenceCallback onSentence,
              bool saveChatHistory) {

  checkChatHistoryTimeout();

  bool apWasUp = suspendLocalNet();

  String ans;

  if (llmProvider == "groq") {

    Serial.printf("[LLM] Provider = Groq | Memory turns = %d (history=%s, streaming)\n",

                  chatHistoryCount, saveChatHistory ? "yes" : "no");

    ans = askGroq(question, onSentence, saveChatHistory);

  } else {

    Serial.printf("[LLM] Provider = Gemini | Memory turns = %d (history=%s, streaming)\n",

                  chatHistoryCount, saveChatHistory ? "yes" : "no");

    ans = askGemini(question, onSentence, saveChatHistory);
  }

  restoreWebAndAP(apWasUp);

  if (ans.length() > 0) {

    // NOTE: With streaming TTS enabled, ACTION tags are executed after the full
    // LLM reply has completed streaming. Therefore, actions (volume, eye skins,
    // timer, etc.) fire after the first sentence (and any streaming sentences)
    // has already been spoken.
    String personaBeforeActions = personalityMode;
    parseAndExecuteActions(ans);
    parseAndSaveMemoriesFromLlm(ans);
    if (saveChatHistory && personalityMode == personaBeforeActions) {
      String cleanHistoryAns = stripAudioCues(stripRobotTags(ans));
      cleanHistoryAns.trim();
      addChatHistory(question, cleanHistoryAns.length() ? cleanHistoryAns : ans);
    }
  }

  return ans;
}
