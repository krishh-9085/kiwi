#include "hardware/WakeWordEngine.h"
#include "hardware/AudioEngine.h"
#include "hardware/SoundLevel.h"
#include "hardware/alexa_model.h"
#include "config/AppConfig.h"
#include "face/FaceEngine.h"
#include "power/PowerManager.h"

#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include <frontend.h>
#include <frontend_util.h>

#include <tensorflow/lite/core/c/common.h>
#include <tensorflow/lite/micro/micro_interpreter.h>
#include <tensorflow/lite/micro/micro_mutable_op_resolver.h>
#include <tensorflow/lite/micro/micro_resource_variable.h>
#include <tensorflow/lite/micro/micro_allocator.h>
#include <tensorflow/lite/schema/schema_generated.h>

// ============================================================
// CONSTANTS & BUFFERS
// ============================================================
static const uint8_t PREPROCESSOR_FEATURE_SIZE = 40;
static const uint8_t FEATURE_DURATION_MS = 30;
static const uint16_t AUDIO_SAMPLE_FREQUENCY = 16000;
static const uint8_t FEATURE_STEP_SIZE_MS = 10;
static const size_t STEP_SAMPLES = (FEATURE_STEP_SIZE_MS * AUDIO_SAMPLE_FREQUENCY) / 1000; // 160 samples (10ms)
static const size_t MAX_SLIDING_WINDOW_SIZE = 8;
static const uint8_t MIN_SLICES_BEFORE_DETECTION = 74; // ~740ms initial priming
static const size_t TENSOR_ARENA_SIZE = 20000;
static const size_t VARIABLE_ARENA_SIZE = 1024;

// Multi-Model Definitions with Official Manifest Specifications
struct WakeWordModelEntry {
  const char *id;
  const char *name;
  const unsigned char *data;
  size_t length;
  float defaultCutoff;
  float minCutoff;
  float maxCutoff;
  size_t windowSize;
  bool isHighCutoffProfile;
};

static const WakeWordModelEntry WAKE_WORD_MODELS[] = {
  { "alexa",      "Alexa",      alexa_model,      sizeof(alexa_model),      0.90f, 0.60f, 0.98f, 5, true }
};
static const size_t NUM_WAKE_WORD_MODELS = sizeof(WAKE_WORD_MODELS) / sizeof(WAKE_WORD_MODELS[0]);

// Mutex protecting frontend state, interpreter and window buffers across cores
static SemaphoreHandle_t s_wwMutex = nullptr;

static uint8_t s_activeModelIndex = 0;
static size_t s_sliding_window_size = 5;
static float s_probability_cutoff = 0.90f;
static float s_gain = 8.0f;
static bool s_enable_spike_trigger = false;
static float s_bargeInBump = 0.08f;

// Frontend State
static FrontendConfig s_frontend_config;
static FrontendState s_frontend_state;
static bool s_frontend_initialized = false;

// TFLite Micro Memory Arenas (16-byte aligned in fast SRAM)
alignas(16) static uint8_t s_tensor_arena[TENSOR_ARENA_SIZE];
alignas(16) static uint8_t s_variable_arena[VARIABLE_ARENA_SIZE];

static tflite::MicroMutableOpResolver<20> *s_op_resolver = nullptr;
static tflite::MicroAllocator *s_micro_allocator = nullptr;
static tflite::MicroResourceVariables *s_micro_resource_vars = nullptr;
static tflite::MicroInterpreter *s_interpreter = nullptr;

static uint8_t s_current_stride_step = 0;
static uint8_t s_sliding_window[MAX_SLIDING_WINDOW_SIZE] = {0};
static size_t s_sliding_index = 0;
static int16_t s_ignore_windows = -MIN_SLICES_BEFORE_DETECTION;

static TaskHandle_t s_wakeWordTaskHandle = nullptr;
static volatile bool s_wakeWordRunning = false;
static volatile bool s_isListening = false;

// High-pass DC offset filter state for audio input
static float s_prevIn = 0.0f;
static float s_prevOut = 0.0f;

// Forward declarations
static void wakeWordTask(void *params);
static bool registerOps(tflite::MicroMutableOpResolver<20> &resolver);
static bool loadModelInternal(uint8_t modelIndex, float preservedCutoff);
static void wakeWordResetLocked();

