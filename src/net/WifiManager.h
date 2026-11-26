#pragma once
#include <Arduino.h>

class DNSServer;

extern DNSServer dnsServer;

// ---------- Captive DNS Pump ----------
void pumpDns();

// ---------- mDNS & AP/STA Lifecycle ----------
void initMdns(const char *mode);
bool mdnsQueryLock(uint32_t waitMs = 15000);
void mdnsQueryUnlock();
void createAPName();
void startAP();
void startAPParallel();
void startConnectHomeAsync();
bool isWifiConnecting();
void finalizeWifiConnected();
void finalizeWifiOffline();
bool connectHome();
void maintainWiFi();

// ---------- NTP Time Sync ----------
void startNtpAsync();
bool checkNtpSync();
bool syncTimeFromNTP();

// ---------- Heap / Local Network Guards ----------
bool suspendLocalNet();
void restoreWebAndAP(bool apWasUp);
