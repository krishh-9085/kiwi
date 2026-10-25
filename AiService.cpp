#include "AiService.h"
#include "AudioEngine.h"
#include "Config.h"
#include "FaceEngine.h"
#include "MemoryService.h"
#include "WebPortal.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

struct HttpStream {
  WiFiClient *c = nullptr;
  bool chunked = false;
  long contentLeft = -1;
  long chunkLeft = 0;
  bool finished = false;
};

void pumpUI();

// ============================================================
// BASE64 & UTF-8 HELPERS
// ============================================================
static const char b64_table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static int8_t b64_lookup[256];

void initBase64Lookup() {
  for (int i = 0; i < 256; i++)
    b64_lookup[i] = -1;
  for (int i = 0; i < 64; i++)
    b64_lookup[(unsigned char)b64_table[i]] = i;
}

int utf8CharLen(unsigned char c) {
  if ((c & 0x80) == 0x00)
    return 1;
  if ((c & 0xE0) == 0xC0)
    return 2;
  if ((c & 0xF0) == 0xE0)
    return 3;
  if ((c & 0xF8) == 0xF0)
    return 4;
  return 1;
}

int utf8Count(const String &s) {
  int n = 0;
  for (size_t i = 0; i < s.length();) {
    i += utf8CharLen((unsigned char)s[i]);
    n++;
  }
  return n;
}

String utf8Truncate(const String &s, int maxChars) {
  int n = 0;
  size_t i = 0;
  size_t lastSpace = (size_t)-1;
  while (i < s.length() && n < maxChars) {
    unsigned char c = (unsigned char)s[i];
    if (c == ' ')
      lastSpace = i;
    i += utf8CharLen(c);
    n++;
  }
  if (i >= s.length())
    return s;
  size_t cut = (lastSpace != (size_t)-1 && lastSpace > 20) ? lastSpace : i;
  String r = s.substring(0, cut);
  r.trim();
  return r;
}

String jsonTextField(const String &json, const char *key) {
  String needle = "\"" + String(key) + "\"";
  int p = json.indexOf(needle);
  if (p < 0)
    return "";
  p = json.indexOf(':', p);
  if (p < 0)
    return "";
  p = json.indexOf('"', p);
  if (p < 0)
    return "";
  String out;
  int e = p + 1;
  while (e < (int)json.length()) {
    char c = json[e];
    if (c == '\\' && e + 1 < (int)json.length()) {
      char n = json[e + 1];
      if (n == 'u' && e + 5 < (int)json.length()) {
        long cp = strtol(json.substring(e + 2, e + 6).c_str(), NULL, 16);
        if (cp >= 0xD800 && cp <= 0xDBFF && e + 11 < (int)json.length() &&
            json[e + 6] == '\\' && json[e + 7] == 'u') {
          long lo = strtol(json.substring(e + 8, e + 12).c_str(), NULL, 16);
          if (lo >= 0xDC00 && lo <= 0xDFFF) {
            cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
            e += 6;
          }
        }
        if (cp < 0x80)
          out += (char)cp;
        else if (cp < 0x800) {
          out += (char)(0xC0 | (cp >> 6));
          out += (char)(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
          out += (char)(0xE0 | (cp >> 12));
          out += (char)(0x80 | ((cp >> 6) & 0x3F));
          out += (char)(0x80 | (cp & 0x3F));
        } else {
          out += (char)(0xF0 | (cp >> 18));
          out += (char)(0x80 | ((cp >> 12) & 0x3F));
          out += (char)(0x80 | ((cp >> 6) & 0x3F));
          out += (char)(0x80 | (cp & 0x3F));
        }
        e += 6;
        continue;
      }
      if (n == 'n' || n == 't')
        out += ' ';
      else
        out += n;
      e += 2;
      continue;
    }
    if (c == '"')
      return out;
    out += c;
    e++;
  }
  return "";
}

String jsonEscape(const String &s) {
  String o;
  o.reserve(s.length() + 16);
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"')
      o += "\\\"";
    else if (c == '\\')
      o += "\\\\";
    else if (c == '\n')
      o += "\\n";
    else if ((unsigned char)c < 0x20)
      o += ' ';
    else
      o += c;
  }
  return o;
}

String cleanForSpeech(const String &in) {
  String o;
  o.reserve(in.length() + 8);
  bool lastSpace = true;
  for (size_t i = 0; i < in.length(); i++) {
    unsigned char c = (unsigned char)in[i];
    // Skip 4-byte UTF-8 emojis (0xF0..0xF4)
    if ((c & 0xF8) == 0xF0 && i + 3 < in.length()) {
      i += 3; // Advances past emoji
      continue;
    }
    if (c >= 0x80) {
      o += (char)c;
      lastSpace = false;
      continue;
    }
    if (c == '*' || c == '#' || c == '`' || c == '_' || c == '~')
      continue;
    if (c < 32)
      c = ' ';
    if (c == ' ') {
      if (lastSpace)
        continue;
      lastSpace = true;
    } else
      lastSpace = false;
    o += (char)c;
  }
  o.trim();
  return utf8Truncate(o, TTS_MAX_CHARS);
}

bool parseUrl(const String &url, String &host, String &path) {
  int p = url.indexOf("://");
  if (p < 0)
    return false;
  String rest = url.substring(p + 3);
  int sl = rest.indexOf('/');
  String hostPort = (sl < 0) ? rest : rest.substring(0, sl);
  path = (sl < 0) ? "/" : rest.substring(sl);
  int colon = hostPort.indexOf(':');
  host = (colon < 0) ? hostPort : hostPort.substring(0, colon);
  return host.length() > 0;
}


