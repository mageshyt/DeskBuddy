#pragma once

#include <TFT_eSPI.h>
#include "interfaces/IScreen.h"
#include "models/DashboardState.h"

class MainDashboardScreen : public IScreen {
 public:
  MainDashboardScreen(TFT_eSPI &tft, const DashboardState &state);

  // IScreen interface overrides
  void onEnter() override;
  void onExit() override;
  bool handleInput(InputEvent event) override;
  void loop() override;

  void render(const DashboardState &state, bool forceRedraw = false);

 private:
  TFT_eSPI &tft_;
  const DashboardState &state_;
  bool hasLastState_ = false;
  DashboardState lastState_{};
  bool isActive_ = false;

  void drawBackground() const;
  void drawTopTimeCardFrame() const;
  void drawTopTimeDynamic(const DashboardState &state) const;
  void drawBottomCardsFrame() const;
  void drawBottomCardsDynamic(const DashboardState &state) const;
  void drawTaskIcon(int16_t x, int16_t y, uint16_t color) const;
  void drawHabitIcon(int16_t x, int16_t y, uint16_t color) const;
  void drawFocusIcon(int16_t x, int16_t y, uint16_t color) const;

  void drawProgressBar(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t completed, uint8_t total,
                       uint16_t accentColor) const;

  bool shouldRedraw(const DashboardState &state, bool forceRedraw) const;
};
