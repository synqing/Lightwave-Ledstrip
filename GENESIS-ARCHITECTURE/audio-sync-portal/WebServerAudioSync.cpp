/**
 * WebServer Audio Sync Enhancement - Enhanced Version
 * 
 * This file contains the enhanced audio synchronization with chunked upload support
 * for large JSON files (15-20MB) and improved network latency compensation.
 */

#include "WebServer.h"
#include "vp_decoder.h"
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <map>

// Add these member variables to your WebServer class:
// private:
//     VPDecoder* vpDecoder;
//     unsigned long syncStartTime;
//     int syncOffset;
//     bool syncActive;
//     
//     // Chunked upload state
//     struct ChunkedUpload {
//         File file;
//         String path;
//         size_t totalChunks;
//         size_t receivedChunks;
//         std::map<size_t, bool> chunkMap;
//         unsigned long lastActivity;
//         size_t totalSize;
//     };
//     std::map<String, ChunkedUpload> activeUploads;
//
//     // Network latency tracking
//     struct LatencyStats {
//         float samples[10];
//         size_t sampleCount;
//         float average;
//         float median;
//         unsigned long lastMeasurement;
//     };
//     LatencyStats latencyStats;

void WebServer::setupAudioSyncHandlers() {
    // Standard file upload handler for small files
    server.on("/upload/audio_data", HTTP_POST, 
        [](AsyncWebServerRequest *request) {
            request->send(200, "text/plain", "Upload complete");
        },
        [this](AsyncWebServerRequest *request, String filename, 
               size_t index, uint8_t *data, size_t len, bool final) {
            handleAudioDataUpload(request, filename, index, data, len, final);
        }
    );
    
    // Chunked upload handler for large files
    server.on("/upload/audio_data/chunk", HTTP_POST,
        [this](AsyncWebServerRequest *request) {
            handleChunkedUploadComplete(request);
        },
        [this](AsyncWebServerRequest *request, const String& filename,
               size_t index, uint8_t *data, size_t len, bool final) {
            handleChunkedUpload(request, filename, index, data, len, final);
        }
    );
    
    // Cleanup stale uploads periodically
    xTaskCreate(
        [](void* param) {
            WebServer* server = (WebServer*)param;
            while (true) {
                vTaskDelay(30000 / portTICK_PERIOD_MS); // Every 30 seconds
                server->cleanupStaleUploads();
            }
        },
        "UploadCleanup", 2048, this, 1, NULL
    );
}

void WebServer::handleChunkedUpload(AsyncWebServerRequest *request, const String& filename,
                                   size_t index, uint8_t *data, size_t len, bool final) {
    // Extract headers
    String uploadId = request->hasHeader("X-Upload-ID") ? 
                     request->getHeader("X-Upload-ID")->value() : "";
    
    if (uploadId.isEmpty()) {
        Serial.println("[Chunked Upload] Missing upload ID");
        return;
    }
    
    size_t chunkIndex = request->hasHeader("X-Chunk-Index") ? 
                       request->getHeader("X-Chunk-Index")->value().toInt() : 0;
    
    size_t totalChunks = request->hasHeader("X-Total-Chunks") ? 
                        request->getHeader("X-Total-Chunks")->value().toInt() : 0;
    
    // Initialize upload if first chunk
    if (activeUploads.find(uploadId) == activeUploads.end()) {
        String originalName = request->hasHeader("X-File-Name") ? 
                            request->getHeader("X-File-Name")->value() : filename;
        
        ChunkedUpload upload;
        upload.path = "/audio/" + originalName;
        upload.totalChunks = totalChunks;
        upload.receivedChunks = 0;
        upload.lastActivity = millis();
        upload.totalSize = 0;
        
        // Ensure directory exists
        if (!SPIFFS.exists("/audio")) {
            SPIFFS.mkdir("/audio");
        }
        
        // Open file for writing
        upload.file = SPIFFS.open(upload.path, "w");
        if (!upload.file) {
            Serial.println("[Chunked Upload] Failed to create file");
            request->send(500, "application/json", "{\"error\":\"Failed to create file\"}");
            return;
        }
        
        activeUploads[uploadId] = upload;
        Serial.printf("[Chunked Upload] Started: %s (%d chunks)\n", 
                     upload.path.c_str(), totalChunks);
    }
    
    // Get active upload
    ChunkedUpload& upload = activeUploads[uploadId];
    upload.lastActivity = millis();
    
    // Seek to correct position for this chunk
    size_t chunkSize = request->hasHeader("X-Chunk-Size") ? 
                      request->getHeader("X-Chunk-Size")->value().toInt() : len;
    
    size_t seekPosition = chunkIndex * chunkSize;
    upload.file.seek(seekPosition);
    
    // Write chunk data
    size_t written = upload.file.write(data, len);
    if (written != len) {
        Serial.printf("[Chunked Upload] Write error: chunk %d\n", chunkIndex);
        request->send(500, "application/json", "{\"error\":\"Write failed\"}");
        return;
    }
    
    // Update chunk tracking
    upload.chunkMap[chunkIndex] = true;
    upload.receivedChunks = upload.chunkMap.size();
    upload.totalSize += written;
    
    // Progress update
    float progress = (float)upload.receivedChunks / upload.totalChunks * 100;
    Serial.printf("[Chunked Upload] Progress: %.1f%% (%d/%d chunks)\n", 
                 progress, upload.receivedChunks, upload.totalChunks);
    
    // Send progress via WebSocket
    JsonDocument doc;
    doc["type"] = "upload_progress";
    doc["upload_id"] = uploadId;
    doc["progress"] = progress;
    doc["chunks_received"] = upload.receivedChunks;
    doc["total_chunks"] = upload.totalChunks;
    
    String output;
    serializeJson(doc, output);
    ws.textAll(output);
}