static bool containsDevanagari(const String &s) {
  for (size_t i = 0; i + 1 < s.length(); i++) {
    if ((uint8_t)s[i] == 0xE0 &&
        ((uint8_t)s[i + 1] == 0xA4 || (uint8_t)s[i + 1] == 0xA5)) {
      return true;
    }
  }
  return false;
}

static String sarvamSttLangCode() {
  if (sttLang == "en")
    return "en-IN";
  if (sttLang == "hi")
    return "hi-IN";
  return ""; // Sarvam AI saaras:v3 auto-detects Hindi, English, and
                    // Hinglish! Empty string = let the API auto-detect.
}

static String whisperSttLangCode() {
  if (sttLang == "en")
    return "en";
  if (sttLang == "hi")
    return "hi";
  return "";
}

// ============================================================
// NETWORK TRANSPORT HELPERS
// ============================================================
bool httpReadLine(WiFiClient &c, String &line, uint32_t timeoutMs) {
  line = "";
  uint32_t start = millis();
  uint32_t idleT0 = millis();
  while ((uint32_t)(millis() - start) < timeoutMs) {
    if (c.available()) {
      char ch = c.read();
      if (ch == '\n') {
        line.trim();
        return true;
      }
      line += ch;
      idleT0 = millis();
    } else {
      if (!c.connected())
        return false;
      if ((uint32_t)(millis() - idleT0) > 5000)
        return false; // 5s idle gap limit
      delay(1);
    }
  }
  return false;
}

int httpReadBlock(HttpStream &s, uint8_t *buf, size_t maxLen) {
  if (s.finished)
    return -1;
  if (s.chunked && s.chunkLeft <= 0) {
    String line;
    do {
      if (!httpReadLine(*s.c, line, 10000)) {
        s.finished = true;
        return -1;
      }
    } while (line.length() == 0);
    s.chunkLeft = strtol(line.c_str(), NULL, 16);
    if (s.chunkLeft <= 0) {
      s.finished = true;
      return -1;
    }
  }
  if (!s.chunked && s.contentLeft == 0) {
    s.finished = true;
    return -1;
  }
  size_t want = maxLen;
  if (s.chunked && (long)want > s.chunkLeft)
    want = s.chunkLeft;
  if (!s.chunked && s.contentLeft > 0 && (long)want > s.contentLeft)
    want = s.contentLeft;
  if (!s.c->available()) {
    if (!s.c->connected()) {
      s.finished = true;
      return -1;
    }
    return 0;
  }
  int n = s.c->read(buf, want);
  if (n <= 0)
    return 0;
  if (s.chunked)
    s.chunkLeft -= n;
  else if (s.contentLeft > 0)
    s.contentLeft -= n;
  return n;
}

bool ensureDns(const String &host) {
  IPAddress ip;
  if (WiFi.hostByName(host.c_str(), ip) == 1) {
    Serial.printf("[DNS] %s -> %s\n", host.c_str(), ip.toString().c_str());
    return true;
  }
  Serial.println("[DNS] Failed, trying 8.8.8.8 / 1.1.1.1");
  WiFi.config(WiFi.localIP(), WiFi.gatewayIP(), WiFi.subnetMask(),
              IPAddress(8, 8, 8, 8), IPAddress(1, 1, 1, 1));
  delay(300);
  if (WiFi.hostByName(host.c_str(), ip) == 1) {
    Serial.printf("[DNS] %s -> %s (fallback)\n", host.c_str(),
                  ip.toString().c_str());
    return true;
  }
  Serial.println("[DNS] Still failing");
  return false;
}

bool tlsConnect(WiFiClientSecure &c, const String &host, const char *tag) {
  for (int attempt = 1; attempt <= 3; attempt++) {
    c.stop();
    Serial.printf("[%s] TLS connect %s try %d heap=%u maxblk=%u\n", tag,
                  host.c_str(), attempt, (unsigned)ESP.getFreeHeap(),
                  (unsigned)ESP.getMaxAllocHeap());
    uint32_t t0 = millis();
    bool ok = c.connect(host.c_str(), 443, 12000);
    Serial.printf("[%s] connect()=%d after %lu ms\n", tag, ok ? 1 : 0,
                  (unsigned long)(millis() - t0));
    if (ok)
      return true;
    char err[100];
    err[0] = 0;
    int code = c.lastError(err, sizeof(err));
    Serial.printf("[%s] TLS error %d: %s\n", tag, code, err);
    if (cancelRequest)
      return false;
    delay(400);
  }
  return false;
}

bool clientWriteAll(WiFiClient &c, const uint8_t *p, size_t n) {
  size_t off = 0;
  uint32_t t0 = millis();
  while (off < n) {
    int w = c.write(p + off, n - off);
    if (w <= 0) {
      if (millis() - t0 > 8000)
        return false;
      delay(2);
      continue;
    }
    off += w;
    t0 = millis();
  }
  return true;
}

// ============================================================
// CONVERSATION HISTORY MEMORY (MULTI-TURN CHAT CONTEXT)
// ============================================================
struct ChatTurn {
  String question;
  String answer;
};

