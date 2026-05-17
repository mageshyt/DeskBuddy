#include "services/SyncSocketService.h"

#include <WiFi.h>

void SyncSocketService::begin(const char* host, uint16_t port, const char* firmwareVersion) {
  host_ = host;
  port_ = port;
  firmwareVersion_ = firmwareVersion;
  socket_.begin(host, port, "/");
  socket_.onEvent([this](WStype_t type, uint8_t* payload, size_t length) {
    onEvent(type, payload, length);
  });
  socket_.setReconnectInterval(0);
  reconnectBackoffMs_ = 3000U;
  nextReconnectMs_ = millis() + reconnectBackoffMs_;
}

void SyncSocketService::tick() {
  const bool wifiUp = WiFi.status() == WL_CONNECTED;
  if (!wifiUp) {
    if (wifiWasUp_ && connected_) {
      sendStatus(false);
    }
    wifiWasUp_ = false;
    return;
  }
  wifiWasUp_ = true;

  socket_.loop();

  const uint32_t now = millis();
  if (!connected_ && host_ != nullptr && now >= nextReconnectMs_) {
    socket_.disconnect();
    socket_.begin(host_, port_, "/");
    nextReconnectMs_ = now + reconnectBackoffMs_;
    if (reconnectBackoffMs_ < kMaxReconnectBackoffMs) {
      reconnectBackoffMs_ = min(reconnectBackoffMs_ * 2U, kMaxReconnectBackoffMs);
    }
  }

  if (connected_ && (now - lastHeartbeatMs_ >= kHeartbeatIntervalMs)) {
    lastHeartbeatMs_ = now;
    sendStatus(true);
  }
}

void SyncSocketService::onEvent(WStype_t type, uint8_t* payload, size_t length) {
  (void)payload;
  (void)length;

  switch (type) {
    case WStype_CONNECTED:
      connected_ = true;
      lastHeartbeatMs_ = millis();
      reconnectBackoffMs_ = 3000U;
      sendStatus(true);
      break;
    case WStype_DISCONNECTED:
      if (connected_) {
        connected_ = false;
        sendStatus(false);
      }
      break;
    default:
      break;
  }
}

void SyncSocketService::sendStatus(bool connected) {
  if (!connected_) {
    return;
  }

  const char* version = firmwareVersion_ ? firmwareVersion_ : "unknown";
  char payload[128];
  snprintf(payload, sizeof(payload),
           "{\"type\":\"esp32:status\",\"payload\":{\"connected\":%s,\"version\":\"%s\"}}",
           connected ? "true" : "false",
           version);
  socket_.sendTXT(payload);
}
