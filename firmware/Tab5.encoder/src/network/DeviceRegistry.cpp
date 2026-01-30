// ============================================================================
// DeviceRegistry Implementation - Tab5.encoder
// ============================================================================
// Multi-device discovery, fingerprinting, and selection with NVS persistence.
// All public methods are mutex-protected for thread safety.
// ============================================================================

#include "DeviceRegistry.h"

#if ENABLE_WIFI

#include <nvs_flash.h>
#include <nvs.h>
#include <esp_task_wdt.h>
#include <cstring>

// ============================================================================
// Construction / Destruction
// ============================================================================

DeviceRegistry::DeviceRegistry()
    : _deviceCount(0)
    , _selectedIndex(-1)
    , _fingerprintTaskHandle(nullptr)
    , _fingerprintCancelRequested(false)
    , _selectionChangedCb(nullptr)
{
    for (uint8_t i = 0; i < MAX_DEVICES; i++) {
        _devices[i].clear();
    }
    _mutex = xSemaphoreCreateMutex();
    if (!_mutex) {
        Serial.println("[DeviceRegistry] FATAL: Failed to create mutex");
    }
}

DeviceRegistry::~DeviceRegistry() {
    // Signal fingerprint task to stop
    _fingerprintCancelRequested = true;

    if (_fingerprintTaskHandle != nullptr) {
        uint32_t timeoutAt = millis() + 1000;
        while (_fingerprintTaskHandle != nullptr && millis() < timeoutAt) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        // Force-delete if it didn't exit cleanly
        if (_fingerprintTaskHandle != nullptr) {
            Serial.println("[DeviceRegistry] WARNING: Force-deleting fingerprint task");
            vTaskDelete(_fingerprintTaskHandle);
            _fingerprintTaskHandle = nullptr;
        }
    }

    if (_mutex) {
        vSemaphoreDelete(_mutex);
        _mutex = nullptr;
    }
}

// ============================================================================
// Initialization
// ============================================================================

bool DeviceRegistry::begin() {
    Serial.println("[DeviceRegistry] Initializing...");

    uint8_t loaded = loadFromNVS();
    if (loaded > 0) {
        Serial.printf("[DeviceRegistry] Loaded %d device(s) from NVS\n", loaded);
        // Mark all restored devices as NVS_RESTORED and not yet reachable
        for (uint8_t i = 0; i < MAX_DEVICES; i++) {
            if (_devices[i].isValid()) {
                _devices[i].discoverySource = DeviceInfo::Source::NVS_RESTORED;
                _devices[i].reachable = false;
                _devices[i].verified = false;
            }
        }
    } else {
        Serial.println("[DeviceRegistry] No saved devices found in NVS");
    }

    Serial.printf("[DeviceRegistry] Ready (%d devices, selected=%d)\n",
                  _deviceCount, _selectedIndex);
    return true;
}

// ============================================================================
// Device Management
// ============================================================================

int8_t DeviceRegistry::addDiscoveredDevice(IPAddress ip, DeviceInfo::Source src,
                                           const char* hostname) {
    if (ip == IPAddress(0, 0, 0, 0)) {
        Serial.println("[DeviceRegistry] Ignoring device with zero IP");
        return -1;
    }

    if (!takeLock()) return -1;

    // Check for existing entry (deduplicate by IP)
    int8_t existing = -1;
    for (uint8_t i = 0; i < MAX_DEVICES; i++) {
        if (_devices[i].isValid() && _devices[i].ip == ip) {
            existing = i;
            break;
        }
    }

    if (existing >= 0) {
        // Update existing entry
        _devices[existing].discoverySource = src;
        _devices[existing].lastSeenMs = millis();
        if (hostname && hostname[0] != '\0') {
            strncpy(_devices[existing].hostname, hostname,
                    sizeof(_devices[existing].hostname) - 1);
            _devices[existing].hostname[sizeof(_devices[existing].hostname) - 1] = '\0';
        }
        releaseLock();

        Serial.printf("[DeviceRegistry] Updated existing device at index %d: %s\n",
                      existing, ip.toString().c_str());
        saveToNVS();
        return existing;
    }

    // Find empty slot
    int8_t slot = findEmptySlot();
    if (slot < 0) {
        releaseLock();
        Serial.println("[DeviceRegistry] Registry full, cannot add device");
        return -1;
    }

    // Populate new entry
    _devices[slot].clear();
    _devices[slot].ip = ip;
    _devices[slot].discoverySource = src;
    _devices[slot].lastSeenMs = millis();
    _devices[slot].rssi = INT32_MIN;

    if (hostname && hostname[0] != '\0') {
        strncpy(_devices[slot].hostname, hostname,
                sizeof(_devices[slot].hostname) - 1);
        _devices[slot].hostname[sizeof(_devices[slot].hostname) - 1] = '\0';
    }

    _deviceCount++;
    releaseLock();

    Serial.printf("[DeviceRegistry] Added device at index %d: %s (source=%d)\n",
                  slot, ip.toString().c_str(), (int)src);
    saveToNVS();
    return slot;
}

