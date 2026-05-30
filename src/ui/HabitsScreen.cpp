#include "ui/HabitsScreen.h"
#include <Arduino.h>

namespace {
uint16_t rgb(TFT_eSPI &tft, uint8_t r, uint8_t g, uint8_t b) {
  return tft.color565(r, g, b);
}

void drawCenteredText(TFT_eSPI &tft, const char *text, int16_t centerX, int16_t y, uint8_t size, uint16_t color, uint16_t bg) {
  tft.setTextSize(size);
  tft.setTextColor(color, bg);
  const int16_t x = centerX - static_cast<int16_t>(tft.textWidth(text) / 2);
  tft.setCursor(x, y);
  tft.print(text);
}
} // namespace

HabitsScreen::HabitsScreen(TFT_eSPI &tft, DashboardState &state) 
    : tft_(tft), state_(state) {}

void HabitsScreen::onEnter() {
  isActive_ = true;
  Serial.println("[HabitsScreen] onEnter");

  // Align global state.habits count to habits list completed items
  uint8_t completedCount = 0;
  for (uint8_t i = 0; i < HABIT_COUNT; i++) {
    if (habits_[i].completed) {
      completedCount++;
    }
  }
  state_.habits.total = HABIT_COUNT;
  state_.habits.completed = completedCount;

  drawBackground();
  drawHeader();
  drawHabitsList(true);
}

void HabitsScreen::onExit() {
  isActive_ = false;
  Serial.println("[HabitsScreen] onExit");
}

bool HabitsScreen::handleInput(InputEvent event) {
  if (event == InputEvent::DPAD_DOWN) {
    uint8_t oldIndex = selectedIndex_;
    selectedIndex_ = (selectedIndex_ + 1) % HABIT_COUNT;
    drawHabitRow(oldIndex, false);
    drawHabitRow(selectedIndex_, true);
    Serial.printf("[HabitsScreen] Down -> Selected habit %d\n", selectedIndex_);
    return true; // Consumed
  } 
  else if (event == InputEvent::DPAD_UP) {
    uint8_t oldIndex = selectedIndex_;
    selectedIndex_ = (selectedIndex_ + HABIT_COUNT - 1) % HABIT_COUNT;
    drawHabitRow(oldIndex, false);
    drawHabitRow(selectedIndex_, true);
    Serial.printf("[HabitsScreen] Up -> Selected habit %d\n", selectedIndex_);
    return true; // Consumed
  } 
  else if (event == InputEvent::ENCODER_PRESS) {
    // Toggle completed state
    habits_[selectedIndex_].completed = !habits_[selectedIndex_].completed;
    
    // Adjust streak based on check status
    if (habits_[selectedIndex_].completed) {
      habits_[selectedIndex_].streak++;
    } else {
      if (habits_[selectedIndex_].streak > 0) {
        habits_[selectedIndex_].streak--;
      }
    }

    // Sync to global DashboardState
    uint8_t completedCount = 0;
    for (uint8_t i = 0; i < HABIT_COUNT; i++) {
      if (habits_[i].completed) {
        completedCount++;
      }
    }
    state_.habits.completed = completedCount;
    
    // Redraw the list
    drawHabitsList(true);
    Serial.printf("[HabitsScreen] Toggle habit %d -> Completed: %s\n", selectedIndex_, habits_[selectedIndex_].completed ? "YES" : "NO");
    return true; // Consumed
  }
  return false; // Let Left/Right Dpad fall through
}

void HabitsScreen::loop() {
  // Static screen, no dynamic updates needed in loop
}

void HabitsScreen::drawBackground() {
  const int16_t w = tft_.width();
  const int16_t h = tft_.height();

  for (int16_t y = 0; y < h; y++) {
    const uint8_t r = 2 + (6 * y) / h;
    const uint8_t g = 8 + (18 * y) / h;
    const uint8_t b = 20 + (36 * y) / h;
    tft_.drawFastHLine(0, y, w, rgb(tft_, r, g, b));
  }

  tft_.drawRoundRect(3, 3, w - 6, h - 6, 10, rgb(tft_, 18, 40, 80));
  tft_.drawRoundRect(6, 6, w - 12, h - 12, 10, rgb(tft_, 10, 24, 50));
}

