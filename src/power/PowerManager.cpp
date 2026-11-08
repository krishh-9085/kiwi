#include "power/PowerManager.h"
#include "config/AppConfig.h"
#include "config/ConfigManager.h"
#include "hardware/AudioEngine.h"
#include "hardware/TouchController.h"
#include "hardware/SoundLevel.h"
#include "face/FaceDisplay.h"
#include "face/FaceEngine.h"
#include "face/SleepyWakeAnim.h"
#include "animation/AnimationEngine.h"
#include "behavior/ActionExecutor.h"
#include "web/WebPortal.h"
#include <WebServer.h>
#include <DNSServer.h>
#include <esp_sleep.h>
#include <esp_rtc_time.h>
#include <esp_private/esp_clk.h>
#include <driver/gpio.h>
#include <sys/time.h>
#include <time.h>
#include <WiFi.h>

// ============================================================
// RTC PERSISTENT MEMORY (survives deep sleep)
// ============================================================
#define POWER_RTC_MAGIC 0x4B495749 // "KIWI"

RTC_DATA_ATTR uint32_t rtcMagic = 0;
RTC_DATA_ATTR uint8_t  rtcSleepMode = 0;        // 0 = none, 1 = circadian deep, 2 = manual deep
RTC_DATA_ATTR uint64_t rtcEpochAtSleepUs = 0;   // epoch time in microseconds, if timeSynced
RTC_DATA_ATTR uint64_t rtcRtcTimeAtSleepUs = 0; // esp_rtc_get_time_us() at sleep start

// ============================================================
// MODULE STATE
// ============================================================
bool suppressBootGreeting = false;
bool skipBootWakeAnim = false;
bool wakeFromDeepSleepWakeAnim = false;

static uint32_t currentCpuFreq = 160;
static uint32_t lastActivityAt = 0;
static uint32_t manualSleepElapsedSec = 0;
static bool isOledDimmed = false;

// ============================================================
// TIME & CIRCADIAN SCHEDULE HELPERS
// ============================================================
static bool isCircadianWindowActive() {
  if (!circadianSleepEnabled || !timeSynced)
    return false;
  struct tm ti;
  if (!getLocalTime(&ti, 20))
    return false;
  int curHour = ti.tm_hour;
  if (circadianSleepStart > circadianSleepEnd) {
    return (curHour >= circadianSleepStart || curHour < circadianSleepEnd);
  } else {
    return (curHour >= circadianSleepStart && curHour < circadianSleepEnd);
  }
}

static int computeCircadianSleepTimerSeconds() {
  struct tm ti;
  if (!getLocalTime(&ti, 50)) {
    return 60;
  }
  int curSec = ti.tm_hour * 3600 + ti.tm_min * 60 + ti.tm_sec;
  int endSec = circadianSleepEnd * 3600;
  int secUntilEnd = 0;
  if (endSec <= curSec) {
    secUntilEnd = (24 * 3600 - curSec) + endSec;
  } else {
    secUntilEnd = endSec - curSec;
  }
  if (secUntilEnd < 60)
    secUntilEnd = 60;
  if (secUntilEnd > 12 * 3600)
    secUntilEnd = 12 * 3600;
  return secUntilEnd;
}

// ============================================================
// SLEEP REQUEST GATE (A2)
// ============================================================
bool powerIsSleepRequested() {
  if (!manualSleep && !inCircadianSleep)
    return false;
  if (busyProcessing)
    return false;
  if (isRobotTimerActive() || isRobotTimerRinging())
    return false;
  if (otaInProgress)
    return false;
  if (kiwiIsOutputting() || isTonePlaying())
    return false;
  if (sleepyFallAsleepIsActive())
    return false;
  uint32_t now = millis();
  if (circadianTempWakeUntil > now)
    return false;
  if (digitalRead(PIN_TOUCH_TOP) == HIGH)
    return false;
  if (now - lastWebRequestAt < 2000)
    return false;
  return true;
}

// ============================================================
// SLEEP EXECUTION (A4, A5, A6)
// ============================================================

