#include "services/AiTtsService.h"
#include "services/AiTransport.h"
#include "services/AiTextTools.h"
#include "config/AppConfig.h"
#include "hardware/AudioEngine.h"
#include "face/FaceEngine.h"
#include "web/WebPortal.h"
#include "behavior/InteractionManager.h"
#include <WiFiClientSecure.h>

// Unified shared audio buffers (prevents out-of-memory on ESP32-C3)
static const size_t TTS_PREBUF_SAMPLES = 2560; // 107ms @ 24kHz / 160ms @ 16kHz
static int16_t ttsPrebuf[TTS_PREBUF_SAMPLES];
static uint8_t ttsRbuf[2048];

// ============================================================
bool speakText(const String &text) {
  String toSpeak = (ttsProvider == "fish" || ttsProvider == "fishaudio") ? text : stripAudioCues(text);
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
  if (ttsProvider == "openai")
    res = ttsPcmOpenAI(toSpeak);
  else if (ttsProvider == "elevenlabs")
    res = ttsPcmEleven(toSpeak);
  else if (ttsProvider == "fish" || ttsProvider == "fishaudio")
    res = ttsFishAudio(toSpeak);
  else
    res = ttsSarvam(toSpeak);

  Serial.printf("[TTS] speakText() returned %d\n", res);
  return res == 1;
}

