#include "ui/MainDashboardScreen.h"

#include <Arduino.h>

namespace {
const char *const kWeekdays[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
const char *const kMonths[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

constexpr int16_t kTopCardX = 12;
constexpr int16_t kTopCardY = 10;
constexpr int16_t kTopCardH = 170;

constexpr int16_t kBottomCardsY = 190;
constexpr int16_t kBottomCardsGap = 6;
constexpr int16_t kCardsMarginX = 12;

uint16_t rgb(const TFT_eSPI &tft, uint8_t r, uint8_t g, uint8_t b) {
  return const_cast<TFT_eSPI &>(tft).color565(r, g, b);
}

uint8_t percentOf(uint8_t value, uint8_t total) {
  if (total == 0) {
    return 0;
  }
  uint16_t p = static_cast<uint16_t>(value) * 100U / total;
  if (p > 100U) {
    p = 100U;
  }
  return static_cast<uint8_t>(p);
}

bool sameClock(const ClockSnapshot &a, const ClockSnapshot &b) {
  // Seconds are intentionally ignored to avoid unnecessary full-screen redraw flicker.
  return a.year == b.year && a.month == b.month && a.day == b.day && a.hour == b.hour && a.minute == b.minute &&
         a.weekdayIndex == b.weekdayIndex;
}

bool sameProgress(const DashboardProgress &a, const DashboardProgress &b) {
  return a.completed == b.completed && a.total == b.total;
}

void drawCenteredText(TFT_eSPI &tft, const char *text, int16_t centerX, int16_t y, uint8_t size, uint16_t color,
                      uint16_t bg) {
  tft.setTextSize(size);
  tft.setTextColor(color, bg);
  const int16_t x = centerX - static_cast<int16_t>(tft.textWidth(text) / 2);
  tft.setCursor(x, y);
  tft.print(text);
}
}  // namespace

MainDashboardScreen::MainDashboardScreen(TFT_eSPI &tft) : tft_(tft) {}

void MainDashboardScreen::setSystemStatus(bool wifiConnected) {
  wifiConnected_ = wifiConnected;
}

bool MainDashboardScreen::shouldRedraw(const DashboardState &state, bool forceRedraw) const {
  if (forceRedraw || !hasLastState_) {
    return true;
  }

  if (!sameClock(state.clock, lastState_.clock)) {
    return true;
  }
  if (!sameProgress(state.tasks, lastState_.tasks)) {
    return true;
  }
  if (!sameProgress(state.habits, lastState_.habits)) {
    return true;
  }
  if (state.focusRunning != lastState_.focusRunning || state.focusMinutesRemaining != lastState_.focusMinutesRemaining) {
    return true;
  }
  return false;
}

void MainDashboardScreen::render(const DashboardState &state, bool forceRedraw) {
  const bool initialPaint = !hasLastState_;
  if (!shouldRedraw(state, forceRedraw)) {
    return;
  }

  if (forceRedraw || initialPaint) {
    drawBackground();
    drawTopTimeCardFrame();
    drawBottomCardsFrame();
    drawTopTimeDynamic(state);
    drawBottomCardsDynamic(state);

    lastState_ = state;
    hasLastState_ = true;
    return;
  }

  const bool clockChanged = !sameClock(state.clock, lastState_.clock);
  const bool tasksChanged = !sameProgress(state.tasks, lastState_.tasks);
  const bool habitsChanged = !sameProgress(state.habits, lastState_.habits);
  const bool focusChanged =
      (state.focusRunning != lastState_.focusRunning) || (state.focusMinutesRemaining != lastState_.focusMinutesRemaining);

  if (clockChanged) {
    drawTopTimeDynamic(state);
  }
  if (tasksChanged || habitsChanged || focusChanged) {
    drawBottomCardsDynamic(state);
  }

  lastState_ = state;
  hasLastState_ = true;
}

void MainDashboardScreen::drawBackground() const {
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

void MainDashboardScreen::drawTopTimeCardFrame() const {
  const int16_t x = kTopCardX;
  const int16_t y = kTopCardY;
  const int16_t w = tft_.width() - (kTopCardX * 2);
  const int16_t h = kTopCardH;

  const uint16_t card = rgb(tft_, 3, 14, 32);
  const uint16_t border = rgb(tft_, 22, 58, 120);
  const uint16_t accent = rgb(tft_, 40, 130, 255);

  tft_.fillRoundRect(x, y, w, h, 10, card);
  tft_.drawRoundRect(x, y, w, h, 10, border);

  tft_.setTextColor(accent, card);
  tft_.setTextSize(1);
  tft_.setCursor(x + 10, y + 10);
  tft_.print("CURRENT TIME");

  drawWifiIcon(x + w - 24, y + 10, accent);
}

void MainDashboardScreen::drawTopTimeDynamic(const DashboardState &state) const {
  const int16_t x = kTopCardX;
  const int16_t y = kTopCardY;
  const int16_t w = tft_.width() - (kTopCardX * 2);
  const int16_t h = kTopCardH;

  const uint16_t card = rgb(tft_, 3, 14, 32);
  const uint16_t accent = rgb(tft_, 40, 130, 255);
  const uint16_t soft = rgb(tft_, 180, 205, 245);

  // Clear only the dynamic region to avoid re-rendering the full screen each minute.
  tft_.fillRect(x + 8, y + 24, w - 16, h - 30, card);

  uint8_t hour12 = state.clock.hour % 12;
  if (hour12 == 0) {
    hour12 = 12;
  }
  const char *meridiem = state.clock.hour >= 12 ? "PM" : "AM";

  char timeText[8];
  snprintf(timeText, sizeof(timeText), "%02u:%02u", hour12, state.clock.minute);

  tft_.setTextSize(6);
  const int16_t timeW = static_cast<int16_t>(tft_.textWidth(timeText));
  tft_.setTextSize(3);
  const int16_t meridiemW = static_cast<int16_t>(tft_.textWidth(meridiem));

  const int16_t timeGap = 8;
  const int16_t timeBlockW = timeW + timeGap + meridiemW;
  const int16_t timeX = x + ((w - timeBlockW) / 2);
  const int16_t timeY = y + 30;
  const int16_t meridiemY = y + 70;

  tft_.setTextColor(TFT_WHITE, card);
  tft_.setTextSize(6);
  tft_.setCursor(timeX, timeY);
  tft_.print(timeText);

  tft_.setTextColor(accent, card);
  tft_.setTextSize(3);
  tft_.setCursor(timeX + timeW + timeGap, meridiemY);
  tft_.print(meridiem);

  const char *weekday = kWeekdays[state.clock.weekdayIndex % 7];
  const char *month = kMonths[(state.clock.month > 0 ? state.clock.month : 1) - 1];

  char dateText[40];
  snprintf(dateText, sizeof(dateText), "%02u %s %04u | %s", state.clock.day, month, state.clock.year, weekday);
  drawCenteredText(tft_, dateText, x + (w / 2), y + 108, 2, soft, card);
}

void MainDashboardScreen::drawBottomCardsFrame() const {
  const int16_t y = kBottomCardsY;
  const int16_t h = tft_.height() - y - 30;
  const int16_t gap = kBottomCardsGap;
  const int16_t x0 = kCardsMarginX;
  const int16_t innerW = tft_.width() - (x0 * 2);
  const int16_t contentW = innerW - (gap * 2);
  const int16_t baseW = contentW / 3;
  const int16_t remainder = contentW - (baseW * 3);
  const int16_t taskW = baseW;
  const int16_t habitW = baseW;
  const int16_t focusW = baseW + remainder;

  const uint16_t card = rgb(tft_, 3, 12, 28);
  const uint16_t border = rgb(tft_, 22, 52, 100);
  const uint16_t blue = rgb(tft_, 50, 145, 255);
  const uint16_t green = rgb(tft_, 35, 230, 150);

  const int16_t taskX = x0;
  const int16_t habitX = taskX + taskW + gap;
  const int16_t focusX = habitX + habitW + gap;

  tft_.fillRoundRect(taskX, y, taskW, h, 10, card);
  tft_.drawRoundRect(taskX, y, taskW, h, 10, border);
  drawTaskIcon(taskX + (taskW / 2) - 12, y + 8, blue);
  drawCenteredText(tft_, "TASKS", taskX + (taskW / 2), y + 40, 1, blue, card);

  tft_.fillRoundRect(habitX, y, habitW, h, 10, card);
  tft_.drawRoundRect(habitX, y, habitW, h, 10, border);
  drawHabitIcon(habitX + (habitW / 2) - 12, y + 8, green);
  drawCenteredText(tft_, "HABITS", habitX + (habitW / 2), y + 40, 1, green, card);

  tft_.fillRoundRect(focusX, y, focusW, h, 10, card);
  tft_.drawRoundRect(focusX, y, focusW, h, 10, border);
  drawFocusIcon(focusX + (focusW / 2) - 12, y + 8, blue);
  drawCenteredText(tft_, "FOCUS", focusX + (focusW / 2), y + 40, 1, blue, card);
}

void MainDashboardScreen::drawBottomCardsDynamic(const DashboardState &state) const {
  const int16_t y = kBottomCardsY;
  const int16_t h = tft_.height() - y - 30;
  const int16_t gap = kBottomCardsGap;
  const int16_t x0 = kCardsMarginX;
  const int16_t innerW = tft_.width() - (x0 * 2);
  const int16_t contentW = innerW - (gap * 2);
  const int16_t baseW = contentW / 3;
  const int16_t remainder = contentW - (baseW * 3);
  const int16_t taskW = baseW;
  const int16_t habitW = baseW;
  const int16_t focusW = baseW + remainder;

  const uint16_t card = rgb(tft_, 3, 12, 28);
  const uint16_t blue = rgb(tft_, 50, 145, 255);
  const uint16_t green = rgb(tft_, 35, 230, 150);
  const uint16_t text = rgb(tft_, 165, 190, 225);

  const int16_t taskX = x0;
  const int16_t habitX = taskX + taskW + gap;
  const int16_t focusX = habitX + habitW + gap;

  tft_.fillRect(taskX + 8, y + 80, taskW - 16, 18, card);
  char taskText[12];
  snprintf(taskText, sizeof(taskText), "%u / %u", state.tasks.completed, state.tasks.total);
  drawCenteredText(tft_, taskText, taskX + (taskW / 2), y + 54, 2, TFT_WHITE, card);
  drawProgressBar(taskX + 12, y + h - 10, taskW - 24, 7, state.tasks.completed, state.tasks.total, blue);

  tft_.fillRect(habitX + 8, y + 80, habitW - 16, 18, card);
  char habitText[12];
  snprintf(habitText, sizeof(habitText), "%u / %u", state.habits.completed, state.habits.total);
  drawCenteredText(tft_, habitText, habitX + (habitW / 2), y + 54, 2, TFT_WHITE, card);
  drawProgressBar(habitX + 12, y + h - 10, habitW - 24, 7, state.habits.completed, state.habits.total, green);

  tft_.fillRect(focusX + 8, y + 80, focusW - 16, 18, card);
  char focusText[20];
  if (state.focusRunning) {
    snprintf(focusText, sizeof(focusText), "%u min left", state.focusMinutesRemaining);
  } else {
    snprintf(focusText, sizeof(focusText), "Not started");
  }
  drawCenteredText(tft_, focusText, focusX + (focusW / 2), y + 54, 1, text, card);

  const uint16_t buttonColor = state.focusRunning ? rgb(tft_, 30, 170, 90) : rgb(tft_, 40, 110, 245);
  tft_.fillRoundRect(focusX + 10, y + h - 16, focusW - 20, 10, 3, buttonColor);
  tft_.drawRoundRect(focusX + 10, y + h - 16, focusW - 20, 10, 3, rgb(tft_, 120, 210, 255));

  const int16_t bx = focusX + (focusW / 2) - 3;
  const int16_t by = y + h - 14;
  if (state.focusRunning) {
    tft_.fillRect(bx - 2, by, 2, 6, TFT_WHITE);
    tft_.fillRect(bx + 2, by, 2, 6, TFT_WHITE);
  } else {
    tft_.fillTriangle(bx - 1, by - 1, bx - 1, by + 6, bx + 4, by + 2, TFT_WHITE);
  }
}

void MainDashboardScreen::drawWifiIcon(int16_t x, int16_t y, uint16_t color) const {
  if (!wifiConnected_) {
    tft_.drawLine(x - 2, y + 10, x + 14, y - 2, color);
    tft_.drawLine(x - 1, y + 10, x + 15, y - 2, color);
    return;
  }

  tft_.drawArc(x + 6, y + 10, 8, 6, 210, 330, color, color, false);
  tft_.drawArc(x + 6, y + 10, 5, 3, 220, 320, color, color, false);
  tft_.fillCircle(x + 6, y + 10, 1, color);
}

void MainDashboardScreen::drawTaskIcon(int16_t x, int16_t y, uint16_t color) const {
  tft_.drawRoundRect(x, y + 2, 24, 28, 2, color);
  tft_.drawRoundRect(x + 9, y, 6, 4, 2, color);
  tft_.drawFastHLine(x + 5, y + 10, 14, color);
  tft_.drawFastHLine(x + 5, y + 16, 14, color);
  tft_.drawFastHLine(x + 5, y + 22, 10, color);
}

void MainDashboardScreen::drawHabitIcon(int16_t x, int16_t y, uint16_t color) const {
  tft_.drawLine(x + 12, y + 10, x + 12, y + 28, color);
  tft_.drawLine(x + 12, y + 16, x + 5, y + 13, color);
  tft_.drawLine(x + 12, y + 21, x + 20, y + 16, color);
  tft_.drawCircle(x + 5, y + 11, 5, color);
  tft_.drawCircle(x + 20, y + 14, 5, color);
}

void MainDashboardScreen::drawFocusIcon(int16_t x, int16_t y, uint16_t color) const {
  tft_.drawCircle(x + 12, y + 15, 12, color);
  tft_.drawLine(x + 12, y + 15, x + 12, y + 8, color);
  tft_.drawLine(x + 12, y + 15, x + 17, y + 19, color);
  tft_.drawFastVLine(x + 10, y, 3, color);
  tft_.drawFastHLine(x + 8, y, 8, color);
}

void MainDashboardScreen::drawProgressBar(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t completed, uint8_t total,
                                          uint16_t accentColor) const {
  const uint16_t trackOuter = rgb(tft_, 36, 60, 96);
  const uint16_t trackInner = rgb(tft_, 16, 30, 54);

  tft_.fillRoundRect(x, y, w, h, h / 2, trackOuter);
  tft_.drawRoundRect(x, y, w, h, h / 2, rgb(tft_, 72, 122, 190));

  const int16_t innerX = x + 1;
  const int16_t innerY = y + 1;
  const int16_t innerW = w - 2;
  const int16_t innerH = h - 2;
  if (innerW <= 0 || innerH <= 0) {
    return;
  }

  tft_.fillRoundRect(innerX, innerY, innerW, innerH, innerH / 2, trackInner);

  const uint8_t p = percentOf(completed, total);
  const int16_t fillW = static_cast<int16_t>((innerW * p) / 100U);
  if (fillW > 0) {
    tft_.fillRoundRect(innerX, innerY, fillW, innerH, innerH / 2, accentColor);
  }
}
