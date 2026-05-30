#include "services/LocalFirstSyncService.h"
#include "config.h"

#include <LittleFS.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <ESPmDNS.h>

void LocalFirstSyncService::begin(const char* host, uint16_t port) {
  host_ = host;
  port_ = port;
  queueSize_ = 0;
  idMapSize_ = 0;
  processing_ = false;
  
  if (!loadQueue()) {
    Serial.println("[Sync] No existing queue found or failed to load. Starting fresh.");
  } else {
    Serial.printf("[Sync] Loaded %u pending events from flash.\n", queueSize_);
  }

  if (!loadIdMap()) {
    Serial.println("[Sync] No existing ID map found or failed to load. Starting fresh.");
  } else {
    Serial.printf("[Sync] Loaded %u ID mappings from flash.\n", idMapSize_);
  }
}

String LocalFirstSyncService::queueEvent(String method, String url, String payload, String dependsOnUuid) {
  if (queueSize_ >= kMaxEvents) {
    Serial.println("[Sync] Sync queue is FULL! Discarding new event.");
    return "";
  }
  
  // Generate a unique client UUID using ESP32 hardware RNG
  char uuidBuf[16];
  snprintf(uuidBuf, sizeof(uuidBuf), "c-%08x", (unsigned int)esp_random());
  String uuid = String(uuidBuf);
  
  SyncEvent ev;
  ev.uuid = uuid;
  ev.method = method;
  ev.url = url;
  ev.payload = payload;
  ev.dependsOnUuid = dependsOnUuid;
  ev.retries = 0;
  ev.nextRetryMs = 0; // Try immediately
  
  queue_[queueSize_++] = ev;
  saveQueue();
  
  Serial.printf("[Sync] Enqueued event %s: %s %s (Depends: %s)\n", 
                uuid.c_str(), method.c_str(), url.c_str(), dependsOnUuid.length() > 0 ? dependsOnUuid.c_str() : "none");
                
  return uuid;
}

int LocalFirstSyncService::resolveId(const String& uuid) const {
  for (size_t i = 0; i < idMapSize_; ++i) {
    if (idMap_[i].uuid == uuid) {
      return idMap_[i].serverId;
    }
  }
  return -1;
}

void LocalFirstSyncService::addIdMapping(const String& uuid, int serverId) {
  if (idMapSize_ >= kMaxIdMapEntries) {
    // Shift left to discard oldest
    for (size_t i = 1; i < kMaxIdMapEntries; ++i) {
      idMap_[i - 1] = idMap_[i];
    }
    idMapSize_ = kMaxIdMapEntries - 1;
  }
  idMap_[idMapSize_++] = {uuid, serverId};
  Serial.printf("[Sync] Map client UUID %s -> Server ID %d\n", uuid.c_str(), serverId);
  saveIdMap();
}

void LocalFirstSyncService::clearQueue() {
  queueSize_ = 0;
  saveQueue();
  Serial.println("[Sync] Cleared sync queue.");
}

void LocalFirstSyncService::tick() {
  if (queueSize_ == 0 || processing_) {
    return;
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }
  
  // Resolve host IP if not cached
  const uint32_t now = millis();
  if (resolvedIp_.length() == 0) {
    if (now - lastResolveAttemptMs_ >= kResolveRetryIntervalMs) {
      lastResolveAttemptMs_ = now;
      resolvedIp_ = resolveHostIp(host_);
    }
    if (resolvedIp_.length() == 0) {
      return; // Can't process yet, DNS unresolved
    }
  }
  
  // Check if first event is ready (past its backoff timer)
  if (now < queue_[0].nextRetryMs) {
    return;
  }
  
  processNextEvent();
}

