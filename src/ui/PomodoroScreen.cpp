#include "ui/PomodoroScreen.h"
#include "services/PomodoroService.h"
#include <Arduino.h>

const char* PomodoroScreen::kCategories[PomodoroScreen::kNumCategories] = {
  "General",
  "Project Work",
  "Studies",
  "Admin",
  "Personal"
};

namespace {
void drawCenteredText(TFT_eSPI &tft, const char *text, int16_t centerX, int16_t y, uint8_t size, uint16_t color, uint16_t bg) {
  tft.setTextSize(size);
  tft.setTextColor(color, bg);
  const int16_t x = centerX - static_cast<int16_t>(tft.textWidth(text) / 2);
  tft.setCursor(x, y);
  tft.print(text);
}
} // namespace

PomodoroScreen::PomodoroScreen(TFT_eSPI &tft, const DashboardState &state, PomodoroService &pomodoroService)
    : tft_(tft), state_(state), pomodoroService_(pomodoroService) {}

void PomodoroScreen::onEnter() {
  isActive_ = true;
  inPickerMode_ = false;
  
  // Invalidate render state caches to force full redraw
  lastRunning_ = !state_.focusRunning;
  lastMode_ = 99;
  lastSecs_ = 9999;
  lastCategory_[0] = '\0';
  lastSessionCount_ = 99;
  lastActiveTaskId_ = -99;
  lastPickerMode_ = inPickerMode_;
  lastSelectedCategoryIndex_ = -1;
  lastCancelConfirm_ = false;
  lastCancelSelection_ = 99;
  inCancelConfirm_ = false;
  cancelConfirmSelection_ = 1;

  drawBackground();
  drawHeader();
  // Draw base glass cards
  drawGlassCard(12, 54, 220, 252, "SESSION COUNTER");
  drawGlassCard(274, 54, 194, 252, "STATUS & SETTINGS");
  
  drawTimerSection(true);
  drawRightPanel(true);
}

void PomodoroScreen::onExit() {
  isActive_ = false;
  Serial.println("[PomodoroScreen] onExit");
}

bool PomodoroScreen::handleInput(InputEvent event) {
  // ─── Cancel Confirmation Dialog ─────────────────────────
  if (inCancelConfirm_) {
    if (event == InputEvent::ENCODER_CW || event == InputEvent::ENCODER_CCW ||
        event == InputEvent::DPAD_UP || event == InputEvent::DPAD_DOWN) {
      // Toggle between Yes(0) and No(1)
      cancelConfirmSelection_ = 1 - cancelConfirmSelection_;
      return true;
    }
    if (event == InputEvent::ENCODER_PRESS) {
      if (cancelConfirmSelection_ == 0) {
        // User chose YES → abandon session and reset
        pomodoroService_.reset();
      }
      // Dismiss dialog in both cases
      inCancelConfirm_ = false;
      cancelConfirmSelection_ = 1; // Reset default to No
      return true;
    }
    if (event == InputEvent::DPAD_LEFT || event == InputEvent::DPAD_RIGHT) {
      // Dismiss dialog without action
      inCancelConfirm_ = false;
      cancelConfirmSelection_ = 1;
      return true;
    }
    return true; // Consume all events while dialog is open
  }

  // ─── Category Picker ───────────────────────────────────
  if (inPickerMode_) {
    if (event == InputEvent::ENCODER_PRESS) {
      pomodoroService_.setCategory(kCategories[selectedCategoryIndex_]);
      inPickerMode_ = false;
      return true;
    }
    
    if (event == InputEvent::DPAD_UP || event == InputEvent::ENCODER_CCW) {
      if (selectedCategoryIndex_ > 0) {
        selectedCategoryIndex_--;
      } else {
        selectedCategoryIndex_ = kNumCategories - 1;
      }
      return true;
    }
    
    if (event == InputEvent::DPAD_DOWN || event == InputEvent::ENCODER_CW) {
      if (selectedCategoryIndex_ < (int8_t)(kNumCategories - 1)) {
        selectedCategoryIndex_++;
      } else {
        selectedCategoryIndex_ = 0;
      }
      return true;
    }
    
    if (event == InputEvent::DPAD_LEFT || event == InputEvent::DPAD_RIGHT) {
      inPickerMode_ = false;
      return true;
    }
    
    return true; // Consume all other events when picker is open
  }

  // ─── Normal Mode Controls ──────────────────────────────
  if (event == InputEvent::ENCODER_PRESS) {
    if (state_.focusRunning) {
      pomodoroService_.pause();
    } else {
      pomodoroService_.start();
    }
    return true;
  }
  
  if (event == InputEvent::DPAD_UP || event == InputEvent::DPAD_DOWN) {
    if (!state_.focusRunning) {
      selectedCategoryIndex_ = 0;
      for (size_t i = 0; i < kNumCategories; ++i) {
        if (strcmp(state_.focusCategory, kCategories[i]) == 0) {
          selectedCategoryIndex_ = i;
          break;
        }
      }
      inPickerMode_ = true;
    }
    return true;
  }
  
  // ─── Encoder Rotation: Mode Switch or Cancel Dialog ────
  if (!state_.focusRunning) {
    if (event == InputEvent::ENCODER_CW || event == InputEvent::ENCODER_CCW) {
      // Check if there's an active session in progress (timer partially consumed)
      const uint32_t totalSecs = pomodoroService_.getDurationSeconds((PomodoroMode)state_.focusMode);
      const bool hasActiveSession = (state_.focusSecondsRemaining < totalSecs);
      
      if (hasActiveSession) {
        // Session in progress → show cancel confirmation dialog
        inCancelConfirm_ = true;
        cancelConfirmSelection_ = 1; // Default to No
        return true;
      }
      
      // No active session → free to switch modes directly
      if (event == InputEvent::ENCODER_CW) {
        uint8_t nextMode = (state_.focusMode + 1) % 3;
        pomodoroService_.setMode((PomodoroMode)nextMode);
      } else {
        uint8_t nextMode = (state_.focusMode + 2) % 3;
        pomodoroService_.setMode((PomodoroMode)nextMode);
      }
      return true;
    }
  }

  return false; // Let Navigation handle screen switching
}

