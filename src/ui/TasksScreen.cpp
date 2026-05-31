#include "ui/TasksScreen.h"
#include "services/NavigationService.h"
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

TasksScreen::TasksScreen(TFT_eSPI &tft, DashboardState &state, SyncRestService &restSync, LocalFirstSyncService &localSync, NavigationService &nav) 
    : tft_(tft), state_(state), restSync_(restSync), localSync_(localSync), nav_(nav) {}

void TasksScreen::onEnter() {
  isActive_ = true;
  Serial.println("[TasksScreen] onEnter");

  // Load from local cache immediately
  restSync_.loadTasksCache(tasks_, taskCount_, MAX_TASKS);
  
  // Align initial state
  uint8_t completedCount = 0;
  for (uint8_t i = 0; i < taskCount_; i++) {
    if (tasks_[i].completed) {
      completedCount++;
    }
  }
  state_.tasks.total = taskCount_;
  state_.tasks.completed = completedCount;
  
  if (selectedIndex_ >= taskCount_ && taskCount_ > 0) {
    selectedIndex_ = taskCount_ - 1;
  }
  scrollOffset_ = 0;

  drawBackground();
  drawHeader();
  drawTaskList(true);
  
  // Determine if cache is stale or invalidated
  const uint32_t now = millis();
  const bool isStale = (state_.lastTasksFetchMs == 0) || 
                       (now - state_.lastTasksFetchMs > 30 * 60 * 1000) || 
                       state_.tasksNeedRefetch;

  if (isStale) {
    Serial.println("[TasksScreen] Cache is stale/invalidated. Fetching from server...");
    uint8_t serverCount = 0;
    LocalTaskItem serverTasks[MAX_TASKS];
    if (restSync_.fetchTasks(serverTasks, serverCount, MAX_TASKS)) {
      taskCount_ = serverCount;
      completedCount = 0;
      for (uint8_t i = 0; i < taskCount_; i++) {
        tasks_[i] = serverTasks[i];
        if (tasks_[i].completed) {
          completedCount++;
        }
      }
      state_.tasks.total = taskCount_;
      state_.tasks.completed = completedCount;
      
      // Update cache timestamp and clear flag
      state_.lastTasksFetchMs = now;
      state_.tasksNeedRefetch = false;
      
      drawTaskList(true);
    }
  } else {
    Serial.println("[TasksScreen] Loaded from cache (no network fetch needed)");
  }
}

void TasksScreen::onExit() {
  isActive_ = false;
  Serial.println("[TasksScreen] onExit");
}

bool TasksScreen::handleInput(InputEvent event) {
  if (taskCount_ == 0) return false;
  
  if (event == InputEvent::DPAD_DOWN) {
    uint8_t oldIndex = selectedIndex_;
    selectedIndex_ = (selectedIndex_ + 1) % taskCount_;
    
    // Scroll management
    if (selectedIndex_ < oldIndex && selectedIndex_ == 0) {
      scrollOffset_ = 0;
      drawTaskList(true);
    } else if (selectedIndex_ >= scrollOffset_ + 4) {
      scrollOffset_ = selectedIndex_ - 3;
      drawTaskList(true);
    } else {
      drawTaskRow(oldIndex, false);
      drawTaskRow(selectedIndex_, true);
    }
    return true; // Consumed
  } 
  else if (event == InputEvent::DPAD_UP) {
    uint8_t oldIndex = selectedIndex_;
    selectedIndex_ = (selectedIndex_ + taskCount_ - 1) % taskCount_;
    
    // Scroll management
    if (selectedIndex_ > oldIndex && selectedIndex_ == taskCount_ - 1) {
      scrollOffset_ = (taskCount_ > 4) ? taskCount_ - 4 : 0;
      drawTaskList(true);
    } else if (selectedIndex_ < scrollOffset_) {
      scrollOffset_ = selectedIndex_;
      drawTaskList(true);
    } else {
      drawTaskRow(oldIndex, false);
      drawTaskRow(selectedIndex_, true);
    }
    return true; // Consumed
  } 
  else if (event == InputEvent::ENCODER_PRESS) {
    // 1. Optimistic Local Update
    tasks_[selectedIndex_].completed = !tasks_[selectedIndex_].completed;
    
    // 2. Persist Cache
    restSync_.saveTasksCache(tasks_, taskCount_);
    
    // 3. Align state counter
    uint8_t completedCount = 0;
    for (uint8_t i = 0; i < taskCount_; i++) {
      if (tasks_[i].completed) {
        completedCount++;
      }
    }
    state_.tasks.completed = completedCount;
    
    // 4. Enqueue LocalFirst Sync Event (PATCH /tasks/{id} with payload {"done": true/false})
    char taskUrl[64];
    snprintf(taskUrl, sizeof(taskUrl), "/tasks/%d", tasks_[selectedIndex_].id);
    
    char payload[32];
    snprintf(payload, sizeof(payload), "{\"done\":%s}", tasks_[selectedIndex_].completed ? "true" : "false");
    localSync_.queueEvent("PATCH", taskUrl, payload);
    
    // Redraw
    drawTaskList(true);
    Serial.printf("[TasksScreen] Optimistic Toggle -> Completed: %s, Queued Sync.\n", 
                  tasks_[selectedIndex_].completed ? "YES" : "NO");
    return true; // Consumed
  }
  else if (event == InputEvent::ENCODER_LONG_PRESS) {
    // 1. Associate selected task with Pomodoro focus session
    state_.activeTaskId = tasks_[selectedIndex_].id;
    strncpy(state_.activeTaskTitle, tasks_[selectedIndex_].title, sizeof(state_.activeTaskTitle) - 1);
    state_.activeTaskTitle[sizeof(state_.activeTaskTitle) - 1] = '\0';
    
    // 2. Configure Pomodoro state for focus session on this task
    state_.focusMode = 0; // FocusMode
    state_.focusRunning = false; // Ready, not running initially
    state_.focusSecondsRemaining = 25 * 60; // 25 minutes
    
    Serial.printf("[TasksScreen] Spin up Pomodoro for task ID %d: '%s'\n", state_.activeTaskId, state_.activeTaskTitle);
    
    // 3. Switch screen to PomodoroScreen (Index 1)
    nav_.navigateTo(1);
    return true; // Consumed
  }
  return false;
}

