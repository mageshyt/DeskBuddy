#pragma once
#include <TFT_eSPI.h>
#include "interfaces/IScreen.h"
#include "models/DashboardState.h"

struct TaskItem {
  const char* title;
  bool completed;
};

class TasksScreen : public IScreen {
 public:
  TasksScreen(TFT_eSPI &tft, DashboardState &state);

  void onEnter() override;
  void onExit() override;
  bool handleInput(InputEvent event) override;
  void loop() override;

 private:
  TFT_eSPI &tft_;
  DashboardState &state_;
  bool isActive_ = false;
  
  static constexpr uint8_t TASK_COUNT = 4;
  TaskItem tasks_[TASK_COUNT] = {
    {"Drink 500ml Water", false},
    {"Stretch & Walk", true},
    {"Check Email Inbox", true},
    {"Antigravity Task", false}
  };
  
  uint8_t selectedIndex_ = 0;
  
  void drawBackground();
  void drawHeader();
  void drawTaskList(bool force = false);
  void drawTaskRow(uint8_t index, bool isSelected);
};
