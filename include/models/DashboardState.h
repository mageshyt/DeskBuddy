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
  uint8_t focusMode;               // 0 = focus, 1 = short_break, 2 = long_break
  uint16_t focusSecondsRemaining;   // Count down in seconds
  char focusCategory[20];          // Current selected category (e.g. "General")
  uint8_t sessionCount;            // Completed sessions today
};
