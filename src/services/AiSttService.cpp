#include "services/AiSttService.h"
#include "services/AiTransport.h"
#include "services/AiTextTools.h"
#include "config/AppConfig.h"
#include "hardware/AudioEngine.h"
#include "face/FaceEngine.h"
#include "web/WebPortal.h"
#include "behavior/InteractionManager.h"
#include <WiFiClientSecure.h>

// ============================================================
// SPEECH-TO-TEXT (STT)
// ============================================================
SttCfg resolveStt() {
  SttCfg c;
  if (sttProvider == "groq") {
    c.url = groqSttEndpoint;
    c.key = groqSttKey.length() ? groqSttKey : (groqKey.length() ? groqKey : llmKey);
    c.model = groqSttModel;
    c.sarvam = false;
  } else if (sttProvider == "openai") {
    c.url = openaiSttEndpoint;
    c.key = openaiSttKey;
    c.model = openaiSttModel;
    c.sarvam = false;
  } else {
    c.url = sttEndpoint;
    c.key = sttKey.length() ? sttKey : (ttsKey.length() ? ttsKey : llmKey);
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
  pumpUI();  // Render listening face immediately before blocking work

  // Finish any active prompt tone before blocking network/TLS calls
  uint32_t toneWaitStart = millis();
  while (isTonePlaying() && (millis() - toneWaitStart < 250)) {
    updateTonePlayer();
    delay(4);
  }

  bool apWasUp = suspendLocalNet();
  pumpUI();  // Keep face alive after network suspend
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
    while (WiFi.status() != WL_CONNECTED && millis() - tw < 4000) {
      pumpUI();
      delay(50);
    }
    if (WiFi.status() != WL_CONNECTED)
      return bail("Wi-Fi dropped");
  }
  pumpUI();
  if (!ensureDns(host))
    return bail("DNS failed");
  pumpUI();
  if (!tlsConnect(client, host, "STT"))
    return bail("connect failed");
  pumpUI();

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
             "नमस्ते कीवी, क्या हाल है? कैसे हो? आज का मौसम कैसा है? क्या समय हुआ है? Hello Kiwi, how are you? सुनो कीवी, एक जोक सुनाओ। Suno kiwi, time kya hua hai, mausam kaisa hai, mera naam kya hai.\r\n";
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
  pumpUI();
  drainMic();
  pumpUI();
  if (!micSlotLocked) {
    detectMicSlot();
    pumpUI();
  }
  Serial.printf("[STT] Recording %lu s (mic slot %d%s, gain %.1f)...\n",
                (unsigned long)recordSeconds, micSlot, micSlotLocked ? " [cached]" : "",
                (double)MIC_GAIN);

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
  const uint32_t TRAILING_SILENCE_SAMPLES = (SAMPLE_RATE * 80) / 100; // 800ms natural speech pause (prevents cutoffs)
  const uint32_t INITIAL_SILENCE_TIMEOUT_SAMPLES = (SAMPLE_RATE * 25) / 10; // 2.5s start timeout
  float noiseSum = 0;
  uint32_t noiseSamples = 0;
  float calibSumL = 0, calibSumR = 0;
  float speechActivityGate = NOISE_GATE_THRESHOLD * 1.5f;

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
      if (sent < SAMPLE_RATE / 5) {
        calibSumL += fabsf((float)(raw[i * 2] >> 16));
        calibSumR += fabsf((float)(raw[i * 2 + 1] >> 16));
        noiseSum += a;
        noiseSamples++;
        if (noiseSamples == SAMPLE_RATE / 5) {
          if (!micSlotLocked) {
            if (calibSumL > 2.2f * calibSumR && calibSumL > 10.0f) {
              micSlot = 0;
            } else if (calibSumR > 2.2f * calibSumL && calibSumR > 10.0f) {
              micSlot = 1;
            }
            Serial.printf("[STT] Auto-calibrated Mic Slot: %d (L=%.1f, R=%.1f)\n",
                          micSlot, calibSumL / noiseSamples, calibSumR / noiseSamples);
          }
          float noiseAverage = noiseSum / noiseSamples;
          speechActivityGate =
              max((float)(NOISE_GATE_THRESHOLD * 1.5f), noiseAverage * 1.15f);
          Serial.printf("[STT] Noise floor %.0f, speech gate %.0f (Mic Slot %d)\n",
                        noiseAverage, speechActivityGate, micSlot);
        }
      }
      if (a > vuMax)
        vuMax = a;
      blkSum += a;
      if (++blkN >= BLK) {
        float avg = blkSum / blkN;
        if (avg > maxBlkAvg)
          maxBlkAvg = avg;
        if (avg >= speechActivityGate) {
          speechStarted = true;
          silenceSamples = 0;
        } else if (speechStarted && sent >= MIN_SPEECH_SAMPLES) {
          silenceSamples += BLK;
          if (silenceSamples >= TRAILING_SILENCE_SAMPLES) {
            captureStopped = true;
            Serial.printf("[STT] Speech ended after %.1f s; stopping early\n",
                          (float)sent / SAMPLE_RATE);
          }
        } else if (!speechStarted && sent >= INITIAL_SILENCE_TIMEOUT_SAMPLES) {
          captureStopped = true;
          Serial.println("[STT] No speech detected in first 1.5s; stopping early");
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
  if (!speechStarted && maxBlkAvg < speechActivityGate) {
    Serial.println("[STT] Silence threshold not met, aborting upload");
    return bail("quiet");
  }

  if (outPos > 0) {
    if (!clientWriteAll(client, out, outPos))
      return bail("upload failed");
  }
  // Ensure the WAV reaches its exact declared Content-Length even if mic stopped or had a partial buffer
  if (sent < recordSamples) {
    static uint8_t silence[UPLOAD_BATCH * 2] = {};
    while (sent < recordSamples) {
      uint32_t remainingSamples = recordSamples - sent;
      size_t silenceBytes = min((uint32_t)sizeof(silence), remainingSamples * 2U);
      if (!clientWriteAll(client, silence, silenceBytes))
        return bail("upload failed");
      sent += silenceBytes / 2;
    }
  }

  Serial.printf("[STT] Done. captured %.2f s, avg %.0f, loudest 100ms block %.0f (gate %.0f)\n",
                (float)capturedSamples / SAMPLE_RATE,
                sumAbs / recordSamples, maxBlkAvg, speechActivityGate);
  if (maxBlkAvg < speechActivityGate)
    return bail("quiet");

  client.print(part2);
  client.print(ending);
  client.flush();
  Serial.println("[STT] Upload complete. Waiting for response...");
  setEmotion(EMOTION_THINKING);

  uint32_t waitStart = millis();
  while (!client.available() && client.connected() &&
         millis() - waitStart < 12000) {
    if (cancelRequest)
      return bail("cancelled");
    pumpUI();
    delay(10);
  }

  Serial.printf("[STT] Response received after %lu ms (connected=%d, avail=%d)\n",
                (unsigned long)(millis() - waitStart), client.connected(), client.available());

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
