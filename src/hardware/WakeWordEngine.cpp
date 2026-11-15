#include "hardware/WakeWordEngine.h"
#include "hardware/AudioEngine.h"
#include "hardware/SoundLevel.h"
#include "hardware/hey_jarvis_model.h"
#include "hardware/hey_edith_model.h"
#include "hardware/alexa_model.h"
#include "config/AppConfig.h"
#include "face/FaceEngine.h"
#include "power/PowerManager.h"

#include <frontend.h>
#include <frontend_util.h>

#include <tensorflow/lite/core/c/common.h>
#include <tensorflow/lite/micro/micro_interpreter.h>
#include <tensorflow/lite/micro/micro_mutable_op_resolver.h>
#include <tensorflow/lite/micro/micro_resource_variable.h>
#include <tensorflow/lite/micro/micro_allocator.h>
#include <tensorflow/lite/schema/schema_generated.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

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
static const size_t TENSOR_ARENA_SIZE = 36000;
static const size_t VARIABLE_ARENA_SIZE = 1024;

// Multi-Model Definitions
struct WakeWordModelEntry {
  const char *id;
  const char *name;
  const unsigned char *data;
  size_t length;
  float defaultCutoff;
  size_t windowSize;
};

static const WakeWordModelEntry WAKE_WORD_MODELS[] = {
  { "hey_jarvis", "Hey Jarvis", hey_jarvis_model, sizeof(hey_jarvis_model), 0.26f, 3 },
  { "hey_edith",  "Hey Edith",  hey_edith_model,  sizeof(hey_edith_model),  0.30f, 3 },
  { "alexa",      "Alexa",      alexa_model,      sizeof(alexa_model),      0.82f, 5 }
};
static const size_t NUM_WAKE_WORD_MODELS = sizeof(WAKE_WORD_MODELS) / sizeof(WAKE_WORD_MODELS[0]);
static uint8_t s_activeModelIndex = 0;
static size_t s_sliding_window_size = 3;

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
static float s_probability_cutoff = 0.26f;

// High-pass DC offset filter state for audio input
static float s_prevIn = 0.0f;
static float s_prevOut = 0.0f;

// Forward declarations
static void wakeWordTask(void *params);
static bool registerOps(tflite::MicroMutableOpResolver<20> &resolver);
static bool loadModelInternal(uint8_t modelIndex);

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
// MODEL LOADER & RUNTIME SWITCHING
// ============================================================
static bool loadModelInternal(uint8_t modelIndex) {
  if (modelIndex >= NUM_WAKE_WORD_MODELS) {
    modelIndex = 0;
  }
  s_activeModelIndex = modelIndex;
  const WakeWordModelEntry &entry = WAKE_WORD_MODELS[modelIndex];

  Serial.printf("[WAKEWORD] Loading model '%s' (%zu bytes)...\n", entry.name, entry.length);

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
    return false;
  }
  s_micro_resource_vars = tflite::MicroResourceVariables::Create(s_micro_allocator, 20);
  if (!s_micro_resource_vars) {
    Serial.println("[WAKEWORD] ERROR: Failed to create MicroResourceVariables!");
    return false;
  }

  const tflite::Model *model = tflite::GetModel(entry.data);
  if (model->version() != TFLITE_SCHEMA_VERSION) {
    Serial.printf("[WAKEWORD] ERROR: Model schema version mismatch (%lu vs %d)\n",
                  model->version(), TFLITE_SCHEMA_VERSION);
    return false;
  }

  s_interpreter = new tflite::MicroInterpreter(
      model, *s_op_resolver, s_tensor_arena, TENSOR_ARENA_SIZE, s_micro_resource_vars);
  if (!s_interpreter) {
    Serial.println("[WAKEWORD] ERROR: Failed to create MicroInterpreter!");
    return false;
  }

  if (s_interpreter->AllocateTensors() != kTfLiteOk) {
    Serial.println("[WAKEWORD] ERROR: AllocateTensors() failed!");
    return false;
  }

  s_sliding_window_size = entry.windowSize;
  s_probability_cutoff = entry.defaultCutoff;
  wakeWordCutoff = entry.defaultCutoff;

  TfLiteTensor *input = s_interpreter->input(0);
  TfLiteTensor *output = s_interpreter->output(0);
  Serial.printf("[WAKEWORD] Model '%s' ready! In: [%d,%d,%d] (type %d), Out: [%d,%d] (type %d), Cutoff: %.2f\n",
                entry.name,
                input->dims->data[0], input->dims->data[1], input->dims->data[2], input->type,
                output->dims->data[0], output->dims->data[1], output->type,
                s_probability_cutoff);
  Serial.printf("[WAKEWORD] Arena used: %u / %u bytes\n",
                (unsigned int)s_interpreter->arena_used_bytes(), (unsigned int)TENSOR_ARENA_SIZE);

  wakeWordReset();
  return true;
}

