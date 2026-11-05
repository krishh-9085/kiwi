#include "services/AiChatHistory.h"
#include "services/AiTextTools.h"
#include "config/AppConfig.h"
#include <utility>

int maxChatHistory = 4;
ChatTurn chatHistory[MAX_CHAT_HISTORY];
int chatHistoryCount = 0;
static uint32_t lastChatTime = 0;
static const uint32_t CHAT_HISTORY_TIMEOUT_MS =
    240000; // 4 minutes inactivity timeout

void clearConversationHistory() {
  chatHistoryCount = 0;
  lastChatTime = 0;
  Serial.println("[CONVO] Conversation history cleared.");
}

int getConversationHistoryCount() { return chatHistoryCount; }

void checkChatHistoryTimeout() {
  if (chatHistoryCount > 0 && lastChatTime > 0) {
    if (millis() - lastChatTime > CHAT_HISTORY_TIMEOUT_MS) {
      Serial.printf("[CONVO] History timed out (%lu ms) -> reset\n",
                    (unsigned long)(millis() - lastChatTime));
      clearConversationHistory();
    }
  }
}

void addChatHistory(const String &q, const String &a) {
  if (q.length() == 0 || a.length() == 0)
    return;
  int limit = (maxChatHistory >= 1 && maxChatHistory <= MAX_CHAT_HISTORY) ? maxChatHistory : 4;
  if (chatHistoryCount >= limit) {
    for (int i = 0; i < chatHistoryCount - 1; i++) {
      chatHistory[i] = std::move(chatHistory[i + 1]);
    }
    chatHistoryCount = limit - 1;
  }
  chatHistory[chatHistoryCount].question = utf8Truncate(q, 160);
  chatHistory[chatHistoryCount].answer = utf8Truncate(a, 220);
  chatHistoryCount++;
  lastChatTime = millis();
  Serial.printf("[CONVO] Stored turn %d/%d in memory (Free heap: %u)\n",
                chatHistoryCount, limit,
                (unsigned)ESP.getFreeHeap());
}
