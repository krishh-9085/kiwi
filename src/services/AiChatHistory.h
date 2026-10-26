#pragma once
#include <Arduino.h>

struct ChatTurn {
  String role;
  String text;
};

void clearConversationHistory();
int getConversationHistoryCount();
void checkChatHistoryTimeout();
void addChatHistory(const String &role, const String &text);
extern ChatTurn chatHistory[12];
extern int chatHistoryCount;
