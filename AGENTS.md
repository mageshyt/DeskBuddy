# DeskBuddy Agent Instructions

## Scope

This project is an ESP32 firmware for a DeskBuddy device. It renders the UI and sends user actions to a host app. The host app (planned) is the system of record for tasks, habits, and timers.

## Architecture Spec (Current + Target)

### Current Firmware Architecture

- **Entry point**: `src/main.cpp`
- **Services**:
  - `ClockService` computes time from a seed.
  - `WiFiTimeService` fetches NTP and tracks sync state.
- **UI**:
  - `MainDashboardScreen` draws the main TFT dashboard.
- **Config**:
  - `include/config.h` holds WiFi and timing constants.

### Target System Architecture (State Sync)

Use **REST + WebSockets** for bi-directional, real-time sync. REST is for initial state and bulk sync; WebSockets for live updates and commands.

**Single Source of Truth**: The Electron app maintains persistent state (SQLite) and broadcasts changes.

**Roles**:

- **Electron App**: Database, state manager, sync engine, WebSocket server.
- **ESP32**: Lightweight renderer + input client. No business rules.

**Data Flow**:

1. ESP32 fetches bootstrap state via REST.
2. ESP32 opens WebSocket connection for live updates.
3. Actions from device (task complete, pomodoro start) are sent via WebSocket.
4. Electron validates, persists, and broadcasts the updated state.

### Planned Domain Models

See `idea.md` for task/habit/calendar/pomodoro schemas.

## Build & Test

- Build: `pio run -e upesy_wroom`
- Upload: `pio run -e upesy_wroom -t upload`
- Test: `pio test`
- Monitor: `pio device monitor -p /dev/cu.SLAB_USBtoUART -b 115200`

## Pitfalls

- `platformio.ini` includes machine-specific serial ports. Adjust `upload_port` and `monitor_port` when needed.
- Avoid storing business rules on the ESP32; keep rules in the Electron app.
- NTP sync can block startup; guard UI rendering accordingly.

## References

- `idea.md` (feature roadmap and schemas)
- `info.md` (hardware notes and pin usage)
- `platformio.ini` (board config and dependencies)