int8_t DeviceRegistry::addManualDevice(IPAddress ip, const char* friendlyName) {
    int8_t idx = addDiscoveredDevice(ip, DeviceInfo::Source::MANUAL);
    if (idx >= 0 && friendlyName && friendlyName[0] != '\0') {
        setFriendlyName(idx, friendlyName);
    }
    return idx;
}

bool DeviceRegistry::removeDevice(uint8_t index) {
    if (index >= MAX_DEVICES) return false;

    if (!takeLock()) return false;

    if (!_devices[index].isValid()) {
        releaseLock();
        return false;
    }

    Serial.printf("[DeviceRegistry] Removing device at index %d: %s\n",
                  index, _devices[index].ip.toString().c_str());

    _devices[index].clear();

    if (_deviceCount > 0) {
        _deviceCount--;
    }

    // Clear selection if the removed device was selected
    bool selectionChanged = false;
    if (_selectedIndex == (int8_t)index) {
        _selectedIndex = -1;
        selectionChanged = true;
    }

    releaseLock();

    saveToNVS();

    if (selectionChanged) {
        notifySelectionChanged();
    }

    return true;
}

void DeviceRegistry::clearAll() {
    if (!takeLock()) return;

    Serial.println("[DeviceRegistry] Clearing all devices");

    for (uint8_t i = 0; i < MAX_DEVICES; i++) {
        _devices[i].clear();
    }
    _deviceCount = 0;

    bool selectionChanged = (_selectedIndex != -1);
    _selectedIndex = -1;

    releaseLock();

    saveToNVS();

    if (selectionChanged) {
        notifySelectionChanged();
    }
}

// ============================================================================
// Fingerprinting
// ============================================================================

bool DeviceRegistry::fingerprintDevice(uint8_t index) {
    if (index >= MAX_DEVICES) return false;

    if (!takeLock()) return false;

    if (!_devices[index].isValid()) {
        releaseLock();
        Serial.printf("[DeviceRegistry] Cannot fingerprint index %d: slot empty\n", index);
        return false;
    }

    if (_fingerprintTaskHandle != nullptr) {
        releaseLock();
        Serial.println("[DeviceRegistry] Fingerprint task already running");
        return false;
    }

    _fingerprintCancelRequested = false;

    // Allocate context on heap (freed by task)
    FingerprintContext* ctx = new FingerprintContext();
    ctx->registry = this;
    ctx->deviceIndex = (int8_t)index;

    BaseType_t taskOk = xTaskCreate(
        &DeviceRegistry::fingerprintTask,
        "dev_fprint",
        4096,
        ctx,
        1,
        &_fingerprintTaskHandle
    );

    if (taskOk != pdPASS) {
        delete ctx;
        _fingerprintTaskHandle = nullptr;
        releaseLock();
        Serial.println("[DeviceRegistry] Failed to create fingerprint task");
        return false;
    }

    releaseLock();

    Serial.printf("[DeviceRegistry] Started fingerprint task for device %d\n", index);
    return true;
}

