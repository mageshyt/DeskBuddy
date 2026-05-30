#include "services/SyncRestService.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>

void SyncRestService::begin(const char* host, uint16_t port) {
  host_ = host;
  port_ = port;
  lastHealthMs_ = 0U;
  serverOnline_ = false;
}

void SyncRestService::tick() {
  if (host_ == nullptr || port_ == 0U) {
    return;
  }
  if (WiFi.status() != WL_CONNECTED) {
    serverOnline_ = false;
    return;
  }

  const uint32_t now = millis();
  if (now - lastHealthMs_ < kHealthIntervalMs) {
    return;
  }
  lastHealthMs_ = now;
  serverOnline_ = checkHealth();
}

bool SyncRestService::checkHealth() {
  String url = String("http://") + host_ + ":" + String(port_) + "/health";
  HTTPClient http;
  http.begin(url);
  const int code = http.GET();
  const bool ok = code >= 200 && code < 300;
  http.end();
  return ok;
}

bool SyncRestService::fetchSummary(DashboardState& state) {
  if (host_ == nullptr || port_ == 0U) {
    return false;
  }
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  String url = String("http://") + host_ + ":" + String(port_) + "/summary";
  HTTPClient http;
  http.begin(url);
  const int code = http.GET();
  if (code < 200 || code >= 300) {
    http.end();
    return false;
  }

  const String payload = http.getString();
  http.end();
  return parseSummary(payload, state);
}

bool SyncRestService::parseSummary(const String& payload, DashboardState& state) {
  StaticJsonDocument<384> doc;
  const DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    return false;
  }

  const JsonVariant tasksNode = doc["tasks"];
  const JsonVariant habitsNode = doc["habits"];
  const JsonVariant pomodoroNode = doc["pomodoro"];
  if (!tasksNode.is<JsonObject>()) {
    return false;
  }

  state.tasks.completed = tasksNode["completed"].as<uint8_t>();
  state.tasks.total = tasksNode["total"].as<uint8_t>();

  if (habitsNode.is<JsonObject>()) {
    state.habits.completed = habitsNode["completed"].as<uint8_t>();
    state.habits.total = habitsNode["total"].as<uint8_t>();
  }

  if (pomodoroNode.is<JsonObject>()) {
    state.sessionCount = pomodoroNode["completed"].as<uint8_t>();
  }

  return true;
}