void HabitsScreen::drawHeader() {
  const int16_t w = tft_.width();
  const uint16_t accent = rgb(tft_, 35, 230, 150);
  const uint16_t card = rgb(tft_, 3, 14, 32);

  tft_.fillRoundRect(12, 10, w - 24, 36, 6, card);
  tft_.drawRoundRect(12, 10, w - 24, 36, 6, rgb(tft_, 22, 58, 120));

  tft_.setTextColor(accent, card);
  tft_.setTextSize(2);
  tft_.setCursor(24, 20);
  tft_.print("HABITS TRACKER");
}

void HabitsScreen::drawHabitsList(bool force) {
  const int16_t w = tft_.width();
  const int16_t h = tft_.height();
  const uint16_t card = rgb(tft_, 3, 14, 32);
  const uint16_t border = rgb(tft_, 22, 58, 120);

  // Clear habits container
  tft_.fillRoundRect(12, 52, w - 24, h - 64, 8, card);
  tft_.drawRoundRect(12, 52, w - 24, h - 64, 8, border);

  for (uint8_t i = 0; i < HABIT_COUNT; i++) {
    drawHabitRow(i, i == selectedIndex_);
  }
  
  // Summary status line at the bottom
  const uint16_t text = rgb(tft_, 165, 190, 225);
  char summaryText[32];
  snprintf(summaryText, sizeof(summaryText), "%d of %d Habits Completed", state_.habits.completed, state_.habits.total);
  drawCenteredText(tft_, summaryText, w / 2, h - 28, 1, text, card);
}

void HabitsScreen::drawHabitRow(uint8_t index, bool isSelected) {
  const int16_t rowY = 62 + (index * 34);
  const int16_t rowH = 28;
  const int16_t rowW = tft_.width() - 40;
  const int16_t rowX = 20;

  const uint16_t cardBg = rgb(tft_, 3, 14, 32);
  const uint16_t activeSelectionBorder = rgb(tft_, 35, 230, 150);
  const uint16_t activeSelectionBg = rgb(tft_, 6, 32, 45);
  const uint16_t lineBorder = rgb(tft_, 12, 34, 70);

  // Render row background based on selection
  if (isSelected) {
    tft_.fillRoundRect(rowX, rowY, rowW, rowH, 4, activeSelectionBg);
    tft_.drawRoundRect(rowX, rowY, rowW, rowH, 4, activeSelectionBorder);
  } else {
    tft_.fillRoundRect(rowX, rowY, rowW, rowH, 4, cardBg);
    tft_.drawRoundRect(rowX, rowY, rowW, rowH, 4, lineBorder);
  }

  // Draw Check Circle
  const int16_t radius = 7;
  const int16_t circleX = rowX + 17;
  const int16_t circleY = rowY + rowH / 2;
  const uint16_t checkBorderColor = isSelected ? activeSelectionBorder : rgb(tft_, 60, 130, 170);
  const uint16_t checkColor = rgb(tft_, 35, 230, 150);

  tft_.drawCircle(circleX, circleY, radius, checkBorderColor);
  if (habits_[index].completed) {
    tft_.fillCircle(circleX, circleY, radius - 2, checkColor);
  }

  // Draw Streak Text ("🔥 12d")
  char streakText[12];
  snprintf(streakText, sizeof(streakText), "%dd streak", habits_[index].streak);
  const uint16_t streakColor = rgb(tft_, 255, 140, 40); // Orange flame color
  
  tft_.setTextSize(1);
  const uint16_t textBg = isSelected ? activeSelectionBg : cardBg;
  tft_.setTextColor(streakColor, textBg);
  tft_.setCursor(rowX + rowW - 70, rowY + 9);
  tft_.print(streakText);

  // Draw Habit Name
  const uint16_t textColor = habits_[index].completed 
                             ? rgb(tft_, 100, 120, 150) // Gray out completed habits
                             : TFT_WHITE;
  
  tft_.setTextColor(textColor, textBg);
  tft_.setCursor(circleX + radius + 12, rowY + 9);
  tft_.print(habits_[index].title);
}
