#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <time.h>

#include "models/DashboardState.h"

class WiFiTimeService {
 public:
  enum class WiFiStatus : uint8_t { Disconnected = 0, Connecting = 1, Connected = 2 };
  using ProgressCallback = void (*)(WiFiStatus status);

  bool begin(const char* ssid, const char* password, ProgressCallback onProgress = nullptr);

  void tick();

  ClockSnapshot snapshotNow() const;

  bool isSynced() const { return synced_; }
  WiFiStatus status() const { return status_; }

 private:
  bool connectWiFi(const char* ssid, const char* password, ProgressCallback onProgress);
  bool syncNTP();

  bool synced_          = false;
  uint32_t lastSyncMs_  = 0;
  WiFiStatus status_    = WiFiStatus::Disconnected;
};