void TasksScreen::loop() {
  if (!isActive_) return;

  // Only refetch when a WebSocket event invalidates the cache
  if (state_.tasksNeedRefetch) {
    Serial.println("[TasksScreen] Invalidation flag set. Refetching tasks list...");
    uint8_t serverCount = 0;
    LocalTaskItem serverTasks[MAX_TASKS];
    if (restSync_.fetchTasks(serverTasks, serverCount, MAX_TASKS)) {
      taskCount_ = serverCount;
      uint8_t completedCount = 0;
      for (uint8_t i = 0; i < taskCount_; i++) {
        tasks_[i] = serverTasks[i];
        if (tasks_[i].completed) {
          completedCount++;
        }
      }
      state_.tasks.total = taskCount_;
      state_.tasks.completed = completedCount;
      
      // Update cache timestamp and clear flag
      state_.lastTasksFetchMs = millis();
      state_.tasksNeedRefetch = false;
      
      drawTaskList(true);
    }
  }
}

void TasksScreen::drawBackground() {
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

void TasksScreen::drawHeader() {
  const int16_t w = tft_.width();
  const uint16_t accent = rgb(tft_, 50, 145, 255);
  const uint16_t card = rgb(tft_, 3, 14, 32);

  tft_.fillRoundRect(12, 10, w - 24, 36, 6, card);
  tft_.drawRoundRect(12, 10, w - 24, 36, 6, rgb(tft_, 22, 58, 120));

  tft_.setTextColor(accent, card);
  tft_.setTextSize(2);
  tft_.setCursor(24, 20);
  tft_.print("DAILY TASKS LIST");
  
  tft_.setTextColor(rgb(tft_, 100, 130, 170), card);
  tft_.setTextSize(1);
  tft_.setCursor(w - 180, 23);
  tft_.print("LOCAL-FIRST ACTIVE");
}

void TasksScreen::drawTaskList(bool force) {
  const int16_t w = tft_.width();
  const int16_t h = tft_.height();
  const uint16_t card = rgb(tft_, 3, 14, 32);
  const uint16_t border = rgb(tft_, 22, 58, 120);

  // Clear task area container
  tft_.fillRoundRect(12, 52, w - 24, h - 64, 8, card);
  tft_.drawRoundRect(12, 52, w - 24, h - 64, 8, border);

  if (taskCount_ == 0) {
    drawCenteredText(tft_, "No tasks scheduled for today.", w / 2, h / 2 - 10, 2, rgb(tft_, 120, 150, 180), card);
    return;
  }

  uint8_t drawCount = (taskCount_ - scrollOffset_ < 4) ? taskCount_ - scrollOffset_ : 4;
  for (uint8_t i = 0; i < drawCount; i++) {
    uint8_t actualIndex = scrollOffset_ + i;
    drawTaskRow(actualIndex, actualIndex == selectedIndex_);
  }
  
  // Summary status line at the bottom
  const uint16_t text = rgb(tft_, 165, 190, 225);
  char summaryText[32];
  snprintf(summaryText, sizeof(summaryText), "%d of %d Tasks Done", state_.tasks.completed, state_.tasks.total);
  drawCenteredText(tft_, summaryText, w / 2, h - 28, 1, text, card);
}

void TasksScreen::drawTaskRow(uint8_t index, bool isSelected) {
  const uint8_t viewIndex = index - scrollOffset_;
  const int16_t rowY = 60 + (viewIndex * 48);
  const int16_t rowH = 40;
  const int16_t rowW = tft_.width() - 40;
  const int16_t rowX = 20;

  const uint16_t cardBg = rgb(tft_, 3, 14, 32);
  const uint16_t activeSelectionBorder = rgb(tft_, 40, 130, 255);
  const uint16_t activeSelectionBg = rgb(tft_, 6, 26, 60);
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

  // Draw Checkbox
  const int16_t checkSize = 16;
  const int16_t checkX = rowX + 15;
  const int16_t checkY = rowY + (rowH - checkSize) / 2;
  const uint16_t checkBorderColor = isSelected ? activeSelectionBorder : rgb(tft_, 60, 130, 170);
  const uint16_t checkColor = rgb(tft_, 40, 220, 140);

  tft_.drawRect(checkX, checkY, checkSize, checkSize, checkBorderColor);
  if (tasks_[index].completed) {
    tft_.fillRect(checkX + 2, checkY + 2, checkSize - 4, checkSize - 4, checkColor);
  }

  // Draw Task Text
  tft_.setTextSize(2);
  const uint16_t textColor = tasks_[index].completed 
                             ? rgb(tft_, 100, 120, 150) // Gray out completed tasks
                             : TFT_WHITE;
  
  tft_.setTextColor(textColor, textBg);
  tft_.setCursor(checkX + checkSize + 15, rowY + 12);
  
  char titleBuf[32];
  strncpy(titleBuf, tasks_[index].title, sizeof(titleBuf) - 1);
  titleBuf[sizeof(titleBuf) - 1] = '\0';
  tft_.print(titleBuf);
}
