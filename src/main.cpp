#include <Arduino.h>
#include <TFT_eSPI.h>

#include "config.h"
#include "models/DashboardState.h"
#include "services/ClockService.h"
#include "services/WiFiTimeService.h"
#include "ui/MainDashboardScreen.h"

namespace {
const char *const kMonthNames[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

TFT_eSPI tft;
ClockService clockService;
WiFiTimeService wifiTime;
MainDashboardScreen dashboard(tft);

DashboardState state{};

uint32_t lastRenderMs = 0;
uint32_t lastFocusMinuteMs = 0;

const uint32_t RENDER_INTERVAL_MS = 120;
const uint32_t FOCUS_TICK_MS = 60000;

uint8_t monthFromAbbrev(const char *abbr) {
  for (uint8_t i = 0; i < 12; i++) {
    if (abbr[0] == kMonthNames[i][0] && abbr[1] == kMonthNames[i][1] && abbr[2] == kMonthNames[i][2]) {
      return static_cast<uint8_t>(i + 1);
    }
  }
  return 1;
}

uint8_t weekdayFromDate(uint16_t year, uint8_t month, uint8_t day) {
  uint16_t y = year;
  uint8_t m = month;
  if (m < 3) {
    m += 12;
    y -= 1;
  }

  const uint16_t k = y % 100;
  const uint16_t j = y / 100;
  const uint8_t h = static_cast<uint8_t>((day + (13U * (m + 1U)) / 5U + k + k / 4U + j / 4U + 5U * j) % 7U);
  return static_cast<uint8_t>((h + 6U) % 7U);
}

ClockSnapshot buildClockSeed() {
  ClockSnapshot seed{};

  const char *date = __DATE__;
  const char *time = __TIME__;

  seed.month = monthFromAbbrev(date);
  seed.day = static_cast<uint8_t>((date[4] == ' ' ? 0 : (date[4] - '0') * 10) + (date[5] - '0'));
  seed.year = static_cast<uint16_t>((date[7] - '0') * 1000 + (date[8] - '0') * 100 + (date[9] - '0') * 10 + (date[10] - '0'));

  seed.hour = static_cast<uint8_t>((time[0] - '0') * 10 + (time[1] - '0'));
  seed.minute = static_cast<uint8_t>((time[3] - '0') * 10 + (time[4] - '0'));
  seed.second = static_cast<uint8_t>((time[6] - '0') * 10 + (time[7] - '0'));
  seed.weekdayIndex = weekdayFromDate(seed.year, seed.month, seed.day);

  return seed;
}

void initializeDemoState() {
  state.tasks.completed = 3;
  state.tasks.total = 7;

  state.habits.completed = 2;
  state.habits.total = 5;

  state.focusRunning = false;
  state.focusMinutesRemaining = 25;
}

void updateFocusTimer() {
  if (!state.focusRunning) {
    return;
  }

  const uint32_t now = millis();
  if ((now - lastFocusMinuteMs) < FOCUS_TICK_MS) {
    return;
  }
  lastFocusMinuteMs = now;

  if (state.focusMinutesRemaining > 0) {
    state.focusMinutesRemaining--;
  }
  if (state.focusMinutesRemaining == 0) {
    state.focusRunning = false;
    state.focusMinutesRemaining = 25;
  }
}

void handleSerialInput() {
  while (Serial.available() > 0) {
    const char input = static_cast<char>(Serial.read());
    switch (input) {
      case 'f':
      case 'F':
        state.focusRunning = !state.focusRunning;
        lastFocusMinuteMs = millis();
        Serial.println(state.focusRunning ? "Focus session started" : "Focus session paused");
        break;
      case '+':
        if (state.tasks.completed < state.tasks.total) {
          state.tasks.completed++;
        }
        break;
      case '-':
        if (state.tasks.completed > 0) {
          state.tasks.completed--;
        }
        break;
      case 'h':
      case 'H':
        if (state.habits.completed < state.habits.total) {
          state.habits.completed++;
        }
        break;
      case 'j':
      case 'J':
        if (state.habits.completed > 0) {
          state.habits.completed--;
        }
        break;
      default:
        break;
    }
  }
}
}  // namespace

void setup() {
  Serial.begin(115200);

  tft.init();
  tft.setRotation(1);

  initializeDemoState();

  // Try to get accurate time via NTP; fall back to compile-time seed if unavailable.
  ClockSnapshot seed;
  if (wifiTime.begin(kWifiSSID, kWifiPassword) && wifiTime.isSynced()) {
    seed = wifiTime.snapshotNow();
    Serial.println("[Main] Using NTP time.");
  } else {
    seed = buildClockSeed();
    Serial.println("[Main] Using compile-time fallback seed.");
  }
  clockService.begin(seed);

  dashboard.setSystemStatus(true);
  state.clock = clockService.now();
  dashboard.render(state, true);

  Serial.println("DeskBuddy main screen ready");
  Serial.println("Controls: f=focus, +=task up, -=task down, h=habit up, j=habit down");
}

void loop() {
  handleSerialInput();
  updateFocusTimer();

  // Periodic NTP re-sync (every 6 hours) to correct millis() drift.
  wifiTime.tick();

  // After a re-sync, re-seed the clock service with fresh NTP time.
  static bool lastSynced = false;
  if (wifiTime.isSynced() && !lastSynced) {
    clockService.begin(wifiTime.snapshotNow());
    lastSynced = true;
  }

  state.clock = clockService.now();

  const uint32_t now = millis();
  if ((now - lastRenderMs) >= RENDER_INTERVAL_MS) {
    lastRenderMs = now;
    dashboard.render(state);
  }
}