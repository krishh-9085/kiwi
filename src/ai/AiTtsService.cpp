#include "ai/AiTtsService.h"
#include "ai/AiTransport.h"
#include "ai/AiTextTools.h"
#include "config/AppConfig.h"
#include "core/Timing.h"
#include "hardware/AudioEngine.h"
#include "face/FaceEngine.h"
#include "net/WifiManager.h"
#include "web/WebPortal.h"
#include "behavior/InteractionManager.h"
#include <WiFiClientSecure.h>

// Unified shared audio buffers with PSRAM dynamic scaling (zero idle internal SRAM waste)
static int16_t *ttsPrebuf = nullptr;
static uint8_t *ttsRbuf = nullptr;
static size_t ttsPrebufMax = 0;
static size_t ttsRbufMax = 0;

void initTtsBuffers() {
  if (ttsPrebuf && ttsRbuf)
    return;
  if (psramFound()) {
    if (!ttsPrebuf) {
      // 9600 samples = 600ms jitter cushion @ 16kHz in PSRAM
      int16_t *p = (int16_t *)heap_caps_malloc(9600 * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
      if (p) {
        ttsPrebuf = p;
        ttsPrebufMax = 9600;
        Serial.println("[TTS] Allocated 600ms jitter buffer in PSRAM");
      }
    }
    if (!ttsRbuf) {
      // 8KB read chunk in PSRAM consumes full TLS records
      uint8_t *r = (uint8_t *)heap_caps_malloc(8192, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
      if (r) {
        ttsRbuf = r;
        ttsRbufMax = 8192;
        Serial.println("[TTS] Allocated 8KB TLS read buffer in PSRAM");
      }
    }
  }
  if (!ttsPrebuf) {
    ttsPrebuf = (int16_t *)malloc(2560 * sizeof(int16_t));
    if (ttsPrebuf)
      ttsPrebufMax = 2560; // 160ms @ 16kHz SRAM fallback
  }
  if (!ttsRbuf) {
    ttsRbuf = (uint8_t *)malloc(2048);
    if (ttsRbuf)
      ttsRbufMax = 2048;
  }
}

// ============================================================
bool speakText(const String &text) {
  initTtsBuffers();
  String toSpeak = (ttsProvider == "fish" || ttsProvider == "fishaudio") ? normalizeFishAudioTags(stripRobotTags(text)) : stripAudioCues(stripRobotTags(text));
  toSpeak.trim();
  Serial.printf("[TTS] speakText() prov='%s' len=%d heap=%u txHandle=%s\n",
                ttsProvider.c_str(), (int)toSpeak.length(), (unsigned)ESP.getFreeHeap(),
                txHandle ? "OK" : "NULL");

  if (!txHandle) {
    Serial.println("[TTS] Error: I2S speaker driver (txHandle) is NULL!");
    addTerminalLog("[TTS] Error: Speaker driver not initialized!");
    return false;
  }
  if (!toSpeak.length()) {
    Serial.println("[TTS] Warning: Text is empty after stripping cues.");
    return false;
  }

  int res = 0;
  if (ttsProvider == "fish" || ttsProvider == "fishaudio")
    res = ttsFishAudio(toSpeak);
  else
    res = ttsSarvam(toSpeak);

  spkEndSpeech();
  if (currentEmotion == EMOTION_SPEAKING) {
    setEmotion(EMOTION_IDLE);
  }
  Serial.printf("[TTS] speakText() returned %d\n", res);
  return res == 1;
}

// ============================================================
// SHARED TTS TRANSPORT & PREBUFFER HELPERS
// ============================================================
struct TtsNetGuard {
  bool apWasUp = false;
  bool active = false;
  void enter() {
    if (!active) {
      apWasUp = suspendLocalNet();
      active = true;
    }
  }
  void leave() {
    if (active) {
      restoreWebAndAP(apWasUp);
      active = false;
    }
  }
  ~TtsNetGuard() { leave(); }
};

struct TtsPrebuf {
  size_t count = 0;
  bool done = false;
  void push(int16_t s) {
    if (!done) {
      ttsPrebuf[count++] = s;
      if (count >= ttsPrebufMax) {
        for (size_t k = 0; k < count; k++)
          spkPushSample(ttsPrebuf[k]);
        count = 0;
        done = true;
      }
    } else {
      spkPushSample(s);
    }
  }
  void flushRemaining() {
    if (!done && count > 0) {
      for (size_t k = 0; k < count; k++)
        spkPushSample(ttsPrebuf[k]);
      count = 0;
      done = true;
    }
  }
};

static bool prepareTtsConnection(const char *tag, const String &url,
                                 const String &key, String &host, String &path,
                                 TtsNetGuard &net, WiFiClientSecure &client,
                                 uint32_t timeoutMs, int handshakeSec,
                                 bool logDetail) {
  if (!key.length()) {
    Serial.printf("[%s] No API key.\n", tag);
    if (logDetail)
      addTerminalLog("[TTS] Error: No Sarvam key in Web Portal Settings!");
    return false;
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[TTS] No Wi-Fi.");
    if (logDetail)
      addTerminalLog("[TTS] Error: Wi-Fi disconnected!");
    return false;
  }
  if (!parseUrl(url, host, path)) {
    if (logDetail) {
      Serial.printf("[TTS] Bad URL: %s\n", url.c_str());
      addTerminalLog("[TTS] Error: Bad URL " + url);
    }
    return false;
  }
  if (logDetail) {
    Serial.printf("[%s] Free heap before: %u\n", tag, (unsigned)ESP.getFreeHeap());
  }
  initTtsBuffers();
  if (!ttsPrebuf || !ttsRbuf) {
    Serial.printf("[%s] Failed to allocate TTS buffers.\n", tag);
    return false;
  }
  net.enter();
  if (!psramFound()) {
    if (ESP.getFreeHeap() < 38000 || ESP.getMaxAllocHeap() < 18000) {
      Serial.printf("[%s] Low heap: free=%u (<38000) or maxblk=%u (<18000)\n",
                    tag, (unsigned)ESP.getFreeHeap(),
                    (unsigned)ESP.getMaxAllocHeap());
      addTerminalLog("[" + String(tag) + "] Low heap (free: " +
                     String(ESP.getFreeHeap()) + ", maxblk: " +
                     String(ESP.getMaxAllocHeap()) + ")");
      return false;
    }
  } else if (ESP.getFreePsram() < 64000 && ESP.getFreeHeap() < 24000) {
    Serial.printf("[%s] Low memory: psram=%u, heap=%u\n", tag,
                  (unsigned)ESP.getFreePsram(), (unsigned)ESP.getFreeHeap());
    return false;
  }
  if (WiFi.status() != WL_CONNECTED) {
    uint32_t tw = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - tw < 4000) {
      serviceUi();
      yield();
    }
    if (WiFi.status() != WL_CONNECTED) {
      if (logDetail) {
        Serial.printf("[%s] Wi-Fi lost while waiting.\n", tag);
        addTerminalLog("[TTS] Error: Wi-Fi lost");
      }
      return false;
    }
  }
  if (!ensureDns(host)) {
    if (logDetail) {
      Serial.printf("[%s] DNS resolution failed for %s\n", tag, host.c_str());
      addTerminalLog("[TTS] Error: DNS failed for " + host);
    }
    return false;
  }

  client.setInsecure();
  client.setTimeout(timeoutMs);
  client.setHandshakeTimeout(handshakeSec);
  if (!tlsConnect(client, host, tag)) {
    client.stop();
    return false;
  }
  return true;
}

static bool readTtsHttpResponse(const char *tag, WiFiClientSecure &client,
                                HttpStream &hs, bool logChunked,
                                bool logTerminalErr) {
  String line;
  if (!httpReadLine(client, line, 20000)) {
    client.stop();
    return false;
  }
  int sp = line.indexOf(' ');
  int status = sp > 0 ? line.substring(sp + 1).toInt() : 0;
  hs.c = &client;
  bool gotBlank = false;
  while (httpReadLine(client, line, 10000)) {
    if (line.length() == 0) {
      gotBlank = true;
      break;
    }
    String l = line;
    l.toLowerCase();
    if (l.startsWith("transfer-encoding:") && l.indexOf("chunked") > 0)
      hs.chunked = true;
    if (l.startsWith("content-length:"))
      hs.contentLeft = l.substring(15).toInt();
  }
  if (!gotBlank) {
    client.stop();
    return false;
  }
  if (logChunked) {
    Serial.printf("[TTS] HTTP %d%s\n", status, hs.chunked ? " (chunked)" : "");
  }
  if (status != 200) {
    uint32_t tErr = millis();
    while (!client.available() && client.connected() && millis() - tErr < 1200) {
      serviceUi();
      yield();
    }
    String errBody;
    while (client.available() && errBody.length() < 400)
      errBody += (char)client.read();
    Serial.printf("[%s] HTTP %d: %s\n", tag, status, errBody.c_str());
    if (logTerminalErr) {
      addTerminalLog("[" + String(tag) + "] HTTP " + String(status) +
                     (errBody.length() ? (": " + errBody) : String("")));
    }
    client.stop();
    return false;
  }
  return true;
}

int ttsSarvam(const String &text) {
  String key = ttsKey.length() ? ttsKey : (sttKey.length() ? sttKey : llmKey);
  String host, path;
  TtsNetGuard net;
  WiFiClientSecure client;
  if (!prepareTtsConnection("TTS/Sarvam", ttsEndpoint, key, host, path, net,
                            client, 8000, 8, true)) {
    return 0;
  }

  // Sarvam bulbul:v3 handles Hindi and Hinglish natively with hi-IN, and English with en-IN
  String langCode = "hi-IN";
  if (replyLang == "en") {
    langCode = "en-IN";
  } else if (replyLang == "hi") {
    langCode = "hi-IN";
  } else if (!containsDevanagari(text) && sttLang == "en") {
    langCode = "en-IN";
  }

  bool isV4 = (ttsModel.indexOf("v4") >= 0);
  String voiceParam = ttsVoice;
  if (isV4) {
    if (voiceParam.indexOf('_') < 0) {
      if (voiceParam.equalsIgnoreCase("amit")) voiceParam = "amit_hi_conversational";
      else if (voiceParam.equalsIgnoreCase("priya")) voiceParam = "ishita_hi_conversational";
      else if (voiceParam.equalsIgnoreCase("aditya")) voiceParam = "aditya_hi_conversational";
      else if (voiceParam.equalsIgnoreCase("ritu")) voiceParam = "ritu_hi_conversational";
      else if (voiceParam.equalsIgnoreCase("shubh")) voiceParam = "shubh_enhi_ads";
      else if (voiceParam.length() == 0) voiceParam = "amit_hi_conversational";
      else voiceParam = voiceParam + "_hi_conversational";
    }
  }

  Serial.printf("[TTS/Sarvam] Model=%s Voice=%s Lang=%s (replyLang=%s)\n",
                ttsModel.c_str(), voiceParam.c_str(), langCode.c_str(), replyLang.c_str());

  String body = "{\"text\":\"" + jsonEscape(text) + "\",";
  if (isV4) {
    body += "\"language_code\":\"" + jsonEscape(langCode) + "\",";
  } else {
    body += "\"target_language_code\":\"" + jsonEscape(langCode) + "\",";
  }
  body += "\"speaker\":\"" + jsonEscape(voiceParam) + "\",";
  body += "\"model\":\"" + jsonEscape(ttsModel) + "\",";
  body += "\"speech_sample_rate\":16000,\"enable_preprocessing\":false}";

  client.printf("POST %s HTTP/1.1\r\n", path.c_str());
  client.printf("Host: %s\r\n", host.c_str());
  client.printf("api-subscription-key: %s\r\n", key.c_str());
  client.print("Content-Type: application/json\r\n");
  client.printf("Content-Length: %u\r\n", (unsigned)body.length());
  client.print("Connection: close\r\n\r\n");
  client.print(body);

  HttpStream hs;
  if (!readTtsHttpResponse("TTS/Sarvam", client, hs, true, false)) {
    return 0;
  }

  enum ScanState { S_KEY, S_BRACKET, S_OPEN_Q, S_DECODE, S_DONE };
  ScanState st = S_KEY;
  static const char KEY[] = "\"audios\"";
  int keyPos = 0;
  int b64val = 0, b64bits = 0;
  enum WavState { WP_RIFF, WP_CH_HDR, WP_FMT, WP_SKIP, WP_DATA };
  WavState wp = WP_RIFF;
  uint8_t wpBuf[16];
  size_t wpPos = 0;
  uint32_t wpChunkLeft = 0;
  uint32_t wpFmtRead = 0;
  bool fmtSeen = false, dataSeen = false;
  uint32_t srcRate = 0;
  uint16_t channels = 1, bits = 16;
  bool playing = false, aborted = false;
  float step = 1.0f, fpos = 0, prev = 0;
  uint16_t chIdx = 0;
  int16_t firstCh = 0;
  bool haveLo = false;
  uint8_t lo = 0;

  // Jitter Pre-Buffer (dynamic 160ms - 600ms): Prevents Wi-Fi packet latency from causing audio gaps
  TtsPrebuf prebuf;

  const size_t CHUNK = 512;
  static int16_t pcmChunk[CHUNK];
  size_t chunkFill = 0;

  auto flushChunk = [&]() {
    if (chunkFill > 0) {
      for (size_t i = 0; i < chunkFill; i++)
        prebuf.push(pcmChunk[i]);
      chunkFill = 0;
    }
  };

  auto emitSample = [&](int16_t s) {
    if (chIdx == 0)
      firstCh = s;
    if (++chIdx >= channels) {
      chIdx = 0;
      // Fast path: Native 16kHz rate requires ZERO float resampling math
      if (srcRate == SAMPLE_RATE) {
        pcmChunk[chunkFill++] = firstCh;
        if (chunkFill >= CHUNK)
          flushChunk();
      } else {
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
    }
  };
  auto onWavByte = [&](uint8_t b) {
    if (aborted)
      return;
    if (wp == WP_DATA) {
      if (!haveLo) {
        lo = b;
        haveLo = true;
      } else {
        int16_t s = (int16_t)(lo | (b << 8));
        haveLo = false;
        emitSample(s);
      }
      return;
    }

    if (wp == WP_RIFF) {
      wpBuf[wpPos++] = b;
      if (wpPos == 12) {
        if (memcmp(wpBuf, "RIFF", 4) != 0 || memcmp(wpBuf + 8, "WAVE", 4) != 0) {
          aborted = true;
          return;
        }
        wp = WP_CH_HDR;
        wpPos = 0;
      }
    } else if (wp == WP_CH_HDR) {
      wpBuf[wpPos++] = b;
      if (wpPos == 8) {
        uint32_t chunkSize = (uint32_t)wpBuf[4] | ((uint32_t)wpBuf[5] << 8) |
                             ((uint32_t)wpBuf[6] << 16) |
                             ((uint32_t)wpBuf[7] << 24);
        wpPos = 0;
        if (memcmp(wpBuf, "fmt ", 4) == 0 && chunkSize >= 16) {
          wp = WP_FMT;
          wpChunkLeft = chunkSize;
          wpFmtRead = 0;
        } else if (memcmp(wpBuf, "data", 4) == 0 && fmtSeen) {
          wp = WP_DATA;
          dataSeen = true;
          step = (float)srcRate / (float)SAMPLE_RATE;
          if (currentEmotion == EMOTION_IDLE ||
              currentEmotion == EMOTION_THINKING) {
            setEmotion(EMOTION_SPEAKING);
          }
          gFaceFrameMs = FACE_FRAME_MS_ACTIVE;
          playing = true;
        } else {
          if (chunkSize == 0) {
            wp = WP_CH_HDR;
          } else {
            wp = WP_SKIP;
            wpChunkLeft = chunkSize + (chunkSize & 1U);
          }
        }
      }
    } else if (wp == WP_FMT) {
      if (wpFmtRead < 16) {
        wpBuf[wpFmtRead] = b;
      }
      wpFmtRead++;
      wpChunkLeft--;
      if (wpFmtRead == 16) {
        channels = wpBuf[2] | (wpBuf[3] << 8);
        srcRate = (uint32_t)wpBuf[4] | ((uint32_t)wpBuf[5] << 8) |
                  ((uint32_t)wpBuf[6] << 16) | ((uint32_t)wpBuf[7] << 24);
        bits = wpBuf[14] | (wpBuf[15] << 8);
        fmtSeen = true;
        if (bits != 16 || channels == 0 || srcRate < 8000 || srcRate > 96000) {
          aborted = true;
          return;
        }
      }
      if (wpChunkLeft == 0) {
        if ((wpFmtRead & 1U) != 0) {
          wp = WP_SKIP;
          wpChunkLeft = 1;
        } else {
          wp = WP_CH_HDR;
          wpPos = 0;
        }
      }
    } else if (wp == WP_SKIP) {
      if (wpChunkLeft > 0)
        wpChunkLeft--;
      if (wpChunkLeft == 0) {
        wp = WP_CH_HDR;
        wpPos = 0;
      }
    }
  };

  uint8_t *rbuf = ttsRbuf;
  int n;
  uint32_t readStart = millis();
  uint32_t lastUiPump = 0;
  while (st != S_DONE) {
    if (cancelRequest) {
      aborted = true;
      break;
    }
    n = httpReadBlock(hs, rbuf, ttsRbufMax);
    if (n < 0)
      break;
    if (n == 0) {
      if (millis() - readStart > 12000)
        break;
      serviceUi();
      yield();
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
    if (st != S_DONE && millis() - lastUiPump >= 25) {
      pumpUI();
      lastUiPump = millis();
    }
  }
  client.stop();
  bool audioPlayed = false;
  if (!cancelRequest && playing) {
    flushChunk();
    prebuf.flushRemaining();
    spkFlush();
    audioPlayed = true;
    for (int i = 0; i < SAMPLE_RATE / 10; i++)
      spkPushSample(0);
  }
  spkEndSpeech();
  gFaceFrameMs = 33;
  net.leave();
  return audioPlayed ? 1 : 0;
}

int ttsFishAudio(const String &text, uint32_t maxPlayMs) {
  String host, path;
  TtsNetGuard net;
  WiFiClientSecure client;
  if (!prepareTtsConnection("TTS/FishAudio", fishEndpoint, fishKey, host, path,
                            net, client, 15000, 12, false)) {
    return 0;
  }

  String body = "{\"text\":\"" + jsonEscape(text) +
                "\",\"format\":\"pcm\",\"sample_rate\":16000";
  if (fishVoice.length() > 0 && fishVoice != "default") {
    body += ",\"reference_id\":\"" + jsonEscape(fishVoice) + "\"";
  }
  body += "}";

  client.printf("POST %s HTTP/1.1\r\n", path.c_str());
  client.printf("Host: %s\r\n", host.c_str());
  client.printf("Authorization: Bearer %s\r\n", fishKey.c_str());
  if (fishModel.length() > 0) {
    client.printf("model: %s\r\n", fishModel.c_str());
  }
  client.print("Content-Type: application/json\r\n");
  client.printf("Content-Length: %u\r\n", (unsigned)body.length());
  client.print("Connection: close\r\n\r\n");
  client.print(body);

  HttpStream hs;
  if (!readTtsHttpResponse("TTS/FishAudio", client, hs, false, true)) {
    return 0;
  }
  if (currentEmotion == EMOTION_IDLE || currentEmotion == EMOTION_THINKING) {
    setEmotion(EMOTION_SPEAKING);
  }
  gFaceFrameMs = FACE_FRAME_MS_ACTIVE;
  bool playing = false, haveLo = false;
  uint8_t lo = 0;
  uint8_t *rbuf = ttsRbuf;
  int n;
  uint32_t readStart = millis();
  uint32_t playStartedAt = 0;

  TtsPrebuf prebuf;

  uint32_t lastUiPump = 0;
  bool hitMaxPlay = false;
  while (true) {
    if (cancelRequest)
      break;
    if (maxPlayMs > 0 && playing && (millis() - playStartedAt) >= maxPlayMs) {
      hitMaxPlay = true;
      break;
    }
    n = httpReadBlock(hs, rbuf, ttsRbufMax);
    if (n < 0)
      break;
    if (n == 0) {
      if (millis() - readStart > 15000)
        break;
      serviceUi();
      yield();
      continue;
    }
    readStart = millis();
    if (!playing) {
      playing = true;
      playStartedAt = readStart;
    }
    for (int i = 0; i < n; i++) {
      if (!haveLo) {
        lo = rbuf[i];
        haveLo = true;
      } else {
        int16_t s = (int16_t)(lo | (rbuf[i] << 8));
        haveLo = false;
        prebuf.push(s);
      }
    }
    if (millis() - lastUiPump >= 25) {
      pumpUI();
      lastUiPump = millis();
    }
  }
  client.stop();
  bool audioPlayed = false;
  if (!cancelRequest && playing) {
    if (!hitMaxPlay) {
      prebuf.flushRemaining();
      audioPlayed = true;
    } else if (prebuf.done) {
      audioPlayed = true;
    }
    spkFlush();
    if (!hitMaxPlay) {
      for (int i = 0; i < SAMPLE_RATE / 10; i++)
        spkPushSample(0);
    }
  }
  spkEndSpeech();
  gFaceFrameMs = 33;
  net.leave();
  return audioPlayed ? 1 : 0;
}

bool speakFishAudio(const String &text, uint32_t maxPlayMs) {
  initTtsBuffers();
  String toSpeak = normalizeFishAudioTags(stripRobotTags(text));
  toSpeak.trim();
  Serial.printf("[TTS] speakFishAudio() model='%s' len=%d keyLen=%d heap=%u txHandle=%s\n",
                fishModel.c_str(), (int)toSpeak.length(), (int)fishKey.length(),
                (unsigned)ESP.getFreeHeap(), txHandle ? "OK" : "NULL");

  if (!txHandle || !fishKey.length() || WiFi.status() != WL_CONNECTED ||
      !toSpeak.length()) {
    return false;
  }

  int res = ttsFishAudio(toSpeak, maxPlayMs);
  spkEndSpeech();
  if (currentEmotion == EMOTION_SPEAKING) {
    setEmotion(EMOTION_IDLE);
  }
  Serial.printf("[TTS] speakFishAudio() returned %d\n", res);
  return res == 1;
}
