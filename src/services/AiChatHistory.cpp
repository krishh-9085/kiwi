#include "services/AiChatHistory.h"
#include "config/AppConfig.h"

// ============================================================
struct ChatTurn {
  String question;
  String answer;
};

static const int MAX_CHAT_HISTORY = 4; // Retain last 4 exchanges (8 messages)
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

static void checkChatHistoryTimeout() {
  if (chatHistoryCount > 0 && lastChatTime > 0) {
    if (millis() - lastChatTime > CHAT_HISTORY_TIMEOUT_MS) {
      Serial.printf("[CONVO] History timed out (%lu ms) -> reset\n",
                    (unsigned long)(millis() - lastChatTime));
      clearConversationHistory();
    }
  }
}

static void addChatHistory(const String &q, const String &a) {
  if (q.length() == 0 || a.length() == 0)
    return;
  if (chatHistoryCount >= MAX_CHAT_HISTORY) {
    for (int i = 0; i < MAX_CHAT_HISTORY - 1; i++) {
      chatHistory[i] = chatHistory[i + 1];
    }
    chatHistoryCount = MAX_CHAT_HISTORY - 1;
  }
  chatHistory[chatHistoryCount].question = utf8Truncate(q, 160);
  chatHistory[chatHistoryCount].answer = utf8Truncate(a, 220);
  chatHistoryCount++;
  lastChatTime = millis();
  Serial.printf("[CONVO] Stored turn %d/%d in memory (Free heap: %u)\n",
                chatHistoryCount, MAX_CHAT_HISTORY,
                (unsigned)ESP.getFreeHeap());
}

// ============================================================
// LARGE LANGUAGE MODELS (LLM)
