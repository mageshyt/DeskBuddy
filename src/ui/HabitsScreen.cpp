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

HabitsScreen::HabitsScreen(TFT_eSPI &tft, DashboardState &state, SyncRestService &restSync, LocalFirstSyncService &localSync) 
    : tft_(tft), state_(state), restSync_(restSync), localSync_(localSync) {}

void HabitsScreen::onEnter() {
  isActive_ = true;
  Serial.println("[HabitsScreen] onEnter");

  // Load from local flash cache immediately
  restSync_.loadHabitsCache(habits_, habitCount_, MAX_HABITS);
  
  // Align initial state
  uint8_t completedCount = 0;
  bestStreak_ = 0;
  for (uint8_t i = 0; i < habitCount_; i++) {
    if (habits_[i].completed) {
      completedCount++;
    }
    if (habits_[i].streak > bestStreak_) {
      bestStreak_ = habits_[i].streak;
    }
  }
  state_.habits.total = habitCount_;
  state_.habits.completed = completedCount;
  
  if (selectedIndex_ >= habitCount_ && habitCount_ > 0) {
    selectedIndex_ = habitCount_ - 1;
  }
  scrollOffset_ = 0;

  drawBackground();
  drawHeader();
  drawHabitsList(true);
  
  // Determine if cache is stale or invalidated
  const uint32_t now = millis();
  const bool isStale = (state_.lastHabitsFetchMs == 0) || 
                       (now - state_.lastHabitsFetchMs > 30 * 60 * 1000) || 
                       state_.habitsNeedRefetch;

  if (isStale) {
    Serial.println("[HabitsScreen] Cache is stale/invalidated. Fetching from server...");
    uint8_t serverCount = 0;
    LocalHabitItem serverHabits[MAX_HABITS];
    if (restSync_.fetchHabits(serverHabits, serverCount, MAX_HABITS)) {
      habitCount_ = serverCount;
      completedCount = 0;
      bestStreak_ = 0;
      for (uint8_t i = 0; i < habitCount_; i++) {
        habits_[i] = serverHabits[i];
        if (habits_[i].completed) {
          completedCount++;
        }
        if (habits_[i].streak > bestStreak_) {
          bestStreak_ = habits_[i].streak;
        }
      }
      state_.habits.total = habitCount_;
      state_.habits.completed = completedCount;
      
      // Update cache timestamps and reset flags
      state_.lastHabitsFetchMs = now;
      state_.habitsNeedRefetch = false;
      
      drawHabitsList(true);
    }
  } else {
    Serial.println("[HabitsScreen] Loaded from cache (no network fetch needed)");
  }
}

void HabitsScreen::onExit() {
  isActive_ = false;
  Serial.println("[HabitsScreen] onExit");
}