void WebServer::handleChunkedUploadComplete(AsyncWebServerRequest *request) {
    String uploadId = request->hasHeader("X-Upload-ID") ? 
                     request->getHeader("X-Upload-ID")->value() : "";
    
    if (activeUploads.find(uploadId) == activeUploads.end()) {
        request->send(404, "application/json", "{\"error\":\"Upload not found\"}");
        return;
    }
    
    ChunkedUpload& upload = activeUploads[uploadId];
    
    // Verify all chunks received
    bool complete = (upload.receivedChunks == upload.totalChunks);
    for (size_t i = 0; i < upload.totalChunks && complete; i++) {
        if (upload.chunkMap.find(i) == upload.chunkMap.end()) {
            complete = false;
            break;
        }
    }
    
    if (!complete) {
        // Return list of missing chunks
        JsonDocument doc;
        JsonArray missing = doc.createNestedArray("missing_chunks");
        
        for (size_t i = 0; i < upload.totalChunks; i++) {
            if (upload.chunkMap.find(i) == upload.chunkMap.end()) {
                missing.add(i);
            }
        }
        
        doc["error"] = "Missing chunks";
        doc["received"] = upload.receivedChunks;
        doc["expected"] = upload.totalChunks;
        
        String response;
        serializeJson(doc, response);
        request->send(400, "application/json", response);
        return;
    }
    
    // Close file
    upload.file.close();
    
    Serial.printf("[Chunked Upload] Complete: %s (%d bytes)\n", 
                 upload.path.c_str(), upload.totalSize);
    
    // Send success response
    JsonDocument response;
    response["status"] = "success";
    response["filename"] = upload.path;
    response["size"] = upload.totalSize;
    response["chunks"] = upload.totalChunks;
    
    String output;
    serializeJson(response, output);
    request->send(200, "application/json", output);
    
    // Notify via WebSocket
    JsonDocument wsMsg;
    wsMsg["type"] = "upload_complete";
    wsMsg["upload_id"] = uploadId;
    wsMsg["filename"] = upload.path;
    wsMsg["size"] = upload.totalSize;
    
    String wsOutput;
    serializeJson(wsMsg, wsOutput);
    ws.textAll(wsOutput);
    
    // Clean up
    activeUploads.erase(uploadId);
}

void WebServer::cleanupStaleUploads() {
    unsigned long now = millis();
    std::vector<String> toRemove;
    
    for (auto& pair : activeUploads) {
        // Remove uploads inactive for more than 5 minutes
        if (now - pair.second.lastActivity > 300000) {
            pair.second.file.close();
            SPIFFS.remove(pair.second.path);
            toRemove.push_back(pair.first);
            Serial.printf("[Chunked Upload] Cleaned up stale upload: %s\n", 
                         pair.second.path.c_str());
        }
    }
    
    for (const String& id : toRemove) {
        activeUploads.erase(id);
    }
}