static const int MAX_CHAT_HISTORY = 4; // Retain last 4 exchanges (8 messages)
static ChatTurn chatHistory[MAX_CHAT_HISTORY];
static int chatHistoryCount = 0;
static uint32_t lastChatTime = 0;
static const uint32_t CHAT_HISTORY_TIMEOUT_MS =
    240000; // 4 minutes inactivity timeout

void clearConversationHistory() {
  chatHistoryCount = 0;
  lastChatTime = 0;
  Serial.println("[CONVO] Conversation history cleared.");
}

int getConversationHistoryCount() { return chatHistoryCount; }

static void checkChatHistoryTimeout() {
  if (chatHistoryCount > 0 && lastChatTime > 0) {
    if (millis() - lastChatTime > CHAT_HISTORY_TIMEOUT_MS) {
      Serial.printf("[CONVO] History timed out (%lu ms) -> reset\n",
                    (unsigned long)(millis() - lastChatTime));
      clearConversationHistory();
    }
  }
}

static void addChatHistory(const String &q, const String &a) {
  if (q.length() == 0 || a.length() == 0)
    return;
  if (chatHistoryCount >= MAX_CHAT_HISTORY) {
    for (int i = 0; i < MAX_CHAT_HISTORY - 1; i++) {
      chatHistory[i] = chatHistory[i + 1];
    }
    chatHistoryCount = MAX_CHAT_HISTORY - 1;
  }
  chatHistory[chatHistoryCount].question = utf8Truncate(q, 160);
  chatHistory[chatHistoryCount].answer = utf8Truncate(a, 220);
  chatHistoryCount++;
  lastChatTime = millis();
  Serial.printf("[CONVO] Stored turn %d/%d in memory (Free heap: %u)\n",
                chatHistoryCount, MAX_CHAT_HISTORY,
                (unsigned)ESP.getFreeHeap());
}

// ============================================================
// LARGE LANGUAGE MODELS (LLM)
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
    Serial.println(resp);
    return "";
  }
  return cleanForSpeech(jsonTextField(resp, "text"));
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

  int reqTokens =
      (groqModel.indexOf("gpt-oss") >= 0 || groqModel.indexOf("qwen3") >= 0)
          ? 400
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
          String(reqTokens) + ",\"temperature\":0.8";
  if (groqModel.indexOf("gpt-oss") >= 0)
    body += ",\"reasoning_effort\":\"low\"";
  body += "}";

  String resp;
  int code = -1;
  {
    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(10);
    HTTPClient http;
    http.setReuse(false);
    http.setTimeout(12000);
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
    Serial.printf("[GROQ] HTTP %d in %.1fs, respLen=%u, heap=%u\n", code,
                  (millis() - t0) / 1000.0f, (unsigned)resp.length(),
                  (unsigned)ESP.getFreeHeap());
  }
  if (code != 200) {
    Serial.println(resp);
    return "";
  }
  return cleanForSpeech(jsonTextField(resp, "content"));
}

String askLLM(const String &question) {
  checkChatHistoryTimeout();
  String ans;
  if (llmProvider == "groq") {
    Serial.printf("[LLM] Provider = Groq | Memory turns = %d\n",
                  chatHistoryCount);
    ans = askGroq(question);
  } else {
    Serial.printf("[LLM] Provider = Gemini | Memory turns = %d\n",
                  chatHistoryCount);
    ans = askGemini(question);
  }
  if (ans.length() > 0) {
    parseAndSaveMemoriesFromLlm(ans);
    addChatHistory(question, ans);
  }
  return ans;
}

// ============================================================
// SPEECH-TO-TEXT (STT)
// ============================================================
SttCfg resolveStt() {
  SttCfg c;
  if (sttProvider == "groq") {
    c.url = groqSttEndpoint;
    c.key = groqSttKey.length() ? groqSttKey : groqKey;
    c.model = groqSttModel;
    c.sarvam = false;
  } else if (sttProvider == "openai") {
    c.url = openaiSttEndpoint;
    c.key = openaiSttKey;
    c.model = openaiSttModel;
    c.sarvam = false;
  } else {
    c.url = sttEndpoint;
    c.key = sttKey;
    c.model = sttModel;
    c.sarvam = true;
  }
  return c;
}

