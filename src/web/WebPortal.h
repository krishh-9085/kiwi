#pragma once
#include <Arduino.h>

class WebServer;
class DNSServer;

extern WebServer server;
extern DNSServer dnsServer;
extern volatile int8_t pendingSound;
void pumpWeb();

// ---------- Wi-Fi & NTP Management ----------
void createAPName();
void startAP();
void startAPParallel();
bool connectHome();
void maintainWiFi();
bool syncTimeFromNTP();

// ---------- Network Suspension / Heap Helpers ----------
bool suspendLocalNet();
void restoreWebAndAP(bool apWasUp);

// ---------- Web Server & Endpoints ----------
void startWeb();
void handleRoot();
void handleSave();
void handleFacesPage();
void handleWiring();
void handleDoAction();
void handleStatus();
void handleConfigApi();
void handleScanNetworks();
void handleOtaPage();
void processPendingSound();
