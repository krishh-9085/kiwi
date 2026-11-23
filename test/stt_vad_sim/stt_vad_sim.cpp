/**
 * stt_vad_sim.cpp — Host-side unit test for the STT adaptive endpointer.
 *
 * Build (Linux/macOS/Windows with MSVC or MinGW):
 *   g++ -std=c++14 -O2 -o stt_vad_sim stt_vad_sim.cpp && ./stt_vad_sim
 *   cl /std:c++14 /O2 /EHsc stt_vad_sim.cpp /Fe:stt_vad_sim.exe && stt_vad_sim.exe
 */

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <cstdarg>
#include <algorithm>
#include <string>
#include <vector>
#include <utility>
#include <cassert>

// ---- Portable constrain / max / min -----------------------------------------
template<typename T> static inline T constrain(T v, T lo, T hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}
template<typename T> static inline T mymax(T a, T b) { return a > b ? a : b; }
template<typename T> static inline T mymin(T a, T b) { return a < b ? a : b; }

// ---- Endpointer parameters (mirrors AppConfig defaults) ---------------------
static const int   SAMPLE_RATE       = 16000;
static const int   BLK               = SAMPLE_RATE / 20; // 50 ms = 800 samples

static       int   sttTrailingMs     = 450;
static       float sttNoiseK         = 2.2f;
static       float sttRelK           = 0.20f;
static       int   sttMaxSilenceMs   = 1200;

// ---- Endpointer state -------------------------------------------------------
struct VadState {
    bool  speechStarted       = false;
    bool  captureStopped      = false;
    bool  calibDone           = false;
    float noiseFloor          = 200.0f;
    float peakSpeechEnergy    = 0.0f;
    float speechActivityGate  = 0.0f;
    uint32_t speechSamples    = 0;
    uint32_t silenceSamples   = 0;
    uint32_t consecutiveSpeechBlocks = 0;
    uint32_t aboveGateSpikes  = 0;
    uint32_t sent             = 0;   // total samples processed
    uint32_t liveSent         = 0;   // samples processed from live mic
    uint32_t endpointAt       = 0;   // sample index when endpoint fired
    std::string endpointKind;        // "trailing" | "hard-cap" | "timeout" | "max-time" | "no-speech"
    std::string log;                 // accumulated log lines
};

static void vadLog(VadState &s, const char *fmt, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    s.log += buf;
    s.log += "\n";
}

// ---- Gate helpers -----------------------------------------------------------
static float computeSpeechGate(float noiseFloor) {
    return mymax(380.0f, noiseFloor * sttNoiseK);
}
static float computeSilenceGate(float noiseFloor, float peak) {
    if (peak > 0.0f) {
        float relThr = noiseFloor + (peak - noiseFloor) * sttRelK;
        float absThr = noiseFloor * 1.5f;
        return mymax(absThr, relThr);
    }
    return mymax(300.0f, computeSpeechGate(noiseFloor) * 0.88f);
}

