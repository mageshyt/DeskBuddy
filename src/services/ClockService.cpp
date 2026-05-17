#include "services/ClockService.h"

void ClockService::begin(const ClockSnapshot &seed) {
  // Validate and normalize incoming seed to avoid undefined time calculations.
  ClockSnapshot normalized = seed;

  if (normalized.year < 1970U) {
    normalized.year = 1970U;
  }
  if (normalized.month < 1U) {
    normalized.month = 1U;
  } else if (normalized.month > 12U) {
    normalized.month = 12U;
  }

  const uint8_t dim = daysInMonth(normalized.year, normalized.month);
  if (normalized.day < 1U) {
    normalized.day = 1U;
  } else if (normalized.day > dim) {
    normalized.day = dim;
  }

  if (normalized.hour > 23U) {
    normalized.hour = 0U;
  }
  if (normalized.minute > 59U) {
    normalized.minute = 0U;
  }
  if (normalized.second > 59U) {
    normalized.second = 0U;
  }

  seed_ = normalized;
  startMillis_ = millis();
}

ClockSnapshot ClockService::now() const {
  ClockSnapshot current = seed_;
  const uint32_t elapsedSeconds = (millis() - startMillis_) / 1000U;

  const uint32_t seedDaySeconds = static_cast<uint32_t>(seed_.hour) * 3600U +
                                  static_cast<uint32_t>(seed_.minute) * 60U +
                                  static_cast<uint32_t>(seed_.second);

  const uint32_t totalSeconds = seedDaySeconds + elapsedSeconds;
  const uint32_t daysPassed = totalSeconds / 86400U;
  const uint32_t dayRemainder = totalSeconds % 86400U;

  current.hour = static_cast<uint8_t>(dayRemainder / 3600U);
  current.minute = static_cast<uint8_t>((dayRemainder % 3600U) / 60U);
  current.second = static_cast<uint8_t>(dayRemainder % 60U);

  uint32_t remainingDays = daysPassed;
  while (remainingDays > 0) {
    const uint8_t dim = daysInMonth(current.year, current.month);
    if (current.day < dim) {
      current.day++;
      remainingDays--;
      continue;
    }

    current.day = 1;
    if (current.month < 12) {
      current.month++;
    } else {
      current.month = 1;
      current.year++;
    }
    remainingDays--;
  }

  current.weekdayIndex = static_cast<uint8_t>((seed_.weekdayIndex + daysPassed) % 7U);
  return current;
}

bool ClockService::isLeapYear(uint16_t year) {
  if ((year % 400U) == 0U) {
    return true;
  }
  if ((year % 100U) == 0U) {
    return false;
  }
  return (year % 4U) == 0U;
}

uint8_t ClockService::daysInMonth(uint16_t year, uint8_t month) {
  static const uint8_t kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month < 1) {
    month = 1;
  } else if (month > 12) {
    month = 12;
  }

  if (month == 2 && isLeapYear(year)) {
    return 29;
  }
  return kDays[month - 1];
}