void PomodoroScreen::loop() {
  if (!isActive_) return;

  // Render Category Picker transition
  if (inPickerMode_ != lastPickerMode_) {
    if (inPickerMode_) {
      drawCategoryPicker();
    } else {
      clearCategoryPicker();
      // Redraw underlying glass cards and their contents
      drawGlassCard(12, 54, 220, 252, "SESSION COUNTER");
      drawGlassCard(274, 54, 194, 252, "STATUS & SETTINGS");
      drawTimerSection(true);
      drawRightPanel(true);
    }
    lastPickerMode_ = inPickerMode_;
  }

  if (inPickerMode_) {
    // Render moving selection highlight
    if (selectedCategoryIndex_ != lastSelectedCategoryIndex_) {
      drawCategoryPicker();
      lastSelectedCategoryIndex_ = selectedCategoryIndex_;
    }
    return;
  }

  // ─── Cancel Confirmation Dialog transition ─────────────
  if (inCancelConfirm_ != lastCancelConfirm_) {
    if (inCancelConfirm_) {
      drawCancelDialog();
    } else {
      clearCancelDialog();
      // Redraw underlying cards after closing dialog
      drawGlassCard(12, 54, 220, 252, "SESSION COUNTER");
      drawGlassCard(274, 54, 194, 252, "STATUS & SETTINGS");
      drawTimerSection(true);
      drawRightPanel(true);
    }
    lastCancelConfirm_ = inCancelConfirm_;
    lastCancelSelection_ = cancelConfirmSelection_;
  }

  if (inCancelConfirm_) {
    // Re-render highlight if selection changed
    if (cancelConfirmSelection_ != lastCancelSelection_) {
      drawCancelDialog();
      lastCancelSelection_ = cancelConfirmSelection_;
    }
    return;
  }

  // Detect state changes and do targeted updates
  bool modeChanged = (state_.focusMode != lastMode_);
  bool runningChanged = (state_.focusRunning != lastRunning_);
  bool secondsChanged = (state_.focusSecondsRemaining != lastSecs_);
  bool categoryChanged = (strcmp(state_.focusCategory, lastCategory_) != 0);
  bool sessionsChanged = (state_.sessionCount != lastSessionCount_);
  bool taskChanged = (state_.activeTaskId != lastActiveTaskId_);

  if (modeChanged || runningChanged || secondsChanged) {
    drawTimerSection(modeChanged || runningChanged);
    lastMode_ = state_.focusMode;
    lastRunning_ = state_.focusRunning;
    lastSecs_ = state_.focusSecondsRemaining;
  }

  if (modeChanged || runningChanged || categoryChanged || sessionsChanged || taskChanged) {
    drawRightPanel(modeChanged || runningChanged || categoryChanged || sessionsChanged || taskChanged);
    if (categoryChanged) {
      strncpy(lastCategory_, state_.focusCategory, sizeof(lastCategory_) - 1);
      lastCategory_[sizeof(lastCategory_) - 1] = '\0';
    }
    lastSessionCount_ = state_.sessionCount;
    lastActiveTaskId_ = state_.activeTaskId;
  }
}

