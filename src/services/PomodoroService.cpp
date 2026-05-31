#include "services/PomodoroService.h"
#include <ArduinoJson.h>

void PomodoroService::begin(DashboardState &state, LocalFirstSyncService &syncService) {
  state_ = &state;
  sync_ = &syncService;
  
  // Set up initial state structures
  state_->focusRunning = running_;
  state_->focusMode = (uint8_t)currentMode_;
  state_->focusSecondsRemaining = secondsRemaining_;
  state_->sessionCount = sessionCount_;
  strncpy(state_->focusCategory, category_, sizeof(state_->focusCategory) - 1);
  state_->focusCategory[sizeof(state_->focusCategory) - 1] = '\0';
}

void PomodoroService::tick() {
  if (state_ && state_->sessionCount != sessionCount_) {
    sessionCount_ = state_->sessionCount;
  }

  if (!running_) {
    return;
  }
  
  const uint32_t now = millis();
  if (now - lastTickMs_ >= 1000) {
    lastTickMs_ += 1000;
    
    if (secondsRemaining_ > 0) {
      secondsRemaining_--;
      state_->focusSecondsRemaining = secondsRemaining_;
    }
    
    if (secondsRemaining_ == 0) {
      handleComplete();
    }
  }
}

void PomodoroService::start() {
  if (running_) return;
  
  // If we are starting from the very beginning, queue the start event
  const uint32_t totalDuration = getDurationSeconds(currentMode_);
  if (secondsRemaining_ == totalDuration) {
    queueStartEvent();
  }
  
  running_ = true;
  lastTickMs_ = millis();
  state_->focusRunning = true;
  Serial.printf("[Pomodoro] Started %s mode\n", getModeLabel(currentMode_));
}

void PomodoroService::pause() {
  if (!running_) return;
  running_ = false;
  state_->focusRunning = false;
  Serial.println("[Pomodoro] Paused");
}

void PomodoroService::resume() {
  if (running_) return;
  running_ = true;
  lastTickMs_ = millis();
  state_->focusRunning = true;
  Serial.println("[Pomodoro] Resumed");
}

void PomodoroService::reset() {
  if (running_ || activeSessionUuid_.length() > 0) {
    queueAbandonEvent();
  }
  
  running_ = false;
  activeSessionUuid_ = "";
  secondsRemaining_ = getDurationSeconds(currentMode_);
  
  state_->focusRunning = false;
  state_->focusSecondsRemaining = secondsRemaining_;
  
  if (state_) {
    state_->activeTaskId = -1;
    state_->activeTaskTitle[0] = '\0';
  }
  Serial.println("[Pomodoro] Reset to default duration and cleared active task");
}

void PomodoroService::setCategory(const char* category) {
  if (category == nullptr || category[0] == '\0') return;
  strncpy(category_, category, sizeof(category_) - 1);
  category_[sizeof(category_) - 1] = '\0';
  if (state_) {
    strncpy(state_->focusCategory, category_, sizeof(state_->focusCategory) - 1);
    state_->focusCategory[sizeof(state_->focusCategory) - 1] = '\0';
  }
  Serial.printf("[Pomodoro] Category set to %s\n", category_);
}

void PomodoroService::setMode(PomodoroMode mode) {
  if (running_) return;
  currentMode_ = mode;
  secondsRemaining_ = getDurationSeconds(currentMode_);
  
  state_->focusMode = (uint8_t)currentMode_;
  state_->focusSecondsRemaining = secondsRemaining_;
  Serial.printf("[Pomodoro] Mode manually set to %s\n", getModeLabel(currentMode_));
}

void PomodoroService::handleComplete() {
  running_ = false;
  state_->focusRunning = false;
  
  queueCompleteEvent();

  if (currentMode_ == PomodoroMode::Focus) {
    if (state_) {
      state_->activeTaskId = -1;
      state_->activeTaskTitle[0] = '\0';
    }
  }
  
  // Shift modes
  if (currentMode_ == PomodoroMode::Focus) {
    sessionCount_++;
    state_->sessionCount = sessionCount_;
    
    if (sessionCount_ % 4 == 0) {
      currentMode_ = PomodoroMode::LongBreak;
    } else {
      currentMode_ = PomodoroMode::ShortBreak;
    }
  } else {
    // Finished a break, return to focus work
    currentMode_ = PomodoroMode::Focus;
  }
  
  secondsRemaining_ = getDurationSeconds(currentMode_);
  state_->focusMode = (uint8_t)currentMode_;
  state_->focusSecondsRemaining = secondsRemaining_;
  
  Serial.printf("[Pomodoro] Completed session! Next mode: %s\n", getModeLabel(currentMode_));
  
  if (autoStart_) {
    Serial.println("[Pomodoro] Auto-starting next mode...");
    start();
  }
}

uint32_t PomodoroService::getDurationSeconds(PomodoroMode mode) const {
  switch (mode) {
    case PomodoroMode::Focus:
      return 25 * 60;
    case PomodoroMode::ShortBreak:
      return 5 * 60;
    case PomodoroMode::LongBreak:
      return 15 * 60;
  }
  return 25 * 60;
}

const char* PomodoroService::getModeLabel(PomodoroMode mode) const {
  switch (mode) {
    case PomodoroMode::Focus:
      return "Focus";
    case PomodoroMode::ShortBreak:
      return "Short Break";
    case PomodoroMode::LongBreak:
      return "Long Break";
  }
  return "Unknown";
}

void PomodoroService::queueStartEvent() {
  JsonDocument doc;
  if (currentMode_ == PomodoroMode::Focus) {
    doc["type"] = "focus";
    doc["category"] = category_;
    if (state_ && state_->activeTaskId != -1) {
      doc["taskId"] = state_->activeTaskId;
    }
  } else if (currentMode_ == PomodoroMode::ShortBreak) {
    doc["type"] = "short_break";
    doc["category"] = "Break";
  } else {
    doc["type"] = "long_break";
    doc["category"] = "Break";
  }
  
  doc["durationMins"] = getDurationSeconds(currentMode_) / 60;
  
  String payload;
  serializeJson(doc, payload);
  
  activeSessionUuid_ = sync_->queueEvent("POST", "/pomodoro/sessions", payload);
}

void PomodoroService::queueCompleteEvent() {
  if (activeSessionUuid_.length() == 0) return;
  
  JsonDocument doc;
  doc["actualMins"] = getDurationSeconds(currentMode_) / 60;
  
  String payload;
  serializeJson(doc, payload);
  
  sync_->queueEvent("PATCH", "/pomodoro/sessions/{id}/complete", payload, activeSessionUuid_);
  activeSessionUuid_ = "";
}

void PomodoroService::queueAbandonEvent() {
  if (activeSessionUuid_.length() == 0) return;
  
  sync_->queueEvent("PATCH", "/pomodoro/sessions/{id}/abandon", "{}", activeSessionUuid_);
  activeSessionUuid_ = "";
}