bool DeviceRegistry::fingerprintAll() {
    if (!takeLock()) return false;

    if (_fingerprintTaskHandle != nullptr) {
        releaseLock();
        Serial.println("[DeviceRegistry] Fingerprint task already running");
        return false;
    }

    _fingerprintCancelRequested = false;

    FingerprintContext* ctx = new FingerprintContext();
    ctx->registry = this;
    ctx->deviceIndex = -1;  // -1 means fingerprint all

    BaseType_t taskOk = xTaskCreate(
        &DeviceRegistry::fingerprintTask,
        "dev_fprint",
        4096,
        ctx,
        1,
        &_fingerprintTaskHandle
    );

    if (taskOk != pdPASS) {
        delete ctx;
        _fingerprintTaskHandle = nullptr;
        releaseLock();
        Serial.println("[DeviceRegistry] Failed to create fingerprint-all task");
        return false;
    }

    releaseLock();

    Serial.println("[DeviceRegistry] Started fingerprint-all task");
    return true;
}

bool DeviceRegistry::isFingerprintRunning() const {
    return _fingerprintTaskHandle != nullptr;
}

void DeviceRegistry::fingerprintTask(void* parameter) {
    FingerprintContext* ctx = static_cast<FingerprintContext*>(parameter);
    DeviceRegistry* reg = ctx->registry;
    int8_t targetIndex = ctx->deviceIndex;
    delete ctx;

    if (targetIndex >= 0) {
        // Single device fingerprint
        reg->runFingerprintSingle((uint8_t)targetIndex);
    } else {
        // Fingerprint all valid devices
        for (uint8_t i = 0; i < MAX_DEVICES; i++) {
            if (reg->_fingerprintCancelRequested) {
                Serial.println("[DeviceRegistry] Fingerprint-all cancelled");
                break;
            }

            if (reg->takeLock()) {
                bool valid = reg->_devices[i].isValid();
                reg->releaseLock();

                if (valid) {
                    reg->runFingerprintSingle(i);
                    // Stagger between devices to avoid network congestion
                    vTaskDelay(pdMS_TO_TICKS(DeviceNVS::FINGERPRINT_STAGGER_MS));
                }
            }
        }
    }

    // Clean up: clear task handle before self-delete
    if (reg->takeLock()) {
        reg->_fingerprintTaskHandle = nullptr;
        reg->releaseLock();
    } else {
        // Fallback: set directly if lock unavailable
        reg->_fingerprintTaskHandle = nullptr;
    }

    Serial.println("[DeviceRegistry] Fingerprint task complete");
    vTaskDelete(nullptr);
}

