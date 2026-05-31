#pragma once
#include <TFT_eSPI.h>
#include "interfaces/IScreen.h"
#include "models/DashboardState.h"
#include "models/HabitTaskModels.h"
#include "services/SyncRestService.h"
#include "services/LocalFirstSyncService.h"

class HabitsScreen : public IScreen {
 public:
  HabitsScreen(TFT_eSPI &tft, DashboardState &state, SyncRestService &restSync, LocalFirstSyncService &localSync);

  void onEnter() override;
  void onExit() override;
  bool handleInput(InputEvent event) override;
  void loop() override;
  void forceRefresh() { state_.habitsNeedRefetch = true; }

 private:
  TFT_eSPI &tft_;
  DashboardState &state_;
  SyncRestService &restSync_;
  LocalFirstSyncService &localSync_;
  
  bool isActive_ = false;
  
  static constexpr uint8_t MAX_HABITS = 8;
  LocalHabitItem habits_[MAX_HABITS];
  uint8_t habitCount_ = 0;
  uint8_t selectedIndex_ = 0;
  uint8_t scrollOffset_ = 0;
  uint8_t bestStreak_ = 0;
  
  void drawBackground();
  void drawHeader();
  void drawHabitsList(bool force = false);
  void drawHabitRow(uint8_t index, bool isSelected);
};
