#pragma once
#include <TFT_eSPI.h>
#include "interfaces/IScreen.h"
#include "models/DashboardState.h"
#include "models/HabitTaskModels.h"
#include "services/SyncRestService.h"
#include "services/LocalFirstSyncService.h"

class NavigationService;

class TasksScreen : public IScreen {
 public:
  TasksScreen(TFT_eSPI &tft, DashboardState &state, SyncRestService &restSync, LocalFirstSyncService &localSync, NavigationService &nav);

  void onEnter() override;
  void onExit() override;
  bool handleInput(InputEvent event) override;
  void loop() override;
  void forceRefresh() { lastFetchMs_ = 0; }

 private:
  TFT_eSPI &tft_;
  DashboardState &state_;
  SyncRestService &restSync_;
  LocalFirstSyncService &localSync_;
  NavigationService &nav_;
  
  bool isActive_ = false;
  
  static constexpr uint8_t MAX_TASKS = 8;
  LocalTaskItem tasks_[MAX_TASKS];
  uint8_t taskCount_ = 0;
  uint8_t selectedIndex_ = 0;
  uint8_t scrollOffset_ = 0;
  
  uint32_t lastFetchMs_ = 0;
  static constexpr uint32_t FETCH_INTERVAL_MS = 15000;
  
  void drawBackground();
  void drawHeader();
  void drawTaskList(bool force = false);
  void drawTaskRow(uint8_t index, bool isSelected);
};
