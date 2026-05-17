#include <Arduino.h>
#include <unity.h>

#include "services/ClockService.h"

void test_isLeapYear(void) {
  TEST_ASSERT_TRUE(ClockService::isLeapYear(2000));
  TEST_ASSERT_FALSE(ClockService::isLeapYear(1900));
  TEST_ASSERT_TRUE(ClockService::isLeapYear(2004));
  TEST_ASSERT_FALSE(ClockService::isLeapYear(2001));
}

void test_daysInMonth(void) {
  TEST_ASSERT_EQUAL_UINT8(31, ClockService::daysInMonth(2021, 1));
  TEST_ASSERT_EQUAL_UINT8(28, ClockService::daysInMonth(2021, 2));
  TEST_ASSERT_EQUAL_UINT8(29, ClockService::daysInMonth(2020, 2));
  TEST_ASSERT_EQUAL_UINT8(30, ClockService::daysInMonth(2021, 4));
  // Out of range months are clamped
  TEST_ASSERT_EQUAL_UINT8(31, ClockService::daysInMonth(2021, 0));
  TEST_ASSERT_EQUAL_UINT8(31, ClockService::daysInMonth(2021, 13));
}

void test_begin_now_consistency(void) {
  ClockService svc;
  ClockSnapshot seed{};
  seed.year = 2022;
  seed.month = 5;
  seed.day = 12;
  seed.hour = 10;
  seed.minute = 30;
  seed.second = 15;
  seed.weekdayIndex = 4;

  svc.begin(seed);
  // Immediately calling now() should return the same or very close values (no elapsed seconds)
  ClockSnapshot now = svc.now();
  TEST_ASSERT_EQUAL_UINT16(seed.year, now.year);
  TEST_ASSERT_EQUAL_UINT8(seed.month, now.month);
  TEST_ASSERT_EQUAL_UINT8(seed.day, now.day);
  TEST_ASSERT_EQUAL_UINT8(seed.hour, now.hour);
  TEST_ASSERT_EQUAL_UINT8(seed.minute, now.minute);
}

void setup() {
  delay(2000);
  UNITY_BEGIN();
  RUN_TEST(test_isLeapYear);
  RUN_TEST(test_daysInMonth);
  RUN_TEST(test_begin_now_consistency);
  UNITY_END();
}

void loop() {
  // no-op
}
