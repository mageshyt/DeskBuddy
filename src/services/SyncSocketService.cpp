#include "services/SyncSocketService.h"

#include <ArduinoJson.h>
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
  switch (type) {
    case WStype_CONNECTED:
      connected_ = true;
      lastHeartbeatMs_ = millis();
      reconnectBackoffMs_ = 3000U;
      Serial.printf("[ws] connected to %s:%u\n", host_ ? host_ : "(null)", port_);
      sendStatus(true);
      break;
    case WStype_DISCONNECTED:
      if (connected_) {
        connected_ = false;
        Serial.println("[ws] disconnected");
        sendStatus(false);
      }
      break;
    case WStype_TEXT: {
      char raw[256];
      const size_t copyLen = min(length, sizeof(raw) - 1);
      memcpy(raw, payload, copyLen);
      raw[copyLen] = '\0';
      Serial.printf("[ws] rx: %s\n", raw);

      StaticJsonDocument<256> doc;
      const DeserializationError error = deserializeJson(doc, payload, length);
      if (error) {
        break;
      }

      const char* typeValue = doc["type"] | "";
      Serial.printf("[ws] event: %s\n", typeValue);
      if (strcmp(typeValue, "task:created") == 0 ||
          strcmp(typeValue, "task:updated") == 0 ||
          strcmp(typeValue, "task:deleted") == 0) {
        summaryRefreshPending_ = true;
      }
      if (strcmp(typeValue, "test:event") == 0) {
        const char* message = doc["payload"]["message"] | "";
        if (message[0] != '\0') {
          Serial.printf("[ws] test:event message: %s\n", message);
          snprintf(lastTestMessage_, sizeof(lastTestMessage_), "%s", message);
          testMessagePending_ = true;
        }
      }
      break;
    }
    default:
      break;
  }
}

bool SyncSocketService::consumeSummaryRefresh() {
  if (!summaryRefreshPending_) {
    return false;
  }

  summaryRefreshPending_ = false;
  return true;
}

bool SyncSocketService::consumeTestMessage(char* buffer, size_t bufferSize) {
  if (!testMessagePending_ || bufferSize == 0) {
    return false;
  }

  snprintf(buffer, bufferSize, "%s", lastTestMessage_);
  testMessagePending_ = false;
  return true;
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
