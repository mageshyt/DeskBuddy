#pragma once
#include <Arduino.h>

struct LocalHabitItem {
  int id;
  char title[32];
  char category[20];
  uint8_t streak;
  bool completed;
  char history[8]; // 7 days of 'o' (done), 'm' (missed), 'e' (empty), null-terminated
};

struct LocalTaskItem {
  int id;
  char title[48];
  bool completed;
};
