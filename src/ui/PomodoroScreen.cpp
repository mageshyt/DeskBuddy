#include "ui/PomodoroScreen.h"
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

PomodoroScreen::PomodoroScreen(TFT_eSPI &tft, DashboardState &state) 
    : tft_(tft), state_(state) {}

void PomodoroScreen::onEnter() {
  isActive_ = true;
  Serial.println("[PomodoroScreen] onEnter");
  drawBackground();
  drawHeader();
  drawTimerFrame();
  drawTimerValue(true);
}

void PomodoroScreen::onExit() {
  isActive_ = false;
  Serial.println("[PomodoroScreen] onExit");
}

bool PomodoroScreen::handleInput(InputEvent event) {
  if (event == InputEvent::ENCODER_PRESS) {
    state_.focusRunning = !state_.focusRunning;
    Serial.printf("[PomodoroScreen] Encoder Press -> Toggle Focus: %s\n", state_.focusRunning ? "RUNNING" : "PAUSED");
    drawTimerValue(true);
    return true; // Consumed event
  }
  return false; // DPAD Left/Right will navigate screens
}

void PomodoroScreen::loop() {
  if (!isActive_) return;

  // Periodically refresh the timer display if state changes
  if (state_.focusRunning != lastFocusRunning_ || state_.focusMinutesRemaining != lastMinutes_) {
    drawTimerValue();
  }
}

void PomodoroScreen::drawBackground() {
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

void PomodoroScreen::drawHeader() {
  const int16_t w = tft_.width();
  const uint16_t accent = rgb(tft_, 40, 130, 255);
  const uint16_t card = rgb(tft_, 3, 14, 32);

  // Draw Header Panel
  tft_.fillRoundRect(12, 10, w - 24, 36, 6, card);
  tft_.drawRoundRect(12, 10, w - 24, 36, 6, rgb(tft_, 22, 58, 120));

  tft_.setTextColor(accent, card);
  tft_.setTextSize(2);
  tft_.setCursor(24, 20);
  tft_.print("POMODORO TIMERS");
}

void PomodoroScreen::drawTimerFrame() {
  const int16_t w = tft_.width();
  const int16_t h = tft_.height();
  const uint16_t card = rgb(tft_, 3, 14, 32);
  const uint16_t border = rgb(tft_, 22, 58, 120);

  // Main Timer Panel
  tft_.fillRoundRect(12, 52, w - 24, h - 64, 8, card);
  tft_.drawRoundRect(12, 52, w - 24, h - 64, 8, border);

  // Decorative Timer Circle / Dial
  tft_.drawCircle(w / 2, 125, 52, rgb(tft_, 22, 58, 120));
  tft_.drawCircle(w / 2, 125, 50, rgb(tft_, 10, 30, 70));
}

void PomodoroScreen::drawTimerValue(bool force) {
  if (!force && state_.focusRunning == lastFocusRunning_ && state_.focusMinutesRemaining == lastMinutes_) {
    return;
  }

  const int16_t w = tft_.width();
  const uint16_t card = rgb(tft_, 3, 14, 32);
  const uint16_t text = rgb(tft_, 180, 205, 245);
  const uint16_t activeGreen = rgb(tft_, 35, 230, 150);
  const uint16_t pauseBlue = rgb(tft_, 50, 145, 255);

  // Clear inner circle text
  tft_.fillCircle(w / 2, 125, 42, card);

  // Print remaining time in the center of the ring
  char timeText[12];
  snprintf(timeText, sizeof(timeText), "%02u:00", state_.focusMinutesRemaining);
  drawCenteredText(tft_, timeText, w / 2, 110, 3, TFT_WHITE, card);

  // Draw status indicator text and play/pause visual
  if (state_.focusRunning) {
    drawCenteredText(tft_, "FOCUS ACTIVE", w / 2, 185, 1, activeGreen, card);
    // Draw green dial indicator highlight
    tft_.drawCircle(w / 2, 125, 51, activeGreen);
  } else {
    drawCenteredText(tft_, "PAUSED", w / 2, 185, 1, pauseBlue, card);
    // Draw blue dial indicator highlight
    tft_.drawCircle(w / 2, 125, 51, pauseBlue);
  }

  drawCenteredText(tft_, "Press Encoder to Start/Pause", w / 2, 205, 1, text, card);

  lastFocusRunning_ = state_.focusRunning;
  lastMinutes_ = state_.focusMinutesRemaining;
}
