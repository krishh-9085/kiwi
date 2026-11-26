#include "power/PowerManager.h"
#include "behavior/CircadianEngine.h"
#include "commands/ActionExecutor.h"
#include "config/AppConfig.h"
#include "config/ConfigManager.h"
#include "face/AnimationEngine.h"
#include "face/BootWakeAnim.h"
#include "face/FaceDisplay.h"
#include "face/FaceEngine.h"
#include "face/SleepyWakeAnim.h"
#include "hardware/AudioEngine.h"
#include "hardware/RgbLed.h"
#include "hardware/SoundLevel.h"
#include "hardware/TouchController.h"
#include "hardware/WakeWordEngine.h"
#include "net/WifiManager.h"
#include "web/WebPortal.h"
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFi.h>
#include <driver/gpio.h>
#include <esp_private/esp_clk.h>
#include <esp_rtc_time.h>
#include <esp_sleep.h>
#include <sys/time.h>
#include <time.h>

// ============================================================
// RTC PERSISTENT MEMORY (survives deep sleep)
// ============================================================
#define POWER_RTC_MAGIC 0x4B495749 // "KIWI"

RTC_DATA_ATTR uint32_t rtcMagic = 0;
RTC_DATA_ATTR uint8_t rtcSleepMode = 0;        // 0 = none, 1 = circadian deep, 2 = manual deep
RTC_DATA_ATTR uint64_t rtcEpochAtSleepUs = 0;   // epoch time in microseconds, if timeSynced
RTC_DATA_ATTR uint64_t rtcRtcTimeAtSleepUs = 0; // esp_rtc_get_time_us() at sleep start

// ============================================================
// MODULE STATE
// ============================================================
bool suppressBootGreeting = false;
bool skipBootWakeAnim = false;
bool wakeFromDeepSleepWakeAnim = false;

static uint32_t currentCpuFreq = 240;
static uint32_t lastActivityAt = 0;
static uint32_t manualSleepElapsedSec = 0;
static bool isOledDimmed = false;
static volatile bool s_inHardwareSleep = false;

bool powerIsHardwareSleeping() { return s_inHardwareSleep; }

// ============================================================
// SLEEP REQUEST GATE (Hardware Eco Deep/Light Sleep Only)
// ============================================================
bool powerIsSleepRequested() {
  if (!bootCompleted)
    return false;
  if (!ecoSleepEnabled)
    return false;
  if (!manualSleep && !inCircadianSleep)
    return false;
  if (busyProcessing || startRecording)
    return false;
  if (isRobotTimerActive() || isRobotTimerRinging())
    return false;
  if (otaInProgress)
    return false;
  if (kiwiIsOutputting() || isTonePlaying())
    return false;
  if (sleepyFallAsleepIsActive() || sleepyWakeIsActive())
    return false;
  if (showClockScreen || showWeatherScreen || bannerActive)
    return false;
  if (isCircadianTempWakeActive())
    return false;
  if (digitalRead(PIN_TOUCH_TOP) == HIGH)
    return false;
  uint32_t now = millis();
  if (now - lastWebRequestAt < 10000)
    return false;
  if (now - lastUserInteractionAt < 5000)
    return false;
  return true;
}

