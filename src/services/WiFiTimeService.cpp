#include "services/WiFiTimeService.h"

#include "config.h"

// ─── public ──────────────────────────────────────────────────────────────────

bool WiFiTimeService::begin(const char* ssid, const char* password) {
  Serial.println("[WiFiTime] Connecting to WiFi...");

  if (!connectWiFi(ssid, password)) {
    Serial.println("[WiFiTime] WiFi connection failed — falling back to compile-time seed.");
    return false;
  }

  if (!syncNTP()) {
    Serial.println("[WiFiTime] NTP sync failed — falling back to compile-time seed.");
    return false;
  }

  // WiFi stays up for future re-syncs; can be disconnected here to save power
  // if re-sync is not needed: WiFi.disconnect(true);

  return true;
}

void WiFiTimeService::tick() {
  if (!synced_) {
    return;
  }

  const uint32_t now = millis();
  if ((now - lastSyncMs_) >= kNtpResyncMs) {
    Serial.println("[WiFiTime] Periodic NTP re-sync...");
    syncNTP();
  }
}

ClockSnapshot WiFiTimeService::snapshotNow() const {
  ClockSnapshot snap{};

  if (!synced_) {
    return snap;  // caller should fall back to compile-time seed
  }

  struct tm timeinfo {};
  if (!getLocalTime(&timeinfo)) {
    Serial.println("[WiFiTime] getLocalTime failed.");
    return snap;
  }

  snap.year         = static_cast<uint16_t>(timeinfo.tm_year + 1900);
  snap.month        = static_cast<uint8_t>(timeinfo.tm_mon + 1);   // tm_mon: 0-11
  snap.day          = static_cast<uint8_t>(timeinfo.tm_mday);
  snap.hour         = static_cast<uint8_t>(timeinfo.tm_hour);
  snap.minute       = static_cast<uint8_t>(timeinfo.tm_min);
  snap.second       = static_cast<uint8_t>(timeinfo.tm_sec);
  snap.weekdayIndex = static_cast<uint8_t>(timeinfo.tm_wday);      // 0=Sun … 6=Sat

  return snap;
}

// ─── private ─────────────────────────────────────────────────────────────────

bool WiFiTimeService::connectWiFi(const char* ssid, const char* password) {
  if (ssid == nullptr || ssid[0] == '\0') {
    Serial.println("[WiFiTime] No SSID configured — skipping WiFi.");
    return false;
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  const uint32_t deadline = millis() + kNtpTimeoutMs;
  while (WiFi.status() != WL_CONNECTED && millis() < deadline) {
    delay(250);
    Serial.print('.');
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  Serial.print("[WiFiTime] Connected — IP: ");
  Serial.println(WiFi.localIP());
  return true;
}

bool WiFiTimeService::syncNTP() {
  configTime(kGmtOffsetSec, kDaylightOffsetSec, kNtpServer1, kNtpServer2);

  Serial.print("[WiFiTime] Waiting for NTP sync");
  const uint32_t deadline = millis() + kNtpTimeoutMs;

  struct tm timeinfo {};
  while (!getLocalTime(&timeinfo) && millis() < deadline) {
    delay(200);
    Serial.print('.');
  }
  Serial.println();

  if (!getLocalTime(&timeinfo)) {
    return false;
  }

  synced_     = true;
  lastSyncMs_ = millis();

  char buf[32];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
  Serial.print("[WiFiTime] Synced — ");
  Serial.println(buf);

  return true;
}