bool DeviceRegistry::runFingerprintSingle(uint8_t index) {
    if (index >= MAX_DEVICES) return false;

    // Read the IP under lock
    IPAddress targetIP;
    if (!takeLock()) return false;
    if (!_devices[index].isValid()) {
        releaseLock();
        return false;
    }
    targetIP = _devices[index].ip;
    releaseLock();

    Serial.printf("[DeviceRegistry] Fingerprinting device %d at %s...\n",
                  index, targetIP.toString().c_str());

    WiFiClient client;
    client.setTimeout(DeviceNVS::FINGERPRINT_TIMEOUT_MS);

    esp_task_wdt_reset();

    if (!client.connect(targetIP, 80)) {
        Serial.printf("[DeviceRegistry] Device %d: connection failed\n", index);
        if (takeLock()) {
            _devices[index].reachable = false;
            _devices[index].lastFingerprinted = millis();
            releaseLock();
        }
        return false;
    }

    esp_task_wdt_reset();

    // Send HTTP GET request
    client.print("GET /api/v1/device/info HTTP/1.1\r\n");
    client.print("Host: ");
    client.print(targetIP.toString());
    client.print("\r\n");
    client.print("Connection: close\r\n\r\n");

    // Wait for response with timeout
    uint32_t startTime = millis();
    while (!client.available() &&
           (millis() - startTime) < DeviceNVS::FINGERPRINT_TIMEOUT_MS) {
        vTaskDelay(pdMS_TO_TICKS(10));
        esp_task_wdt_reset();
    }

    if (!client.available()) {
        Serial.printf("[DeviceRegistry] Device %d: response timeout\n", index);
        client.stop();
        if (takeLock()) {
            _devices[index].reachable = false;
            _devices[index].lastFingerprinted = millis();
            releaseLock();
        }
        return false;
    }

    esp_task_wdt_reset();

    // Read response (max 512 bytes)
    String response = "";
    startTime = millis();
    while (client.available() &&
           (millis() - startTime) < DeviceNVS::FINGERPRINT_TIMEOUT_MS) {
        response += (char)client.read();
        if (response.length() >= 512) break;
        esp_task_wdt_reset();
    }
    client.stop();

    // Check for LightwaveOS signature
    bool isLightwave = (response.indexOf("lightwaveos") >= 0 ||
                        response.indexOf("LightwaveOS") >= 0 ||
                        response.indexOf("\"board\":\"ESP32-S3\"") >= 0);

    // Try to extract JSON body (after blank line in HTTP response)
    String firmwareVer = "";
    uint8_t effectCnt = 0;

    if (isLightwave) {
        int bodyStart = response.indexOf("\r\n\r\n");
        if (bodyStart < 0) {
            bodyStart = response.indexOf("\n\n");
        }

        if (bodyStart >= 0) {
            String body = response.substring(bodyStart + (response.charAt(bodyStart) == '\r' ? 4 : 2));

            // Simple JSON field extraction (avoid ArduinoJson in this task to save stack)
            // Look for "firmware":"x.y.z" or "firmwareVersion":"x.y.z"
            int fwIdx = body.indexOf("\"firmware\":");
            if (fwIdx < 0) fwIdx = body.indexOf("\"firmwareVersion\":");
            if (fwIdx >= 0) {
                int valStart = body.indexOf('"', body.indexOf(':', fwIdx) + 1);
                if (valStart >= 0) {
                    int valEnd = body.indexOf('"', valStart + 1);
                    if (valEnd > valStart && (valEnd - valStart) < 16) {
                        firmwareVer = body.substring(valStart + 1, valEnd);
                    }
                }
            }

            // Look for "effectCount":N or "effects":N
            int ecIdx = body.indexOf("\"effectCount\":");
            if (ecIdx < 0) ecIdx = body.indexOf("\"effects\":");
            if (ecIdx >= 0) {
                int colonPos = body.indexOf(':', ecIdx);
                if (colonPos >= 0) {
                    String numStr = "";
                    for (int j = colonPos + 1; j < body.length() && j < colonPos + 6; j++) {
                        char c = body.charAt(j);
                        if (c >= '0' && c <= '9') {
                            numStr += c;
                        } else if (numStr.length() > 0) {
                            break;
                        }
                    }
                    if (numStr.length() > 0) {
                        effectCnt = (uint8_t)numStr.toInt();
                    }
                }
            }
        }
    }

    // Update device info under lock
    if (takeLock()) {
        _devices[index].reachable = true;
        _devices[index].verified = isLightwave;
        _devices[index].lastSeenMs = millis();
        _devices[index].lastFingerprinted = millis();

        if (isLightwave) {
            if (firmwareVer.length() > 0) {
                strncpy(_devices[index].firmwareVersion, firmwareVer.c_str(),
                        sizeof(_devices[index].firmwareVersion) - 1);
                _devices[index].firmwareVersion[sizeof(_devices[index].firmwareVersion) - 1] = '\0';
            }
            if (effectCnt > 0) {
                _devices[index].effectCount = effectCnt;
            }
        }

        releaseLock();
    }

    Serial.printf("[DeviceRegistry] Device %d: %s (verified=%s, fw=%s, effects=%d)\n",
                  index, targetIP.toString().c_str(),
                  isLightwave ? "YES" : "NO",
                  firmwareVer.length() > 0 ? firmwareVer.c_str() : "?",
                  effectCnt);

    return isLightwave;
}

// ============================================================================
// Selection
// ============================================================================

bool DeviceRegistry::selectDevice(int8_t index) {
    if (!takeLock()) return false;

    // Validate index
    if (index >= 0) {
        if ((uint8_t)index >= MAX_DEVICES || !_devices[index].isValid()) {
            releaseLock();
            Serial.printf("[DeviceRegistry] Cannot select index %d: invalid or empty\n", index);
            return false;
        }
    }

    if (_selectedIndex == index) {
        releaseLock();
        return true;  // Already selected, no change needed
    }

    int8_t oldIndex = _selectedIndex;
    _selectedIndex = index;

    releaseLock();

    if (index >= 0) {
        Serial.printf("[DeviceRegistry] Selected device %d: %s\n",
                      index, _devices[index].displayName());
    } else {
        Serial.printf("[DeviceRegistry] Deselected device (was %d)\n", oldIndex);
    }

    saveToNVS();
    notifySelectionChanged();
    return true;
}