// ---- Process one 50-ms block ------------------------------------------------
static void processBlock(VadState &s, float avg, bool inPreWake) {
    if (!s.calibDone && !inPreWake) return;

    const uint32_t TRAILING  = (uint32_t)((uint64_t)SAMPLE_RATE * sttTrailingMs  / 1000);
    const uint32_t MAXSIL    = (uint32_t)((uint64_t)SAMPLE_RATE * sttMaxSilenceMs / 1000);
    const uint32_t MIN_SPEECH = (SAMPLE_RATE * 20) / 100; // 200 ms
    const uint32_t INITIAL_TO = (SAMPLE_RATE * 35) / 10;  // 3.5 s

    const float NOISE_EMA = 0.08f;
    const float ENTRY_HYST = 1.4f;

    s.speechActivityGate = computeSpeechGate(s.noiseFloor);
    float silGate = computeSilenceGate(s.noiseFloor, s.peakSpeechEnergy);

    if (!s.speechStarted) {
        if (avg >= s.speechActivityGate) {
            s.consecutiveSpeechBlocks++;
            if (s.consecutiveSpeechBlocks >= 2 || avg >= s.speechActivityGate * ENTRY_HYST) {
                s.speechStarted = true;
                s.speechSamples = BLK * s.consecutiveSpeechBlocks;
                s.peakSpeechEnergy = avg;
                s.silenceSamples = 0;
                s.aboveGateSpikes = 0;
                vadLog(s, "  [VAD] Speech started at %.2fs (level=%.0f gate=%.0f noise=%.0f%s)",
                       (float)s.sent / SAMPLE_RATE, avg, s.speechActivityGate, s.noiseFloor,
                       inPreWake ? " pre-wake" : "");
            }
        } else {
            s.consecutiveSpeechBlocks = 0;
            s.noiseFloor = mymin(400.0f, s.noiseFloor * (1.0f - NOISE_EMA) + avg * NOISE_EMA);
            s.speechActivityGate = computeSpeechGate(s.noiseFloor);
            if (!inPreWake && s.liveSent >= INITIAL_TO) {
                s.captureStopped = true;
                s.endpointKind = "timeout";
                s.endpointAt = s.sent;
                vadLog(s, "  [VAD] No speech in first %.1fs; timeout", (float)INITIAL_TO / SAMPLE_RATE);
            }
        }
    } else {
        if (avg > s.peakSpeechEnergy) s.peakSpeechEnergy = avg;

        if (avg >= silGate) {
            if (s.silenceSamples == 0) {
                s.speechSamples += BLK;
                s.aboveGateSpikes = 0;
            } else {
                s.aboveGateSpikes++;
                if (avg >= s.speechActivityGate || s.aboveGateSpikes >= 2) {
                    s.speechSamples += BLK * s.aboveGateSpikes;
                    s.silenceSamples = 0;
                    s.aboveGateSpikes = 0;
                }
            }
        } else {
            s.aboveGateSpikes = 0;
            s.silenceSamples += BLK;
            s.noiseFloor = mymin(400.0f,
                s.noiseFloor * (1.0f - NOISE_EMA * 0.5f) + avg * (NOISE_EMA * 0.5f));

            // Endpointing NEVER triggers inside pre-wake buffer!
            if (!inPreWake && s.speechSamples >= MIN_SPEECH) {
                if (s.silenceSamples >= TRAILING) {
                    s.captureStopped = true;
                    s.endpointKind = "trailing";
                    s.endpointAt = s.sent;
                    vadLog(s, "  [VAD] Endpoint (trailing) at %.2fs speech=%.2fs sil=%lums "
                           "noise=%.0f peak=%.0f silGate=%.0f",
                           (float)s.sent / SAMPLE_RATE, (float)s.speechSamples / SAMPLE_RATE,
                           (unsigned long)(s.silenceSamples * 1000UL / SAMPLE_RATE),
                           s.noiseFloor, s.peakSpeechEnergy, silGate);
                } else if (s.silenceSamples >= MAXSIL) {
                    s.captureStopped = true;
                    s.endpointKind = "hard-cap";
                    s.endpointAt = s.sent;
                    vadLog(s, "  [VAD] Endpoint (hard-cap) at %.2fs sil=%lums",
                           (float)s.sent / SAMPLE_RATE,
                           (unsigned long)(s.silenceSamples * 1000UL / SAMPLE_RATE));
                }
            }
        }
    }
}

// ---- Run a simulation -------------------------------------------------------
struct SimConfig {
    std::string name;
    bool usePreWake;
    size_t preWakeSamples; // how many of the initial samples are pre-wake (default 24000)
    std::vector<std::pair<int,float>> segments;
    std::string expectedKind;
    int expectedSilMs_lo;
    int expectedSilMs_hi;
    SimConfig() : usePreWake(false), preWakeSamples(24000), expectedSilMs_lo(-1), expectedSilMs_hi(-1) {}
};