void WebServer::handleAudioSyncCommand(JsonDocument& doc) {
    String cmd = doc["cmd"];
    
    if (cmd == "measure_latency") {
        // Enhanced latency measurement
        handleLatencyMeasurement(doc);
        
    } else if (cmd == "load_audio_data") {
        String filename = doc["filename"];
        bool streaming = doc["streaming"] | false;
        
        Serial.printf("[Audio Sync] Loading audio data: %s (streaming: %s)\n", 
                     filename.c_str(), streaming ? "true" : "false");
        
        if (!vpDecoder) {
            vpDecoder = new VPDecoder();
        }
        
        bool success = false;
        if (streaming) {
            // Use streaming parser for large files
            success = vpDecoder->loadFromFile(filename);
        } else {
            // Load entire file for small files
            File file = SPIFFS.open(filename, "r");
            if (file) {
                String jsonData = file.readString();
                file.close();
                success = vpDecoder->loadFromJson(jsonData);
            }
        }
        
        if (success) {
            sendSyncStatus("loaded", filename);
            Serial.printf("[Audio Sync] Successfully loaded. Duration: %.1fs, BPM: %d\n", 
                         vpDecoder->getDuration() / 1000.0f, vpDecoder->getBPM());
        } else {
            sendSyncStatus("error", "Failed to load audio data");
        }
        
    } else if (cmd == "prepare_sync_play") {
        // Enhanced sync with latency compensation
        unsigned long startTime = doc["start_time"];
        syncOffset = doc["offset"] | 0;
        float networkLatency = doc["network_latency"] | latencyStats.average;
        syncActive = true;
        
        Serial.printf("[Audio Sync] Preparing sync play\n");
        Serial.printf("  Start time: %lu\n", startTime);
        Serial.printf("  Sync offset: %d ms\n", syncOffset);
        Serial.printf("  Network latency: %.1f ms (avg: %.1f ms)\n", 
                     networkLatency, latencyStats.average);
        
        // Advanced timing compensation
        unsigned long now = millis();
        unsigned long clientTime = doc["client_time"] | now;
        unsigned long measuredLatency = (now - clientTime) / 2;
        
        // Use the better of provided or measured latency
        float compensationLatency = (networkLatency > 0) ? networkLatency : measuredLatency;
        
        // Adjust start time for network latency and user offset
        unsigned long adjustedStartTime = startTime - (unsigned long)compensationLatency + syncOffset;
        unsigned long delay = 0;
        
        if (adjustedStartTime > now) {
            delay = adjustedStartTime - now;
        }
        
        Serial.printf("  Compensation latency: %.1f ms\n", compensationLatency);
        Serial.printf("  Adjusted delay: %lu ms\n", delay);
        
        // High-precision scheduling
        schedulePlaybackStart(delay);
        
        JsonDocument response;
        response["type"] = "sync_prepared";
        response["delay"] = delay;
        response["latency_compensation"] = compensationLatency;
        String output;
        serializeJson(response, output);
        ws.textAll(output);
        
    } else if (cmd == "sync_heartbeat") {
        handleSyncHeartbeat(doc);
        
    } else if (cmd == "stop_playback") {
        stopSyncPlayback();
    }
}

void WebServer::handleLatencyMeasurement(JsonDocument& doc) {
    static unsigned long pingSequence = 0;
    
    if (doc["type"] == "ping") {
        // Immediate pong response with timestamps
        JsonDocument pong;
        pong["type"] = "pong";
        pong["sequence"] = doc["sequence"];
        pong["client_time"] = doc["timestamp"];
        pong["server_time"] = millis();
        
        String output;
        serializeJson(pong, output);
        ws.textAll(output);
        
    } else if (doc["type"] == "latency_report") {
        // Client calculated latency
        float latency = doc["latency"];
        
        // Add to rolling samples
        if (latencyStats.sampleCount < 10) {
            latencyStats.samples[latencyStats.sampleCount++] = latency;
        } else {
            // Shift samples and add new one
            for (int i = 0; i < 9; i++) {
                latencyStats.samples[i] = latencyStats.samples[i + 1];
            }
            latencyStats.samples[9] = latency;
        }
        
        // Calculate statistics
        updateLatencyStats();
        
        Serial.printf("[Latency] New sample: %.1fms, Average: %.1fms, Median: %.1fms\n",
                     latency, latencyStats.average, latencyStats.median);
    }
}

void WebServer::updateLatencyStats() {
    if (latencyStats.sampleCount == 0) return;
    
    // Calculate average
    float sum = 0;
    for (size_t i = 0; i < latencyStats.sampleCount; i++) {
        sum += latencyStats.samples[i];
    }
    latencyStats.average = sum / latencyStats.sampleCount;
    
    // Calculate median
    float sorted[10];
    memcpy(sorted, latencyStats.samples, latencyStats.sampleCount * sizeof(float));
    
    // Simple bubble sort for small array
    for (size_t i = 0; i < latencyStats.sampleCount - 1; i++) {
        for (size_t j = 0; j < latencyStats.sampleCount - i - 1; j++) {
            if (sorted[j] > sorted[j + 1]) {
                float temp = sorted[j];
                sorted[j] = sorted[j + 1];
                sorted[j + 1] = temp;
            }
        }
    }
    
    if (latencyStats.sampleCount % 2 == 0) {
        size_t mid = latencyStats.sampleCount / 2;
        latencyStats.median = (sorted[mid - 1] + sorted[mid]) / 2;
    } else {
        latencyStats.median = sorted[latencyStats.sampleCount / 2];
    }
    
    latencyStats.lastMeasurement = millis();
}

