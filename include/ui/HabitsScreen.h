#pragma once
#include <TFT_eSPI.h>
#include "interfaces/IScreen.h"
#include "models/DashboardState.h"

struct HabitItem {
  const char* title;
  uint8_t streak;
  bool completed;
};

class HabitsScreen : public IScreen {
 public:
  HabitsScreen(TFT_eSPI &tft, DashboardState &state);

  void onEnter() override;
  void onExit() override;
  bool handleInput(InputEvent event) override;
  void loop() override;

 private:
  TFT_eSPI &tft_;
  DashboardState &state_;
  bool isActive_ = false;
  
  static constexpr uint8_t HABIT_COUNT = 4;
  HabitItem habits_[HABIT_COUNT] = {
    {"Morning Exercise", 5, true},
    {"Read Book (10 pages)", 12, true},
    {"Code 1 Hour", 8, false},
    {"Clean Desk", 2, false}
  };
  
  uint8_t selectedIndex_ = 0;
  
  void drawBackground();
  void drawHeader();
  void drawHabitsList(bool force = false);
  void drawHabitRow(uint8_t index, bool isSelected);
};
