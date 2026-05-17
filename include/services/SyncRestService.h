#pragma once

#include <Arduino.h>

#include "models/DashboardState.h"

class SyncRestService {
 public:
  void begin(const char* host, uint16_t port);
  void tick();
  bool fetchSummary(DashboardProgress& tasks, DashboardProgress& habits);
  bool isServerOnline() const { return serverOnline_; }

 private:
  bool checkHealth();
  bool parseSummary(const String& payload, DashboardProgress& tasks, DashboardProgress& habits);

  const char* host_ = nullptr;
  uint16_t port_ = 0U;
  bool serverOnline_ = false;
  uint32_t lastHealthMs_ = 0U;
  static constexpr uint32_t kHealthIntervalMs = 30000U;
};