// ============================================================
// SLEEP EXECUTION (Light Sleep & Deep Sleep)
// ============================================================
static void enterLightSleep(uint64_t wakeUs, const char *reason) {
  s_inHardwareSleep = true;
  powerSetCpuFreq(160);
  setDisplayPower(false);
  rgbLedShow(0, 0, 0);
  audioSleepPrepare();

  WiFi.setSleep(true);

  Serial.printf("[POWER] Entering LIGHT sleep, reason=%s, wake_in=%llu s\n",
                reason, (unsigned long long)(wakeUs / 1000000ULL));
  Serial.flush();

  gpio_wakeup_enable((gpio_num_t)PIN_TOUCH_TOP, GPIO_INTR_HIGH_LEVEL);
  esp_sleep_enable_gpio_wakeup();
  esp_sleep_enable_timer_wakeup(wakeUs);

  esp_err_t err = esp_light_sleep_start();
  if (err != ESP_OK) {
    Serial.printf("[POWER] esp_light_sleep_start failed: 0x%x\n", err);
  }

  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  Serial.printf("[POWER] Woke from LIGHT sleep, cause=%d\n", (int)cause);

  audioSleepWake();

  if (wifiSSID.length() > 0 && WiFi.status() != WL_CONNECTED) {
    Serial.println("[POWER] WiFi disconnected after light sleep, reconnecting...");
    WiFi.reconnect();
  }

  if (cause == ESP_SLEEP_WAKEUP_GPIO) {
    s_inHardwareSleep = false;
    WiFi.setSleep(false);
    Serial.println("[POWER] Touch wake routine (light sleep): waking briefly");
    wakeKiwiFromSleep(false, CIRCADIAN_TEMP_WAKE_MS, true);
    resyncTouch();
    powerNotifyActivity();
  } else if (cause == ESP_SLEEP_WAKEUP_TIMER) {
    if (manualSleep) {
      manualSleepElapsedSec += (uint32_t)(wakeUs / 1000000ULL);
      Serial.printf("[POWER] Manual light sleep elapsed: %lu s (max=%lu s)\n",
                    (unsigned long)manualSleepElapsedSec,
                    (unsigned long)(MANUAL_LIGHT_SLEEP_MAX_MIN * 60));
    }
    s_inHardwareSleep = false;
    WiFi.setSleep(false);
  } else {
    s_inHardwareSleep = false;
    WiFi.setSleep(false);
  }
}

static void enterDeepSleep(uint8_t mode, uint64_t wakeUs, const char *reason) {
  s_inHardwareSleep = true;
  saveConfigImmediate();
  powerSetCpuFreq(160);
  setDisplayPower(false);
  rgbLedShow(0, 0, 0);
  audioSleepPrepare();

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  dnsServer.stop();
  server.stop();

  rtcMagic = POWER_RTC_MAGIC;
  rtcSleepMode = mode;
  if (timeSynced) {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    rtcEpochAtSleepUs = (uint64_t)tv.tv_sec * 1000000ULL + tv.tv_usec;
  } else {
    rtcEpochAtSleepUs = 0;
  }
  rtcRtcTimeAtSleepUs = esp_rtc_get_time_us();

  if (wakeUs > 0) {
    Serial.printf("[POWER] Entering DEEP sleep, reason=%s, wake_in=%llu s\n",
                  reason, (unsigned long long)(wakeUs / 1000000ULL));
    esp_sleep_enable_timer_wakeup(wakeUs);
  } else {
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
    Serial.printf("[POWER] Entering DEEP sleep, reason=%s, wake_in=none\n", reason);
  }
  Serial.flush();

#if defined(ESP_GPIO_WAKEUP_GPIO_HIGH)
  esp_deep_sleep_enable_gpio_wakeup(1ULL << PIN_TOUCH_TOP, ESP_GPIO_WAKEUP_GPIO_HIGH);
#else
  esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_TOUCH_TOP, 1);
#endif
  esp_deep_sleep_start();
}