// ============================================================
// OPERATOR REGISTRATION
// ============================================================
static bool registerOps(tflite::MicroMutableOpResolver<20> &resolver) {
  if (resolver.AddCallOnce() != kTfLiteOk) return false;
  if (resolver.AddVarHandle() != kTfLiteOk) return false;
  if (resolver.AddReshape() != kTfLiteOk) return false;
  if (resolver.AddReadVariable() != kTfLiteOk) return false;
  if (resolver.AddStridedSlice() != kTfLiteOk) return false;
  if (resolver.AddConcatenation() != kTfLiteOk) return false;
  if (resolver.AddAssignVariable() != kTfLiteOk) return false;
  if (resolver.AddConv2D() != kTfLiteOk) return false;
  if (resolver.AddMul() != kTfLiteOk) return false;
  if (resolver.AddAdd() != kTfLiteOk) return false;
  if (resolver.AddMean() != kTfLiteOk) return false;
  if (resolver.AddFullyConnected() != kTfLiteOk) return false;
  if (resolver.AddLogistic() != kTfLiteOk) return false;
  if (resolver.AddQuantize() != kTfLiteOk) return false;
  if (resolver.AddDepthwiseConv2D() != kTfLiteOk) return false;
  if (resolver.AddAveragePool2D() != kTfLiteOk) return false;
  if (resolver.AddMaxPool2D() != kTfLiteOk) return false;
  if (resolver.AddPad() != kTfLiteOk) return false;
  if (resolver.AddPack() != kTfLiteOk) return false;
  if (resolver.AddSplitV() != kTfLiteOk) return false;
  return true;
}

// ============================================================
// MODEL LOADER & RUNTIME SWITCHING (WITH ROLLBACK & MUTEX)
// ============================================================
static bool loadModelInternal(uint8_t modelIndex, float preservedCutoff) {
  if (modelIndex >= NUM_WAKE_WORD_MODELS) {
    modelIndex = 0;
  }
  uint8_t prevIndex = s_activeModelIndex;
  float prevCutoff = s_probability_cutoff;
  const WakeWordModelEntry &entry = WAKE_WORD_MODELS[modelIndex];

  Serial.printf("[WAKEWORD] Loading model '%s' (%zu bytes)...\n", entry.name, entry.length);

  const tflite::Model *model = tflite::GetModel(entry.data);
  if (!model || model->version() != TFLITE_SCHEMA_VERSION) {
    Serial.printf("[WAKEWORD] ERROR: Model schema mismatch for '%s' (%lu vs %d)\n",
                  entry.name, model ? model->version() : 0, TFLITE_SCHEMA_VERSION);
    return false;
  }

  if (s_interpreter) {
    delete s_interpreter;
    s_interpreter = nullptr;
  }

  // Clear arenas for fresh tensor placement
  memset(s_variable_arena, 0, sizeof(s_variable_arena));
  memset(s_tensor_arena, 0, sizeof(s_tensor_arena));

  s_micro_allocator = tflite::MicroAllocator::Create(s_variable_arena, VARIABLE_ARENA_SIZE);
  if (!s_micro_allocator) {
    Serial.println("[WAKEWORD] ERROR: Failed to create MicroAllocator!");
    if (modelIndex != prevIndex && prevIndex < NUM_WAKE_WORD_MODELS) {
      Serial.printf("[WAKEWORD] Rolling back to '%s'...\n", WAKE_WORD_MODELS[prevIndex].name);
      loadModelInternal(prevIndex, prevCutoff);
    }
    return false;
  }
  s_micro_resource_vars = tflite::MicroResourceVariables::Create(s_micro_allocator, 20);
  if (!s_micro_resource_vars) {
    Serial.println("[WAKEWORD] ERROR: Failed to create MicroResourceVariables!");
    if (modelIndex != prevIndex && prevIndex < NUM_WAKE_WORD_MODELS) {
      Serial.printf("[WAKEWORD] Rolling back to '%s'...\n", WAKE_WORD_MODELS[prevIndex].name);
      loadModelInternal(prevIndex, prevCutoff);
    }
    return false;
  }

  s_interpreter = new tflite::MicroInterpreter(
      model, *s_op_resolver, s_tensor_arena, TENSOR_ARENA_SIZE, s_micro_resource_vars);
  if (!s_interpreter) {
    Serial.println("[WAKEWORD] ERROR: Failed to create MicroInterpreter!");
    if (modelIndex != prevIndex && prevIndex < NUM_WAKE_WORD_MODELS) {
      Serial.printf("[WAKEWORD] Rolling back to '%s'...\n", WAKE_WORD_MODELS[prevIndex].name);
      loadModelInternal(prevIndex, prevCutoff);
    }
    return false;
  }

  if (s_interpreter->AllocateTensors() != kTfLiteOk) {
    Serial.println("[WAKEWORD] ERROR: AllocateTensors() failed!");
    delete s_interpreter;
    s_interpreter = nullptr;
    if (modelIndex != prevIndex && prevIndex < NUM_WAKE_WORD_MODELS) {
      Serial.printf("[WAKEWORD] Rolling back to '%s'...\n", WAKE_WORD_MODELS[prevIndex].name);
      loadModelInternal(prevIndex, prevCutoff);
    }
    return false;
  }

  s_activeModelIndex = modelIndex;
  s_sliding_window_size = entry.windowSize;

  // P1: Validate preserved cutoff against target model's range; otherwise fall back to manifest default
  float targetCutoff = entry.defaultCutoff;
  if (preservedCutoff >= entry.minCutoff && preservedCutoff <= entry.maxCutoff) {
    targetCutoff = preservedCutoff;
  }
  s_probability_cutoff = targetCutoff;
  wakeWordCutoff = targetCutoff;

  TfLiteTensor *input = s_interpreter->input(0);
  TfLiteTensor *output = s_interpreter->output(0);
  Serial.printf("[WAKEWORD] Model '%s' ready! In: [%d,%d,%d] (type %d), Out: [%d,%d] (type %d), Cutoff: %.2f (range %.2f-%.2f, window %zu)\n",
                entry.name,
                input->dims->data[0], input->dims->data[1], input->dims->data[2], input->type,
                output->dims->data[0], output->dims->data[1], output->type,
                s_probability_cutoff, entry.minCutoff, entry.maxCutoff, s_sliding_window_size);
  Serial.printf("[WAKEWORD] Arena used: %u / %u bytes\n",
                (unsigned int)s_interpreter->arena_used_bytes(), (unsigned int)TENSOR_ARENA_SIZE);

  wakeWordResetLocked();
  return true;
}

