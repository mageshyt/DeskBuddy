#pragma once

#include <TFT_eSPI.h>

#include "models/DashboardState.h"

class MainDashboardScreen {
 public:
  explicit MainDashboardScreen(TFT_eSPI &tft);

  void setSystemStatus(bool wifiConnected);
  void render(const DashboardState &state, bool forceRedraw = false);

 private:
  TFT_eSPI &tft_;
  bool wifiConnected_ = true;

  bool hasLastState_ = false;
  DashboardState lastState_{};

  void drawBackground() const;
  void drawTopTimeCardFrame() const;
  void drawTopTimeDynamic(const DashboardState &state) const;
  void drawBottomCardsFrame() const;
  void drawBottomCardsDynamic(const DashboardState &state) const;

  void drawWifiIcon(int16_t x, int16_t y, uint16_t color) const;
  void drawTaskIcon(int16_t x, int16_t y, uint16_t color) const;
  void drawHabitIcon(int16_t x, int16_t y, uint16_t color) const;
  void drawFocusIcon(int16_t x, int16_t y, uint16_t color) const;

  void drawProgressBar(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t completed, uint8_t total,
                       uint16_t accentColor) const;

  bool shouldRedraw(const DashboardState &state, bool forceRedraw) const;
};