String recordAndTranscribe(uint32_t recordSeconds) {
  sttFail = "";
  SttCfg cfg = resolveStt();
  if (!cfg.key.length()) {
    Serial.println("[STT] No API key.");
    sttFail = "no API key";
    return "";
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[STT] No Wi-Fi.");
    sttFail = "no Wi-Fi";
    return "";
  }
  if (!rxHandle) {
    Serial.println("[STT] Mic not ready.");
    sttFail = "mic not ready";
    return "";
  }
  String host, path;
  if (!parseUrl(cfg.url, host, path)) {
    sttFail = "bad URL";
    return "";
  }
  Serial.printf("[STT] Provider=%s lang=%s model=%s\n", sttProvider.c_str(),
                sttLang.c_str(), cfg.model.c_str());
  Serial.printf("[STT] Free heap: %u  max block: %u\n",
                (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
  setEmotion(EMOTION_LISTENING);

  bool apWasUp = suspendLocalNet();
  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(20);
  client.setHandshakeTimeout(10);

  auto bail = [&](const char *why) -> String {
    Serial.printf("[STT] FAIL: %s\n", why);
    sttFail = why;
    client.stop();
    restoreWebAndAP(apWasUp);
    return String("");
  };

  if (ESP.getFreeHeap() < 35000)
    return bail("low memory");
  if (WiFi.status() != WL_CONNECTED) {
    uint32_t tw = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - tw < 4000)
      delay(100);
    if (WiFi.status() != WL_CONNECTED)
      return bail("Wi-Fi dropped");
  }
  if (!ensureDns(host))
    return bail("DNS failed");
  if (!tlsConnect(client, host, "STT"))
    return bail("connect failed");

  const String boundary = "----MyRobotBoundary9A7F";
  const uint32_t recordSamples = SAMPLE_RATE * recordSeconds;
  const uint32_t audioBytes = recordSamples * 2;

  String part1 = "--" + boundary +
                 "\r\n"
                 "Content-Disposition: form-data; name=\"file\"; "
                 "filename=\"audio.wav\"\r\n"
                 "Content-Type: audio/wav\r\n\r\n";

  String part2;
  if (cfg.sarvam) {
    String sarvamLang = sarvamSttLangCode();
    part2 =
        "\r\n--" + boundary +
        "\r\n"
        "Content-Disposition: form-data; name=\"model\"\r\n\r\n" +
        cfg.model +
        "\r\n";
    if (sarvamLang.length()) {
      part2 += "--" + boundary +
              "\r\n"
              "Content-Disposition: form-data; name=\"language_code\"\r\n\r\n" +
              sarvamLang + "\r\n";
    }
    part2 += "--" + boundary +
        "\r\n"
        "Content-Disposition: form-data; name=\"mode\"\r\n\r\ntranscribe\r\n";
  } else {
    String wl = whisperSttLangCode();
    part2 = "\r\n--" + boundary +
            "\r\n"
            "Content-Disposition: form-data; name=\"model\"\r\n\r\n" +
            cfg.model +
            "\r\n"
            "--" +
            boundary +
            "\r\n"
            "Content-Disposition: form-data; "
            "name=\"response_format\"\r\n\r\njson\r\n";
    if (wl.length()) {
      part2 += "--" + boundary +
               "\r\n"
               "Content-Disposition: form-data; name=\"language\"\r\n\r\n" +
               wl + "\r\n";
    }
    part2 += "--" + boundary +
             "\r\n"
             "Content-Disposition: form-data; name=\"prompt\"\r\n\r\n"
             "Kiwi, hello, namaste, kaisa hai, kaise ho, kya hal hai, suno, bolo, "
             "mausam, time, jokes, desk robot conversation.\r\n";
  }

  String ending = "--" + boundary + "--\r\n";
  uint32_t total =
      part1.length() + 44 + audioBytes + part2.length() + ending.length();

  client.printf("POST %s HTTP/1.1\r\n", path.c_str());
  client.printf("Host: %s\r\n", host.c_str());
  if (cfg.sarvam)
    client.printf("api-subscription-key: %s\r\n", cfg.key.c_str());
  else
    client.printf("Authorization: Bearer %s\r\n", cfg.key.c_str());
  client.printf("Content-Type: multipart/form-data; boundary=%s\r\n",
                boundary.c_str());
  client.printf("Content-Length: %lu\r\n", (unsigned long)total);
  client.print("Connection: close\r\n\r\n");
  client.print(part1);

  static uint8_t wav[44];
  makeWavHeader(wav, audioBytes);
  if (!clientWriteAll(client, wav, 44))
    return bail("upload failed");

  setEmotion(EMOTION_LISTENING);
  drainMic();
  detectMicSlot();
  Serial.printf("[STT] Recording %lu s (mic slot %d, gain %.1f)...\n",
                (unsigned long)recordSeconds, micSlot, (double)MIC_GAIN);

  static int32_t raw[AUDIO_FRAMES * 2];
  static uint8_t out[UPLOAD_BATCH * 2];
  int outPos = 0;
  uint32_t sent = 0, lastProgress = 0;
  float prevIn = 0, prevOut = 0;
  float sumAbs = 0;
  uint32_t vuMax = 0;
  const uint32_t BLK = SAMPLE_RATE / 10;
  uint32_t blkN = 0;
  float blkSum = 0, maxBlkAvg = 0;
  bool speechStarted = false;
  bool captureStopped = false;
  uint32_t silenceSamples = 0;
  const uint32_t MIN_SPEECH_SAMPLES = SAMPLE_RATE / 2;
  const uint32_t TRAILING_SILENCE_SAMPLES = SAMPLE_RATE * 45 / 100;
  const float SPEECH_ACTIVITY_GATE = NOISE_GATE_THRESHOLD * 1.5f;

  while (sent < recordSamples && !captureStopped) {
    if (cancelRequest)
      return bail("cancelled");
    size_t bytesRead = 0;
    esp_err_t e =
        i2s_channel_read(rxHandle, raw, sizeof(raw), &bytesRead, 1000);
    if (e != ESP_OK)
      return bail("mic read failed");
    int frames = bytesRead / (2 * sizeof(int32_t));
    for (int i = 0; i < frames && sent < recordSamples; i++) {
      int32_t s32 = raw[i * 2 + micSlot];
      float x = (float)(s32 >> 8) / 256.0f;
      float y = x - prevIn + 0.995f * prevOut;
      prevIn = x;
      prevOut = y;
      y *= MIC_GAIN;
      // Soft saturation compression knee prevents harsh digital square-wave clipping
      if (y > 28000.0f) {
        y = 28000.0f + (y - 28000.0f) * 0.25f;
      } else if (y < -28000.0f) {
        y = -28000.0f + (y + 28000.0f) * 0.25f;
      }
      if (y > 32767.0f)
        y = 32767.0f;
      if (y < -32768.0f)
        y = -32768.0f;
      int16_t pcm = (int16_t)y;
      uint32_t a = abs((int)pcm);
      sumAbs += a;
      if (a > vuMax)
        vuMax = a;
      blkSum += a;
      if (++blkN >= BLK) {
        float avg = blkSum / blkN;
        if (avg > maxBlkAvg)
          maxBlkAvg = avg;
        if (avg >= SPEECH_ACTIVITY_GATE) {
          speechStarted = true;
          silenceSamples = 0;
        } else if (speechStarted && sent >= MIN_SPEECH_SAMPLES) {
          silenceSamples += BLK;
          if (silenceSamples >= TRAILING_SILENCE_SAMPLES) {
            captureStopped = true;
            Serial.printf("[STT] Speech ended after %.1f s; stopping early\n",
                          (float)sent / SAMPLE_RATE);
          }
        }
        blkSum = 0;
        blkN = 0;
      }
      out[outPos++] = pcm & 0xff;
      out[outPos++] = (pcm >> 8) & 0xff;
      sent++;
      if (outPos >= (int)sizeof(out)) {
        if (!clientWriteAll(client, out, outPos))
          return bail("upload failed");
        outPos = 0;
      }
    }
    if (sent - lastProgress >= SAMPLE_RATE) {
      lastProgress = sent;
      Serial.printf("[STT] %.0f sec, avg %.0f peak %lu loudest-block %.0f\n",
                    (float)sent / SAMPLE_RATE, sumAbs / sent,
                    (unsigned long)vuMax, maxBlkAvg);
      vuMax = 0;
    }
    pumpUI();
  }

  uint32_t capturedSamples = sent;
  if (outPos > 0) {
    if (!clientWriteAll(client, out, outPos))
      return bail("upload failed");
  }
  // Keep the WAV at its declared length while avoiding another blocking mic read.
  if (captureStopped && sent < recordSamples) {
    static uint8_t silence[UPLOAD_BATCH * 2] = {};
    while (sent < recordSamples) {
      uint32_t remainingSamples = recordSamples - sent;
      size_t silenceBytes = min((uint32_t)sizeof(silence), remainingSamples * 2U);
      if (!clientWriteAll(client, silence, silenceBytes))
        return bail("upload failed");
      sent += silenceBytes / 2;
    }
  }

  Serial.printf("[STT] Done. captured %.2f s, avg %.0f, loudest 100ms block %.0f (gate %d)\n",
                (float)capturedSamples / SAMPLE_RATE,
                sumAbs / recordSamples, maxBlkAvg, NOISE_GATE_THRESHOLD);
  if (maxBlkAvg < NOISE_GATE_THRESHOLD)
    return bail("quiet");

  client.print(part2);
  client.print(ending);
  setEmotion(EMOTION_THINKING);

  uint32_t waitStart = millis();
  while (!client.available() && client.connected() &&
         millis() - waitStart < 25000) {
    if (cancelRequest)
      return bail("cancelled");
    pumpUI();
    delay(10);
  }

  String response;
  uint32_t readStart = millis();
  while ((client.connected() || client.available()) &&
         millis() - readStart < 10000 && response.length() < 6000) {
    if (cancelRequest)
      break;
    while (client.available() && response.length() < 6000)
      response += (char)client.read();
    delay(1);
  }
  client.stop();
  if (cancelRequest) {
    restoreWebAndAP(apWasUp);
    return "";
  }

  int nlIdx = response.indexOf("\r\n");
  String statusLine = (nlIdx > 0) ? response.substring(0, nlIdx) : response;
  int statusCode = 0;
  if (statusLine.startsWith("HTTP/1.")) {
    int sp = statusLine.indexOf(' ');
    if (sp > 0)
      statusCode = statusLine.substring(sp + 1, sp + 4).toInt();
  }
  Serial.printf("[STT] HTTP %d\n", statusCode);
  int body = response.indexOf("\r\n\r\n");
  if (body >= 0)
    response = response.substring(body + 4);
  if (statusCode != 200) {
    Serial.print("[STT] Error body: ");
    Serial.println(response.substring(0, 300));
    sttFail =
        (statusCode > 0) ? ("HTTP " + String(statusCode)) : String("no reply");
    restoreWebAndAP(apWasUp);
    return "";
  }
  String text = jsonTextField(response, "transcript");
  if (!text.length())
    text = jsonTextField(response, "text");
  text.trim();
  Serial.print("[STT] Text: ");
  Serial.println(text);
  if (!text.length())
    sttFail = "empty";
  restoreWebAndAP(apWasUp);
  return text;
}

// ============================================================
// TEXT-TO-SPEECH (TTS)
// ============================================================
bool speakText(const String &text) {
  if (!txHandle || !text.length())
    return false;
  if (ttsProvider == "openai")
    return ttsPcmOpenAI(text) == 1;
  if (ttsProvider == "elevenlabs")
    return ttsPcmEleven(text) == 1;
  return ttsSarvam(text) == 1;
}

int ttsSarvam(const String &text) {
  String key = ttsKey.length() ? ttsKey : sttKey;
  if (!key.length()) {
    Serial.println("[TTS] No Sarvam key.");
    return 0;
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[TTS] No Wi-Fi.");
    return 0;
  }
  String host, path;
  if (!parseUrl(ttsEndpoint, host, path)) {
    Serial.println("[TTS] Bad URL.");
    return 0;
  }
  Serial.printf("[TTS/Sarvam] Free heap before: %u\n",
                (unsigned)ESP.getFreeHeap());
  bool apWasUp = suspendLocalNet();
  if (ESP.getFreeHeap() < 35000) {
    restoreWebAndAP(apWasUp);
    return 0;
  }
  if (WiFi.status() != WL_CONNECTED) {
    uint32_t tw = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - tw < 4000)
      delay(100);
    if (WiFi.status() != WL_CONNECTED) {
      restoreWebAndAP(apWasUp);
      return 0;
    }
  }
  if (!ensureDns(host)) {
    restoreWebAndAP(apWasUp);
    return 0;
  }

  // Sarvam bulbul:v3 strictly requires hi-IN for Devanagari script and en-IN
  // for Latin script (English & Roman Hinglish).
  String langCode = containsDevanagari(text) ? "hi-IN" : "en-IN";
  Serial.printf("[TTS/Sarvam] Script=%s -> target_language_code=%s\n",
                containsDevanagari(text) ? "Devanagari (hi-IN)"
                                         : "Latin/Hinglish (en-IN)",
                langCode.c_str());

  String body = "{\"text\":\"" + jsonEscape(text) +
                "\","
                "\"target_language_code\":\"" +
                jsonEscape(langCode) +
                "\","
                "\"speaker\":\"" +
                jsonEscape(ttsVoice) +
                "\","
                "\"model\":\"" +
                jsonEscape(ttsModel) +
                "\","
                "\"speech_sample_rate\":24000,"
                "\"enable_preprocessing\":true}";

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(20);
  client.setHandshakeTimeout(10);
  if (!tlsConnect(client, host, "TTS")) {
    client.stop();
    restoreWebAndAP(apWasUp);
    return 0;
  }
  client.printf("POST %s HTTP/1.1\r\n", path.c_str());
  client.printf("Host: %s\r\n", host.c_str());
  client.printf("api-subscription-key: %s\r\n", key.c_str());
  client.print("Content-Type: application/json\r\n");
  client.printf("Content-Length: %u\r\n", (unsigned)body.length());
  client.print("Connection: close\r\n\r\n");
  client.print(body);

  String line;
  if (!httpReadLine(client, line, 20000)) {
    client.stop();
    restoreWebAndAP(apWasUp);
    return 0;
  }
  int sp = line.indexOf(' ');
  int status = sp > 0 ? line.substring(sp + 1).toInt() : 0;
  HttpStream hs;
  hs.c = &client;
  while (httpReadLine(client, line, 10000) && line.length() > 0) {
    String l = line;
    l.toLowerCase();
    if (l.startsWith("transfer-encoding:") && l.indexOf("chunked") > 0)
      hs.chunked = true;
    if (l.startsWith("content-length:"))
      hs.contentLeft = l.substring(15).toInt();
  }
  Serial.printf("[TTS] HTTP %d%s\n", status, hs.chunked ? " (chunked)" : "");
  if (status != 200) {
    String errBody;
    while (client.available() && errBody.length() < 300)
      errBody += (char)client.read();
    Serial.printf("[TTS] Error body: %s\n", errBody.c_str());
    client.stop();
    restoreWebAndAP(apWasUp);
    return 0;
  }

  enum ScanState { S_KEY, S_BRACKET, S_OPEN_Q, S_DECODE, S_DONE };
  ScanState st = S_KEY;
  static const char KEY[] = "\"audios\"";
  int keyPos = 0;
  int b64val = 0, b64bits = 0;
  static uint8_t hdr[64];
  size_t hdrLen = 0;
  bool fmtSeen = false, dataSeen = false;
  uint32_t srcRate = 0;
  uint16_t channels = 1, bits = 16;
  bool playing = false, aborted = false;
  float step = 1.0f, fpos = 0, prev = 0;
  uint16_t chIdx = 0;
  int16_t firstCh = 0;
  bool haveLo = false;
  uint8_t lo = 0;
  const size_t CHUNK = 512;
  static int16_t pcmChunk[CHUNK];
  size_t chunkFill = 0;

  auto flushChunk = [&]() {
    if (chunkFill > 0) {
      for (size_t i = 0; i < chunkFill; i++)
        spkPushSample(pcmChunk[i]);
      chunkFill = 0;
    }
  };
  auto emitSample = [&](int16_t s) {
    if (chIdx == 0)
      firstCh = s;
    if (++chIdx >= channels) {
      chIdx = 0;
      float cur = (float)firstCh;
      while (fpos < 1.0f) {
        int16_t out = (int16_t)(prev + (cur - prev) * fpos);
        pcmChunk[chunkFill++] = out;
        if (chunkFill >= CHUNK)
          flushChunk();
        fpos += step;
      }
      fpos -= 1.0f;
      prev = cur;
    }
  };
  auto onWavByte = [&](uint8_t b) {
    if (aborted)
      return;
    if (!dataSeen) {
      if (hdrLen < sizeof(hdr))
        hdr[hdrLen++] = b;
      if (!fmtSeen && hdrLen >= 36) {
        if (hdrLen >= 12 &&
            (memcmp(hdr, "RIFF", 4) != 0 || memcmp(hdr + 8, "WAVE", 4) != 0)) {
          aborted = true;
          return;
        }
        if (hdrLen >= 36 && memcmp(hdr + 12, "fmt ", 4) == 0) {
          channels = hdr[22] | (hdr[23] << 8);
          srcRate = hdr[24] | (hdr[25] << 8) | (hdr[26] << 16) |
                    ((uint32_t)hdr[27] << 24);
          bits = hdr[34] | (hdr[35] << 8);
          fmtSeen = true;
          if (bits != 16 || srcRate < 8000 || srcRate > 96000) {
            aborted = true;
            return;
          }
        }
      }
      if (fmtSeen && !dataSeen && hdrLen >= 44) {
        if (memcmp(hdr + 36, "data", 4) == 0) {
          dataSeen = true;
          step = (float)srcRate / (float)SAMPLE_RATE;
          setEmotion(EMOTION_SPEAKING);
          gFaceFrameMs = 300;
          playing = true;
          for (size_t i = 44; i < hdrLen; i++) {
            uint8_t b2 = hdr[i];
            if (!haveLo) {
              lo = b2;
              haveLo = true;
            } else {
              int16_t s = (int16_t)(lo | (b2 << 8));
              haveLo = false;
              emitSample(s);
            }
          }
        }
      }
    } else {
      if (!haveLo) {
        lo = b;
        haveLo = true;
      } else {
        int16_t s = (int16_t)(lo | (b << 8));
        haveLo = false;
        emitSample(s);
      }
    }
  };

  static uint8_t rbuf[4096];
  int n;
  uint32_t readStart = millis();
  while (st != S_DONE) {
    if (cancelRequest) {
      aborted = true;
      break;
    }
    n = httpReadBlock(hs, rbuf, sizeof(rbuf));
    if (n < 0)
      break;
    if (n == 0) {
      if (millis() - readStart > 30000)
        break;
      pumpUI();
      delay(1);
      continue;
    }
    readStart = millis();
    for (int i = 0; i < n && st != S_DONE; i++) {
      char c = (char)rbuf[i];
      if (st == S_KEY) {
        if (c == KEY[keyPos]) {
          keyPos++;
          if (KEY[keyPos] == 0) {
            st = S_BRACKET;
            keyPos = 0;
          }
        } else
          keyPos = (c == KEY[0]) ? 1 : 0;
      } else if (st == S_BRACKET) {
        if (c == '[')
          st = S_OPEN_Q;
        else if (c != ' ' && c != '\t' && c != '\n' && c != '\r' && c != ':') {
          st = S_KEY;
          keyPos = 0;
        }
      } else if (st == S_OPEN_Q) {
        if (c == '"')
          st = S_DECODE;
        else if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
          st = S_KEY;
          keyPos = 0;
        }
      } else if (st == S_DECODE) {
        if (c == '"') {
          st = S_DONE;
          break;
        }
        int8_t d = b64_lookup[(unsigned char)c];
        if (d < 0)
          continue;
        b64val = (b64val << 6) | d;
        b64bits += 6;
        if (b64bits >= 8) {
          b64bits -= 8;
          uint8_t byte = (b64val >> b64bits) & 0xFF;
          onWavByte(byte);
          if (aborted) {
            st = S_DONE;
            break;
          }
        }
      }
    }
    if (st != S_DONE)
      pumpUI();
  }
  client.stop();
  flushChunk();
  spkFlush();
  for (int i = 0; i < SAMPLE_RATE / 10; i++)
    spkPushSample(0);
  spkFlush();
  gFaceFrameMs = 33;
  restoreWebAndAP(apWasUp);
  return playing ? 1 : 0;
}