// ============================================================
// BOOT INITIALIZATION
// ============================================================
void powerInit() {
  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  Serial.printf("[POWER] Booting, wakeup_cause=%d, rtcMagic=0x%08lX\n",
                (int)cause, (unsigned long)rtcMagic);

  // Restore configured timezone early so localtime() matches user's zone
  char tzStr[16];
  int totalMin = tzOffsetMin;
  // POSIX TZ sign is inverted relative to UTC offset (e.g. IST +5:30 -> "UTC-5:30")
  int absMin = abs(totalMin);
  int h = absMin / 60;
  int m = absMin % 60;
  snprintf(tzStr, sizeof(tzStr), "UTC%c%d:%02d", (totalMin >= 0) ? '-' : '+', h, m);
  setenv("TZ", tzStr, 1);
  tzset();

  if (cause != ESP_SLEEP_WAKEUP_UNDEFINED && rtcMagic == POWER_RTC_MAGIC) {
    uint64_t nowRtcUs = esp_rtc_get_time_us();
    uint64_t elapsedUs = (nowRtcUs > rtcRtcTimeAtSleepUs) ? (nowRtcUs - rtcRtcTimeAtSleepUs) : 0;
    if (rtcEpochAtSleepUs > 0) {
      uint64_t newEpochUs = rtcEpochAtSleepUs + elapsedUs;
      struct timeval tv;
      tv.tv_sec = (time_t)(newEpochUs / 1000000ULL);
      tv.tv_usec = (suseconds_t)(newEpochUs % 1000000ULL);
      settimeofday(&tv, nullptr);
      timeSynced = true;
      ntpNeedsRefresh = true;
      Serial.printf("[POWER] Wall-clock time restored from RTC: elapsed=%llu s (NTP refresh queued)\n",
                    (unsigned long long)(elapsedUs / 1000000ULL));
    }

    if (rtcSleepMode == 1) {
      inCircadianSleep = true;
      manualSleep = false;
    } else if (rtcSleepMode == 2) {
      manualSleep = true;
      inCircadianSleep = false;
    }

    if (cause == ESP_SLEEP_WAKEUP_TIMER) {
      bool inWindow = isCircadianScheduleActiveNow();
      if (timeSynced && inWindow && (rtcSleepMode == 1 || rtcSleepMode == 2)) {
        Serial.println("[POWER] Timer woke within circadian sleep window. Returning to deep sleep.");
        int secUntilEnd = computeSecondsUntilScheduleWake();
        if (secUntilEnd > 0) {
          enterDeepSleep(rtcSleepMode, (uint64_t)secUntilEnd * 1000000ULL, "circadian_reenter");
        }
      }
      Serial.println("[POWER] Scheduled sleep window finished (or time unsynced). Waking up.");
      inCircadianSleep = false;
      manualSleep = false;
      rtcSleepMode = 0;
      rtcMagic = 0;
      suppressBootGreeting = !greetingsEnabled;
      skipBootWakeAnim = true;
      bootCompleted = true;
      bootWakeForceDone();
      wakeFromDeepSleepWakeAnim = true;
      circadianTempWakeUntil = millis() + CIRCADIAN_TEMP_WAKE_MS;
    } else if (cause == ESP_SLEEP_WAKEUP_GPIO || cause == ESP_SLEEP_WAKEUP_EXT0) {
      Serial.println("[POWER] Deep sleep wake by touch: Waking up with sleepy wake anim.");
      inCircadianSleep = false;
      manualSleep = false;
      rtcSleepMode = 0;
      rtcMagic = 0;
      suppressBootGreeting = true;
      skipBootWakeAnim = true;
      bootCompleted = true;
      bootWakeForceDone();
      wakeFromDeepSleepWakeAnim = true;
      circadianTempWakeUntil = millis() + CIRCADIAN_TEMP_WAKE_MS;
    }
  } else {
    rtcMagic = 0;
    rtcSleepMode = 0;
    suppressBootGreeting = false;
    skipBootWakeAnim = false;
    circadianTempWakeUntil = millis() + 30000;
  }
  lastActivityAt = millis();
}

