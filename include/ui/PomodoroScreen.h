#pragma once
#include <TFT_eSPI.h>
#include "interfaces/IScreen.h"
#include "models/DashboardState.h"

class PomodoroScreen : public IScreen {
 public:
  PomodoroScreen(TFT_eSPI &tft, DashboardState &state);

  void onEnter() override;
  void onExit() override;
  bool handleInput(InputEvent event) override;
  void loop() override;

 private:
  TFT_eSPI &tft_;
  DashboardState &state_;
  bool isActive_ = false;
  
  // Track last rendered state for delta updates
  bool lastFocusRunning_ = false;
  uint16_t lastMinutes_ = 9999;
  uint32_t lastTickMs_ = 0;
  
  void drawBackground();
  void drawHeader();
  void drawTimerFrame();
  void drawTimerValue(bool force = false);
};