int ttsPcmOpenAI(const String &text) {
  String key = openaiTtsKey.length() ? openaiTtsKey : openaiSttKey;
  if (!key.length()) {
    Serial.println("[TTS/OpenAI] No API key.");
    return 0;
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[TTS] No Wi-Fi.");
    return 0;
  }
  String host, path;
  if (!parseUrl(openaiTtsEndpoint, host, path))
    return 0;

  bool apWasUp = suspendLocalNet();
  if (ESP.getFreeHeap() < 35000) {
    restoreWebAndAP(apWasUp);
    return 0;
  }
  if (WiFi.status() != WL_CONNECTED) {
    uint32_t tw = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - tw < 4000)
      delay(100);
    if (WiFi.status() != WL_CONNECTED) {
      restoreWebAndAP(apWasUp);
      return 0;
    }
  }
  if (!ensureDns(host)) {
    restoreWebAndAP(apWasUp);
    return 0;
  }

  String body = "{\"model\":\"" + jsonEscape(openaiTtsModel) +
                "\","
                "\"input\":\"" +
                jsonEscape(text) +
                "\","
                "\"voice\":\"" +
                jsonEscape(openaiTtsVoice) +
                "\","
                "\"response_format\":\"pcm\"}";

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(20);
  client.setHandshakeTimeout(10);
  if (!tlsConnect(client, host, "TTS")) {
    client.stop();
    restoreWebAndAP(apWasUp);
    return 0;
  }
  client.printf("POST %s HTTP/1.1\r\n", path.c_str());
  client.printf("Host: %s\r\n", host.c_str());
  client.printf("Authorization: Bearer %s\r\n", key.c_str());
  client.print("Content-Type: application/json\r\n");
  client.printf("Content-Length: %u\r\n", (unsigned)body.length());
  client.print("Connection: close\r\n\r\n");
  client.print(body);

  String line;
  if (!httpReadLine(client, line, 20000)) {
    client.stop();
    restoreWebAndAP(apWasUp);
    return 0;
  }
  int sp = line.indexOf(' ');
  int status = sp > 0 ? line.substring(sp + 1).toInt() : 0;
  HttpStream hs;
  hs.c = &client;
  while (httpReadLine(client, line, 10000) && line.length() > 0) {
    String l = line;
    l.toLowerCase();
    if (l.startsWith("transfer-encoding:") && l.indexOf("chunked") > 0)
      hs.chunked = true;
    if (l.startsWith("content-length:"))
      hs.contentLeft = l.substring(15).toInt();
  }
  if (status != 200) {
    client.stop();
    restoreWebAndAP(apWasUp);
    return 0;
  }
  setEmotion(EMOTION_SPEAKING);
  gFaceFrameMs = 300;
  bool playing = false, haveLo = false;
  uint8_t lo = 0;
  static uint8_t rbuf[2048];
  int n;
  uint32_t readStart = millis();
  while (true) {
    if (cancelRequest)
      break;
    n = httpReadBlock(hs, rbuf, sizeof(rbuf));
    if (n < 0)
      break;
    if (n == 0) {
      if (millis() - readStart > 15000)
        break;
      pumpUI();
      delay(1);
      continue;
    }
    readStart = millis();
    playing = true;
    for (int i = 0; i < n; i++) {
      if (!haveLo) {
        lo = rbuf[i];
        haveLo = true;
      } else {
        int16_t s = (int16_t)(lo | (rbuf[i] << 8));
        haveLo = false;
        spkPushSample(s);
      }
    }
    pumpUI();
  }
  client.stop();
  spkFlush();
  for (int i = 0; i < SAMPLE_RATE / 10; i++)
    spkPushSample(0);
  spkFlush();
  gFaceFrameMs = 33;
  restoreWebAndAP(apWasUp);
  return playing ? 1 : 0;
}