bool HabitsScreen::handleInput(InputEvent event) {
  if (habitCount_ == 0) return false;
  
  if (event == InputEvent::DPAD_DOWN) {
    uint8_t oldIndex = selectedIndex_;
    selectedIndex_ = (selectedIndex_ + 1) % habitCount_;
    
    // Scroll management
    if (selectedIndex_ < oldIndex && selectedIndex_ == 0) {
      scrollOffset_ = 0;
      drawHabitsList(true);
    } else if (selectedIndex_ >= scrollOffset_ + 4) {
      scrollOffset_ = selectedIndex_ - 3;
      drawHabitsList(true);
    } else {
      drawHabitRow(oldIndex, false);
      drawHabitRow(selectedIndex_, true);
    }
    return true; // Consumed
  } 
  else if (event == InputEvent::DPAD_UP) {
    uint8_t oldIndex = selectedIndex_;
    selectedIndex_ = (selectedIndex_ + habitCount_ - 1) % habitCount_;
    
    // Scroll management
    if (selectedIndex_ > oldIndex && selectedIndex_ == habitCount_ - 1) {
      scrollOffset_ = (habitCount_ > 4) ? habitCount_ - 4 : 0;
      drawHabitsList(true);
    } else if (selectedIndex_ < scrollOffset_) {
      scrollOffset_ = selectedIndex_;
      drawHabitsList(true);
    } else {
      drawHabitRow(oldIndex, false);
      drawHabitRow(selectedIndex_, true);
    }
    return true; // Consumed
  } 
  else if (event == InputEvent::ENCODER_PRESS) {
    // 1. Optimistic Local Update
    habits_[selectedIndex_].completed = !habits_[selectedIndex_].completed;
    
    if (habits_[selectedIndex_].completed) {
      habits_[selectedIndex_].streak++;
    } else {
      if (habits_[selectedIndex_].streak > 0) {
        habits_[selectedIndex_].streak--;
      }
    }
    
    // 2. Persist Cache
    restSync_.saveHabitsCache(habits_, habitCount_);
    
    // 3. Align state counter
    uint8_t completedCount = 0;
    bestStreak_ = 0;
    for (uint8_t i = 0; i < habitCount_; i++) {
      if (habits_[i].completed) {
        completedCount++;
      }
      if (habits_[i].streak > bestStreak_) {
        bestStreak_ = habits_[i].streak;
      }
    }
    state_.habits.completed = completedCount;
    
    // 4. Enqueue LocalFirst Sync Event
    char toggleUrl[64];
    snprintf(toggleUrl, sizeof(toggleUrl), "/habits/%d/toggle", habits_[selectedIndex_].id);
    localSync_.queueEvent("POST", toggleUrl, "{}");
    
    // Redraw
    drawHabitsList(true);
    Serial.printf("[HabitsScreen] Optimistic Toggle -> Completed: %s, Streak: %d, Queued Sync.\n", 
                  habits_[selectedIndex_].completed ? "YES" : "NO", habits_[selectedIndex_].streak);
    return true; // Consumed
  }
  return false;
}

void HabitsScreen::loop() {
  if (!isActive_) return;

  // Only refetch when a WebSocket event invalidates the cache
  if (state_.habitsNeedRefetch) {
    Serial.println("[HabitsScreen] Invalidation flag set. Refetching habits list...");
    uint8_t serverCount = 0;
    LocalHabitItem serverHabits[MAX_HABITS];
    if (restSync_.fetchHabits(serverHabits, serverCount, MAX_HABITS)) {
      habitCount_ = serverCount;
      uint8_t completedCount = 0;
      bestStreak_ = 0;
      for (uint8_t i = 0; i < habitCount_; i++) {
        habits_[i] = serverHabits[i];
        if (habits_[i].completed) {
          completedCount++;
        }
        if (habits_[i].streak > bestStreak_) {
          bestStreak_ = habits_[i].streak;
        }
      }
      state_.habits.total = habitCount_;
      state_.habits.completed = completedCount;
      
      // Update cache timestamp and clear flag
      state_.lastHabitsFetchMs = millis();
      state_.habitsNeedRefetch = false;
      
      drawHabitsList(true);
    }
  }
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
  
  tft_.setTextColor(rgb(tft_, 100, 130, 170), card);
  tft_.setTextSize(1);
  tft_.setCursor(w - 180, 23);
  tft_.print("LOCAL-FIRST ACTIVE");
}