int ttsSarvam(const String &text) {
  String key = ttsKey.length() ? ttsKey : (sttKey.length() ? sttKey : llmKey);
  if (!key.length()) {
    Serial.println("[TTS] No Sarvam key configured in Web Portal.");
    addTerminalLog("[TTS] Error: No Sarvam key in Web Portal Settings!");
    return 0;
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[TTS] No Wi-Fi.");
    addTerminalLog("[TTS] Error: Wi-Fi disconnected!");
    return 0;
  }
  String host, path;
  if (!parseUrl(ttsEndpoint, host, path)) {
    Serial.printf("[TTS] Bad URL: %s\n", ttsEndpoint.c_str());
    addTerminalLog("[TTS] Error: Bad URL " + ttsEndpoint);
    return 0;
  }
  Serial.printf("[TTS/Sarvam] Free heap before: %u\n", (unsigned)ESP.getFreeHeap());
  bool apWasUp = suspendLocalNet();
  if (ESP.getFreeHeap() < 35000) {
    Serial.printf("[TTS/Sarvam] Low heap: %u < 35000\n", (unsigned)ESP.getFreeHeap());
    addTerminalLog("[TTS] Low heap (" + String(ESP.getFreeHeap()) + " < 35000)");
    restoreWebAndAP(apWasUp);
    return 0;
  }
  if (WiFi.status() != WL_CONNECTED) {
    uint32_t tw = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - tw < 4000)
      delay(100);
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("[TTS/Sarvam] Wi-Fi lost while waiting.");
      addTerminalLog("[TTS] Error: Wi-Fi lost");
      restoreWebAndAP(apWasUp);
      return 0;
    }
  }
  if (!ensureDns(host)) {
    Serial.printf("[TTS/Sarvam] DNS resolution failed for %s\n", host.c_str());
    addTerminalLog("[TTS] Error: DNS failed for " + host);
    restoreWebAndAP(apWasUp);
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
    uint32_t tErr = millis();
    while (!client.available() && client.connected() && millis() - tErr < 1200) delay(10);
    String errBody;
    while (client.available() && errBody.length() < 400)
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

  // Jitter Pre-Buffer (256ms @ 16kHz): Prevents Wi-Fi packet latency from causing audio gaps
  const size_t PREBUF_SAMPLES = TTS_PREBUF_SAMPLES;
  int16_t *prebuf = ttsPrebuf;
  size_t prebufCount = 0;
  bool prebufDone = false;

  const size_t CHUNK = 512;
  static int16_t pcmChunk[CHUNK];
  size_t chunkFill = 0;

  auto flushChunk = [&]() {
    if (chunkFill > 0) {
      if (!prebufDone) {
        for (size_t i = 0; i < chunkFill; i++) {
          prebuf[prebufCount++] = pcmChunk[i];
          if (prebufCount >= PREBUF_SAMPLES) {
            for (size_t k = 0; k < prebufCount; k++)
              spkPushSample(prebuf[k]);
            prebufCount = 0;
            prebufDone = true;
          }
        }
      } else {
        for (size_t i = 0; i < chunkFill; i++)
          spkPushSample(pcmChunk[i]);
      }
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
          gFaceFrameMs = 100;
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

  uint8_t *rbuf = ttsRbuf;
  int n;
  uint32_t readStart = millis();
  while (st != S_DONE) {
    if (cancelRequest) {
      aborted = true;
      break;
    }
    n = httpReadBlock(hs, rbuf, sizeof(ttsRbuf));
    if (n < 0)
      break;
    if (n == 0) {
      if (millis() - readStart > 12000)
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
  if (!prebufDone && prebufCount > 0) {
    for (size_t k = 0; k < prebufCount; k++)
      spkPushSample(prebuf[k]);
    prebufCount = 0;
    prebufDone = true;
  }
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
  String key = openaiTtsKey.length() ? openaiTtsKey : (openaiSttKey.length() ? openaiSttKey : llmKey);
  if (!key.length()) {
    Serial.println("[TTS/OpenAI] No OpenAI TTS API key configured.");
    addTerminalLog("[TTS/OpenAI] Error: No API key in Web Portal!");
    return 0;
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[TTS/OpenAI] No Wi-Fi.");
    addTerminalLog("[TTS/OpenAI] Error: Wi-Fi disconnected!");
    return 0;
  }
  String host, path;
  if (!parseUrl(openaiTtsEndpoint, host, path)) {
    Serial.printf("[TTS/OpenAI] Bad URL: %s\n", openaiTtsEndpoint.c_str());
    addTerminalLog("[TTS/OpenAI] Error: Bad URL " + openaiTtsEndpoint);
    return 0;
  }

  bool apWasUp = suspendLocalNet();
  if (ESP.getFreeHeap() < 35000) {
    Serial.printf("[TTS/OpenAI] Low heap (%u < 35000)\n", (unsigned)ESP.getFreeHeap());
    addTerminalLog("[TTS/OpenAI] Low heap (" + String(ESP.getFreeHeap()) + " < 35000)");
    restoreWebAndAP(apWasUp);
    return 0;
  }
  if (WiFi.status() != WL_CONNECTED) {
    uint32_t tw = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - tw < 4000)
      delay(100);
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("[TTS/OpenAI] Wi-Fi lost while waiting.");
      addTerminalLog("[TTS/OpenAI] Wi-Fi lost.");
      restoreWebAndAP(apWasUp);
      return 0;
    }
  }
  if (!ensureDns(host)) {
    Serial.printf("[TTS/OpenAI] DNS resolution failed for %s\n", host.c_str());
    addTerminalLog("[TTS/OpenAI] DNS failed for " + host);
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
  gFaceFrameMs = 100;
  bool playing = false, haveLo = false;
  uint8_t lo = 0;
  uint8_t *rbuf = ttsRbuf;
  int n;
  uint32_t readStart = millis();

  const size_t PREBUF_SAMPLES = TTS_PREBUF_SAMPLES;
  int16_t *prebuf = ttsPrebuf;
  size_t prebufCount = 0;
  bool prebufDone = false;

  auto pushWithPrebuf = [&](int16_t s) {
    if (!prebufDone) {
      prebuf[prebufCount++] = s;
      if (prebufCount >= PREBUF_SAMPLES) {
        for (size_t k = 0; k < prebufCount; k++)
          spkPushSample(prebuf[k]);
        prebufCount = 0;
        prebufDone = true;
      }
    } else {
      spkPushSample(s);
    }
  };

  uint32_t lastUiPump = 0;
  while (true) {
    if (cancelRequest)
      break;
    n = httpReadBlock(hs, rbuf, sizeof(ttsRbuf));
    if (n < 0)
      break;
    if (n == 0) {
      if (millis() - readStart > 15000)
        break;
      if (millis() - lastUiPump >= 60) {
        pumpUI();
        lastUiPump = millis();
      }
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
        pushWithPrebuf(s);
      }
    }
    if (millis() - lastUiPump >= 60) {
      pumpUI();
      lastUiPump = millis();
    }
  }
  client.stop();
  if (!prebufDone && prebufCount > 0) {
    for (size_t k = 0; k < prebufCount; k++)
      spkPushSample(prebuf[k]);
    prebufCount = 0;
    prebufDone = true;
  }
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
  gFaceFrameMs = 100;
  bool playing = false, haveLo = false;
  uint8_t lo = 0;
  uint8_t *rbuf = ttsRbuf;
  int n;
  uint32_t readStart = millis();

  const size_t PREBUF_SAMPLES = TTS_PREBUF_SAMPLES;
  int16_t *prebuf = ttsPrebuf;
  size_t prebufCount = 0;
  bool prebufDone = false;

  auto pushWithPrebuf = [&](int16_t s) {
    if (!prebufDone) {
      prebuf[prebufCount++] = s;
      if (prebufCount >= PREBUF_SAMPLES) {
        for (size_t k = 0; k < prebufCount; k++)
          spkPushSample(prebuf[k]);
        prebufCount = 0;
        prebufDone = true;
      }
    } else {
      spkPushSample(s);
    }
  };

  uint32_t lastUiPump = 0;
  while (true) {
    if (cancelRequest)
      break;
    n = httpReadBlock(hs, rbuf, sizeof(ttsRbuf));
    if (n < 0)
      break;
    if (n == 0) {
      if (millis() - readStart > 15000)
        break;
      if (millis() - lastUiPump >= 60) {
        pumpUI();
        lastUiPump = millis();
      }
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
        pushWithPrebuf(s);
      }
    }
    if (millis() - lastUiPump >= 60) {
      pumpUI();
      lastUiPump = millis();
    }
  }
  client.stop();
  if (!prebufDone && prebufCount > 0) {
    for (size_t k = 0; k < prebufCount; k++)
      spkPushSample(prebuf[k]);
    prebufCount = 0;
    prebufDone = true;
  }
  spkFlush();
  for (int i = 0; i < SAMPLE_RATE / 10; i++)
    spkPushSample(0);
  spkFlush();
  gFaceFrameMs = 33;
  restoreWebAndAP(apWasUp);
  return playing ? 1 : 0;
}

int ttsFishAudio(const String &text) {
  if (!fishKey.length()) {
    Serial.println("[TTS/FishAudio] No API key.");
    return 0;
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[TTS] No Wi-Fi.");
    return 0;
  }
  String host, path;
  if (!parseUrl(fishEndpoint, host, path))
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

  String body = "{\"text\":\"" + jsonEscape(text) +
                "\",\"format\":\"pcm\",\"sample_rate\":24000";
  if (fishVoice.length() > 0) {
    body += ",\"reference_id\":\"" + jsonEscape(fishVoice) + "\"";
  }
  body += "}";

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(20);
  client.setHandshakeTimeout(10);
  if (!tlsConnect(client, host, "TTS/FishAudio")) {
    client.stop();
    restoreWebAndAP(apWasUp);
    return 0;
  }
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
    uint32_t tErr = millis();
    while (!client.available() && client.connected() && millis() - tErr < 1200) delay(10);
    String errBody;
    while (client.available() && errBody.length() < 400)
      errBody += (char)client.read();
    Serial.printf("[TTS/FishAudio] HTTP %d: %s\n", status, errBody.c_str());
    client.stop();
    restoreWebAndAP(apWasUp);
    return 0;
  }
  setEmotion(EMOTION_SPEAKING);
  gFaceFrameMs = 100;
  bool playing = false, haveLo = false;
  uint8_t lo = 0;
  uint8_t *rbuf = ttsRbuf;
  int n;
  uint32_t readStart = millis();

  const size_t PREBUF_SAMPLES = TTS_PREBUF_SAMPLES;
  int16_t *prebuf = ttsPrebuf;
  size_t prebufCount = 0;
  bool prebufDone = false;

  auto pushWithPrebuf = [&](int16_t s) {
    if (!prebufDone) {
      prebuf[prebufCount++] = s;
      if (prebufCount >= PREBUF_SAMPLES) {
        for (size_t k = 0; k < prebufCount; k++)
          spkPushSample(prebuf[k]);
        prebufCount = 0;
        prebufDone = true;
      }
    } else {
      spkPushSample(s);
    }
  };

  uint32_t lastUiPump = 0;
  while (true) {
    if (cancelRequest)
      break;
    n = httpReadBlock(hs, rbuf, sizeof(ttsRbuf));
    if (n < 0)
      break;
    if (n == 0) {
      if (millis() - readStart > 15000)
        break;
      if (millis() - lastUiPump >= 60) {
        pumpUI();
        lastUiPump = millis();
      }
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
        pushWithPrebuf(s);
      }
    }
    if (millis() - lastUiPump >= 60) {
      pumpUI();
      lastUiPump = millis();
    }
  }
  client.stop();
  if (!prebufDone && prebufCount > 0) {
    for (size_t k = 0; k < prebufCount; k++)
      spkPushSample(prebuf[k]);
    prebufCount = 0;
    prebufDone = true;
  }
  spkFlush();
  for (int i = 0; i < SAMPLE_RATE / 10; i++)
    spkPushSample(0);
  spkFlush();
  gFaceFrameMs = 33;
  restoreWebAndAP(apWasUp);
  return playing ? 1 : 0;
}
