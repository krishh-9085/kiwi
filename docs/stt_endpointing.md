# STT Adaptive Endpointing — Design Note

## Problem

The original VAD used a fixed absolute gate for silence detection:

```
speechOngoingGate = max(340, speechActivityGate * 0.88)
```

On devices where the room hum or mic self-noise stays above ~340, every post-speech block still counts as "above gate", so `silenceSamples` never reaches the 450 ms threshold and recording runs for the full 7-second max.

The pre-wake drain loop also set `speechStarted = true` without logging and without calibrating the noise floor first, so the state machine started with wrong gate values.

---

## Algorithm

### 1. Noise Floor Estimation (EMA)

A single floating-point `noiseFloor` variable represents the ambient noise level.

- **Pre-wake path**: a first pass over the drained buffer accumulates block averages that fall below the initial speech gate; their mean is used as the initial noise floor. This avoids corrupting the floor with speech blocks.
- **Live-mic calibration window** (first 250 ms, skipped when pre-wake is used): the raw sum of |PCM| values is averaged to produce an initial `noiseFloor`, clamped to [80, 400].
- **Ongoing adaptation**: during both pre-speech and post-speech silence blocks, `noiseFloor` is updated with an EMA (`α = 0.08`):
  ```
  noiseFloor = noiseFloor * (1 - α) + blockAvg * α
  ```
  Post-speech adaptation uses half-alpha to avoid being pulled up by residual speech energy.
- `noiseFloor` is capped at 400 to prevent runaway from loud transients.

### 2. Speech Entry Gate (hysteresis on entry)

```
speechGate = max(380.0, noiseFloor * sttNoiseK)   // default sttNoiseK = 2.2
```

Speech is declared started when:
- Two consecutive blocks ≥ speechGate, **OR**
- One block ≥ speechGate × 1.4 (big spike — instant entry)

### 3. Silence Gate (adaptive, hysteresis on exit)

```
silenceGate = max(noiseFloor * 1.5,
                  noiseFloor + (peakSpeechEnergy - noiseFloor) * sttRelK)
```
Default `sttRelK = 0.20`. This means: "silence when the block level has
dropped to within 20% of the gap between noise floor and speech peak."

**Example** (the failing log):
- noiseFloor ≈ 185 (avg 1482 / gain-corrected)
- peakSpeechEnergy ≈ 8353
- silenceGate ≈ 185 + (8353 − 185) × 0.20 ≈ **1819**

A room hum at 300 is well below 1819 → silence detected correctly.

The old fixed gate was 340, which a 300-amplitude hum could not trigger.

### 4. Silence Accumulation & Cutoff

After `MIN_SPEECH_SAMPLES` (200 ms) of speech have been detected:

| Condition | Action |
|-----------|--------|
| `silenceSamples >= TRAILING_SILENCE_SAMPLES` (`sttTrailingMs`, default 450 ms) | Stop — emit `[STT] Endpoint at ...` |
| `silenceSamples >= MAX_SILENCE_SAMPLES` (`sttMaxSilenceMs`, default 1200 ms) | Stop — emit `[STT] Endpoint (hard-cap) at ...` |
| 7-second max reached without endpoint | Emit `[STT] Endpoint at ... [max-time]` |

The two-block anti-transient protection is preserved: a single above-gate
block after a gap of silence does not reset `silenceSamples` immediately;
two consecutive above-gate blocks are required.

### 5. Pre-Wake Path & Wake Word Boundary Handling