// ============================================================
// INITIALIZATION
// ============================================================
bool wakeWordInit() {
  Serial.println("[WAKEWORD] Initializing microWakeWord engine...");

  // Mutex initialization
  if (!s_wwMutex) {
    s_wwMutex = xSemaphoreCreateMutex();
    if (!s_wwMutex) {
      Serial.println("[WAKEWORD] ERROR: Failed to create s_wwMutex!");
      return false;
    }
  }

  // Sync runtime settings from AppConfig
  s_gain = wakeWordGain;
  s_enable_spike_trigger = wakeWordSpikeTrigger;
  s_bargeInBump = wakeWordBargeInBump;

  // 1. Setup Mel-filterbank preprocessor frontend
  s_frontend_config.window.size_ms = FEATURE_DURATION_MS;
  s_frontend_config.window.step_size_ms = FEATURE_STEP_SIZE_MS;
  s_frontend_config.filterbank.num_channels = PREPROCESSOR_FEATURE_SIZE;
  s_frontend_config.filterbank.lower_band_limit = 125.0f;
  s_frontend_config.filterbank.upper_band_limit = 7500.0f;
  s_frontend_config.noise_reduction.smoothing_bits = 10;
  s_frontend_config.noise_reduction.even_smoothing = 0.025f;
  s_frontend_config.noise_reduction.odd_smoothing = 0.06f;
  s_frontend_config.noise_reduction.min_signal_remaining = 0.05f;
  s_frontend_config.pcan_gain_control.enable_pcan = 1;
  s_frontend_config.pcan_gain_control.strength = 0.95f;
  s_frontend_config.pcan_gain_control.offset = 80.0f;
  s_frontend_config.pcan_gain_control.gain_bits = 21;
  s_frontend_config.log_scale.enable_log = 1;
  s_frontend_config.log_scale.scale_shift = 6;

  if (!FrontendPopulateState(&s_frontend_config, &s_frontend_state, AUDIO_SAMPLE_FREQUENCY)) {
    Serial.println("[WAKEWORD] ERROR: FrontendPopulateState failed!");
    return false;
  }
  s_frontend_initialized = true;

  // 2. Setup TFLite Micro Operation Resolver
  if (!s_op_resolver) {
    s_op_resolver = new tflite::MicroMutableOpResolver<20>();
    if (!registerOps(*s_op_resolver)) {
      Serial.println("[WAKEWORD] ERROR: Failed to register TFLite Micro ops!");
      return false;
    }
  }

  // 3. P1 & P2: Always load model with user-saved cutoff preserved
  float savedCutoff = wakeWordCutoff;
  if (!loadModelInternal(wakeWordModel, savedCutoff)) {
    Serial.println("[WAKEWORD] ERROR: Initial loadModelInternal failed!");
    return false;
  }

  // P2: Task runs unconditionally; running flag gates listening
  s_wakeWordRunning = wakeWordEnabled;

  // 4. Launch FreeRTOS background task pinned to Core 0 (leaving Core 1 for UI & Face)
  if (!s_wakeWordTaskHandle) {
    BaseType_t res = xTaskCreatePinnedToCore(
        wakeWordTask,
        "wake_word_task",
        6144, // 6 KB stack
        NULL,
        1,    // Priority 1
        &s_wakeWordTaskHandle,
        0     // Core 0
    );

    if (res != pdPASS) {
      Serial.println("[WAKEWORD] ERROR: Failed to create wakeWordTask!");
      return false;
    }
  }

  Serial.printf("[WAKEWORD] Active listener '%s' initialized on Core 0 (cutoff=%.2f, gain=%.1fx, micSlot=%d%s, running=%s)\n",
                wakeWordGetModelName(), s_probability_cutoff, s_gain, getMicSlot(),
                isMicSlotLocked() ? "[locked]" : "", s_wakeWordRunning ? "YES" : "PAUSED");
  return true;
}