static VadState runSim(const SimConfig &cfg) {
    VadState s;
    s.noiseFloor = 200.0f;

    std::vector<float> samples;
    for (auto &seg : cfg.segments) {
        for (int i = 0; i < seg.first; i++)
            samples.push_back(seg.second);
    }

    const int MAX_SAMPLES = SAMPLE_RATE * 7;
    uint32_t total = (uint32_t)samples.size();
    if (total > (uint32_t)MAX_SAMPLES) total = MAX_SAMPLES;

    uint32_t blkN = 0;
    float blkSum = 0;

    size_t preWakeCount = cfg.usePreWake ? mymin((size_t)total, cfg.preWakeSamples) : 0;

    // Pre-wake path
    if (cfg.usePreWake && preWakeCount > 0) {
        // Pass 1: Calibrate noise from non-speech blocks in pre-wake
        float preWakeNoiseAcc = 0.0f;
        uint32_t preWakeNoiseCnt = 0;
        uint32_t pN = 0; float pSum = 0;
        for (size_t i = 0; i < preWakeCount; i++) {
            pSum += fabsf(samples[i]);
            if (++pN >= (uint32_t)BLK) {
                float avg = pSum / (float)pN;
                if (avg < computeSpeechGate(s.noiseFloor)) {
                    preWakeNoiseAcc += avg;
                    preWakeNoiseCnt++;
                }
                pSum = 0; pN = 0;
            }
        }
        if (preWakeNoiseCnt > 0) {
            float mn = preWakeNoiseAcc / (float)preWakeNoiseCnt;
            s.noiseFloor = constrain(mn, 80.0f, 400.0f);
        }
        s.calibDone = true;
        vadLog(s, "  [PRE-WAKE] noiseFloor=%.0f gate=%.0f", s.noiseFloor, computeSpeechGate(s.noiseFloor));

        // Pass 2: Process pre-wake blocks through state machine
        pN = 0; pSum = 0;
        for (size_t i = 0; i < preWakeCount; i++) {
            pSum += fabsf(samples[i]);
            s.sent++;
            if (++pN >= (uint32_t)BLK) {
                processBlock(s, pSum / (float)pN, true);
                pSum = 0; pN = 0;
            }
        }

        // Post-prewake boundary: if user paused after "Alexa", reset to pre-speech so live mic
        // waits for the user's actual command instead of cutting off on the wake word's pause!
        if (s.silenceSamples >= (uint32_t)BLK * 2) {
            s.speechStarted = false;
            s.speechSamples = 0;
            s.silenceSamples = 0;
            s.aboveGateSpikes = 0;
            s.consecutiveSpeechBlocks = 0;
            vadLog(s, "  [PRE-WAKE] Wake word concluded with pause; waiting for command in live audio");
        } else {
            s.silenceSamples = 0;
            s.aboveGateSpikes = 0;
        }
    }

    // Live mic recording loop
    blkN = 0; blkSum = 0;
    const uint32_t CALIB_WIN = SAMPLE_RATE / 4;
    float calibSum = 0; uint32_t calibN = 0;

    for (size_t i = preWakeCount; i < total && !s.captureStopped; i++) {
        float a = fabsf(samples[i]);
        s.sent++;
        s.liveSent++;

        if (!s.calibDone) {
            calibSum += a;
            calibN++;
            if (calibN == CALIB_WIN) {
                float mn = calibSum / (float)calibN;
                s.noiseFloor = constrain(mn, 80.0f, 400.0f);
                s.speechActivityGate = computeSpeechGate(s.noiseFloor);
                s.calibDone = true;
                vadLog(s, "  [CALIB] noiseFloor=%.0f gate=%.0f", s.noiseFloor, s.speechActivityGate);
            }
        } else {
            blkSum += a;
            if (++blkN >= (uint32_t)BLK) {
                processBlock(s, blkSum / (float)blkN, false);
                blkSum = 0; blkN = 0;
            }
        }
    }

    if (!s.captureStopped) {
        s.endpointKind = s.speechStarted ? "max-time" : "no-speech";
        s.endpointAt = s.sent;
    }

    return s;
}

// ---- Test runner ------------------------------------------------------------
static int gPass = 0, gFail = 0;

static void runTest(const SimConfig &cfg) {
    printf("TEST: %s\n", cfg.name.c_str());
    VadState s = runSim(cfg);
    printf("%s", s.log.c_str());

    bool ok = true;
    if (s.endpointKind != cfg.expectedKind) {
        printf("  FAIL: expected kind='%s' got='%s'\n",
               cfg.expectedKind.c_str(), s.endpointKind.c_str());
        ok = false;
    }
    if (cfg.expectedSilMs_lo >= 0 && s.speechStarted) {
        int gotSilMs = (int)(s.silenceSamples * 1000 / SAMPLE_RATE);
        if (gotSilMs < cfg.expectedSilMs_lo || gotSilMs > cfg.expectedSilMs_hi) {
            printf("  FAIL: expected trailing sil %d–%d ms, got %d ms\n",
                   cfg.expectedSilMs_lo, cfg.expectedSilMs_hi, gotSilMs);
            ok = false;
        }
    }
    if (ok) {
        printf("  PASS (endpoint='%s' at %.2fs)\n",
               s.endpointKind.c_str(), (float)s.endpointAt / SAMPLE_RATE);
        gPass++;
    } else {
        gFail++;
    }
    printf("\n");
}