void PomodoroScreen::drawBackground() const {
  const int16_t w = tft_.width();
  const int16_t h = tft_.height();

  // Subtle dark mesh background
  for (int16_t y = 0; y < h; y++) {
    const uint8_t r = 2 + (4 * y) / h;
    const uint8_t g = 6 + (10 * y) / h;
    const uint8_t b = 15 + (18 * y) / h;
    tft_.drawFastHLine(0, y, w, rgb(r, g, b));
  }

  // Frame highlights
  tft_.drawRoundRect(3, 3, w - 6, h - 6, 12, rgb(15, 32, 64));
  tft_.drawRoundRect(5, 5, w - 10, h - 10, 10, rgb(8, 18, 38));
}

void PomodoroScreen::drawHeader() const {
  const int16_t w = tft_.width();
  const uint16_t cardBg = rgb(2, 8, 22);
  const uint16_t border = rgb(15, 45, 95);
  const uint16_t textGold = rgb(240, 195, 80);

  // Top header capsule
  tft_.fillRoundRect(12, 10, w - 24, 34, 8, cardBg);
  tft_.drawRoundRect(12, 10, w - 24, 34, 8, border);

  tft_.setTextColor(textGold, cardBg);
  tft_.setTextSize(2);
  tft_.setCursor(24, 19);
  tft_.print("POMODORO ENGINE");
  
  tft_.setTextColor(rgb(120, 150, 190), cardBg);
  tft_.setTextSize(1);
  tft_.setCursor(w - 180, 23);
  tft_.print("LOCAL-FIRST SYNC ACTIVE");
}
void PomodoroScreen::drawGlassCard(int16_t x, int16_t y, int16_t w, int16_t h, const char* title) const {
  const uint16_t cardBg = rgb(3, 12, 28);
  const uint16_t border = rgb(18, 48, 98);
  const uint16_t labelColor = rgb(100, 130, 175);

  tft_.fillRoundRect(x, y, w, h, 8, cardBg);
  tft_.drawRoundRect(x, y, w, h, 8, border);
  
  // Card title highlight
  tft_.setTextColor(labelColor, cardBg);
  tft_.setTextSize(1);
  tft_.setCursor(x + 12, y + 10);
  tft_.print(title);
}

void PomodoroScreen::drawTimerSection(bool force) const {
  const int16_t cx = 137;
  const int16_t cy = 185;
  const int16_t rInner = 68;
  const int16_t rOuter = 78;
  const uint16_t cardBg = rgb(3, 12, 28);
  
  uint16_t activeColor = getModeColor(state_.focusMode);
  uint16_t inactiveColor = rgb(15, 30, 60);
  
  // Calculate completion percentage
  uint32_t totalSecs = pomodoroService_.getDurationSeconds((PomodoroMode)state_.focusMode);
  float percent = 0.0f;
  if (totalSecs > 0) {
    percent = (float)(totalSecs - state_.focusSecondsRemaining) / totalSecs;
  }
  
  // 1. Draw Dial Ticks
  drawDialTicks(cx, cy, rInner, rOuter, percent, activeColor, inactiveColor);
  
  // 2. Big Countdown Value (MM:SS)
  uint8_t m = state_.focusSecondsRemaining / 60;
  uint8_t s = state_.focusSecondsRemaining % 60;
  char timeStr[8];
  snprintf(timeStr, sizeof(timeStr), "%02u:%02u", m, s);
  
  // Clear only inner digital block area
  tft_.fillCircle(cx, cy, rInner - 4, cardBg);
  
  drawCenteredText(tft_, timeStr, cx, cy - 14, 4, TFT_WHITE, cardBg);
  
  // Status string
  if (state_.focusRunning) {
    drawCenteredText(tft_, "TICKING", cx, cy + 22, 1, activeColor, cardBg);
  } else {
    if (state_.focusSecondsRemaining == totalSecs) {
      drawCenteredText(tft_, "READY", cx, cy + 22, 1, rgb(150, 175, 210), cardBg);
    } else {
      drawCenteredText(tft_, "PAUSED", cx, cy + 22, 1, rgb(220, 150, 50), cardBg);
    }
  }
}

