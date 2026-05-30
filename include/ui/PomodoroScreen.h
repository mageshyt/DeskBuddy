#pragma once

#include <TFT_eSPI.h>
#include "interfaces/IScreen.h"
#include "models/DashboardState.h"

class PomodoroService;

class PomodoroScreen : public IScreen {
 public:
  PomodoroScreen(TFT_eSPI &tft, const DashboardState &state, PomodoroService &pomodoroService);

  void onEnter() override;
  void onExit() override;
  bool handleInput(InputEvent event) override;
  void loop() override;

 private:
  TFT_eSPI &tft_;
  const DashboardState &state_;
  PomodoroService &pomodoroService_;
  bool isActive_ = false;

  // Category Configuration
  static constexpr size_t kNumCategories = 5;
  static const char* kCategories[kNumCategories];
  
  // Interactive UI State
  bool inPickerMode_ = false;
  int8_t selectedCategoryIndex_ = 0;
  uint32_t pressStartMs_ = 0;

  // Cancel Confirmation Dialog
  bool inCancelConfirm_ = false;
  uint8_t cancelConfirmSelection_ = 1; // 0=Yes, 1=No (default No)
  
  // Track last rendered state for delta updates
  bool lastRunning_ = false;
  uint8_t lastMode_ = 99;
  uint16_t lastSecs_ = 9999;
  char lastCategory_[20] = "";
  uint8_t lastSessionCount_ = 99;
  bool lastPickerMode_ = false;
  int8_t lastSelectedCategoryIndex_ = -1;
  bool lastCancelConfirm_ = false;
  uint8_t lastCancelSelection_ = 99;

  // Drawing Helpers
  void drawBackground() const;
  void drawHeader() const;
  void drawGlassCard(int16_t x, int16_t y, int16_t w, int16_t h, const char* title) const;
  void drawTimerSection(bool force = false) const;
  void drawDialTicks(int16_t cx, int16_t cy, int16_t innerR, int16_t outerR, float percent, uint16_t activeColor, uint16_t inactiveColor) const;
  
  void drawRightPanel(bool force = false) const;
  void drawCategoryPicker() const;
  void clearCategoryPicker() const;
  void drawCancelDialog() const;
  void clearCancelDialog() const;
  
  uint16_t getModeColor(uint8_t mode) const;
  uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) const { return tft_.color565(r, g, b); }
};
