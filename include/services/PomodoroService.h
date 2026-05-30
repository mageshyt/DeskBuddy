#pragma once

#include <Arduino.h>
#include "models/DashboardState.h"
#include "services/LocalFirstSyncService.h"

enum class PomodoroMode : uint8_t {
  Focus = 0,
  ShortBreak = 1,
  LongBreak = 2
};

class PomodoroService {
 public:
  void begin(DashboardState &state, LocalFirstSyncService &syncService);
  void tick();
  
  // Actions
  void start();
  void pause();
  void resume();
  void reset(); // Abandon current session
  
  void setCategory(const char* category);
  void setMode(PomodoroMode mode);
  void setAutoStart(bool enable) { autoStart_ = enable; }
  
  bool isRunning() const { return running_; }
  PomodoroMode getMode() const { return currentMode_; }
  uint16_t getSecondsRemaining() const { return secondsRemaining_; }
  const char* getCategory() const { return category_; }
  bool getAutoStart() const { return autoStart_; }
  
  // Helpers
  uint32_t getDurationSeconds(PomodoroMode mode) const;
  const char* getModeLabel(PomodoroMode mode) const;

 private:
  void handleComplete();
  void queueStartEvent();
  void queueCompleteEvent();
  void queueAbandonEvent();
  
  DashboardState* state_ = nullptr;
  LocalFirstSyncService* sync_ = nullptr;
  
  bool running_ = false;
  PomodoroMode currentMode_ = PomodoroMode::Focus;
  uint16_t secondsRemaining_ = 25 * 60;
  char category_[20] = "General";
  uint8_t sessionCount_ = 0;
  bool autoStart_ = true; // Auto-transition and start next mode
  
  uint32_t lastTickMs_ = 0;
  
  // Track active session UUID in LocalFirstSyncService
  String activeSessionUuid_ = "";
};