int8_t DeviceRegistry::getSelectedIndex() const {
    if (!takeLock()) return -1;
    int8_t idx = _selectedIndex;
    releaseLock();
    return idx;
}

IPAddress DeviceRegistry::getSelectedIP() const {
    if (!takeLock()) return IPAddress(INADDR_NONE);

    IPAddress result(INADDR_NONE);
    if (_selectedIndex >= 0 && _selectedIndex < MAX_DEVICES &&
        _devices[_selectedIndex].isValid()) {
        result = _devices[_selectedIndex].ip;
    }

    releaseLock();
    return result;
}

const DeviceInfo* DeviceRegistry::getSelectedDevice() const {
    if (!takeLock()) return nullptr;

    const DeviceInfo* result = nullptr;
    if (_selectedIndex >= 0 && _selectedIndex < MAX_DEVICES &&
        _devices[_selectedIndex].isValid()) {
        result = &_devices[_selectedIndex];
    }

    releaseLock();
    return result;
}

bool DeviceRegistry::autoSelect() {
    if (!takeLock()) return false;

    // Priority 1: NVS saved selection (if still valid)
    if (_selectedIndex >= 0 && _selectedIndex < MAX_DEVICES &&
        _devices[_selectedIndex].isValid()) {
        releaseLock();
        Serial.printf("[DeviceRegistry] Auto-select: keeping NVS selection (index %d)\n",
                      _selectedIndex);
        return true;
    }

    // Priority 2: Verified device with strongest RSSI
    int8_t bestVerified = -1;
    int32_t bestRSSI = INT32_MIN;
    for (uint8_t i = 0; i < MAX_DEVICES; i++) {
        if (_devices[i].isValid() && _devices[i].verified) {
            if (_devices[i].rssi > bestRSSI) {
                bestRSSI = _devices[i].rssi;
                bestVerified = i;
            }
        }
    }
    if (bestVerified >= 0) {
        _selectedIndex = bestVerified;
        releaseLock();
        Serial.printf("[DeviceRegistry] Auto-select: verified device %d (RSSI %d)\n",
                      bestVerified, bestRSSI);
        saveToNVS();
        notifySelectionChanged();
        return true;
    }

    // Priority 3: First verified device
    for (uint8_t i = 0; i < MAX_DEVICES; i++) {
        if (_devices[i].isValid() && _devices[i].verified) {
            _selectedIndex = i;
            releaseLock();
            Serial.printf("[DeviceRegistry] Auto-select: first verified device %d\n", i);
            saveToNVS();
            notifySelectionChanged();
            return true;
        }
    }

    // Priority 4: First reachable device
    for (uint8_t i = 0; i < MAX_DEVICES; i++) {
        if (_devices[i].isValid() && _devices[i].reachable) {
            _selectedIndex = i;
            releaseLock();
            Serial.printf("[DeviceRegistry] Auto-select: first reachable device %d\n", i);
            saveToNVS();
            notifySelectionChanged();
            return true;
        }
    }

    // Priority 5: First valid device
    for (uint8_t i = 0; i < MAX_DEVICES; i++) {
        if (_devices[i].isValid()) {
            _selectedIndex = i;
            releaseLock();
            Serial.printf("[DeviceRegistry] Auto-select: first valid device %d\n", i);
            saveToNVS();
            notifySelectionChanged();
            return true;
        }
    }

    releaseLock();
    Serial.println("[DeviceRegistry] Auto-select: no devices available");
    return false;
}

// ============================================================================
// Query
// ============================================================================

uint8_t DeviceRegistry::getDeviceCount() const {
    if (!takeLock()) return 0;
    uint8_t count = _deviceCount;
    releaseLock();
    return count;
}

const DeviceInfo* DeviceRegistry::getDevice(uint8_t index) const {
    if (index >= MAX_DEVICES) return nullptr;

    if (!takeLock()) return nullptr;

    const DeviceInfo* result = nullptr;
    if (_devices[index].isValid()) {
        result = &_devices[index];
    }

    releaseLock();
    return result;
}