void PomodoroScreen::drawDialTicks(int16_t cx, int16_t cy, int16_t innerR, int16_t outerR, float percent, uint16_t activeColor, uint16_t inactiveColor) const {
  const int numTicks = 45; // Modern dashboard look
  const float step = 360.0f / numTicks;
  
  int litCount = percent * numTicks;
  
  for (int i = 0; i < numTicks; ++i) {
    float angle = i * step - 90.0f; // Start ticks at 12 o'clock
    float rad = angle * DEG_TO_RAD;
    
    int16_t xs = cx + cos(rad) * innerR;
    int16_t ys = cy + sin(rad) * innerR;
    int16_t xe = cx + cos(rad) * outerR;
    int16_t ye = cy + sin(rad) * outerR;
    
    uint16_t col = (i < litCount) ? activeColor : inactiveColor;
    tft_.drawLine(xs, ys, xe, ye, col);
  }
}

void PomodoroScreen::drawRightPanel(bool force) const {
  const int16_t rx = 286;
  const uint16_t cardBg = rgb(3, 12, 28);
  const uint16_t subText = rgb(110, 140, 180);
  
  // 1. Mode Banner
  tft_.fillRoundRect(rx, 76, 170, 36, 6, getModeColor(state_.focusMode));
  drawCenteredText(tft_, pomodoroService_.getModeLabel((PomodoroMode)state_.focusMode), rx + 85, 85, 2, cardBg, getModeColor(state_.focusMode));
  
  // 2. Category Pill
  tft_.fillRoundRect(rx, 130, 170, 48, 6, rgb(8, 20, 42));
  tft_.drawRoundRect(rx, 130, 170, 48, 6, rgb(20, 50, 95));
  tft_.setTextColor(subText, rgb(8, 20, 42));
  tft_.setTextSize(1);
  tft_.setCursor(rx + 10, 136);
  tft_.print("Active Category:");
  
  tft_.setTextColor(TFT_WHITE, rgb(8, 20, 42));
  tft_.setTextSize(1);
  tft_.setCursor(rx + 10, 156);
  tft_.print(state_.focusCategory);
  
  // 3. Session Progress Indicators
  tft_.fillRoundRect(rx, 194, 170, 44, 6, rgb(8, 20, 42));
  tft_.drawRoundRect(rx, 194, 170, 44, 6, rgb(20, 50, 95));
  
  tft_.setTextColor(subText, rgb(8, 20, 42));
  tft_.setTextSize(1);
  tft_.setCursor(rx + 10, 200);
  tft_.print("Completed Today:");
  
  // Draw progress circles (up to 4 focus sessions in a block)
  uint8_t currentSessions = state_.sessionCount % 4;
  for (int i = 0; i < 4; ++i) {
    int16_t circleX = rx + 115 + i * 13;
    int16_t circleY = 205;
    
    if (i < currentSessions) {
      tft_.fillCircle(circleX, circleY, 4, rgb(40, 220, 140)); // Solid green
    } else {
      tft_.drawCircle(circleX, circleY, 4, rgb(30, 60, 100)); // Border only
    }
  }
  
  // Print overall counter text
  char sCountStr[12];
  snprintf(sCountStr, sizeof(sCountStr), "%u completed", state_.sessionCount);
  tft_.setTextColor(TFT_WHITE, rgb(8, 20, 42));
  tft_.setCursor(rx + 10, 222);
  tft_.print(sCountStr);

  // Draw active task association at the bottom
  if (state_.activeTaskId != -1) {
    tft_.fillRoundRect(rx, 248, 170, 48, 6, rgb(8, 20, 42));
    tft_.drawRoundRect(rx, 248, 170, 48, 6, rgb(20, 50, 95));
    tft_.setTextColor(subText, rgb(8, 20, 42));
    tft_.setTextSize(1);
    tft_.setCursor(rx + 10, 254);
    tft_.print("Active Task:");
    
    tft_.setTextColor(TFT_WHITE, rgb(8, 20, 42));
    tft_.setTextSize(1);
    tft_.setCursor(rx + 10, 274);
    
    char taskTitleBuf[22];
    strncpy(taskTitleBuf, state_.activeTaskTitle, sizeof(taskTitleBuf) - 1);
    taskTitleBuf[sizeof(taskTitleBuf) - 1] = '\0';
    tft_.print(taskTitleBuf);
  } else {
    tft_.fillRect(rx, 248, 170, 50, cardBg);
  }
}

