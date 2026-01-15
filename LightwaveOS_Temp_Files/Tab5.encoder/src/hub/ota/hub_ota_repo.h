/**
 * @file hub_ota_repo.h
 * @brief OTA Repository - serves firmware manifest and binaries from LittleFS
 */

#pragma once

#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

struct ota_release_t {
    char version[32];
    char url[128];
    char sha256[65];
    uint32_t size;
};

class HubOtaRepo {
public:
    HubOtaRepo();
    
    bool begin(FS* fs);
    
    // Query manifest for a specific track (e.g. "stable", "beta")
    bool getReleaseForTrack(const char* platform, const char* track, ota_release_t* release_out);
    
    // Validate that a URL path maps to an existing .bin file
    bool validateBinaryPath(const char* urlPath);
    
    // Convert URL path to filesystem path (e.g. "/ota/k1/s3/v1.2.3.bin" -> "/ota/k1/s3/v1.2.3.bin")
    String urlToFsPath(const char* urlPath);
    
private:
    FS* fs_;
    JsonDocument manifest_;
    bool loaded_;
    
    bool loadManifest();
};
