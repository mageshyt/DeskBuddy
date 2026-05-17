#pragma once

#include <Arduino.h>

#include "models/DashboardState.h"

class ClockService {
 public:
  void begin(const ClockSnapshot &seed);
  ClockSnapshot now() const;
  private:
  ClockSnapshot seed_{};
  uint32_t startMillis_ = 0;

  public:
  // Exposed for unit testing and utility use
  static bool isLeapYear(uint16_t year);
  static uint8_t daysInMonth(uint16_t year, uint8_t month);
};