// ============================================================
// INITIALIZATION
// ============================================================
bool wakeWordInit() {
  Serial.println("[WAKEWORD] Initializing microWakeWord engine...");

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

  // 3. Load active model from AppConfig
  if (!loadModelInternal(wakeWordModel)) {
    Serial.println("[WAKEWORD] ERROR: Initial loadModelInternal failed!");
    return false;
  }

  // If saved custom cutoff matches model profile, use it
  if (wakeWordCutoff >= 0.10f && wakeWordCutoff <= 0.99f) {
    bool rangeMatch = (wakeWordModel == 2) ? (wakeWordCutoff > 0.50f) : (wakeWordCutoff <= 0.60f);
    if (rangeMatch) {
      s_probability_cutoff = wakeWordCutoff;
    }
  }

  wakeWordReset();
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

  Serial.printf("[WAKEWORD] Active listener '%s' started on Core 0 (cutoff=%.2f)!\n",
                wakeWordGetModelName(), s_probability_cutoff);
  return true;
}

// ============================================================
// RESET & TUNING
// ============================================================
void wakeWordReset() {
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
  s_probability_cutoff = cutoff;
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

  bool wasRunning = s_wakeWordRunning;
  s_wakeWordRunning = false;
  s_isListening = false;
  vTaskDelay(pdMS_TO_TICKS(60));

  bool ok = loadModelInternal(modelIndex);
  wakeWordModel = modelIndex;

  s_wakeWordRunning = wasRunning && wakeWordEnabled;
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
    for (int i = 0; i < frames; i++) {
      int32_t s32L = raw_i2s[i * 2];
      int32_t s32R = raw_i2s[i * 2 + 1];
      int16_t sampleL = abs((int16_t)(s32L >> 16));
      int16_t sampleR = abs((int16_t)(s32R >> 16));
      if (sampleL > peakL) peakL = sampleL;
      if (sampleR > peakR) peakR = sampleR;

      int32_t s32 = (micSlot == 1) ? s32R : s32L;
      float x = (float)(s32 >> 8) / 256.0f;
      float y = x - s_prevIn + 0.995f * s_prevOut;
      s_prevIn = x;
      s_prevOut = y;

      // Far-field wake word gain boost (8.0x) with soft knee compression
      y *= 8.0f;
      if (y > 26000.0f) {
        y = 26000.0f + (y - 26000.0f) * 0.22f;
      } else if (y < -26000.0f) {
        y = -26000.0f + (y + 26000.0f) * 0.22f;
      }
      if (y > 32767.0f) y = 32767.0f;
      else if (y < -32768.0f) y = -32768.0f;
      pcm_samples[i] = (int16_t)y;
    }

    // Auto-swap mic slot if the active slot is silent but the other slot has live audio
    if (micSlot == 0 && peakR > 500 && peakL < 80) {
      if (++slotMismatchCount > 30) {
        micSlot = 1;
        slotMismatchCount = 0;
        Serial.println("[WAKEWORD] Auto-switched to micSlot 1 (Right channel active)");
      }
    } else if (micSlot == 1 && peakL > 500 && peakR < 80) {
      if (++slotMismatchCount > 30) {
        micSlot = 0;
        slotMismatchCount = 0;
        Serial.println("[WAKEWORD] Auto-switched to micSlot 0 (Left channel active)");
      }
    } else {
      slotMismatchCount = 0;
    }

    int16_t currentPeak = (micSlot == 1) ? peakR : peakL;
    if (currentPeak > maxPeakDiag) maxPeakDiag = currentPeak;

    if (soundReactiveEnabled) {
      soundLevelFeedSamples(pcm_samples, frames);
    }

    // Generate 40 Mel-filterbank features using micro frontend
    size_t num_samples_read = 0;
    struct FrontendOutput frontend_output = FrontendProcessSamples(
        &s_frontend_state, pcm_samples, frames, &num_samples_read);

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

            float effectiveCutoff = isSpeaking ? (s_probability_cutoff + 0.08f) : s_probability_cutoff;

            // Log keyword spikes so user can see it react to their voice
            if (avg_prob > 0.15f || prob_raw > 0.25f) {
              Serial.printf("[WAKEWORD] Voice spike%s: raw=%.2f, avg=%.2f (cutoff=%.2f)\n",
                            isSpeaking ? " [barge-in]" : "", prob_raw, avg_prob, effectiveCutoff);
            }

            // Trigger on running window average OR sharp voice spike with positive context
            bool triggered = false;
            if (isSpeaking) {
              // During speech playback: require stronger voice spike over Kiwi's speaker audio
              triggered = (avg_prob >= effectiveCutoff && prob_raw >= 0.45f) ||
                          (prob_raw >= 0.60f && avg_prob >= 0.22f);
            } else {
              // Normal idle listening: tuned for effortless 1-3 meter far-field pickup
              if (s_probability_cutoff >= 0.60f) {
                // High-cutoff models (Alexa)
                triggered = (avg_prob >= s_probability_cutoff) ||
                            (prob_raw >= (s_probability_cutoff + 0.05f) && avg_prob >= (s_probability_cutoff - 0.15f));
              } else {
                // Low-cutoff models (Jarvis, Edith)
                triggered = (avg_prob >= s_probability_cutoff) ||
                            (prob_raw >= 0.45f && avg_prob >= 0.16f);
              }
            }

            if (triggered) {
              if (isSpeaking) {
                Serial.printf("\n[WAKEWORD] *** Voice Barge-In DETECTED during speech! avg=%.3f, raw=%.3f ***\n",
                              avg_prob, prob_raw);
                cancelRequest = true;
                bargeInTriggered = true;
                toneStop();
                spkAbortSpeech();
                wakeWordReset();
                s_isListening = false;
                powerNotifyActivity();
                setEmotion(EMOTION_LISTENING);
              } else {
                Serial.printf("\n[WAKEWORD] *** '%s' DETECTED! avg=%.3f, raw=%.3f (cutoff=%.2f) ***\n",
                              wakeWordGetModelName(), avg_prob, prob_raw, s_probability_cutoff);

                // Reset buffers & ignore future triggers until next conversation finishes
                wakeWordReset();
                s_isListening = false;

                // Trigger speech recording & listening face immediately
                powerNotifyActivity();
                showClockScreen = false;
                showWeatherScreen = false;
                setEmotion(EMOTION_LISTENING);
                startRecording = true;
              }
            }
          }
        }
      }
    }

    // Periodic heartbeat log every 3 seconds
    if (millis() - lastDiagAt >= 3000) {
      lastDiagAt = millis();
      Serial.printf("[WAKEWORD] Listening '%s'%s... peak=%d, prob_max=%.2f, cutoff=%.2f, slot=%d\n",
                    wakeWordGetModelName(),
                    isSpeaking ? " [barge-in]" : "",
                    maxPeakDiag, maxProbDiag, isSpeaking ? (s_probability_cutoff + 0.08f) : s_probability_cutoff, micSlot);
      maxProbDiag = 0.0f;
      maxPeakDiag = 0;
    }

    // Give FreeRTOS IDLE0 task a 1ms window to reset Task Watchdog (TWDT)
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