// ============================================================
// RESET & TUNING
// ============================================================
static void wakeWordResetLocked() {
  s_current_stride_step = 0;
  s_sliding_index = 0;
  for (size_t k = 0; k < MAX_SLIDING_WINDOW_SIZE; k++) {
    s_sliding_window[k] = 0;
  }
  s_ignore_windows = -MIN_SLICES_BEFORE_DETECTION;
  if (s_frontend_initialized) {
    FrontendReset(&s_frontend_state);
  }
}

void wakeWordReset() {
  if (s_wwMutex) xSemaphoreTake(s_wwMutex, portMAX_DELAY);
  wakeWordResetLocked();
  if (s_wwMutex) xSemaphoreGive(s_wwMutex);
}

void wakeWordStart() {
  s_wakeWordRunning = true;
  wakeWordReset();
}

void wakeWordStop() {
  s_wakeWordRunning = false;
  s_isListening = false;
  wakeWordReset();
}

void wakeWordSetSensitivity(float cutoff) {
  if (cutoff < 0.10f) cutoff = 0.10f;
  if (cutoff > 0.99f) cutoff = 0.99f;
  if (s_wwMutex) xSemaphoreTake(s_wwMutex, portMAX_DELAY);
  s_probability_cutoff = cutoff;
  wakeWordCutoff = cutoff;
  if (s_wwMutex) xSemaphoreGive(s_wwMutex);
  Serial.printf("[WAKEWORD] Sensitivity cutoff updated: %.2f\n", s_probability_cutoff);
}

float wakeWordGetSensitivity() {
  return s_probability_cutoff;
}

bool wakeWordIsEnabled() {
  return wakeWordEnabled;
}

void wakeWordSetEnabled(bool en) {
  wakeWordEnabled = en;
  if (en) {
    wakeWordStart();
  } else {
    wakeWordStop();
  }
}

bool wakeWordSetModel(uint8_t modelIndex) {
  if (modelIndex >= NUM_WAKE_WORD_MODELS) return false;
  if (modelIndex == s_activeModelIndex && s_interpreter != nullptr) {
    return true; // Already loaded
  }

  if (s_wwMutex) xSemaphoreTake(s_wwMutex, portMAX_DELAY);

  bool wasRunning = s_wakeWordRunning;
  s_wakeWordRunning = false;
  s_isListening = false;

  float savedCutoff = wakeWordCutoff;
  bool ok = loadModelInternal(modelIndex, savedCutoff);
  if (ok) {
    wakeWordModel = modelIndex;
  }

  s_wakeWordRunning = wasRunning && wakeWordEnabled;

  if (s_wwMutex) xSemaphoreGive(s_wwMutex);

  return ok;
}

uint8_t wakeWordGetModel() {
  return s_activeModelIndex;
}

const char* wakeWordGetModelName() {
  if (s_activeModelIndex < NUM_WAKE_WORD_MODELS) {
    return WAKE_WORD_MODELS[s_activeModelIndex].name;
  }
  return "Unknown";
}

bool wakeWordIsListening() {
  return s_isListening;
}

void wakeWordSetGain(float gain) {
  if (gain < 0.5f) gain = 0.5f;
  if (gain > 20.0f) gain = 20.0f;
  s_gain = gain;
  wakeWordGain = gain;
  Serial.printf("[WAKEWORD] Input gain updated: %.2fx (soft-knee %s)\n",
                s_gain, (s_gain == 1.0f) ? "BYPASS" : "ACTIVE");
}

float wakeWordGetGain() {
  return s_gain;
}

void wakeWordSetSpikeTrigger(bool en) {
  s_enable_spike_trigger = en;
  wakeWordSpikeTrigger = en;
  Serial.printf("[WAKEWORD] Secondary spike trigger: %s\n", en ? "ENABLED" : "DISABLED (pure avg)");
}