void HabitsScreen::drawHabitsList(bool force) {
  const int16_t w = tft_.width();
  const int16_t h = tft_.height();
  const uint16_t card = rgb(tft_, 3, 14, 32);
  const uint16_t border = rgb(tft_, 22, 58, 120);

  // Clear habits container
  tft_.fillRoundRect(12, 52, w - 24, h - 64, 8, card);
  tft_.drawRoundRect(12, 52, w - 24, h - 64, 8, border);

  if (habitCount_ == 0) {
    drawCenteredText(tft_, "No habits configured.", w / 2, h / 2 - 10, 2, rgb(tft_, 120, 150, 180), card);
    return;
  }

  uint8_t drawCount = (habitCount_ - scrollOffset_ < 4) ? habitCount_ - scrollOffset_ : 4;
  for (uint8_t i = 0; i < drawCount; i++) {
    uint8_t actualIndex = scrollOffset_ + i;
    drawHabitRow(actualIndex, actualIndex == selectedIndex_);
  }
  
  // Summary status line at the bottom
  const uint16_t text = rgb(tft_, 165, 190, 225);
  char summaryText[64];
  snprintf(summaryText, sizeof(summaryText), "%d of %d Habits Completed Today", state_.habits.completed, state_.habits.total);
  drawCenteredText(tft_, summaryText, w / 2, h - 28, 1, text, card);
}

void HabitsScreen::drawHabitRow(uint8_t index, bool isSelected) {
  const uint8_t viewIndex = index - scrollOffset_;
  const int16_t rowY = 60 + (viewIndex * 48);
  const int16_t rowH = 40;
  const int16_t rowW = tft_.width() - 40;
  const int16_t rowX = 20;

  const uint16_t cardBg = rgb(tft_, 3, 14, 32);
  const uint16_t activeSelectionBorder = rgb(tft_, 35, 230, 150);
  const uint16_t activeSelectionBg = rgb(tft_, 6, 32, 45);
  const uint16_t lineBorder = rgb(tft_, 12, 34, 70);

  // Render row background based on selection
  if (isSelected) {
    tft_.fillRoundRect(rowX, rowY, rowW, rowH, 6, activeSelectionBg);
    tft_.drawRoundRect(rowX, rowY, rowW, rowH, 6, activeSelectionBorder);
  } else {
    tft_.fillRoundRect(rowX, rowY, rowW, rowH, 6, cardBg);
    tft_.drawRoundRect(rowX, rowY, rowW, rowH, 6, lineBorder);
  }

  const uint16_t textBg = isSelected ? activeSelectionBg : cardBg;

  // 1. Draw Check Circle
  const int16_t radius = 8;
  const int16_t circleX = rowX + 18;
  const int16_t circleY = rowY + rowH / 2;
  const uint16_t checkBorderColor = isSelected ? activeSelectionBorder : rgb(tft_, 60, 130, 170);
  const uint16_t checkColor = rgb(tft_, 35, 230, 150);

  tft_.drawCircle(circleX, circleY, radius, checkBorderColor);
  if (habits_[index].completed) {
    tft_.fillCircle(circleX, circleY, radius - 2, checkColor);
  }

  // 2. Draw Habit Name
  const uint16_t textColor = habits_[index].completed 
                             ? rgb(tft_, 100, 120, 150) // Gray out completed habits
                             : TFT_WHITE;
  
  tft_.setTextSize(2);
  tft_.setTextColor(textColor, textBg);
  tft_.setCursor(circleX + radius + 15, rowY + 12);
  
  char titleBuf[24];
  strncpy(titleBuf, habits_[index].title, sizeof(titleBuf) - 1);
  titleBuf[sizeof(titleBuf) - 1] = '\0';
  tft_.print(titleBuf);

  // 3. Draw Weekly Sparkline Dot Matrix
  const int16_t sparkX = rowX + 340;
  tft_.setTextSize(1);
  
  for (uint8_t day = 0; day < 7; ++day) {
    int16_t dotX = sparkX + (day * 12);
    int16_t dotY = rowY + (rowH - 8) / 2;
    uint16_t dotColor = rgb(tft_, 15, 35, 75); // 'e' empty
    
    char status = habits_[index].history[day];
    if (status == 'o') {
      dotColor = rgb(tft_, 35, 230, 150); // optimized/done
    } else if (status == 'm') {
      dotColor = rgb(tft_, 230, 80, 80); // missed
    }
    
    tft_.fillRoundRect(dotX, dotY, 8, 8, 2, dotColor);
  }
}