// Light Sleep: Modem sleep, keeps STA connected and WebServer alive.
// Web portal responds after the next wake from light sleep.
static void enterLightSleep(uint64_t wakeUs, const char *reason) {
  powerSetCpuFreq(160);
  setDisplayPower(false);
  audioSleepPrepare();

  WiFi.setSleep(true);

  Serial.printf("[POWER] Entering LIGHT sleep, reason=%s, wake_in=%llu s\n",
                reason, (unsigned long long)(wakeUs / 1000000ULL));
  Serial.println("[POWER] Note: Web portal will respond after the next wake from light sleep.");
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

  // Non-blocking WiFi reconnect if disconnected
  if (wifiSSID.length() > 0 && WiFi.status() != WL_CONNECTED) {
    Serial.println("[POWER] WiFi disconnected after light sleep, calling WiFi.reconnect()...");
    WiFi.reconnect();
  }

  if (cause == ESP_SLEEP_WAKEUP_GPIO) {
    // Touch wake routine (A7)
    Serial.println("[POWER] Touch wake routine (light sleep): display active for 10 s");
    circadianTempWakeUntil = millis() + 10000;
    setDisplayPower(true);
    setOledDim(false);
    updateDisplayBrightnessMode();
    resyncTouch();
    powerNotifyActivity();
  } else if (cause == ESP_SLEEP_WAKEUP_TIMER) {
    if (manualSleep) {
      manualSleepElapsedSec += (uint32_t)(wakeUs / 1000000ULL);
      Serial.printf("[POWER] Manual light sleep elapsed: %lu s (max=%lu s)\n",
                    (unsigned long)manualSleepElapsedSec,
                    (unsigned long)(MANUAL_LIGHT_SLEEP_MAX_MIN * 60));
    }
  }
}

// Deep Sleep: WiFi radio off, WebServer and DNSServer stopped.
// Web portal is completely unreachable during deep sleep.
static void enterDeepSleep(uint8_t mode, uint64_t wakeUs, const char *reason) {
  saveConfigImmediate();
  powerSetCpuFreq(160);
  setDisplayPower(false);
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
  // Using esp_rtc_get_time_us() from esp_rtc_time.h to measure elapsed RTC time
  rtcRtcTimeAtSleepUs = esp_rtc_get_time_us();

  if (wakeUs > 0) {
    Serial.printf("[POWER] Entering DEEP sleep, reason=%s, wake_in=%llu s\n",
                  reason, (unsigned long long)(wakeUs / 1000000ULL));
    esp_sleep_enable_timer_wakeup(wakeUs);
  } else {
    Serial.printf("[POWER] Entering DEEP sleep, reason=%s, wake_in=none\n", reason);
  }
  Serial.println("[POWER] Note: Web portal is UNREACHABLE during deep sleep.");
  Serial.flush();

  esp_deep_sleep_enable_gpio_wakeup(1ULL << PIN_TOUCH_TOP, ESP_GPIO_WAKEUP_GPIO_HIGH);
  esp_deep_sleep_start();
  // Never returns
}

// ============================================================
// BOOT INITIALIZATION (A6, A8)
// ============================================================
void powerInit() {
  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  Serial.printf("[POWER] Booting, wakeup_cause=%d, rtcMagic=0x%08lX\n",
                (int)cause, (unsigned long)rtcMagic);

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
      Serial.printf("[POWER] Wall-clock time restored from RTC: elapsed=%llu s\n",
                    (unsigned long long)(elapsedUs / 1000000ULL));
    }

    if (rtcSleepMode == 1) {
      inCircadianSleep = true;
      manualSleep = false;
    } else if (rtcSleepMode == 2) {
      manualSleep = true;
      inCircadianSleep = true;
    }

    if (cause == ESP_SLEEP_WAKEUP_TIMER) {
      bool inWindow = isCircadianWindowActive();
      if (rtcSleepMode == 1 && (inWindow || !timeSynced)) {
        Serial.printf("[POWER] Timer woke within circadian sleep window (timeSynced=%d). Returning to deep sleep.\n", timeSynced);
        int secUntilEnd = computeCircadianSleepTimerSeconds();
        enterDeepSleep(1, (uint64_t)secUntilEnd * 1000000ULL, "circadian_reenter");
      }
      Serial.println("[POWER] Circadian sleep window finished. Waking up.");
      inCircadianSleep = false;
      manualSleep = false;
      rtcSleepMode = 0;
      rtcMagic = 0;
      suppressBootGreeting = true;
      skipBootWakeAnim = true;
      bootCompleted = true; // Skip boot animation on timer wake
    } else if (cause == ESP_SLEEP_WAKEUP_GPIO) {
      Serial.println("[POWER] Deep sleep wake by touch: Waking up fully and queuing sleepy wake anim.");
      inCircadianSleep = false;
      manualSleep = false;
      rtcSleepMode = 0;
      rtcMagic = 0;
      suppressBootGreeting = true;
      skipBootWakeAnim = true;
      bootCompleted = true; // Skip cold-boot animation on touch wake so sleepyWakeAnim plays in loop()
      wakeFromDeepSleepWakeAnim = true;
    }
  } else {
    rtcMagic = 0;
    rtcSleepMode = 0;
    suppressBootGreeting = false;
    skipBootWakeAnim = false;
    // Cold boot grace period (30s) so turning on at night does not instantly sleep
    circadianTempWakeUntil = millis() + 30000;
  }
  lastActivityAt = millis();
}