void PomodoroScreen::drawCategoryPicker() const {
  const int16_t px = 100;
  const int16_t py = 80;
  const int16_t pw = 280;
  const int16_t ph = 180;
  const uint16_t bg = rgb(6, 18, 38);
  const uint16_t border = rgb(25, 75, 150); // Softer, lower-contrast blue
  const uint16_t textNormal = rgb(170, 200, 235);
  const uint16_t textSelect = TFT_BLACK;
  const uint16_t barSelect = rgb(45, 170, 255);

  // Draw Popup Frame
  tft_.fillRoundRect(px, py, pw, ph, 10, bg);
  tft_.drawRoundRect(px, py, pw, ph, 10, border);
  tft_.drawRoundRect(px + 2, py + 2, pw - 4, ph - 4, 8, rgb(12, 35, 75));

  // Dialog Title
  drawCenteredText(tft_, "SELECT WORK CATEGORY", px + pw/2, py + 12, 1, rgb(45, 170, 255), bg);
  tft_.drawFastHLine(px + 10, py + 26, pw - 20, rgb(20, 60, 120));

  // Render items
  for (int8_t i = 0; i < (int8_t)kNumCategories; ++i) {
    int16_t itemY = py + 34 + i * 26;
    bool isSelected = (i == selectedCategoryIndex_);

    if (isSelected) {
      tft_.fillRoundRect(px + 10, itemY, pw - 20, 22, 4, barSelect);
      tft_.setTextColor(textSelect, barSelect);
    } else {
      tft_.setTextColor(textNormal, bg);
    }
    
    tft_.setTextSize(1);
    tft_.setCursor(px + 20, itemY + 7);
    tft_.print(kCategories[i]);
  }
}

void PomodoroScreen::clearCategoryPicker() const {
  // Redraw black viewport where dialog was sitting
  const int16_t px = 98;
  const int16_t py = 78;
  const int16_t pw = 284;
  const int16_t ph = 184;
  
  // Just clear it using the background rendering loop
  for (int16_t y = py; y < py + ph; y++) {
    const uint8_t r = 2 + (4 * y) / tft_.height();
    const uint8_t g = 6 + (10 * y) / tft_.height();
    const uint8_t b = 15 + (18 * y) / tft_.height();
    tft_.drawFastHLine(px, y, pw, rgb(r, g, b));
  }
}