void WebServer::handleSyncHeartbeat(JsonDocument& doc) {
    float clientTime = doc["time_ms"];
    float drift = doc["drift_ms"];
    
    if (vpDecoder && vpDecoder->isPlaying()) {
        float deviceTime = vpDecoder->getCurrentTime();
        float deviceDrift = abs(deviceTime - clientTime);
        
        // Adaptive sync correction
        if (deviceDrift > 50) {
            Serial.printf("[Audio Sync] Drift detected - Client: %.1fms, Device: %.1fms, Drift: %.1fms\n", 
                         clientTime, deviceTime, deviceDrift);
            
            // Send drift correction command
            JsonDocument correction;
            correction["type"] = "drift_correction";
            correction["device_time"] = deviceTime;
            correction["suggested_rate"] = (deviceTime > clientTime) ? 0.98f : 1.02f;
            
            String output;
            serializeJson(correction, output);
            ws.textAll(output);
        }
        
        // Update sync metrics
        static unsigned long lastMetricUpdate = 0;
        if (millis() - lastMetricUpdate > 1000) {
            JsonDocument metrics;
            metrics["type"] = "sync_metrics";
            metrics["device_time"] = deviceTime;
            metrics["client_time"] = clientTime;
            metrics["drift"] = deviceDrift;
            metrics["network_latency"] = latencyStats.average;
            metrics["on_beat"] = vpDecoder->isOnBeat();
            
            String output;
            serializeJson(metrics, output);
            ws.textAll(output);
            
            lastMetricUpdate = millis();
        }
    }
}

void WebServer::schedulePlaybackStart(unsigned long delayMs) {
    // Create a high-priority timer for precise start
    TimerHandle_t timer = xTimerCreate(
        "SyncStart",
        pdMS_TO_TICKS(delayMs),
        pdFALSE,  // One-shot timer
        this,     // Timer ID (this pointer)
        [](TimerHandle_t xTimer) {
            WebServer* server = (WebServer*)pvTimerGetTimerID(xTimer);
            
            // Start VP decoder playback
            if (server->vpDecoder) {
                server->vpDecoder->startPlayback();
                server->syncStartTime = millis();
                Serial.println("[Audio Sync] Playback started!");
                
                // Notify clients with precise timing
                JsonDocument status;
                status["type"] = "playback_started";
                status["device_time"] = server->syncStartTime;
                status["latency_stats"]["average"] = server->latencyStats.average;
                status["latency_stats"]["median"] = server->latencyStats.median;
                
                String output;
                serializeJson(status, output);
                server->ws.textAll(output);
            }
            
            // Delete timer after use
            xTimerDelete(xTimer, 0);
        }
    );
    
    if (timer != NULL) {
        xTimerStart(timer, 0);
    } else {
        Serial.println("[Audio Sync] Failed to create sync timer!");
    }
}

// Integration with main loop - enhanced version
void WebServer::updateAudioSync() {
    if (!vpDecoder || !vpDecoder->isPlaying()) {
        return;
    }
    
    // Get current audio frame from VP decoder
    AudioFrame audioFrame = vpDecoder->getCurrentFrame();
    
    // Make audio frame available to effects engine
    if (effectEngine) {
        effectEngine->setAudioFrame(audioFrame);
    }
    
    // Beat detection with visual feedback
    static bool lastBeatState = false;
    bool currentBeatState = vpDecoder->isOnBeat();
    
    if (currentBeatState && !lastBeatState) {
        // Beat detected - trigger visual feedback
        if (beatCallback) {
            beatCallback();
        }
        
        // Send beat notification to web clients
        JsonDocument beat;
        beat["type"] = "beat";
        beat["time"] = vpDecoder->getCurrentTime();
        beat["confidence"] = audioFrame.beat_confidence;
        
        String output;
        serializeJson(beat, output);
        ws.textAll(output);
    }
    
    lastBeatState = currentBeatState;
    
    // Check if VP decoder needs data refresh (for large files)
    static unsigned long lastRefreshCheck = 0;
    if (millis() - lastRefreshCheck > 1000) {
        if (vpDecoder->needsDataRefresh(vpDecoder->getCurrentTime())) {
            // VP decoder will handle loading next data window internally
            Serial.println("[Audio Sync] VP Decoder refreshing data window");
        }
        lastRefreshCheck = millis();
    }
}