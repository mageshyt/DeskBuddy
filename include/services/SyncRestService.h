#pragma once

#include <Arduino.h>

#include "models/DashboardState.h"
#include "models/HabitTaskModels.h"

class SyncRestService {
 public:
  void begin(const char* host, uint16_t port);
  void tick();
  bool fetchSummary(DashboardState& state);
  bool isServerOnline() const { return serverOnline_; }

  // Habits & Tasks REST fetching
  bool fetchHabits(LocalHabitItem* habits, uint8_t& count, uint8_t maxCount);
  bool fetchTasks(LocalTaskItem* tasks, uint8_t& count, uint8_t maxCount);

  // Habits & Tasks cache persistence helpers
  bool saveHabitsCache(const LocalHabitItem* habits, uint8_t count);
  bool loadHabitsCache(LocalHabitItem* habits, uint8_t& count, uint8_t maxCount);
  bool saveTasksCache(const LocalTaskItem* tasks, uint8_t count);
  bool loadTasksCache(LocalTaskItem* tasks, uint8_t& count, uint8_t maxCount);

 private:
  bool checkHealth();
  bool parseSummary(const String& payload, DashboardState& state);

  const char* host_ = nullptr;
  uint16_t port_ = 0U;
  bool serverOnline_ = false;
  uint32_t lastHealthMs_ = 0U;
  static constexpr uint32_t kHealthIntervalMs = 30000U;
  
  static constexpr const char* kHabitsCachePath = "/habits_cache.json";
  static constexpr const char* kTasksCachePath = "/tasks_cache.json";
};