void PomodoroScreen::drawCancelDialog() const {
  const int16_t px = 110;
  const int16_t py = 95;
  const int16_t pw = 260;
  const int16_t ph = 130;
  const uint16_t bg = rgb(8, 14, 32);
  const uint16_t borderOuter = rgb(140, 40, 40); // Softer red
  const uint16_t borderInner = rgb(80, 20, 20); // Softer dark red
  const uint16_t textDim = rgb(170, 190, 220);
  const uint16_t textBright = TFT_WHITE;
  const uint16_t btnYesBg = rgb(200, 45, 45);
  const uint16_t btnNoBg = rgb(25, 55, 110);
  const uint16_t btnInactive = rgb(12, 28, 55);
  const uint16_t btnBorderActive = rgb(255, 255, 255);
  const uint16_t btnBorderInactive = rgb(40, 70, 120);

  // ── Dialog frame ──
  tft_.fillRoundRect(px, py, pw, ph, 10, bg);
  tft_.drawRoundRect(px, py, pw, ph, 10, borderOuter);
  tft_.drawRoundRect(px + 2, py + 2, pw - 4, ph - 4, 8, borderInner);

  // ── Warning icon: triangle "⚠" approximation ──
  const int16_t triCx = px + pw / 2;
  const int16_t triTop = py + 14;
  tft_.fillTriangle(triCx, triTop, triCx - 10, triTop + 16, triCx + 10, triTop + 16, rgb(255, 180, 40));
  tft_.setTextColor(rgb(30, 10, 0), rgb(255, 180, 40));
  tft_.setTextSize(1);
  tft_.setCursor(triCx - 2, triTop + 6);
  tft_.print("!");

  // ── Question text ──
  drawCenteredText(tft_, "Cancel this session?", px + pw / 2, py + 38, 2, textBright, bg);
  drawCenteredText(tft_, "Progress will be lost", px + pw / 2, py + 60, 1, textDim, bg);

  // ── Separator ──
  tft_.drawFastHLine(px + 16, py + 76, pw - 32, rgb(40, 60, 100));

  // ── Buttons ──
  const int16_t btnW = 90;
  const int16_t btnH = 30;
  const int16_t btnY = py + 86;
  const int16_t btnGap = 24;
  const int16_t yesX = px + pw / 2 - btnW - btnGap / 2;
  const int16_t noX = px + pw / 2 + btnGap / 2;

  // YES button
  if (cancelConfirmSelection_ == 0) {
    tft_.fillRoundRect(yesX, btnY, btnW, btnH, 6, btnYesBg);
    tft_.drawRoundRect(yesX, btnY, btnW, btnH, 6, btnBorderActive);
    drawCenteredText(tft_, "YES", yesX + btnW / 2, btnY + 8, 2, textBright, btnYesBg);
  } else {
    tft_.fillRoundRect(yesX, btnY, btnW, btnH, 6, btnInactive);
    tft_.drawRoundRect(yesX, btnY, btnW, btnH, 6, btnBorderInactive);
    drawCenteredText(tft_, "YES", yesX + btnW / 2, btnY + 8, 2, textDim, btnInactive);
  }

  // NO button
  if (cancelConfirmSelection_ == 1) {
    tft_.fillRoundRect(noX, btnY, btnW, btnH, 6, btnNoBg);
    tft_.drawRoundRect(noX, btnY, btnW, btnH, 6, btnBorderActive);
    drawCenteredText(tft_, "NO", noX + btnW / 2, btnY + 8, 2, textBright, btnNoBg);
  } else {
    tft_.fillRoundRect(noX, btnY, btnW, btnH, 6, btnInactive);
    tft_.drawRoundRect(noX, btnY, btnW, btnH, 6, btnBorderInactive);
    drawCenteredText(tft_, "NO", noX + btnW / 2, btnY + 8, 2, textDim, btnInactive);
  }
}

void PomodoroScreen::clearCancelDialog() const {
  const int16_t px = 108;
  const int16_t py = 93;
  const int16_t pw = 264;
  const int16_t ph = 134;

  for (int16_t y = py; y < py + ph; y++) {
    const uint8_t r = 2 + (4 * y) / tft_.height();
    const uint8_t g = 6 + (10 * y) / tft_.height();
    const uint8_t b = 15 + (18 * y) / tft_.height();
    tft_.drawFastHLine(px, y, pw, rgb(r, g, b));
  }
}

uint16_t PomodoroScreen::getModeColor(uint8_t mode) const {
  switch (mode) {
    case 0: // Focus
      return rgb(45, 140, 255); // Vibrant blue/cyan
    case 1: // ShortBreak
      return rgb(35, 210, 125); // Emerald green
    case 2: // LongBreak
      return rgb(175, 80, 250); // Royal Purple
  }
  return rgb(45, 140, 255);
}