void LocalFirstSyncService::processNextEvent() {
  processing_ = true;
  SyncEvent& ev = queue_[0];
  
  // Resolve URL placeholders (like {id}) if they depend on an earlier UUID
  String finalUrl = resolveUrlTemplate(ev.url, ev.dependsOnUuid);
  if (finalUrl.indexOf("{id}") != -1) {
    // Check if the parent UUID exists in the queue or ID map
    int serverId = resolveId(ev.dependsOnUuid);
    bool parentExists = (serverId != -1) || isUuidInQueue(ev.dependsOnUuid);
    
    if (!parentExists) {
      // The parent event is gone and was never resolved. We must discard this event to avoid deadlock.
      Serial.printf("[Sync] Event %s blocked on missing/unresolved parent UUID %s. Discarding to prevent deadlock.\n", 
                    ev.uuid.c_str(), ev.dependsOnUuid.c_str());
      discardDependentEvents(ev.uuid);
      
      // Remove from queue (shift remaining left)
      for (size_t i = 1; i < queueSize_; ++i) {
        queue_[i - 1] = queue_[i];
      }
      queueSize_--;
      saveQueue();
      processing_ = false;
      return;
    }

    // Still waiting for parent POST request ID mapping
    Serial.printf("[Sync] Event %s blocked on parent UUID %s resolution. Waiting.\n", 
                  ev.uuid.c_str(), ev.dependsOnUuid.c_str());
    ev.nextRetryMs = millis() + 5000; // Delay retry
    processing_ = false;
    return;
  }
  
  String fullUrl = "http://" + resolvedIp_ + ":" + String(port_) + finalUrl;
  Serial.printf("[Sync] Processing event %s -> %s %s\n", ev.uuid.c_str(), ev.method.c_str(), fullUrl.c_str());
  
  HTTPClient http;
  http.begin(fullUrl);
  http.addHeader("Content-Type", "application/json");
  
  int httpCode = -1;
  if (ev.method == "POST") {
    httpCode = http.POST(ev.payload);
  } else if (ev.method == "PATCH") {
    httpCode = http.PATCH(ev.payload);
  } else if (ev.method == "DELETE") {
    httpCode = http.sendRequest("DELETE", ev.payload);
  } else if (ev.method == "PUT") {
    httpCode = http.PUT(ev.payload);
  }
  
  bool success = (httpCode >= 200 && httpCode < 300);
  
  if (success) {
    String response = http.getString();
    Serial.printf("[Sync] Event %s succeeded: HTTP %d\n", ev.uuid.c_str(), httpCode);
    
    // Parse response body to extract Server ID for mapping (specifically for POST starts)
    if (ev.method == "POST") {
      JsonDocument doc;
      DeserializationError err = deserializeJson(doc, response);
      if (!err) {
        int serverId = doc["data"]["id"] | -1;
        if (serverId != -1) {
          addIdMapping(ev.uuid, serverId);
        }
      }
    }
    
    // Remove completed event from queue (shift remaining events left)
    for (size_t i = 1; i < queueSize_; ++i) {
      queue_[i - 1] = queue_[i];
    }
    queueSize_--;
    saveQueue();
  } else {
    Serial.printf("[Sync] Event %s failed: HTTP %d (Retries: %u/%u)\n", 
                  ev.uuid.c_str(), httpCode, ev.retries + 1, kSyncMaxRetries);
    
    // Clear IP cache if connection refused
    if (httpCode == HTTPC_ERROR_CONNECTION_REFUSED || httpCode < 0) {
      resolvedIp_ = "";
    }
    
    ev.retries++;
    if (ev.retries >= kSyncMaxRetries) {
      Serial.printf("[Sync] Event %s exceeded maximum retries. Discarding.\n", ev.uuid.c_str());
      discardDependentEvents(ev.uuid);
      for (size_t i = 1; i < queueSize_; ++i) {
        queue_[i - 1] = queue_[i];
      }
      queueSize_--;
      saveQueue();
    } else {
      // Exponential backoff
      uint32_t backoff = kSyncInitialBackoffMs * (1U << ev.retries);
      if (backoff > 60000) backoff = 60000; // Cap backoff at 60s
      ev.nextRetryMs = millis() + backoff;
    }
  }
  
  http.end();
  processing_ = false;
}

String LocalFirstSyncService::resolveUrlTemplate(const String& url, const String& dependsOnUuid) {
  if (dependsOnUuid.length() == 0 || url.indexOf("{id}") == -1) {
    return url;
  }
  
  int serverId = resolveId(dependsOnUuid);
  if (serverId != -1) {
    String resolved = url;
    resolved.replace("{id}", String(serverId));
    return resolved;
  }
  
  return url;
}

String LocalFirstSyncService::resolveHostIp(const char* host) {
  if (host == nullptr || host[0] == '\0') {
    return "";
  }
  IPAddress ip;
  if (ip.fromString(host)) {
    return String(host);
  }
  
  String hostStr = String(host);
  if (hostStr.endsWith(".local")) {
    String mdnsHost = hostStr.substring(0, hostStr.length() - 6);
    Serial.printf("[Sync] Resolving mDNS '%s'...\n", mdnsHost.c_str());
    ip = MDNS.queryHost(mdnsHost);
    if (ip != IPAddress(0, 0, 0, 0)) {
      Serial.printf("[Sync] Resolved '%s' to %s\n", host, ip.toString().c_str());
      return ip.toString();
    }
    Serial.printf("[Sync] Failed to resolve mDNS '%s'\n", host);
    return "";
  }
  
  Serial.printf("[Sync] Resolving standard DNS '%s'...\n", host);
  if (WiFi.hostByName(host, ip)) {
    Serial.printf("[Sync] Resolved '%s' to %s\n", host, ip.toString().c_str());
    return ip.toString();
  }
  Serial.printf("[Sync] Failed to resolve DNS '%s'\n", host);
  return "";
}