int ttsPcmEleven(const String &text) {
  if (!elevenKey.length()) {
    Serial.println("[TTS/Eleven] No API key.");
    return 0;
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[TTS] No Wi-Fi.");
    return 0;
  }
  String host, basePath;
  if (!parseUrl(elevenEndpoint, host, basePath))
    return 0;
  if (!basePath.endsWith("/"))
    basePath += "/";
  String path = basePath + elevenVoice + "?output_format=pcm_24000";

  bool apWasUp = suspendLocalNet();
  if (ESP.getFreeHeap() < 35000) {
    restoreWebAndAP(apWasUp);
    return 0;
  }
  if (WiFi.status() != WL_CONNECTED) {
    uint32_t tw = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - tw < 4000)
      delay(100);
    if (WiFi.status() != WL_CONNECTED) {
      restoreWebAndAP(apWasUp);
      return 0;
    }
  }
  if (!ensureDns(host)) {
    restoreWebAndAP(apWasUp);
    return 0;
  }

  String body = "{\"text\":\"" + jsonEscape(text) +
                "\","
                "\"model_id\":\"" +
                jsonEscape(elevenModel) + "\"}";

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(20);
  client.setHandshakeTimeout(10);
  if (!tlsConnect(client, host, "TTS")) {
    client.stop();
    restoreWebAndAP(apWasUp);
    return 0;
  }
  client.printf("POST %s HTTP/1.1\r\n", path.c_str());
  client.printf("Host: %s\r\n", host.c_str());
  client.printf("xi-api-key: %s\r\n", elevenKey.c_str());
  client.print("Content-Type: application/json\r\n");
  client.print("Accept: audio/pcm\r\n");
  client.printf("Content-Length: %u\r\n", (unsigned)body.length());
  client.print("Connection: close\r\n\r\n");
  client.print(body);

  String line;
  if (!httpReadLine(client, line, 20000)) {
    client.stop();
    restoreWebAndAP(apWasUp);
    return 0;
  }
  int sp = line.indexOf(' ');
  int status = sp > 0 ? line.substring(sp + 1).toInt() : 0;
  HttpStream hs;
  hs.c = &client;
  while (httpReadLine(client, line, 10000) && line.length() > 0) {
    String l = line;
    l.toLowerCase();
    if (l.startsWith("transfer-encoding:") && l.indexOf("chunked") > 0)
      hs.chunked = true;
    if (l.startsWith("content-length:"))
      hs.contentLeft = l.substring(15).toInt();
  }
  if (status != 200) {
    client.stop();
    restoreWebAndAP(apWasUp);
    return 0;
  }
  setEmotion(EMOTION_SPEAKING);
  gFaceFrameMs = 300;
  bool playing = false, haveLo = false;
  uint8_t lo = 0;
  static uint8_t rbuf[2048];
  int n;
  uint32_t readStart = millis();
  while (true) {
    if (cancelRequest)
      break;
    n = httpReadBlock(hs, rbuf, sizeof(rbuf));
    if (n < 0)
      break;
    if (n == 0) {
      if (millis() - readStart > 15000)
        break;
      pumpUI();
      delay(1);
      continue;
    }
    readStart = millis();
    playing = true;
    for (int i = 0; i < n; i++) {
      if (!haveLo) {
        lo = rbuf[i];
        haveLo = true;
      } else {
        int16_t s = (int16_t)(lo | (rbuf[i] << 8));
        haveLo = false;
        spkPushSample(s);
      }
    }
    pumpUI();
  }
  client.stop();
  spkFlush();
  for (int i = 0; i < SAMPLE_RATE / 10; i++)
    spkPushSample(0);
  spkFlush();
  gFaceFrameMs = 33;
  restoreWebAndAP(apWasUp);
  return playing ? 1 : 0;
}
