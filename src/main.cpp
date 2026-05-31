#include <Arduino.h>
#include <TFT_eSPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <ESPmDNS.h>
#include <LittleFS.h>

#include "config.h"
#include "models/DashboardState.h"
#include "services/ClockService.h"
#include "services/SyncRestService.h"
#include "services/SyncSocketService.h"
#include "services/WiFiTimeService.h"
#include "services/NavigationService.h"
#include "services/InputService.h"
#include "services/LocalFirstSyncService.h"
#include "services/PomodoroService.h"
#include "ui/MainDashboardScreen.h"
#include "ui/PomodoroScreen.h"
#include "ui/TasksScreen.h"
#include "ui/HabitsScreen.h"
#include "services/OledService.h"

namespace {
const char *const kMonthNames[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

TFT_eSPI tft;
Adafruit_SH1106G oled(128, 64, &Wire, -1);
OledService oledService(oled);
ClockService clockService;
SyncRestService restSync;
SyncSocketService syncSocket;
WiFiTimeService wifiTime;
LocalFirstSyncService localSync;
PomodoroService pomodoroService;

DashboardState state{};

NavigationService navigationService;
MainDashboardScreen dashboard(tft, state);
PomodoroScreen pomodoroScreen(tft, state, pomodoroService);
TasksScreen tasksScreen(tft, state, restSync, localSync, navigationService);
HabitsScreen habitsScreen(tft, state, restSync, localSync);

InputService inputService(navigationService);

uint32_t lastRenderMs = 0;
uint32_t lastFocusMinuteMs = 0;
uint32_t lastSummaryMs = 0;
uint32_t lastOledMessageMs = 0;
uint32_t activeOledMessageTimeoutMs = 5000;
const uint32_t OLED_MESSAGE_TIMEOUT_MS = 5000;

bool oledReady = false;
char oledMessage[96] = {0};

const uint32_t RENDER_INTERVAL_MS = 120;
const uint32_t FOCUS_TICK_MS = 60000;
const uint32_t SUMMARY_REFRESH_MS = 30000;


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
  state.focusMode = 0; // Focus mode
  state.focusSecondsRemaining = 25 * 60;
  strncpy(state.focusCategory, "General", sizeof(state.focusCategory) - 1);
  state.focusCategory[sizeof(state.focusCategory) - 1] = '\0';
  state.sessionCount = 0;
}

void handleSerialInput() {
  while (Serial.available() > 0) {
    const char input = static_cast<char>(Serial.read());
    switch (input) {
      case 'f':
      case 'F':
        if (pomodoroService.isRunning()) {
          pomodoroService.pause();
        } else {
          pomodoroService.start();
        }
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
      case 'q':
      case 'Q':
        localSync.clearQueue();
        Serial.println("[Main] Cleared sync queue via Serial console command.");
        break;
      default:
        break;
    }
  }
}

void renderOledMessage(const char* message) {
  if (!oledReady) {
    return;
  }
  oledService.displayMessage("WS event:", message);
}

void drawOledDefaultImage() {
  if (!oledReady) {
    return;
  }
  oledService.drawIdleImage();
}
}  // namespace

