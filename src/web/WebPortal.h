#pragma once
#include <Arduino.h>

class WebServer;

extern WebServer server;
extern volatile int8_t pendingSound;
extern volatile bool otaInProgress;
extern volatile uint32_t lastWebRequestAt;

void pumpWeb();
void notifyWebRequest();

// ---------- Web Server & Endpoints ----------
void startWeb();
String maskSecret(const String &s);
bool isMaskedSecret(const String &s);
void handleRoot();
void handleSave();
void handleDoAction();
void handleStatus();
void handleLogs();
void handleConfigApi();
void handleScanNetworks();
void handleOtaStatus();
void handleOtaInstall();
void handleOtaRollback();
void processPendingSound();