int8_t DeviceRegistry::findByIP(IPAddress ip) const {
    if (!takeLock()) return -1;

    for (uint8_t i = 0; i < MAX_DEVICES; i++) {
        if (_devices[i].isValid() && _devices[i].ip == ip) {
            releaseLock();
            return i;
        }
    }

    releaseLock();
    return -1;
}

bool DeviceRegistry::setFriendlyName(uint8_t index, const char* name) {
    if (index >= MAX_DEVICES || !name) return false;

    if (!takeLock()) return false;

    if (!_devices[index].isValid()) {
        releaseLock();
        return false;
    }

    strncpy(_devices[index].friendlyName, name,
            sizeof(_devices[index].friendlyName) - 1);
    _devices[index].friendlyName[sizeof(_devices[index].friendlyName) - 1] = '\0';

    releaseLock();

    Serial.printf("[DeviceRegistry] Set friendly name for device %d: %s\n",
                  index, name);
    saveToNVS();
    return true;
}

void DeviceRegistry::updateReachability(uint8_t index, bool reachable,
                                        int32_t rssi) {
    if (index >= MAX_DEVICES) return;

    if (!takeLock()) return;

    if (!_devices[index].isValid()) {
        releaseLock();
        return;
    }

    _devices[index].reachable = reachable;
    if (reachable) {
        _devices[index].lastSeenMs = millis();
    }
    if (rssi != INT32_MIN) {
        _devices[index].rssi = rssi;
    }

    releaseLock();
}

// ============================================================================
// Callbacks
// ============================================================================

void DeviceRegistry::onSelectionChanged(SelectionChangedCallback cb) {
    _selectionChangedCb = cb;
}

void DeviceRegistry::notifySelectionChanged() {
    if (_selectionChangedCb) {
        const DeviceInfo* dev = nullptr;
        if (_selectedIndex >= 0 && _selectedIndex < MAX_DEVICES &&
            _devices[_selectedIndex].isValid()) {
            dev = &_devices[_selectedIndex];
        }
        _selectionChangedCb(_selectedIndex, dev);
    }
}

// ============================================================================
// NVS Persistence
// ============================================================================

bool DeviceRegistry::saveToNVS() {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(DeviceNVS::NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        Serial.printf("[DeviceRegistry] NVS open failed: %s\n", esp_err_to_name(err));
        return false;
    }

    if (!takeLock()) {
        nvs_close(handle);
        return false;
    }

    // Save device count
    nvs_set_u8(handle, DeviceNVS::KEY_COUNT, _deviceCount);

    // Save selected index (stored as uint8_t with 0xFF meaning -1)
    uint8_t selStored = (_selectedIndex >= 0) ? (uint8_t)_selectedIndex : 0xFF;
    nvs_set_u8(handle, DeviceNVS::KEY_SELECTED, selStored);

    // Save per-device data
    char key[16];
    for (uint8_t i = 0; i < MAX_DEVICES; i++) {
        if (_devices[i].isValid()) {
            // Save IP as string
            String ipStr = _devices[i].ip.toString();
            makeNvsKey(key, sizeof(key), i, "ip");
            nvs_set_str(handle, key, ipStr.c_str());

            // Save friendly name
            makeNvsKey(key, sizeof(key), i, "name");
            nvs_set_str(handle, key, _devices[i].friendlyName);

            // Save hostname
            makeNvsKey(key, sizeof(key), i, "host");
            nvs_set_str(handle, key, _devices[i].hostname);
        } else {
            // Erase keys for empty slots
            makeNvsKey(key, sizeof(key), i, "ip");
            nvs_erase_key(handle, key);  // Ignore errors (key may not exist)

            makeNvsKey(key, sizeof(key), i, "name");
            nvs_erase_key(handle, key);

            makeNvsKey(key, sizeof(key), i, "host");
            nvs_erase_key(handle, key);
        }
    }

    releaseLock();

    err = nvs_commit(handle);
    nvs_close(handle);

    if (err != ESP_OK) {
        Serial.printf("[DeviceRegistry] NVS commit failed: %s\n", esp_err_to_name(err));
        return false;
    }

    return true;
}

