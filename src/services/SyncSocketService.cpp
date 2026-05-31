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

      StaticJsonDocument<2048> doc;
      const DeserializationError error = deserializeJson(doc, payload, length);
      if (error) {
        break;
      }

      const char* typeValue = doc["type"] | "";
      Serial.printf("[ws] event: %s\n", typeValue);
      if (strcmp(typeValue, "task:created") == 0 ||
          strcmp(typeValue, "task:updated") == 0 ||
          strcmp(typeValue, "task:deleted") == 0 ||
          strcmp(typeValue, "habit:updated") == 0 ||
          strcmp(typeValue, "pomodoro:started") == 0 ||
          strcmp(typeValue, "pomodoro:completed") == 0 ||
          strcmp(typeValue, "pomodoro:abandoned") == 0) {
        summaryRefreshPending_ = true;
      }
      if (strcmp(typeValue, "test:event") == 0) {
        const char* message = doc["payload"]["message"] | "";
        const uint32_t durationMs = doc["payload"]["durationMs"] | 30000;
        if (message[0] != '\0') {
          Serial.printf("[ws] test:event message: %s (duration=%u)\n", message, durationMs);
          snprintf(lastTestMessage_, sizeof(lastTestMessage_), "%s", message);
          testMessageDurationMs_ = durationMs;
          testMessagePending_ = true;
        }
      }
      if (strcmp(typeValue, "oled:image") == 0) {
        const char* base64 = doc["payload"]["image"] | "";
        const bool persistent = doc["payload"]["persistent"] | true;
        const uint32_t durationMs = doc["payload"]["durationMs"] | 30000;
        if (base64[0] != '\0') {
          Serial.printf("[ws] oled:image event received (persistent=%s, duration=%u)\n", persistent ? "true" : "false", durationMs);
          snprintf(lastOledImageBase64_, sizeof(lastOledImageBase64_), "%s", base64);
          oledImagePersistent_ = persistent;
          oledImageDurationMs_ = durationMs;
          oledImagePending_ = true;
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

bool SyncSocketService::consumeTestMessage(char* buffer, size_t bufferSize, uint32_t& durationMs) {
  if (!testMessagePending_ || bufferSize == 0) {
    return false;
  }

  snprintf(buffer, bufferSize, "%s", lastTestMessage_);
  durationMs = testMessageDurationMs_;
  testMessagePending_ = false;
  return true;
}

bool SyncSocketService::consumeOledImage(char* buffer, size_t bufferSize, bool& persistent, uint32_t& durationMs) {
  if (!oledImagePending_ || bufferSize == 0) {
    return false;
  }

  snprintf(buffer, bufferSize, "%s", lastOledImageBase64_);
  persistent = oledImagePersistent_;
  durationMs = oledImageDurationMs_;
  oledImagePending_ = false;
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