bool LocalFirstSyncService::loadQueue() {
  if (!LittleFS.exists(kQueueFilePath)) {
    return false;
  }
  
  File file = LittleFS.open(kQueueFilePath, "r");
  if (!file) {
    return false;
  }
  
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, file);
  file.close();
  
  if (err) {
    Serial.printf("[Sync] JSON Deserialization error: %s\n", err.c_str());
    return false;
  }
  
  JsonArray arr = doc.as<JsonArray>();
  queueSize_ = 0;
  bool queueModified = false;
  
  for (JsonObject obj : arr) {
    if (queueSize_ >= kMaxEvents) break;
    
    String uuid = obj["uuid"].as<String>();
    String dependsOn = obj["dependsOn"].as<String>();
    
    // Purge any corrupted events from the old random seed bug
    if (uuid == "c-00000000" || dependsOn == "c-00000000") {
      Serial.printf("[Sync] Purging corrupted event %s (depends: %s) from old firmware bug.\n", 
                    uuid.c_str(), dependsOn.c_str());
      queueModified = true;
      continue;
    }
    
    SyncEvent ev;
    ev.uuid = uuid;
    ev.method = obj["method"].as<String>();
    ev.url = obj["url"].as<String>();
    ev.payload = obj["payload"].as<String>();
    ev.dependsOnUuid = dependsOn;
    ev.retries = obj["retries"] | 0;
    ev.nextRetryMs = millis(); // Reset retry timers on boot to try immediately
    
    queue_[queueSize_++] = ev;
  }
  
  if (queueModified) {
    saveQueue();
  }
  
  return true;
}

bool LocalFirstSyncService::saveQueue() {
  File file = LittleFS.open(kQueueFilePath, "w");
  if (!file) {
    Serial.println("[Sync] Failed to open queue file for writing!");
    return false;
  }
  
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  
  for (size_t i = 0; i < queueSize_; ++i) {
    JsonObject obj = arr.add<JsonObject>();
    obj["uuid"] = queue_[i].uuid;
    obj["method"] = queue_[i].method;
    obj["url"] = queue_[i].url;
    obj["payload"] = queue_[i].payload;
    obj["dependsOn"] = queue_[i].dependsOnUuid;
    obj["retries"] = queue_[i].retries;
  }
  
  serializeJson(doc, file);
  file.close();
  return true;
}

void LocalFirstSyncService::discardDependentEvents(const String& uuid) {
  for (size_t i = 0; i < queueSize_;) {
    if (queue_[i].dependsOnUuid == uuid) {
      Serial.printf("[Sync] Discarding dependent orphan event %s\n", queue_[i].uuid.c_str());
      String childUuid = queue_[i].uuid;
      
      // Shift remaining left
      for (size_t j = i + 1; j < queueSize_; ++j) {
        queue_[j - 1] = queue_[j];
      }
      queueSize_--;
      
      // Recursively check if anything depends on the child we just discarded
      discardDependentEvents(childUuid);
    } else {
      ++i;
    }
  }
}

bool LocalFirstSyncService::isUuidInQueue(const String& uuid) const {
  // Start from index 1 to avoid matching the current event at queue_[0] against itself
  for (size_t i = 1; i < queueSize_; ++i) {
    if (queue_[i].uuid == uuid && queue_[i].method == "POST") {
      return true;
    }
  }
  return false;
}

bool LocalFirstSyncService::saveIdMap() {
  File file = LittleFS.open(kIdMapFilePath, "w");
  if (!file) {
    Serial.println("[Sync] Failed to open ID map file for writing!");
    return false;
  }
  
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  
  for (size_t i = 0; i < idMapSize_; ++i) {
    JsonObject obj = arr.add<JsonObject>();
    obj["uuid"] = idMap_[i].uuid;
    obj["id"] = idMap_[i].serverId;
  }
  
  serializeJson(doc, file);
  file.close();
  return true;
}

bool LocalFirstSyncService::loadIdMap() {
  if (!LittleFS.exists(kIdMapFilePath)) {
    return false;
  }
  
  File file = LittleFS.open(kIdMapFilePath, "r");
  if (!file) {
    return false;
  }
  
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, file);
  file.close();
  
  if (err) {
    Serial.printf("[Sync] ID map JSON Deserialization error: %s\n", err.c_str());
    return false;
  }
  
  JsonArray arr = doc.as<JsonArray>();
  idMapSize_ = 0;
  
  for (JsonObject obj : arr) {
    if (idMapSize_ >= kMaxIdMapEntries) break;
    idMap_[idMapSize_++] = {obj["uuid"].as<String>(), obj["id"].as<int>()};
  }
  
  return true;
}