void setup() {
  Serial.begin(115200);

  // Initialize LittleFS for local-first sync queue persistence
  if (!LittleFS.begin(true)) {
    Serial.println("[Main] LittleFS Mount Failed!");
  } else {
    Serial.println("[Main] LittleFS Mounted Successfully.");
  }

  tft.init();
  tft.setRotation(1);

  // Register screens with navigation service
  navigationService.registerScreen(&dashboard);
  navigationService.registerScreen(&pomodoroScreen);
  navigationService.registerScreen(&tasksScreen);
  navigationService.registerScreen(&habitsScreen);

  initializeDemoState();

  // Seed the clock before WiFi connect.
  ClockSnapshot seed = buildClockSeed();
  clockService.begin(seed);
  state.clock = clockService.now();
  
  // Set default active screen (triggers initial render of dashboard)
  navigationService.navigateTo(&dashboard);

  // Try to get accurate time via NTP; fall back to compile-time seed if unavailable.
  if (wifiTime.begin(kWifiSSID, kWifiPassword) && wifiTime.isSynced()) {
    seed = wifiTime.snapshotNow();
    Serial.println("[Main] Using NTP time.");
  } else {
    seed = buildClockSeed();
    Serial.println("[Main] Using compile-time fallback seed.");
  }
  clockService.begin(seed);
  state.clock = clockService.now();

  // Start mDNS so the ESP32 can resolve .local hostnames (e.g. magesh.local)
  if (!MDNS.begin("deskbuddy")) {
    Serial.println("[Main] mDNS failed to start");
  } else {
    Serial.println("[Main] mDNS started — can resolve .local hostnames");
  }

  Wire.begin(kOledSdaPin, kOledSclPin);
  if (oled.begin(kOledAddressPrimary, true)) {
    Serial.printf("[OLED] Ready at 0x%02X\n", kOledAddressPrimary);
    oledReady = true;
  } else if (oled.begin(kOledAddressSecondary, true)) {
    Serial.printf("[OLED] Ready at 0x%02X\n", kOledAddressSecondary);
    oledReady = true;
  } else {
    Serial.println("[OLED] init failed");
  }
  if (oledReady) {
    oledService.begin();
  }

  // Initialize hardware inputs via InputService after Wire (I2C) is ready
  if (!inputService.begin()) {
    Serial.println("[Main] InputService failed to initialize!");
  }

  restSync.begin(kSyncServerHost, kSyncServerPort);
  restSync.fetchSummary(state);
  
  // Initialize local-first sync service and Pomodoro engine
  localSync.begin(kSyncServerHost, kSyncServerPort);
  pomodoroService.begin(state, localSync);

  syncSocket.begin(kSyncServerHost, kSyncServerPort, kDeviceFirmwareVersion);

  Serial.println("DeskBuddy main screen ready");
  Serial.println("Controls: f=focus, +=task up, -=task down, h=habit up, j=habit down");
}

void loop() {
  // Poll hardware buttons and encoder
  inputService.tick();

  handleSerialInput();
  
  // Tick sync queue and Pomodoro service
  localSync.tick();
  pomodoroService.tick();

  // Periodic NTP re-sync (every 6 hours) to correct millis() drift.
  wifiTime.tick();

  // WebSocket sync heartbeat and status updates
  syncSocket.tick();

  const uint32_t now = millis();

  if (syncSocket.consumeSummaryRefresh()) {
    state.tasksNeedRefetch = true;
    state.habitsNeedRefetch = true;
    if (restSync.fetchSummary(state)) {
      lastSummaryMs = now;
    }
  }
  uint32_t msgDuration = 30000;
  if (syncSocket.consumeTestMessage(oledMessage, sizeof(oledMessage), msgDuration)) {
    renderOledMessage(oledMessage);
    lastOledMessageMs = now;
    activeOledMessageTimeoutMs = msgDuration;
  }
  
  static char base64ImageBuf[1500];
  bool isPersistent = false;
  uint32_t imgDuration = 30000;
  if (syncSocket.consumeOledImage(base64ImageBuf, sizeof(base64ImageBuf), isPersistent, imgDuration)) {
    oledService.handleImageEvent(base64ImageBuf, isPersistent);
    if (!isPersistent) {
      lastOledMessageMs = now;
      activeOledMessageTimeoutMs = imgDuration;
    }
  }
  if ((now - lastSummaryMs) >= SUMMARY_REFRESH_MS) {
    lastSummaryMs = now;
    restSync.fetchSummary(state);
  }


  // After a re-sync, re-seed the clock service with fresh NTP time.
  static bool lastSynced = false;
  if (wifiTime.isSynced() && !lastSynced) {
    clockService.begin(wifiTime.snapshotNow());
    lastSynced = true;
  }

  state.clock = clockService.now();

  // Tick the active screen's update and rendering loop
  navigationService.loop();

  // If a temporary text message is active, restore the default idle image after timeout
  if (lastOledMessageMs != 0 && (now - lastOledMessageMs) >= activeOledMessageTimeoutMs) {
    lastOledMessageMs = 0;
    drawOledDefaultImage();
  }
}