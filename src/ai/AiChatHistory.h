#pragma once
#include <Arduino.h>

static const int MAX_CHAT_HISTORY = 8;
extern int maxChatHistory;

struct ChatTurn {
  String question;
  String answer;
};

void clearConversationHistory();
int getConversationHistoryCount();
void checkChatHistoryTimeout();
void addChatHistory(const String &q, const String &a);
extern ChatTurn chatHistory[MAX_CHAT_HISTORY];
extern int chatHistoryCount;