The pre-wake ring buffer stores the audio preceding STT connection (~1.5s). When activated by the wake word ("Alexa"):
1. The drain loop makes a **first pass** to calibrate `noiseFloor` from non-speech blocks.
2. Sets `calibDone = true` to skip the live-mic calibration window.
3. Makes a **second pass** running `processBlock()` over drained samples.
4. **Endpointing Safety**: `captureStopped` is **strictly forbidden** from triggering during pre-wake (`inPreWake == true`). An endpoint can only trigger on live microphone audio.
5. **Wake Word Boundary Disambiguation**:
   - The wake word ("Alexa") is detected in pre-wake, and naturally followed by a brief user pause while TLS connects (300–800 ms of bridge silence).
   - If the pre-wake audio concludes with silence (`silenceSamples >= 100 ms`), the system recognizes that the wake word has finished and the user is pausing before uttering their command. It resets `speechStarted = false`, `speechSamples = 0`, and `silenceSamples = 0`.
   - The live mic recording then enters the pre-speech waiting state, granting the user the full 3.5s timeout (`INITIAL_SILENCE_TIMEOUT_SAMPLES`) to speak their query, rather than cutting them off 450 ms after "Alexa".
   - If the user spoke continuously ("Alexa, what's the weather?") without a pause, `speechStarted` remains `true` and flows seamlessly into live mic capture.

### 6. Logging

- **Always logged**: `[STT] Endpoint at ...` (or `Endpoint (hard-cap) at ...` or `[max-time]`).
- **Debug-only** (when `currentLogLevel >= LOG_LEVEL_DEBUG`): per-block `[STT-BLK]` lines showing timestamp, avg, silence gate, speech gate, silence duration, peak, noise floor. Enable by setting log level to DEBUG in the serial console or web UI.

---

## Parameters

| Variable | NVS key | Default | Range | Meaning |
|----------|---------|---------|-------|---------|
| `sttTrailingMs` | `stt_trail_ms` | 450 | 100–3000 ms | Trailing silence to cut after speech |
| `sttNoiseK` | `stt_noise_k` | 2.2 | 1.1–10.0 | Noise multiplier for speech entry gate |
| `sttRelK` | `stt_rel_k` | 0.20 | 0.0–1.0 | Speech-peak fraction for silence gate |
| `sttMaxSilenceMs` | `stt_max_sil` | 1200 | 300–5000 ms | Hard cap on post-speech silence |

Exposed via `/api/config` (GET) and `/save` (POST). Persisted to NVS via `ConfigManager`.

---

## Tuning Guide

| Symptom | Adjustment |
|---------|------------|
| Still listens too long after speech | Lower `sttRelK` (e.g. 0.10) or lower `sttTrailingMs` |
| Cuts off mid-word (too eager) | Raise `sttRelK` (e.g. 0.30) or raise `sttTrailingMs` |
| Speech not detected (quiet speaker) | Lower `sttNoiseK` (e.g. 1.6) |
| Background noise triggers speech | Raise `sttNoiseK` (e.g. 2.8) |
| Hard silence cap triggers before speech ends | Raise `sttMaxSilenceMs` |

---

## What Was Not Verified on Hardware

This change was not tested on a physical device. After flashing, check the serial log for:

```
[STT] Pre-wake noise floor NNN -> speech gate GGG        // pre-wake path
[STT] Ambient noise floor NNN (raw NNN) -> speech gate GGG (Mic Slot N)  // live-mic path
[STT] Speech started at X.XXs (level=NNN gate=GGG noise=NNN [pre-wake])
[STT] Endpoint at X.XXs (speech Y.YYs, trailing_sil ZZZms, noise NNN, peak PPP, silGate TTT)
```

If you still see `Endpoint at ... [max-time]`, enable debug logging to see `[STT-BLK]` lines and check whether `silGate` is too high relative to the post-speech block averages. Lower `sttRelK` or `sttNoiseK` accordingly.

If the endpoint fires too early, raise `sttRelK`.

---

## Files Changed

| File | Change |
|------|--------|
| `src/services/AiSttService.cpp` | Complete VAD rewrite with adaptive EMA endpointer; pre-wake calibration pass; `processBlock()` lambda; diagnostic logging |
| `src/config/AppConfig.h` | 4 new `extern` VAD parameters |
| `src/config/AppConfig.cpp` | Default values for 4 new VAD parameters |
| `src/config/ConfigManager.cpp` | NVS load and save for 4 new VAD parameters |
| `src/web/WebPortalApi.cpp` | Expose VAD params in GET `/api/config` and POST `/save` |
| `docs/stt_endpointing.md` | This file |
| `test/stt_vad_sim/` | Host-side unit test harness |
