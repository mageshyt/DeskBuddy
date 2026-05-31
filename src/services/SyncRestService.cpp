#include "services/SyncRestService.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <LittleFS.h>

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

bool SyncRestService::fetchHabits(LocalHabitItem* habits, uint8_t& count, uint8_t maxCount) {
  if (host_ == nullptr || port_ == 0U || habits == nullptr) {
    return false;
  }
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  String url = String("http://") + host_ + ":" + String(port_) + "/habits";
  HTTPClient http;
  http.begin(url);
  const int code = http.GET();
  if (code < 200 || code >= 300) {
    http.end();
    return false;
  }

  const String payload = http.getString();
  http.end();

  DynamicJsonDocument doc(4096);
  const DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    Serial.printf("[SyncREST] habits JSON parse error: %s\n", error.c_str());
    return false;
  }

  const JsonVariant dataNode = doc["data"];
  if (!dataNode.is<JsonArray>()) {
    return false;
  }

  JsonArray arr = dataNode.as<JsonArray>();
  count = 0;
  for (JsonObject obj : arr) {
    if (count >= maxCount) break;

    LocalHabitItem& h = habits[count];
    h.id = obj["id"].as<int>();
    strncpy(h.title, obj["title"] | "", sizeof(h.title) - 1);
    h.title[sizeof(h.title) - 1] = '\0';
    strncpy(h.category, obj["category"] | "", sizeof(h.category) - 1);
    h.category[sizeof(h.category) - 1] = '\0';
    h.streak = obj["streak"] | 0;
    h.completed = obj["done"] | false;

    // Parse history array
    JsonArray historyArr = obj["history"].as<JsonArray>();
    uint8_t hIdx = 0;
    for (JsonVariant val : historyArr) {
      if (hIdx >= 7) break;
      String status = val.as<String>();
      if (status == "optimized") {
        h.history[hIdx] = 'o';
      } else if (status == "missed") {
        h.history[hIdx] = 'm';
      } else {
        h.history[hIdx] = 'e';
      }
      hIdx++;
    }
    h.history[hIdx] = '\0';
    
    count++;
  }

  saveHabitsCache(habits, count);
  return true;
}

bool SyncRestService::fetchTasks(LocalTaskItem* tasks, uint8_t& count, uint8_t maxCount) {
  if (host_ == nullptr || port_ == 0U || tasks == nullptr) {
    return false;
  }
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  String url = String("http://") + host_ + ":" + String(port_) + "/tasks?pageSize=7";
  HTTPClient http;
  http.begin(url);
  const int code = http.GET();
  if (code < 200 || code >= 300) {
    http.end();
    return false;
  }

  const String payload = http.getString();
  http.end();

  DynamicJsonDocument doc(2048);
  const DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    Serial.printf("[SyncREST] tasks JSON parse error: %s\n", error.c_str());
    return false;
  }

  const JsonVariant dataNode = doc["data"];
  if (!dataNode.is<JsonArray>()) {
    return false;
  }

  JsonArray arr = dataNode.as<JsonArray>();
  count = 0;
  for (JsonObject obj : arr) {
    if (count >= maxCount) break;

    LocalTaskItem& t = tasks[count];
    t.id = obj["id"].as<int>();
    strncpy(t.title, obj["title"] | "", sizeof(t.title) - 1);
    t.title[sizeof(t.title) - 1] = '\0';
    
    String status = obj["status"] | "";
    bool isDone = obj["done"] | false;
    t.completed = (status == "DONE" || isDone);

    count++;
  }

  saveTasksCache(tasks, count);
  return true;
}

bool SyncRestService::saveHabitsCache(const LocalHabitItem* habits, uint8_t count) {
  File file = LittleFS.open(kHabitsCachePath, "w");
  if (!file) {
    Serial.println("[SyncREST] Failed to open habits cache file for writing");
    return false;
  }

  DynamicJsonDocument doc(2048);
  JsonArray arr = doc.to<JsonArray>();
  for (uint8_t i = 0; i < count; ++i) {
    JsonObject obj = arr.add<JsonObject>();
    obj["id"] = habits[i].id;
    obj["title"] = habits[i].title;
    obj["category"] = habits[i].category;
    obj["streak"] = habits[i].streak;
    obj["completed"] = habits[i].completed;
    obj["history"] = habits[i].history;
  }

  serializeJson(doc, file);
  file.close();
  return true;
}

bool SyncRestService::loadHabitsCache(LocalHabitItem* habits, uint8_t& count, uint8_t maxCount) {
  if (habits == nullptr || !LittleFS.exists(kHabitsCachePath)) {
    count = 0;
    return false;
  }

  File file = LittleFS.open(kHabitsCachePath, "r");
  if (!file) {
    count = 0;
    return false;
  }

  DynamicJsonDocument doc(2048);
  DeserializationError err = deserializeJson(doc, file);
  file.close();

  if (err) {
    Serial.printf("[SyncREST] habits cache load error: %s\n", err.c_str());
    count = 0;
    return false;
  }

  JsonArray arr = doc.as<JsonArray>();
  count = 0;
  for (JsonObject obj : arr) {
    if (count >= maxCount) break;

    LocalHabitItem& h = habits[count];
    h.id = obj["id"] | -1;
    strncpy(h.title, obj["title"] | "", sizeof(h.title) - 1);
    h.title[sizeof(h.title) - 1] = '\0';
    strncpy(h.category, obj["category"] | "", sizeof(h.category) - 1);
    h.category[sizeof(h.category) - 1] = '\0';
    h.streak = obj["streak"] | 0;
    h.completed = obj["completed"] | false;
    strncpy(h.history, obj["history"] | "eeeeeee", sizeof(h.history) - 1);
    h.history[sizeof(h.history) - 1] = '\0';

    count++;
  }

  return true;
}

bool SyncRestService::saveTasksCache(const LocalTaskItem* tasks, uint8_t count) {
  File file = LittleFS.open(kTasksCachePath, "w");
  if (!file) {
    Serial.println("[SyncREST] Failed to open tasks cache file for writing");
    return false;
  }

  DynamicJsonDocument doc(1024);
  JsonArray arr = doc.to<JsonArray>();
  for (uint8_t i = 0; i < count; ++i) {
    JsonObject obj = arr.add<JsonObject>();
    obj["id"] = tasks[i].id;
    obj["title"] = tasks[i].title;
    obj["completed"] = tasks[i].completed;
  }

  serializeJson(doc, file);
  file.close();
  return true;
}

bool SyncRestService::loadTasksCache(LocalTaskItem* tasks, uint8_t& count, uint8_t maxCount) {
  if (tasks == nullptr || !LittleFS.exists(kTasksCachePath)) {
    count = 0;
    return false;
  }

  File file = LittleFS.open(kTasksCachePath, "r");
  if (!file) {
    count = 0;
    return false;
  }

  DynamicJsonDocument doc(1024);
  DeserializationError err = deserializeJson(doc, file);
  file.close();

  if (err) {
    Serial.printf("[SyncREST] tasks cache load error: %s\n", err.c_str());
    count = 0;
    return false;
  }

  JsonArray arr = doc.as<JsonArray>();
  count = 0;
  for (JsonObject obj : arr) {
    if (count >= maxCount) break;

    LocalTaskItem& t = tasks[count];
    t.id = obj["id"] | -1;
    strncpy(t.title, obj["title"] | "", sizeof(t.title) - 1);
    t.title[sizeof(t.title) - 1] = '\0';
    t.completed = obj["completed"] | false;

    count++;
  }

  return true;
}
