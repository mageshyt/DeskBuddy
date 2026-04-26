#pragma once

#include <stdint.h>

struct ClockSnapshot {
  uint16_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
  uint8_t weekdayIndex;  // 0 = Sunday, 6 = Saturday
};

struct DashboardProgress {
  uint8_t completed;
  uint8_t total;
};

struct DashboardState {
  ClockSnapshot clock;
  DashboardProgress tasks;
  DashboardProgress habits;
  bool focusRunning;
  uint16_t focusMinutesRemaining;
};
