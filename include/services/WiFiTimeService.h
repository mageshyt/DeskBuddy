#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <time.h>

#include "models/DashboardState.h"

class WiFiTimeService {
 public:
  bool begin(const char* ssid, const char* password);

  void tick();

  ClockSnapshot snapshotNow() const;

  bool isSynced() const { return synced_; }

 private:
  bool connectWiFi(const char* ssid, const char* password);
  bool syncNTP();

  bool synced_          = false;
  uint32_t lastSyncMs_  = 0;
};