// ============================================================
// MAIN LOOP UPDATE (A1, B4, C1, C4)
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
    Serial.println("[POWER] Starting sleepy wake animation after touch wake from deep sleep!");
  }

  if (!manualSleep) {
    manualSleepElapsedSec = 0;
  }

  // --- PART A: Real Sleep Check & Entry ---
  if (powerIsSleepRequested()) {
    if (manualSleep) {
      if (manualSleepElapsedSec >= (MANUAL_LIGHT_SLEEP_MAX_MIN * 60)) {
        enterDeepSleep(2, 0, "manual_deep_after_timeout");
      } else {
        enterLightSleep(60000000ULL, "manual_light");
        return;
      }
    } else if (isCircadianWindowActive()) {
      int secUntilEnd = computeCircadianSleepTimerSeconds();
      enterDeepSleep(1, (uint64_t)secUntilEnd * 1000000ULL, "circadian_schedule");
    } else if (inCircadianSleep && !timeSynced) {
      enterLightSleep(60000000ULL, "circadian_nosync");
      return;
    }
  }

  // --- PART B4: Idle OLED Dimming (60 s) ---
  bool faceActivity = (now - lastUserInteractionAt < 60000) ||
                      kiwiIsOutputting() || isTonePlaying() ||
                      (soundReactiveEnabled && soundLevelGet() > NOISE_GATE_THRESHOLD) ||
                      sleepyWakeIsActive() || sleepyFallAsleepIsActive() ||
                      animationIsActive() || showClockScreen || showWeatherScreen || bannerActive ||
                      (currentEmotion != EMOTION_IDLE && currentEmotion != EMOTION_SLEEPING);

  if (faceActivity) {
    if (isOledDimmed) {
      isOledDimmed = false;
      setOledDim(false);
      powerSetCpuFreq(160);
      Serial.println("[POWER] Face activity resumed -> setOledDim(false)");
    }
  } else {
    if (!isOledDimmed && !inCircadianSleep && !manualSleep) {
      isOledDimmed = true;
      setOledDim(true);
      Serial.println("[POWER] Face idle for 60s -> setOledDim(true)");
    }
  }

  // --- PART C: CPU Frequency Scaling (80 MHz vs 160 MHz) ---
  // Drop to 80 MHz ONLY in this state:
  // - display is off or dimmed AND
  // - no speech, tone, or audio output AND
  // - no network request in progress AND
  // - no robot timer ringing AND
  // - not in deep sleep
  // Wait at least 10 s in idle before switching down.
  bool canDropTo80 = (!displayPowerState || isOledDimmed) &&
                     !kiwiIsOutputting() && !isTonePlaying() &&
                     !busyProcessing && (now - lastWebRequestAt >= 2000) &&
                     !isRobotTimerRinging() &&
                     (now - lastActivityAt >= 10000);

  if (canDropTo80) {
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
// CPU FREQUENCY MANAGEMENT (C1, C2, C3)
// ============================================================
void powerNotifyActivity() {
  lastActivityAt = millis();
  if (currentCpuFreq != 160) {
    powerSetCpuFreq(160);
  }
}

void powerSetCpuFreq(uint32_t mhz) {
  if (mhz == currentCpuFreq)
    return;
  if (mhz != 80 && mhz != 160)
    return;

  // ESP32-C3 Note:
  // The APB clock remains at 80 MHz whether CPU frequency is set to 160 MHz or 80 MHz.
  // Hardware UART baud rate generator derived from APB clock remains completely stable.
  // Built-in USB CDC / JTAG runs from an independent 48 MHz PLL, unaffected by CPU scaling.
  setCpuFrequencyMhz(mhz);
  currentCpuFreq = getCpuFrequencyMhz();
  Serial.printf("[POWER] CPU freq changed to %lu MHz (esp_clk_cpu_freq=%d Hz)\n",
                (unsigned long)currentCpuFreq, esp_clk_cpu_freq());
}

uint32_t powerGetCpuFreq() {
  return currentCpuFreq;
}