int main() {
    printf("===== STT VAD Simulation Test Suite =====\n\n");

    // Test 1: Pure noise — no speech should be detected, timeout at 3.5s
    {
        SimConfig c;
        c.name = "1. Pure noise (120 amp) -> no speech detected, timeout at 3.5s";
        c.usePreWake = false;
        c.segments = {
            { SAMPLE_RATE * 7, 120.0f }
        };
        c.expectedKind = "timeout";
        runTest(c);
    }

    // Test 2: Speech burst then quiet -> endpoint at ~450–600 ms of silence
    {
        SimConfig c;
        c.name = "2. Speech (3000) then quiet (60) -> endpoint 450-600 ms after speech";
        c.usePreWake = false;
        c.segments = {
            { SAMPLE_RATE * 250 / 1000, 150.0f },   // 250 ms noise for calib
            { SAMPLE_RATE * 1000 / 1000, 3000.0f },  // 1 s speech
            { SAMPLE_RATE * 3000 / 1000, 60.0f }     // 3 s quiet
        };
        c.expectedKind = "trailing";
        c.expectedSilMs_lo = 450;
        c.expectedSilMs_hi = 600;
        runTest(c);
    }

    // Test 3: Speech then constant hum at 300 -> endpoint (hum is silence)
    {
        SimConfig c;
        c.name = "3. Speech (3000) then hum at 300 -> endpoint (hum is silence)";
        c.usePreWake = false;
        c.segments = {
            { SAMPLE_RATE * 250 / 1000, 150.0f },
            { SAMPLE_RATE * 800 / 1000, 3000.0f },
            { SAMPLE_RATE * 3000 / 1000, 300.0f }
        };
        c.expectedKind = "trailing";
        c.expectedSilMs_lo = 450;
        c.expectedSilMs_hi = 650;
        runTest(c);
    }

    // Test 4: WAKE WORD SCENARIO: User says "Alexa" (in pre-wake), pauses 500ms, then says command!
    // Must NOT cut off on the 500ms pause after "Alexa"! Must record the command and cut off after command!
    {
        SimConfig c;
        c.name = "4. Wake Word + Pause: 'Alexa' in pre-wake, 500ms pause, then 1s command -> cuts off after command!";
        c.usePreWake = true;
        c.preWakeSamples = (SAMPLE_RATE * 15) / 10; // 1.5s pre-wake
        c.segments = {
            // --- Pre-wake (1.5s total) ---
            { SAMPLE_RATE * 500 / 1000, 120.0f },   // 500ms quiet before "Alexa"
            { SAMPLE_RATE * 400 / 1000, 3500.0f },  // 400ms "Alexa"
            { SAMPLE_RATE * 600 / 1000, 120.0f },   // 600ms bridge silence after "Alexa"
            // --- Live mic audio ---
            { SAMPLE_RATE * 400 / 1000, 120.0f },   // 400ms more pause as user gets ready
            { SAMPLE_RATE * 1200 / 1000, 3000.0f }, // 1.2s user command: "kya hal hai kiwi"
            { SAMPLE_RATE * 2000 / 1000, 80.0f }    // 2s quiet after command
        };
        c.expectedKind = "trailing";
        c.expectedSilMs_lo = 450;
        c.expectedSilMs_hi = 650;
        runTest(c);
    }

    // Test 5: WAKE WORD SCENARIO: User says "Alexa" and says NOTHING.
    // Must wait 3.5s and timeout cleanly (not cut off in 450ms)!
    {
        SimConfig c;
        c.name = "5. Wake Word + Silence: 'Alexa' in pre-wake, user says nothing -> timeout after 3.5s live";
        c.usePreWake = true;
        c.preWakeSamples = (SAMPLE_RATE * 15) / 10;
        c.segments = {
            // --- Pre-wake (1.5s) ---
            { SAMPLE_RATE * 500 / 1000, 120.0f },
            { SAMPLE_RATE * 400 / 1000, 3500.0f },  // "Alexa"
            { SAMPLE_RATE * 600 / 1000, 120.0f },
            // --- Live mic: silence ---
            { SAMPLE_RATE * 5000 / 1000, 120.0f }
        };
        c.expectedKind = "timeout";
        runTest(c);
    }

    // Test 6: Hard-cap: trailing>max_sil -> hard-cap fires first
    {
        sttTrailingMs   = 1500;
        sttMaxSilenceMs = 800;
        SimConfig c;
        c.name = "6. Hard-cap: trailing>max_sil -> hard-cap fires at 800ms";
        c.usePreWake = false;
        c.segments = {
            { SAMPLE_RATE * 250 / 1000, 150.0f },
            { SAMPLE_RATE * 800 / 1000, 3000.0f },
            { SAMPLE_RATE * 3000 / 1000, 60.0f }
        };
        c.expectedKind = "hard-cap";
        c.expectedSilMs_lo = 800;
        c.expectedSilMs_hi = 1000;
        runTest(c);
        sttTrailingMs   = 450;
        sttMaxSilenceMs = 1200;
    }

    // Test 7: Noisy room (noise 350, speech 1200) -> endpoint fires
    {
        SimConfig c;
        c.name = "7. Noisy room (noise 350, speech 1200) -> endpoint fires";
        c.usePreWake = false;
        c.segments = {
            { SAMPLE_RATE * 250 / 1000, 350.0f },
            { SAMPLE_RATE * 1000 / 1000, 1200.0f },
            { SAMPLE_RATE * 3000 / 1000, 350.0f }
        };
        c.expectedKind = "trailing";
        c.expectedSilMs_lo = 450;
        c.expectedSilMs_hi = 700;
        runTest(c);
    }

    printf("===== Results: %d passed, %d failed =====\n", gPass, gFail);
    return gFail > 0 ? 1 : 0;
}