// ============================================================
// MAIN LOOP UPDATE
// ============================================================
void powerUpdate() {
  uint32_t now = millis();

  if (wakeFromDeepSleepWakeAnim) {
    wakeFromDeepSleepWakeAnim = false;
    resyncTouch();
    setDisplayPower(true);
    setOledDim(false);
    updateDisplayBrightnessMode();
    sleepyWakeStart();
    Serial.println("[POWER] Starting sleepy wake animation after wake from deep sleep!");
  }

  if (!manualSleep) {
    manualSleepElapsedSec = 0;
  }

  // --- PART A: Eco Hardware Sleep Check & Entry ---
  if (powerIsSleepRequested()) {
    if (manualSleep) {
      if (manualSleepElapsedSec >= (MANUAL_LIGHT_SLEEP_MAX_MIN * 60)) {
        uint64_t wakeUs = 0;
        if (circadianSleepEnabled && timeSynced) {
          int secUntilEnd = computeSecondsUntilScheduleWake();
          if (secUntilEnd > 0) {
            wakeUs = (uint64_t)secUntilEnd * 1000000ULL;
          }
        }
        enterDeepSleep(2, wakeUs, "manual_deep_after_timeout");
      } else {
        enterLightSleep(60000000ULL, "manual_light");
        return;
      }
    } else if (isCircadianScheduleActiveNow()) {
      int secUntilEnd = computeSecondsUntilScheduleWake();
      if (secUntilEnd > 0) {
        enterDeepSleep(1, (uint64_t)secUntilEnd * 1000000ULL, "circadian_schedule");
      }
    } else if (inCircadianSleep && !timeSynced) {
      enterLightSleep(60000000ULL, "circadian_nosync");
      return;
    }
  }

  // --- PART B: Idle OLED Dimming (60 s) ---
  bool faceActivity = ((now - lastUserInteractionAt) < 60000) ||
                      kiwiIsOutputting() || isTonePlaying() ||
                      (soundReactiveEnabled && soundLevelGet() > SOUND_REACT_ACTIVE_LEVEL) ||
                      sleepyWakeIsActive() || sleepyFallAsleepIsActive() ||
                      animationIsActive() || showClockScreen || showWeatherScreen || bannerActive ||
                      (currentEmotion != EMOTION_IDLE && currentEmotion != EMOTION_SLEEPING);

  if (faceActivity) {
    if (isOledDimmed) {
      isOledDimmed = false;
      setOledDim(false);
      if (currentCpuFreq < 160) {
        powerSetCpuFreq(160);
      }
      Serial.println("[POWER] Face activity resumed -> setOledDim(false)");
    }
  } else {
    if (!isOledDimmed && !inCircadianSleep && !manualSleep) {
      isOledDimmed = true;
      setOledDim(true);
      Serial.println("[POWER] Face idle for 60s -> setOledDim(true)");
    }
  }

  // --- PART C: CPU Frequency Scaling (80 MHz / 160 MHz / 240 MHz) ---
  bool needTurbo = busyProcessing || startRecording || isTonePlaying() ||
                   isSpeechPlaying() || otaInProgress;
  bool sleepQuiet = (inCircadianSleep || manualSleep) &&
                    !sleepyFallAsleepIsActive() && !sleepyWakeIsActive() &&
                    !showClockScreen && !showWeatherScreen;
  bool canDropTo80 = (!displayPowerState || isOledDimmed || sleepQuiet) &&
                     !wakeWordIsListening() &&
                     !kiwiIsOutputting() && !isTonePlaying() &&
                     !busyProcessing && (now - lastWebRequestAt >= 2000) &&
                     !isRobotTimerRinging() &&
                     (now - lastActivityAt >= 10000);

  if (needTurbo) {
    if (currentCpuFreq != 240) {
      powerSetCpuFreq(240);
    }
  } else if (canDropTo80) {
    if (currentCpuFreq != 80) {
      powerSetCpuFreq(80);
    }
  } else {
    if (currentCpuFreq != 160) {
      powerSetCpuFreq(160);
    }
  }
}

// ============================================================
// CPU FREQUENCY MANAGEMENT
// ============================================================
void powerNotifyActivity() {
  lastActivityAt = millis();
  if (currentCpuFreq < 160) {
    powerSetCpuFreq(160);
  }
}

void powerSetCpuFreq(uint32_t mhz) {
  if (mhz == currentCpuFreq)
    return;
  if (mhz != 80 && mhz != 160 && mhz != 240)
    return;

  setCpuFrequencyMhz(mhz);
  currentCpuFreq = getCpuFrequencyMhz();
  Serial.printf("[POWER] CPU freq changed to %lu MHz (esp_clk_cpu_freq=%d Hz)\n",
                (unsigned long)currentCpuFreq, esp_clk_cpu_freq());
}

uint32_t powerGetCpuFreq() {
  return currentCpuFreq;
}