uint8_t DeviceRegistry::loadFromNVS() {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(DeviceNVS::NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            Serial.println("[DeviceRegistry] NVS namespace not found (first boot)");
        } else {
            Serial.printf("[DeviceRegistry] NVS open failed: %s\n", esp_err_to_name(err));
        }
        return 0;
    }

    if (!takeLock()) {
        nvs_close(handle);
        return 0;
    }

    // Load device count
    uint8_t savedCount = 0;
    err = nvs_get_u8(handle, DeviceNVS::KEY_COUNT, &savedCount);
    if (err != ESP_OK) {
        releaseLock();
        nvs_close(handle);
        return 0;
    }

    // Load selected index
    uint8_t selStored = 0xFF;
    nvs_get_u8(handle, DeviceNVS::KEY_SELECTED, &selStored);
    _selectedIndex = (selStored != 0xFF && selStored < MAX_DEVICES)
                     ? (int8_t)selStored : -1;

    // Load per-device data
    char key[16];
    char valueBuf[64];
    uint8_t loadedCount = 0;

    for (uint8_t i = 0; i < MAX_DEVICES; i++) {
        _devices[i].clear();

        // Load IP
        makeNvsKey(key, sizeof(key), i, "ip");
        size_t valueLen = sizeof(valueBuf);
        err = nvs_get_str(handle, key, valueBuf, &valueLen);
        if (err != ESP_OK || valueLen == 0) {
            continue;  // No IP saved for this slot
        }

        IPAddress loadedIP;
        if (!loadedIP.fromString(valueBuf)) {
            Serial.printf("[DeviceRegistry] Invalid IP in NVS slot %d: %s\n", i, valueBuf);
            continue;
        }

        _devices[i].ip = loadedIP;
        _devices[i].discoverySource = DeviceInfo::Source::NVS_RESTORED;
        _devices[i].rssi = INT32_MIN;

        // Load friendly name
        makeNvsKey(key, sizeof(key), i, "name");
        valueLen = sizeof(_devices[i].friendlyName);
        err = nvs_get_str(handle, key, _devices[i].friendlyName, &valueLen);
        if (err != ESP_OK) {
            _devices[i].friendlyName[0] = '\0';
        }

        // Load hostname
        makeNvsKey(key, sizeof(key), i, "host");
        valueLen = sizeof(_devices[i].hostname);
        err = nvs_get_str(handle, key, _devices[i].hostname, &valueLen);
        if (err != ESP_OK) {
            _devices[i].hostname[0] = '\0';
        }

        loadedCount++;
        Serial.printf("[DeviceRegistry] Loaded slot %d: IP=%s name=%s host=%s\n",
                      i, valueBuf,
                      _devices[i].friendlyName[0] ? _devices[i].friendlyName : "(none)",
                      _devices[i].hostname[0] ? _devices[i].hostname : "(none)");
    }

    _deviceCount = loadedCount;

    // Validate saved selection against loaded data
    if (_selectedIndex >= 0 && !_devices[_selectedIndex].isValid()) {
        Serial.printf("[DeviceRegistry] Saved selection %d is no longer valid\n",
                      _selectedIndex);
        _selectedIndex = -1;
    }

    releaseLock();
    nvs_close(handle);

    return loadedCount;
}

// ============================================================================
// Internal Helpers
// ============================================================================

bool DeviceRegistry::takeLock() const {
    if (!_mutex) return false;
    return xSemaphoreTake(_mutex, pdMS_TO_TICKS(100)) == pdTRUE;
}

void DeviceRegistry::releaseLock() const {
    if (_mutex) {
        xSemaphoreGive(_mutex);
    }
}

int8_t DeviceRegistry::findEmptySlot() const {
    // Caller must hold lock
    for (uint8_t i = 0; i < MAX_DEVICES; i++) {
        if (!_devices[i].isValid()) {
            return i;
        }
    }
    return -1;
}

void DeviceRegistry::makeNvsKey(char* out, size_t outLen, uint8_t index,
                                const char* suffix) {
    snprintf(out, outLen, "d%d_%s", index, suffix);
}

#endif // ENABLE_WIFI