bool wakeWordGetSpikeTrigger() {
  return s_enable_spike_trigger;
}

void wakeWordSetBargeInBump(float bump) {
  if (bump < 0.0f) bump = 0.0f;
  if (bump > 0.40f) bump = 0.40f;
  s_bargeInBump = bump;
  wakeWordBargeInBump = bump;
  Serial.printf("[WAKEWORD] Barge-in bump updated: +%.2f\n", s_bargeInBump);
}

float wakeWordGetBargeInBump() {
  return s_bargeInBump;
}

void wakeWordPrintStatus() {
  const WakeWordModelEntry &entry = WAKE_WORD_MODELS[s_activeModelIndex];
  Serial.println("================= WAKE WORD ENGINE STATUS =================");
  Serial.printf("  Active Model    : [%u] %s\n", s_activeModelIndex, entry.name);
  Serial.printf("  Running / Listen: %s / %s\n",
                s_wakeWordRunning ? "YES" : "NO",
                s_isListening ? "YES" : "NO");
  Serial.printf("  Sensitivity     : active=%.2f, saved=%.2f (manifest def=%.2f, min=%.2f, max=%.2f)\n",
                s_probability_cutoff, wakeWordCutoff, entry.defaultCutoff, entry.minCutoff, entry.maxCutoff);
  Serial.printf("  Window Size     : %zu slices\n", s_sliding_window_size);
  Serial.printf("  Input Gain      : %.2fx (%s)\n", s_gain, (s_gain == 1.0f) ? "Bypass Soft-Knee" : "Soft-Knee Active");
  Serial.printf("  Spike Clause    : %s\n", s_enable_spike_trigger ? "ON (legacy fallback)" : "OFF (pure average trigger)");
  Serial.printf("  Barge-In Bump   : +%.2f (effective in speech = %.2f)\n", s_bargeInBump, s_probability_cutoff + s_bargeInBump);
  Serial.printf("  Mic Slot        : %d (%s, lock=%s)\n",
                getMicSlot(), (getMicSlot() == 1) ? "Right" : "Left",
                isMicSlotLocked() ? "LOCKED" : "auto-detect");
  Serial.println("===========================================================");
}

void wakeWordProcessSerialCommand(const String &cmd) {
  String c = cmd;
  c.trim();
  if (c.equalsIgnoreCase("ww status") || c.equalsIgnoreCase("ww")) {
    wakeWordPrintStatus();
    return;
  }

  if (c.startsWith("ww gain ") || c.startsWith("ww_gain ")) {
    float g = c.substring(c.indexOf(' ', 3) + 1).toFloat();
    wakeWordSetGain(g);
    Preferences p;
    p.begin("myrobot", false);
    p.putFloat("ww_gain", wakeWordGain);
    p.end();
    return;
  }

  if (c.startsWith("ww cutoff ") || c.startsWith("ww_cutoff ")) {
    float co = c.substring(c.indexOf(' ', 3) + 1).toFloat();
    wakeWordSetSensitivity(co);
    Preferences p;
    p.begin("myrobot", false);
    p.putFloat("ww_cutoff", wakeWordCutoff);
    p.end();
    return;
  }

  if (c.startsWith("ww model ") || c.startsWith("ww_model ")) {
    uint8_t mIdx = 0; // Alexa
    if (wakeWordSetModel(mIdx)) {
      Preferences p;
      p.begin("myrobot", false);
      p.putUChar("ww_model", wakeWordModel);
      p.putFloat("ww_cutoff", wakeWordCutoff);
      p.end();
      Serial.printf("[WAKEWORD] Model changed via serial to %s (cutoff=%.2f)\n",
                    wakeWordGetModelName(), wakeWordCutoff);
    } else {
      Serial.println("[WAKEWORD] Failed to change model via serial!");
    }
    return;
  }

  if (c.startsWith("ww spike ") || c.startsWith("ww_spike ")) {
    String val = c.substring(c.indexOf(' ', 3) + 1);
    val.trim();
    bool sp = (val == "1" || val.equalsIgnoreCase("true") || val.equalsIgnoreCase("on"));
    wakeWordSetSpikeTrigger(sp);
    Preferences p;
    p.begin("myrobot", false);
    p.putBool("ww_spike", wakeWordSpikeTrigger);
    p.end();
    return;
  }

  if (c.startsWith("ww bump ") || c.startsWith("ww_bump ")) {
    float b = c.substring(c.indexOf(' ', 3) + 1).toFloat();
    wakeWordSetBargeInBump(b);
    Preferences p;
    p.begin("myrobot", false);
    p.putFloat("ww_bargein_bump", wakeWordBargeInBump);
    p.end();
    return;
  }

  if (c.startsWith("ww mic ") || c.startsWith("ww_mic ")) {
    int s = c.substring(c.indexOf(' ', 3) + 1).toInt();
    setMicSlot(s, true, true);
    return;
  }

  if (c.startsWith("ww lock ") || c.startsWith("ww_lock ")) {
    String lkStr = c.substring(c.indexOf(' ', 3) + 1);
    lkStr.trim();
    bool lk = (lkStr == "1" || lkStr.equalsIgnoreCase("true") || lkStr.equalsIgnoreCase("on"));
    setMicSlotLocked(lk);
    Preferences p;
    p.begin("myrobot", false);
    p.putBool("micslot_lock", lk);
    p.end();
    Serial.printf("[WAKEWORD] Mic slot lock set to: %s\n", lk ? "LOCKED" : "UNLOCKED");
    return;
  }

  if (c.startsWith("ww ")) {
    Serial.println("[WAKEWORD] Unknown command. Available commands:");
    Serial.println("  ww status");
    Serial.println("  ww gain <float>       (e.g. ww gain 1.0 or ww gain 8.0)");
    Serial.println("  ww cutoff <float>     (e.g. ww cutoff 0.36)");
    Serial.println("  ww model <0>          (0=Alexa)");
    Serial.println("  ww spike <0|1>        (0=disable spike trigger, 1=enable)");
    Serial.println("  ww bump <float>       (barge-in bump, e.g. ww bump 0.08)");
    Serial.println("  ww mic <0|1>          (0=Left, 1=Right)");
    Serial.println("  ww lock <0|1>         (lock mic slot to prevent auto-detection)");
  }
}

