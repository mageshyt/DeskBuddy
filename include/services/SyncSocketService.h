#pragma once

#include <Arduino.h>
#include <WebSocketsClient.h>

class SyncSocketService {
 public:
  void begin(const char* host, uint16_t port, const char* firmwareVersion);
  void tick();
  bool isConnected() const { return connected_; }
  bool consumeSummaryRefresh();
  bool consumeTestMessage(char* buffer, size_t bufferSize, uint32_t& durationMs);
  bool consumeOledImage(char* buffer, size_t bufferSize, bool& persistent, uint32_t& durationMs);

 private:
  void sendStatus(bool connected);
  void onEvent(WStype_t type, uint8_t* payload, size_t length);

  WebSocketsClient socket_{};
  bool connected_ = false;
  bool wifiWasUp_ = false;
  const char* firmwareVersion_ = nullptr;
  uint32_t lastHeartbeatMs_ = 0;
  uint32_t nextReconnectMs_ = 0;
  uint32_t reconnectBackoffMs_ = 3000U;
  bool summaryRefreshPending_ = false;
  bool testMessagePending_ = false;
  char lastTestMessage_[96] = {0};
  uint32_t testMessageDurationMs_ = 30000;
  bool oledImagePending_ = false;
  bool oledImagePersistent_ = false;
  uint32_t oledImageDurationMs_ = 30000;
  char lastOledImageBase64_[1500] = {0};
  const char* host_ = nullptr;
  uint16_t port_ = 0U;
  static constexpr uint32_t kHeartbeatIntervalMs = 10000U;
  static constexpr uint32_t kMaxReconnectBackoffMs = 30000U;
};
