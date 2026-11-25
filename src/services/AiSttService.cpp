#include "services/AiSttService.h"
#include "services/AiTransport.h"
#include "services/AiTextTools.h"
#include "services/MemoryService.h"
#include "config/AppConfig.h"
#include "core/Timing.h"
#include "hardware/AudioEngine.h"
#include "hardware/WakeWordEngine.h"
#include "face/FaceEngine.h"
#include "web/WebPortal.h"
#include "behavior/InteractionManager.h"
#include <WiFiClientSecure.h>

// ============================================================
// HTTP/1.1 CHUNKED TRANSFER HELPER
// ============================================================
static bool writeChunk(WiFiClient &c, const uint8_t *p, size_t n) {
  if (n == 0) return true;
  char hexBuf[16];
  snprintf(hexBuf, sizeof(hexBuf), "%X\r\n", (unsigned int)n);
  if (!clientWriteAll(c, (const uint8_t *)hexBuf, strlen(hexBuf)))
    return false;
  if (!clientWriteAll(c, p, n))
    return false;
  if (!clientWriteAll(c, (const uint8_t *)"\r\n", 2))
    return false;
  return true;
}

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
  // Smoothly ease face into listening pose before blocking network work
  waitMs(120);

  // Finish any active prompt tone before blocking network/TLS calls
  uint32_t toneWaitStart = millis();
  while (isTonePlaying() && (millis() - toneWaitStart < 250)) {
    updateTonePlayer();
    serviceUi();
    yield();
  }

  bool apWasUp = suspendLocalNet();
  serviceUi();  // Keep face alive after network suspend
  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(8000);
  client.setHandshakeTimeout(8);

  auto bail = [&](const char *why) -> String {
    Serial.printf("[STT] FAIL: %s\n", why);
    sttFail = why;
    client.stop();
    restoreWebAndAP(apWasUp);
    return String("");
  };

  if (!psramFound() && ESP.getMaxAllocHeap() < 30000)
    return bail("low memory");
  if (WiFi.status() != WL_CONNECTED) {
    uint32_t tw = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - tw < 4000) {
      serviceUi();
      yield();
    }
    if (WiFi.status() != WL_CONNECTED)
      return bail("Wi-Fi dropped");
  }
  serviceUi();
  if (!ensureDns(host))
    return bail("DNS failed");
  pumpUI();
  if (!tlsConnect(client, host, "STT"))
    return bail("connect failed");
  pumpUI();

  const String boundary = "----MyRobotBoundary9A7F";
  const uint32_t recordSamples = SAMPLE_RATE * recordSeconds;

  String part1 = "--" + boundary +
                 "\r\n"
                 "Content-Disposition: form-data; name=\"file\"; "
                 "filename=\"audio.wav\"\r\n"
                 "Content-Type: audio/wav\r\n\r\n";

  String part2;
  part2.reserve(384);
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
    String sttPrompt = "नमस्ते कीवी, अलेक्सा, Alexa, Kiwi, युनय, Yunay";
    String owner = getOwnerName();
    if (owner.length() > 0) {
      sttPrompt += ", " + owner;
    }
    String friends = getFriendsFamily();
    if (friends.length() > 0) {
      sttPrompt += ", " + friends;
      String lowerFr = friends;
      lowerFr.toLowerCase();
      if (lowerFr.indexOf("yunay") >= 0 && friends.indexOf("युनय") < 0) {
        sttPrompt += ", युनय";
      }
    }
    sttPrompt += "। क्या हाल है? सुनो कीवी, आज का मौसम कैसा है? क्या समय हुआ है? Hello Kiwi, how are you? Suno kiwi, time kya hua hai, mausam kaisa hai.\r\n";

    part2 += "--" + boundary +
             "\r\n"
             "Content-Disposition: form-data; name=\"prompt\"\r\n\r\n" +
             sttPrompt;
  }

  String ending = "--" + boundary + "--\r\n";

  client.printf("POST %s HTTP/1.1\r\n", path.c_str());
  client.printf("Host: %s\r\n", host.c_str());
  if (cfg.sarvam)
    client.printf("api-subscription-key: %s\r\n", cfg.key.c_str());
  else
    client.printf("Authorization: Bearer %s\r\n", cfg.key.c_str());
  client.printf("Content-Type: multipart/form-data; boundary=%s\r\n",
                boundary.c_str());
  client.print("Transfer-Encoding: chunked\r\n");
  client.print("Connection: close\r\n\r\n");

  if (!writeChunk(client, (const uint8_t *)part1.c_str(), part1.length()))
    return bail("upload failed");

  static uint8_t wav[44];
  makeWavHeader(wav, 0xFFFFFFFF);
  if (!writeChunk(client, wav, 44))
    return bail("upload failed");

  bool hasPreWake = preWakeHasData();
  if (hasPreWake) {
    preWakeStopBridge();
    vTaskDelay(pdMS_TO_TICKS(15));
  }

  setEmotion(EMOTION_LISTENING);
  pumpUI();
  if (!hasPreWake) {
    drainMic();
    pumpUI();
  }
  if (!micSlotLocked && getMicSlot() < 0) {
    detectMicSlot();
    pumpUI();
  }

  // Play Wall-E ready chime ONLY when manually triggered (Touch/Web)
  // When wake word triggers, user is ALREADY speaking, so chime is skipped to prevent drowning out voice!
  if (!hasPreWake) {
    soundListening();
    uint32_t chimeWait = millis();
    while (isTonePlaying() && (millis() - chimeWait < 220)) {
      updateTonePlayer();
      serviceUi();
      yield();
    }
    delay(40);
    drainMic(); // Flush prompt chime audio from mic buffer before recording user speech
  }
  robotStateDesc = "Listening...";
  setEmotion(EMOTION_LISTENING);
  Serial.printf("[STT] Recording %lu s (mic slot %d%s, gain %.1f, preWake=%s)...\n",
                (unsigned long)recordSeconds, micSlot, micSlotLocked ? " [cached]" : "",
                (double)MIC_GAIN, hasPreWake ? "YES" : "NO");

  constexpr size_t RAW_BYTES = AUDIO_FRAMES * 2 * sizeof(int32_t);
  constexpr size_t OUT_BYTES = UPLOAD_BATCH * 2;
  struct SttWorkBufs {
    int32_t *raw = nullptr;
    uint8_t *out = nullptr;
    bool init(size_t rawBytes, size_t outBytes) {
      raw = (int32_t *)malloc(rawBytes);
      if (psramFound()) {
        out = (uint8_t *)heap_caps_malloc(outBytes,
                                          MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
      }
      if (!out) {
        out = (uint8_t *)malloc(outBytes);
      }
      return raw != nullptr && out != nullptr;
    }
    void release() {
      if (raw) {
        free(raw);
        raw = nullptr;
      }
      if (out) {
        free(out);
        out = nullptr;
      }
    }
    ~SttWorkBufs() { release(); }
  };
  SttWorkBufs workBufs;
  if (!workBufs.init(RAW_BYTES, OUT_BYTES))
    return bail("low memory");
  int32_t *raw = workBufs.raw;
  uint8_t *out = workBufs.out;
  int outPos = 0;
  uint32_t sent = 0, lastProgress = 0;
  float prevIn = 0, prevOut = 0;
  float sumAbs = 0;
  uint32_t vuMax = 0;

  // ---- Endpointer constants (read from config, applied once) ----------------
  // Block size: 50 ms
  const uint32_t BLK = SAMPLE_RATE / 20;
  // Trailing silence threshold (samples)
  const uint32_t TRAILING_SILENCE_SAMPLES =
      (uint32_t)((uint64_t)SAMPLE_RATE * (uint32_t)sttTrailingMs / 1000UL);
  // Hard-cap: stop unconditionally after this much silence post-speech
  const uint32_t MAX_SILENCE_SAMPLES =
      (uint32_t)((uint64_t)SAMPLE_RATE * (uint32_t)sttMaxSilenceMs / 1000UL);
  // Minimum speech before we look for silence
  const uint32_t MIN_SPEECH_SAMPLES = (SAMPLE_RATE * 20) / 100; // 200 ms
  // Initial timeout before any speech (3.5 s)
  const uint32_t INITIAL_SILENCE_TIMEOUT_SAMPLES = (SAMPLE_RATE * 35) / 10;
  // Hysteresis: re-enter speech only if 2+ consecutive blocks or big spike
  const float SPEECH_ENTRY_HYST = 1.4f; // single-block entry if avg > gate*1.4

  // ---- Endpointer state variables -------------------------------------------
  uint32_t blkN = 0;
  float blkSum = 0, maxBlkAvg = 0;
  bool speechStarted = false;
  bool captureStopped = false;
  uint32_t consecutiveSpeechBlocks = 0;
  uint32_t silenceSamples = 0;
  uint32_t speechSamples = 0;
  uint32_t aboveGateSpikes = 0;
  uint32_t liveSent = 0;

  // ---- Noise floor & gate ---------------------------------------------------
  // Calibration accumulates the first 250 ms of live mic samples.
  // The pre-wake drain uses the initial estimate (200) and updates it afterwards
  // from the FIRST calibration window that is classified as non-speech.
  float noiseFloor = 200.0f;         // initial estimate; refined during calib
  float peakSpeechEnergy = 0.0f;     // running max over speech blocks
  // EMA alpha for ambient noise adaptation (non-speech blocks only)
  const float NOISE_EMA_ALPHA = 0.08f;
  // Speech-entry gate: noiseFloor * sttNoiseK (clamped to >=380)
  auto computeSpeechGate = [&]() -> float {
    return max(380.0f, noiseFloor * sttNoiseK);
  };
  // Silence gate (hysteresis lower bound):
  //   max(noiseFloor * 1.5,  peakSpeechEnergy * sttRelK + noiseFloor)
  // If peak is known: use relative threshold so a "hum at 300" that is
  //   well below the speech peak is classified as silence.
  // Before any speech: same as entry gate * 0.88 (old behaviour).
  auto computeSilenceGate = [&]() -> float {
    if (peakSpeechEnergy > 0.0f) {
      float relThr = noiseFloor + (peakSpeechEnergy - noiseFloor) * sttRelK;
      float absThr = noiseFloor * 1.5f;
      return max(absThr, relThr);
    }
    return max(300.0f, computeSpeechGate() * 0.88f);
  };

  float speechActivityGate = computeSpeechGate(); // updated after calib

  // NVS calib accumulators (live mic path only)
  float noiseSum = 0;
  uint32_t noiseSamples = 0;
  float calibSumL = 0, calibSumR = 0;
  bool calibDone = hasPreWake; // pre-wake path skips live calib window

  // ---- Inline block processor -----------------------------------------------
  // Called with the per-block average `avg` and current `sent` count.
  // Updates speechStarted, speechSamples, silenceSamples, peakSpeechEnergy,
  // captureStopped, and the noise EMA.  Returns immediately if calib not done.
  auto processBlock = [&](float avg, bool inPreWake) {
    if (!calibDone && !inPreWake) return; // calib not complete yet

    if (avg > maxBlkAvg) maxBlkAvg = avg;
    speechActivityGate = computeSpeechGate();
    float silGate = computeSilenceGate();

    if (!speechStarted) {
      // --- Pre-speech: detect onset, adapt noise floor ---
      if (avg >= speechActivityGate) {
        consecutiveSpeechBlocks++;
        // Enter speech immediately on big spike OR after 2 consecutive blocks
        if (consecutiveSpeechBlocks >= 2 || avg >= speechActivityGate * SPEECH_ENTRY_HYST) {
          speechStarted = true;
          speechSamples = BLK * consecutiveSpeechBlocks;
          peakSpeechEnergy = avg;
          silenceSamples = 0;
          aboveGateSpikes = 0;
          Serial.printf("[STT] Speech started at %.2fs (level=%.0f gate=%.0f noise=%.0f%s)\n",
                        (float)sent / SAMPLE_RATE, avg, speechActivityGate, noiseFloor,
                        inPreWake ? " pre-wake" : "");
        }
      } else {
        consecutiveSpeechBlocks = 0;
        // Adapt noise EMA upward-only while pre-speech (cap at 400)
        noiseFloor = min(400.0f, noiseFloor * (1.0f - NOISE_EMA_ALPHA) + avg * NOISE_EMA_ALPHA);
        speechActivityGate = computeSpeechGate();

        if (!inPreWake && liveSent >= INITIAL_SILENCE_TIMEOUT_SAMPLES) {
          captureStopped = true;
          Serial.printf("[STT] No speech detected in first %.1fs; stopping early\n",
                        (float)INITIAL_SILENCE_TIMEOUT_SAMPLES / SAMPLE_RATE);
        }
      }
    } else {
      // --- Post-speech: track peak, detect silence, adapt noise ---
      if (avg > peakSpeechEnergy) peakSpeechEnergy = avg;

      if (currentLogLevel >= LOG_LEVEL_DEBUG) {
        Serial.printf("[STT-BLK] t=%.2fs avg=%.0f silGate=%.0f spGate=%.0f sil=%lums peak=%.0f noise=%.0f\n",
                      (float)sent / SAMPLE_RATE, avg, silGate, speechActivityGate,
                      (unsigned long)(silenceSamples * 1000UL / SAMPLE_RATE),
                      peakSpeechEnergy, noiseFloor);
      }

      if (avg >= silGate) {
        // Block is speech-level
        if (silenceSamples == 0) {
          speechSamples += BLK;
          aboveGateSpikes = 0;
        } else {
          aboveGateSpikes++;
          // Strong speech spike (>= speechActivityGate) or 2 consecutive blocks resets silence counter
          if (avg >= speechActivityGate || aboveGateSpikes >= 2) {
            speechSamples += BLK * aboveGateSpikes;
            silenceSamples = 0;
            aboveGateSpikes = 0;
          }
        }
      } else {
        aboveGateSpikes = 0;
        silenceSamples += BLK;
        // Adapt noise EMA down slowly during post-speech silence
        noiseFloor = min(400.0f, noiseFloor * (1.0f - NOISE_EMA_ALPHA * 0.5f)
                         + avg * (NOISE_EMA_ALPHA * 0.5f));

        // ENDPOINTING CUTOFF ONLY APPLIES TO LIVE MIC AUDIO, NEVER TO PRE-WAKE DRAIN!
        if (!inPreWake && speechSamples >= MIN_SPEECH_SAMPLES) {
          // Normal snappy cutoff: TRAILING_SILENCE_SAMPLES after min speech
          if (silenceSamples >= TRAILING_SILENCE_SAMPLES) {
            captureStopped = true;
            Serial.printf(
              "[STT] Endpoint at %.2fs (speech %.2fs, trailing_sil %lums, "
              "noise %.0f, peak %.0f, silGate %.0f)\n",
              (float)sent / SAMPLE_RATE,
              (float)speechSamples / SAMPLE_RATE,
              (unsigned long)(silenceSamples * 1000UL / SAMPLE_RATE),
              noiseFloor, peakSpeechEnergy, silGate);
          }
          // Hard cap: stop even if trailing didn't trigger
          else if (silenceSamples >= MAX_SILENCE_SAMPLES) {
            captureStopped = true;
            Serial.printf(
              "[STT] Endpoint (hard-cap) at %.2fs (speech %.2fs, sil %lums, "
              "noise %.0f, peak %.0f, silGate %.0f)\n",
              (float)sent / SAMPLE_RATE,
              (float)speechSamples / SAMPLE_RATE,
              (unsigned long)(silenceSamples * 1000UL / SAMPLE_RATE),
              noiseFloor, peakSpeechEnergy, silGate);
          }
        }
      }
    }
  };

  // Stream pre-wake audio from PSRAM ring buffer if available!
  if (hasPreWake) {
    const size_t DRAIN_MAX = 24000;
    static int16_t *s_psramDrainBuf = nullptr;
    if (psramFound() && !s_psramDrainBuf) {
      s_psramDrainBuf = (int16_t *)heap_caps_malloc(
          DRAIN_MAX * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
    int16_t *drainBuf = s_psramDrainBuf;
    if (!drainBuf) drainBuf = (int16_t *)malloc(DRAIN_MAX * sizeof(int16_t));
    if (drainBuf) {
      size_t nDrained = preWakeDrain(drainBuf, DRAIN_MAX);
      Serial.printf("[STT] Streaming %u pre-wake samples (%.2fs) from PSRAM ring buffer\n",
                    (unsigned)nDrained, (float)nDrained / SAMPLE_RATE);

      // First pass: calibrate noise floor from quiet pre-wake blocks
      // We use a two-pass approach: accumulate block averages below initial gate,
      // set noiseFloor, then process them through the state machine.
      float preWakeNoiseAcc = 0.0f;
      uint32_t preWakeNoiseCnt = 0;
      {
        uint32_t pN = 0; float pSum = 0;
        for (size_t i = 0; i < nDrained; i++) {
          pSum += abs((int)drainBuf[i]);
          if (++pN >= BLK) {
            float avg = pSum / (float)pN;
            if (avg < computeSpeechGate()) { // non-speech block
              preWakeNoiseAcc += avg;
              preWakeNoiseCnt++;
            }
            pSum = 0; pN = 0;
          }
        }
      }
      if (preWakeNoiseCnt > 0) {
        float measuredNoise = preWakeNoiseAcc / (float)preWakeNoiseCnt;
        noiseFloor = constrain(measuredNoise, 80.0f, 400.0f);
        speechActivityGate = computeSpeechGate();
        Serial.printf("[STT] Pre-wake noise floor %.0f -> speech gate %.0f\n",
                      noiseFloor, speechActivityGate);
      }
      calibDone = true; // noise calibrated from pre-wake; skip live calib window

      // Second pass: run full block state machine over drained samples
      uint32_t pN = 0; float pSum = 0;
      for (size_t i = 0; i < nDrained && sent < recordSamples; i++) {
        int16_t pcm = drainBuf[i];
        uint32_t a = abs((int)pcm);
        sumAbs += a;
        if (a > vuMax) vuMax = a;
        pSum += a;
        if (++pN >= BLK) {
          float avg = pSum / (float)pN;
          processBlock(avg, true);
          pSum = 0; pN = 0;
        }
        out[outPos++] = pcm & 0xff;
        out[outPos++] = (pcm >> 8) & 0xff;
        sent++;
        if (outPos >= (int)OUT_BYTES) {
          if (!writeChunk(client, out, outPos)) {
            if (drainBuf != s_psramDrainBuf) free(drainBuf);
            return bail("upload failed");
          }
          outPos = 0;
        }
      }
      if (drainBuf != s_psramDrainBuf) free(drainBuf);

      // Pre-wake audio contains the wake word "Alexa" and any trailing bridge silence.
      // If the user paused after saying "Alexa" (silence at the end of the pre-wake buffer),
      // the wake word has concluded and the robot is now waiting for the user's actual command.
      // Reset speechStarted to false so the user gets the full 3.5s timeout window to speak their query,
      // rather than being cut off 450ms after the wake word!
      if (silenceSamples >= BLK * 2) {
        speechStarted = false;
        speechSamples = 0;
        silenceSamples = 0;
        aboveGateSpikes = 0;
        consecutiveSpeechBlocks = 0;
        Serial.println("[STT] Wake word concluded with pause; waiting for command in live audio");
      } else {
        // Speech was actively ongoing at the end of pre-wake (continuous utterance: "Alexa, what...")
        silenceSamples = 0;
        aboveGateSpikes = 0;
      }
    }
  }

  while (sent < recordSamples && !captureStopped) {
    if (cancelRequest)
      return bail("cancelled");
    size_t bytesRead = 0;
    esp_err_t e =
        i2s_channel_read(rxHandle, raw, RAW_BYTES, &bytesRead, 1000);
    if (e != ESP_OK)
      return bail("mic read failed");
    int frames = bytesRead / (2 * sizeof(int32_t));
    for (int i = 0; i < frames && sent < recordSamples; i++) {
      liveSent++;
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
      else if (y < -32768.0f)
        y = -32768.0f;
      int16_t pcm = (int16_t)y;
      uint32_t a = abs((int)pcm);
      sumAbs += a;

      // ---- Calibration window (first 250 ms of live mic) ----
      // Skipped when calibDone==true (pre-wake path sets calibDone=true)
      if (!calibDone) {
        if (sent < SAMPLE_RATE / 4) {
          calibSumL += fabsf((float)(raw[i * 2] >> 16));
          calibSumR += fabsf((float)(raw[i * 2 + 1] >> 16));
          noiseSum += a;
          noiseSamples++;
          if (noiseSamples == SAMPLE_RATE / 4) {
            if (!micSlotLocked) {
              if (calibSumL > 3.0f * calibSumR && calibSumL > 20.0f) {
                setMicSlot(0, false);
              } else if (calibSumR > 3.0f * calibSumL && calibSumR > 20.0f) {
                setMicSlot(1, false);
              }
              Serial.printf("[STT] Auto-calibrated Mic Slot: %d (L=%.1f, R=%.1f)\n",
                            micSlot, calibSumL / noiseSamples, calibSumR / noiseSamples);
            }
            float measuredNoise = noiseSum / (float)noiseSamples;
            noiseFloor = constrain(measuredNoise, 80.0f, 400.0f);
            speechActivityGate = computeSpeechGate();
            calibDone = true;
            Serial.printf("[STT] Ambient noise floor %.0f (raw %.0f) -> speech gate %.0f (Mic Slot %d)\n",
                          noiseFloor, measuredNoise, speechActivityGate, micSlot);
          }
        }
      } else {
        // ---- Endpointer block accumulation ----
        if (a > vuMax) vuMax = a;
        blkSum += a;
        if (++blkN >= BLK) {
          float avg = blkSum / (float)blkN;
          processBlock(avg, false);
          blkSum = 0;
          blkN = 0;
        }
      }

      out[outPos++] = pcm & 0xff;
      out[outPos++] = (pcm >> 8) & 0xff;
      sent++;
      if (outPos >= (int)OUT_BYTES) {
        if (!writeChunk(client, out, outPos))
          return bail("upload failed");
        outPos = 0;
      }
    }
    if (sent - lastProgress >= SAMPLE_RATE) {
      lastProgress = sent;
      if (currentLogLevel >= LOG_LEVEL_DEBUG) {
        Serial.printf("[STT] %.0f sec, avg %.0f peak %lu loudest-block %.0f\n",
                      (float)sent / SAMPLE_RATE, sumAbs / sent,
                      (unsigned long)vuMax, maxBlkAvg);
      }
      vuMax = 0;
    }
    serviceUi();
  }

  uint32_t capturedSamples = sent;
  if (!speechStarted) {
    Serial.println("[STT] Silence threshold not met (no speech detected), aborting upload");
    return bail("quiet");
  }

  robotStateDesc = "Transcribing...";

  if (outPos > 0) {
    if (!writeChunk(client, out, outPos))
      return bail("upload failed");
    outPos = 0;
  }

  // Always log the final endpoint summary (even if max-time was hit)
  if (!captureStopped) {
    // Recording reached the hard 7-second max without endpoint firing
    Serial.printf(
      "[STT] Endpoint at %.2fs (speech %.2fs, trailing_sil %lums [max-time], "
      "noise %.0f, peak %.0f, spGate %.0f)\n",
      (float)capturedSamples / SAMPLE_RATE,
      (float)speechSamples / SAMPLE_RATE,
      (unsigned long)(silenceSamples * 1000UL / SAMPLE_RATE),
      noiseFloor, peakSpeechEnergy, speechActivityGate);
  }

  Serial.printf("[STT] Done. captured %.2f s, avg %.0f, loudest 100ms block %.0f (gate %.0f)\n",
                (float)capturedSamples / SAMPLE_RATE,
                (sent > 0 ? sumAbs / sent : 0.0f), maxBlkAvg, speechActivityGate);
  if (!speechStarted)
    return bail("quiet");

  if (!writeChunk(client, (const uint8_t *)part2.c_str(), part2.length()))
    return bail("upload failed");
  if (!writeChunk(client, (const uint8_t *)ending.c_str(), ending.length()))
    return bail("upload failed");

  // Send HTTP/1.1 chunked transfer stream terminator (0\r\n\r\n)
  static const char chunkTerminator[] = "0\r\n\r\n";
  if (!clientWriteAll(client, (const uint8_t *)chunkTerminator, 5))
    return bail("upload failed");
  client.flush();
  workBufs.release();
  Serial.println("[STT] Upload complete. Waiting for response...");
  setEmotion(EMOTION_THINKING);

  // Speculatively pre-resolve DNS for active LLM & TTS hosts while waiting for STT response
  if (llmProvider == "groq") ensureDns("api.groq.com");
  else ensureDns("generativelanguage.googleapis.com");
  String ttsHost, ttsPath;
  if (ttsProvider == "fish" || ttsProvider == "fishaudio") {
    if (parseUrl(fishEndpoint, ttsHost, ttsPath)) ensureDns(ttsHost);
  } else {
    if (parseUrl(ttsEndpoint, ttsHost, ttsPath)) ensureDns(ttsHost);
  }

  uint32_t waitStart = millis();
  while (!client.available() && client.connected() &&
         millis() - waitStart < 12000) {
    if (cancelRequest)
      return bail("cancelled");
    serviceUi();
    vTaskDelay(pdMS_TO_TICKS(1));
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
    serviceUi();
    yield();
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
