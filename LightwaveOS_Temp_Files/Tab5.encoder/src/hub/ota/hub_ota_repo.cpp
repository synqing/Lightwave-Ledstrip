/**
 * @file hub_ota_repo.cpp
 * @brief OTA Repository Implementation
 */

#include "hub_ota_repo.h"
#include "../../common/util/logging.h"

#define LW_LOG_TAG "HubOtaRepo"

HubOtaRepo::HubOtaRepo() : fs_(nullptr), loaded_(false) {}

bool HubOtaRepo::begin(FS* fs) {
    fs_ = fs;
    
    if (!fs_) {
        LW_LOGE("Filesystem not provided");
        return false;
    }
    
    return loadManifest();
}

bool HubOtaRepo::loadManifest() {
    if (!fs_->exists("/ota/manifest.json")) {
        LW_LOGW("OTA manifest not found at /ota/manifest.json");
        return false;
    }
    
    File f = fs_->open("/ota/manifest.json", "r");
    if (!f) {
        LW_LOGE("Failed to open manifest");
        return false;
    }
    
    DeserializationError error = deserializeJson(manifest_, f);
    f.close();
    
    if (error) {
        LW_LOGE("Failed to parse manifest: %s", error.c_str());
        return false;
    }
    
    loaded_ = true;
    LW_LOGI("OTA manifest loaded successfully");
    return true;
}

bool HubOtaRepo::getReleaseForTrack(const char* platform, const char* track, ota_release_t* release_out) {
    if (!loaded_) {
        LW_LOGE("Manifest not loaded");
        return false;
    }
    
    // Navigate manifest: platforms.<platform>.releases.<track>
    JsonObject platforms = manifest_["platforms"];
    if (!platforms) {
        LW_LOGE("No platforms in manifest");
        return false;
    }
    
    JsonObject platformObj = platforms[platform];
    if (!platformObj) {
        LW_LOGE("Platform '%s' not found in manifest", platform);
        return false;
    }
    
    JsonObject releases = platformObj["releases"];
    if (!releases) {
        LW_LOGE("No releases for platform '%s'", platform);
        return false;
    }
    
    JsonObject release = releases[track];
    if (!release) {
        LW_LOGE("Track '%s' not found for platform '%s'", track, platform);
        return false;
    }
    
    // Populate release struct
    strlcpy(release_out->version, release["version"] | "", sizeof(release_out->version));
    strlcpy(release_out->url, release["url"] | "", sizeof(release_out->url));
    strlcpy(release_out->sha256, release["sha256"] | "", sizeof(release_out->sha256));
    release_out->size = release["size"] | 0;
    
    LW_LOGI("Found release: platform=%s track=%s version=%s", platform, track, release_out->version);
    return true;
}

bool HubOtaRepo::validateBinaryPath(const char* urlPath) {
    // Convert URL path to filesystem path
    String fsPath = urlToFsPath(urlPath);
    
    // Check if file exists
    if (!fs_->exists(fsPath)) {
        LW_LOGW("Binary not found: %s", fsPath.c_str());
        return false;
    }
    
    return true;
}

String HubOtaRepo::urlToFsPath(const char* urlPath) {
    // Validate path doesn't contain directory traversal
    if (strstr(urlPath, "..")) {
        LW_LOGW("Path traversal detected: %s", urlPath);
        return "";
    }
    
    // URL path already includes /ota/ prefix, use as-is
    return String(urlPath);
}
