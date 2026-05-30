#pragma once

#include <Arduino.h>

struct SyncEvent {
  String uuid;           // Client-assigned UUID
  String method;         // "POST", "PATCH", "DELETE"
  String url;            // Target endpoint, e.g. "/pomodoro/sessions/{id}/complete"
  String payload;        // JSON request body string
  String dependsOnUuid;  // UUID of POST request this event depends on
  uint8_t retries = 0;   // Retry counter
  uint32_t nextRetryMs = 0; // Expiration of backoff timer (millis)
};

class LocalFirstSyncService {
 public:
  void begin(const char* host, uint16_t port);
  void tick();
  
  // Enqueue a sync mutation.
  // Returns client uuid.
  String queueEvent(String method, String url, String payload, String dependsOnUuid = "");
  
  // Resolve an offline ID.
  // Returns mapped server ID if resolved, or -1 if not found.
  int resolveId(const String& uuid) const;
  
  size_t getQueueSize() const { return queueSize_; }
  bool isProcessing() const { return processing_; }
  void clearQueue();

 private:
  bool loadQueue();
  bool saveQueue();
  void processNextEvent();
  void discardDependentEvents(const String& uuid);
  String resolveHostIp(const char* host);
  
  static constexpr const char* kQueueFilePath = "/sync_queue.json";
  static constexpr size_t kMaxEvents = 50;
  
  SyncEvent queue_[kMaxEvents];
  size_t queueSize_ = 0;
  
  const char* host_ = nullptr;
  uint16_t port_ = 0U;
  bool processing_ = false;
  uint32_t lastProcessAttemptMs_ = 0;
  
  // Resolved IP Cache for non-blocking requests
  String resolvedIp_ = "";
  uint32_t lastResolveAttemptMs_ = 0;
  static constexpr uint32_t kResolveRetryIntervalMs = 30000; // 30s
  
  // In-memory mapping of client-assigned UUID to server-assigned ID
  struct IdMapEntry {
    String uuid;
    int serverId;
  };
  static constexpr size_t kMaxIdMapEntries = 50;
  IdMapEntry idMap_[kMaxIdMapEntries];
  size_t idMapSize_ = 0;
  
  void addIdMapping(const String& uuid, int serverId);
  String resolveUrlTemplate(const String& url, const String& dependsOnUuid);
};