// ============================================================
// BACKGROUND WORKER TASK (RUNS ON CORE 0)
// ============================================================
static void wakeWordTask(void *params) {
  Serial.println("[WAKEWORD] Task running on Core " + String(xPortGetCoreID()));

  static int32_t raw_i2s[STEP_SAMPLES * 2]; // 160 frames stereo = 320 x int32_t
  static int16_t pcm_samples[STEP_SAMPLES]; // 160 samples mono = 320 bytes

  uint32_t lastDiagAt = millis();
  float maxProbDiag = 0.0f;
  int16_t maxPeakDiag = 0;
  uint8_t slotMismatchCount = 0;

  for (;;) {
    bool isSpeaking = isSpeechPlaying() || currentEmotion == EMOTION_SPEAKING;
    // Don't listen during STT recording or LLM thinking, or during short tone beeps
    bool isSttOrThinking = (busyProcessing && !isSpeaking) || startRecording || isTonePlaying();

    // Listen when robot is idle OR when Kiwi is actively speaking (for Barge-in)
    bool canListen = s_wakeWordRunning && wakeWordEnabled &&
                     (s_interpreter != nullptr) &&
                     !isSttOrThinking &&
                     !manualSleep && !inCircadianSleep &&
                     rxHandle != nullptr;

    if (!canListen) {
      if (s_isListening) {
        s_isListening = false;
        wakeWordReset();
      }
      vTaskDelay(pdMS_TO_TICKS(50));
      continue;
    }

    if (!s_isListening) {
      s_isListening = true;
      wakeWordReset();
      lastDiagAt = millis();
    }

    // Read 10ms of audio (160 samples at 16kHz) from INMP441 microphone
    size_t bytesRead = 0;
    esp_err_t err = i2s_channel_read(rxHandle, raw_i2s, sizeof(raw_i2s), &bytesRead, pdMS_TO_TICKS(100));
    if (err != ESP_OK || bytesRead == 0) {
      vTaskDelay(pdMS_TO_TICKS(5));
      continue;
    }

    int frames = bytesRead / (2 * sizeof(int32_t));
    if (frames != (int)STEP_SAMPLES) {
      continue;
    }

    // Measure energy on both slots and extract mono PCM samples
    int16_t peakL = 0;
    int16_t peakR = 0;
    int currentSlot = getMicSlot();

    for (int i = 0; i < frames; i++) {
      int32_t s32L = raw_i2s[i * 2];
      int32_t s32R = raw_i2s[i * 2 + 1];
      int16_t sampleL = abs((int16_t)(s32L >> 16));
      int16_t sampleR = abs((int16_t)(s32R >> 16));
      if (sampleL > peakL) peakL = sampleL;
      if (sampleR > peakR) peakR = sampleR;

      int32_t s32 = (currentSlot == 1) ? s32R : s32L;
      float x = (float)(s32 >> 8) / 256.0f;
      float y = x - s_prevIn + 0.995f * s_prevOut;
      s_prevIn = x;
      s_prevOut = y;

      // F1: Runtime Gain tuning (default 8.0x) with soft knee compression; bypass soft-knee when 1.0x
      if (s_gain != 1.0f) {
        y *= s_gain;
        if (y > 26000.0f) {
          y = 26000.0f + (y - 26000.0f) * 0.22f;
        } else if (y < -26000.0f) {
          y = -26000.0f + (y + 26000.0f) * 0.22f;
        }
      }
      if (y > 32767.0f) y = 32767.0f;
      else if (y < -32768.0f) y = -32768.0f;
      pcm_samples[i] = (int16_t)y;
    }

    // F3: Auto-swap mic slot respects micSlotLocked; routes through setMicSlot(slot, false)
    if (!isMicSlotLocked()) {
      if (currentSlot == 0 && peakR > 500 && peakL < 80) {
        if (++slotMismatchCount > 30) {
          setMicSlot(1, false);
          slotMismatchCount = 0;
          Serial.println("[WAKEWORD] Auto-switched to micSlot 1 (Right channel active, not persisted)");
        }
      } else if (currentSlot == 1 && peakL > 500 && peakR < 80) {
        if (++slotMismatchCount > 30) {
          setMicSlot(0, false);
          slotMismatchCount = 0;
          Serial.println("[WAKEWORD] Auto-switched to micSlot 0 (Left channel active, not persisted)");
        }
      } else {
        slotMismatchCount = 0;
      }
    } else {
      slotMismatchCount = 0;
    }

    int16_t currentPeak = (getMicSlot() == 1) ? peakR : peakL;
    if (currentPeak > maxPeakDiag) maxPeakDiag = currentPeak;

    if (soundReactiveEnabled) {
      soundLevelFeedSamples(pcm_samples, frames);
    }

    // F3.e: Process all `frames` samples by advancing pcm pointer and remaining count
    int16_t *pcm_ptr = pcm_samples;
    size_t samples_remaining = frames;
    uint8_t zeroReadCounter = 0;

    // P3: Acquire mutex around frontend processing and model inference
    if (s_wwMutex && xSemaphoreTake(s_wwMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
      const WakeWordModelEntry &activeEntry = WAKE_WORD_MODELS[s_activeModelIndex];

      while (samples_remaining > 0 && s_interpreter != nullptr) {
        size_t num_samples_read = 0;
        struct FrontendOutput frontend_output = FrontendProcessSamples(
            &s_frontend_state, pcm_ptr, samples_remaining, &num_samples_read);

        if (num_samples_read == 0) {
          zeroReadCounter++;
          if (zeroReadCounter > 5) {
            static uint32_t lastZeroReadWarn = 0;
            if (millis() - lastZeroReadWarn > 5000) {
              lastZeroReadWarn = millis();
              Serial.println("[WAKEWORD] WARN: FrontendProcessSamples read 0 samples repeatedly");
            }
            break; // Avoid infinite loop
          }
        } else {
          zeroReadCounter = 0;
          pcm_ptr += num_samples_read;
          if (num_samples_read <= samples_remaining) {
            samples_remaining -= num_samples_read;
          } else {
            samples_remaining = 0;
          }
        }

        if (frontend_output.values != nullptr && frontend_output.size == PREPROCESSOR_FEATURE_SIZE) {
          int8_t features[PREPROCESSOR_FEATURE_SIZE];
          constexpr int32_t value_scale = 256;
          constexpr int32_t value_div = 666; // 25.6 * 26.0 rounded
          for (size_t i = 0; i < frontend_output.size; ++i) {
            int32_t value = ((frontend_output.values[i] * value_scale) + (value_div / 2)) / value_div;
            value -= 128;
            if (value < -128) value = -128;
            if (value > 127) value = 127;
            features[i] = (int8_t)value;
          }

          // Stream into TFLite model input tensor
          TfLiteTensor *input = s_interpreter->input(0);
          uint8_t stride = input->dims->data[1];
          std::memmove((int8_t *)(tflite::GetTensorData<int8_t>(input)) +
                       PREPROCESSOR_FEATURE_SIZE * s_current_stride_step,
                       features, PREPROCESSOR_FEATURE_SIZE);
          s_current_stride_step++;

          if (s_current_stride_step >= stride) {
            s_current_stride_step = 0;
            if (s_interpreter->Invoke() == kTfLiteOk) {
              TfLiteTensor *output = s_interpreter->output(0);
              uint8_t prob_u8 = output->data.uint8[0];
              float prob_raw = (float)prob_u8 / 255.0f;

              s_sliding_window[s_sliding_index] = prob_u8;
              s_sliding_index = (s_sliding_index + 1) % s_sliding_window_size;

              if (s_ignore_windows < 0) {
                s_ignore_windows++;
              } else {
                uint32_t sum = 0;
                for (size_t k = 0; k < s_sliding_window_size; k++) {
                  sum += s_sliding_window[k];
                }
                float avg_prob = (float)sum / (255.0f * (float)s_sliding_window_size);
                if (avg_prob > maxProbDiag) maxProbDiag = avg_prob;

                float effectiveCutoff = isSpeaking ? (s_probability_cutoff + s_bargeInBump) : s_probability_cutoff;

                // Log voice activity spike if either probability is notable
                if (avg_prob > 0.15f || prob_raw > 0.25f) {
                  Serial.printf("[WAKEWORD] Voice spike%s: raw=%.2f, avg=%.2f (cutoff=%.2f, gain=%.1f)\n",
                                isSpeaking ? " [barge-in]" : "", prob_raw, avg_prob, effectiveCutoff, s_gain);
                }

                // F2: Honor the cutoff in trigger logic (pure average by default; spike clause behind flag)
                bool triggered = false;
                if (isSpeaking) {
                  if (s_enable_spike_trigger) {
                    triggered = (avg_prob >= effectiveCutoff && prob_raw >= 0.45f) ||
                                (prob_raw >= 0.60f && avg_prob >= 0.22f);
                  } else {
                    triggered = (avg_prob >= effectiveCutoff);
                  }
                } else {
                  if (s_enable_spike_trigger) {
                    if (activeEntry.isHighCutoffProfile) {
                      triggered = (avg_prob >= effectiveCutoff) ||
                                  (prob_raw >= (effectiveCutoff + 0.05f) && avg_prob >= (effectiveCutoff - 0.15f));
                    } else {
                      triggered = (avg_prob >= effectiveCutoff) ||
                                  (prob_raw >= 0.45f && avg_prob >= 0.16f);
                    }
                  } else {
                    triggered = (avg_prob >= effectiveCutoff);
                  }
                }

                if (triggered) {
                  if (isSpeaking) {
                    Serial.printf("\n[WAKEWORD] *** Voice Barge-In DETECTED during speech! avg=%.3f, raw=%.3f (cutoff=%.2f, gain=%.1f, slot=%d) ***\n",
                                  avg_prob, prob_raw, effectiveCutoff, s_gain, getMicSlot());
                    cancelRequest = true;
                    bargeInTriggered = true;
                    toneStop();
                    spkAbortSpeech();
                    wakeWordResetLocked();
                    s_isListening = false;
                    powerNotifyActivity();
                    setEmotion(EMOTION_LISTENING);
                  } else {
                    Serial.printf("\n[WAKEWORD] *** '%s' DETECTED! avg=%.3f, raw=%.3f (cutoff=%.2f, gain=%.1f, slot=%d) ***\n",
                                  activeEntry.name, avg_prob, prob_raw, s_probability_cutoff, s_gain, getMicSlot());

                    // Reset buffers & ignore future triggers until next conversation finishes
                    wakeWordResetLocked();
                    s_isListening = false;

                    // Trigger speech recording & listening face immediately
                    powerNotifyActivity();
                    showClockScreen = false;
                    showWeatherScreen = false;
                    setEmotion(EMOTION_LISTENING);
                    startRecording = true;
                  }
                  break; // Exit while loop for this frame block
                }
              }
            }
          }
        }
      }
      xSemaphoreGive(s_wwMutex);
    }

    // Periodic heartbeat log every 3 seconds
    if (millis() - lastDiagAt >= 3000) {
      lastDiagAt = millis();
      float effectiveCutoff = isSpeaking ? (s_probability_cutoff + s_bargeInBump) : s_probability_cutoff;
      Serial.printf("[WAKEWORD] Listening '%s'%s... peak=%d, prob_max=%.2f, cutoff=%.2f (saved=%.2f), gain=%.1f, slot=%d%s\n",
                    wakeWordGetModelName(),
                    isSpeaking ? " [barge-in]" : "",
                    maxPeakDiag, maxProbDiag,
                    effectiveCutoff,
                    wakeWordCutoff,
                    s_gain,
                    getMicSlot(),
                    isMicSlotLocked() ? " [lock]" : "");
      maxProbDiag = 0.0f;
      maxPeakDiag = 0;
    }

    // Give FreeRTOS IDLE0 task a 1ms window to reset Task Watchdog (TWDT)
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}
