#pragma once
// ============================================================================
// DeviceRegistry - Tab5.encoder Multi-Device Support
// ============================================================================
// Manages discovery, fingerprinting, and selection of LightwaveOS devices
// on the local network. Supports up to 8 devices with NVS persistence.
//
// Features:
//   - Deduplication by IP address
//   - Background FreeRTOS fingerprint probes (GET /api/v1/device/info)
//   - NVS persistence of device list and selection
//   - Mutex-protected for thread safety
//   - Selection changed callback for UI updates
//
// Discovery Sources:
//   MANUAL       - User-entered IP address
//   MDNS         - mDNS service discovery
//   GATEWAY      - WiFi gateway IP (LightwaveOS AP mode)
//   NETWORK_SCAN - Subnet scan results
//   NVS_RESTORED - Loaded from NVS on boot
//
// NVS Layout:
//   Namespace: "tab5dev"
//   Keys: "dev_count" (u8), "dev_sel" (i8),
//         "d0_ip".."d7_ip" (str), "d0_name".."d7_name" (str),
//         "d0_host".."d7_host" (str)
//
// Usage:
//   DeviceRegistry registry;
//   registry.begin();                     // Load from NVS
//   registry.addDiscoveredDevice(ip, DeviceInfo::Source::MDNS);
//   registry.fingerprintDevice(0);        // Background probe
//   registry.selectDevice(0);             // Select for control
//   registry.onSelectionChanged(myCallback);
// ============================================================================

#include "config/Config.h"

#if ENABLE_WIFI

#include <WiFi.h>
#include <WiFiClient.h>
#include <IPAddress.h>
#include <cstdint>
#include <cstring>
#include <climits>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include "config/network_config.h"

/**
 * Information about a discovered LightwaveOS device (~128 bytes)
 */
struct DeviceInfo {
    IPAddress ip;
    char hostname[32];          // mDNS hostname (e.g., "lightwaveos")
    char friendlyName[32];      // User-assigned label
    char firmwareVersion[16];   // From /api/v1/device/info response
    uint8_t effectCount;        // Number of effects on this device
    int32_t rssi;               // Last known WiFi RSSI (INT32_MIN = unknown)
    uint32_t lastSeenMs;        // millis() of last successful contact
    uint32_t lastFingerprinted; // millis() of last fingerprint probe
    bool verified;              // Confirmed as LightwaveOS device
    bool reachable;             // Last probe succeeded

    enum class Source : uint8_t {
        MANUAL = 0,
        MDNS = 1,
        GATEWAY = 2,
        NETWORK_SCAN = 3,
        NVS_RESTORED = 4
    };
    Source discoverySource;

    /**
     * Zero-initialize all fields
     */
    void clear() {
        ip = IPAddress(0, 0, 0, 0);
        memset(hostname, 0, sizeof(hostname));
        memset(friendlyName, 0, sizeof(friendlyName));
        memset(firmwareVersion, 0, sizeof(firmwareVersion));
        effectCount = 0;
        rssi = INT32_MIN;
        lastSeenMs = 0;
        lastFingerprinted = 0;
        verified = false;
        reachable = false;
        discoverySource = Source::MANUAL;
    }

    /**
     * Check if this slot contains a valid device entry
     * @return true if IP is non-zero
     */
    bool isValid() const {
        return ip != IPAddress(0, 0, 0, 0);
    }

    /**
     * Get the best display name for this device
     * @return friendlyName if set, otherwise IP address string
     */
    const char* displayName() const {
        if (friendlyName[0] != '\0') {
            return friendlyName;
        }
        // Use a static buffer for IP string conversion
        static char ipBuf[16];
        snprintf(ipBuf, sizeof(ipBuf), "%d.%d.%d.%d",
                 ip[0], ip[1], ip[2], ip[3]);
        return ipBuf;
    }
};

/**
 * Registry of discovered LightwaveOS devices with NVS persistence
 */
class DeviceRegistry {
public:
    static constexpr uint8_t MAX_DEVICES = 8;

    using SelectionChangedCallback = void(*)(int8_t newIndex, const DeviceInfo* device);

    DeviceRegistry();
    ~DeviceRegistry();

    // ========================================================================
    // Initialization
    // ========================================================================

    /**
     * Initialize the registry and load saved devices from NVS.
     * @return true if NVS loaded successfully (or no saved data)
     */
    bool begin();

    // ========================================================================
    // Device Management
    // ========================================================================

    /**
     * Add a device discovered via network scan, mDNS, or gateway.
     * Deduplicates by IP address - updates existing entry if found.
     * @param ip Device IP address
     * @param src How the device was discovered
     * @param hostname Optional mDNS hostname
     * @return Index of added/updated device, or -1 if registry full
     */
    int8_t addDiscoveredDevice(IPAddress ip, DeviceInfo::Source src,
                               const char* hostname = nullptr);

    /**
     * Add a device manually by IP address.
     * @param ip Device IP address
     * @param friendlyName Optional user label
     * @return Index of added device, or -1 if registry full
     */
    int8_t addManualDevice(IPAddress ip, const char* friendlyName = nullptr);

    /**
     * Remove a device from the registry.
     * If the removed device was selected, selection is cleared.
     * @param index Device index (0 to MAX_DEVICES-1)
     * @return true if device was removed
     */
    bool removeDevice(uint8_t index);

    /**
     * Remove all devices from the registry and clear NVS.
     */
    void clearAll();

    // ========================================================================
    // Fingerprinting (Background Probes)
    // ========================================================================

    /**
     * Start a background FreeRTOS task to fingerprint a device.
     * Probes GET /api/v1/device/info and checks for LightwaveOS signature.
     * @param index Device index to fingerprint
     * @return true if fingerprint task was started
     */
    bool fingerprintDevice(uint8_t index);

    /**
     * Fingerprint all valid devices sequentially (background task).
     * @return true if fingerprint task was started
     */
    bool fingerprintAll();

    /**
     * Check if a fingerprint task is currently running.
     * @return true if fingerprinting is in progress
     */
    bool isFingerprintRunning() const;

    // ========================================================================
    // Selection
    // ========================================================================

    /**
     * Select a device as the active control target.
     * @param index Device index (0 to MAX_DEVICES-1), or -1 to deselect
     * @return true if selection changed
     */
    bool selectDevice(int8_t index);

    /**
     * Get the currently selected device index.
     * @return Selected index (0 to MAX_DEVICES-1), or -1 if none selected
     */
    int8_t getSelectedIndex() const;

    /**
     * Get the IP address of the selected device.
     * @return Selected device IP, or INADDR_NONE if none selected
     */
    IPAddress getSelectedIP() const;

    /**
     * Get the selected device info.
     * @return Pointer to selected DeviceInfo, or nullptr if none selected
     */
    const DeviceInfo* getSelectedDevice() const;

    /**
     * Auto-select the best available device.
     * Priority: NVS saved selection -> strongest verified RSSI ->
     *           first verified -> first reachable -> first valid
     * @return true if a device was selected
     */
    bool autoSelect();

    // ========================================================================
    // Query
    // ========================================================================

    /**
     * Get the number of valid devices in the registry.
     * @return Device count (0 to MAX_DEVICES)
     */
    uint8_t getDeviceCount() const;

    /**
     * Get device info by index.
     * @param index Device index (0 to MAX_DEVICES-1)
     * @return Pointer to DeviceInfo, or nullptr if index invalid/empty
     */
    const DeviceInfo* getDevice(uint8_t index) const;

    /**
     * Find a device by IP address.
     * @param ip IP address to search for
     * @return Device index, or -1 if not found
     */
    int8_t findByIP(IPAddress ip) const;

    /**
     * Set a user-friendly name for a device.
     * @param index Device index
     * @param name Friendly name (max 31 chars)
     * @return true if name was set
     */
    bool setFriendlyName(uint8_t index, const char* name);

    /**
     * Update reachability status for a device.
     * @param index Device index
     * @param reachable Whether the device responded
     * @param rssi Signal strength (INT32_MIN to skip RSSI update)
     */
    void updateReachability(uint8_t index, bool reachable,
                            int32_t rssi = INT32_MIN);

    // ========================================================================
    // Callbacks
    // ========================================================================

    /**
     * Register a callback for selection changes.
     * @param cb Callback function (called with new index and device pointer)
     */
    void onSelectionChanged(SelectionChangedCallback cb);

    // ========================================================================
    // NVS Persistence
    // ========================================================================

    /**
     * Save all devices and selection to NVS.
     * @return true if save succeeded
     */
    bool saveToNVS();

    /**
     * Load devices from NVS.
     * @return Number of devices loaded
     */
    uint8_t loadFromNVS();

private:
    DeviceInfo _devices[MAX_DEVICES];
    uint8_t _deviceCount;
    int8_t _selectedIndex;

    mutable SemaphoreHandle_t _mutex;
    TaskHandle_t _fingerprintTaskHandle;
    volatile bool _fingerprintCancelRequested;

    SelectionChangedCallback _selectionChangedCb;

    // ========================================================================
    // Internal Helpers
    // ========================================================================

    bool takeLock() const;
    void releaseLock() const;

    void notifySelectionChanged();

    int8_t findEmptySlot() const;

    static void makeNvsKey(char* out, size_t outLen, uint8_t index,
                           const char* suffix);

    // Fingerprint task context
    struct FingerprintContext {
        DeviceRegistry* registry;
        int8_t deviceIndex;     // -1 means fingerprint all
    };

    static void fingerprintTask(void* parameter);
    bool runFingerprintSingle(uint8_t index);
};

#else // ENABLE_WIFI == 0

// ============================================================================
// Stub implementation when WiFi is disabled
// ============================================================================

struct DeviceInfo {
    IPAddress ip;
    char hostname[32];
    char friendlyName[32];
    char firmwareVersion[16];
    uint8_t effectCount;
    int32_t rssi;
    uint32_t lastSeenMs;
    uint32_t lastFingerprinted;
    bool verified;
    bool reachable;
    enum class Source : uint8_t {
        MANUAL = 0, MDNS = 1, GATEWAY = 2, NETWORK_SCAN = 3, NVS_RESTORED = 4
    };
    Source discoverySource;
    void clear() {}
    bool isValid() const { return false; }
    const char* displayName() const { return "N/A"; }
};

class DeviceRegistry {
public:
    static constexpr uint8_t MAX_DEVICES = 8;
    using SelectionChangedCallback = void(*)(int8_t, const DeviceInfo*);

    DeviceRegistry() {}
    ~DeviceRegistry() {}
    bool begin() { return false; }
    int8_t addDiscoveredDevice(IPAddress, DeviceInfo::Source, const char* = nullptr) { return -1; }
    int8_t addManualDevice(IPAddress, const char* = nullptr) { return -1; }
    bool removeDevice(uint8_t) { return false; }
    void clearAll() {}
    bool fingerprintDevice(uint8_t) { return false; }
    bool fingerprintAll() { return false; }
    bool isFingerprintRunning() const { return false; }
    bool selectDevice(int8_t) { return false; }
    int8_t getSelectedIndex() const { return -1; }
    IPAddress getSelectedIP() const { return IPAddress(INADDR_NONE); }
    const DeviceInfo* getSelectedDevice() const { return nullptr; }
    bool autoSelect() { return false; }
    uint8_t getDeviceCount() const { return 0; }
    const DeviceInfo* getDevice(uint8_t) const { return nullptr; }
    int8_t findByIP(IPAddress) const { return -1; }
    bool setFriendlyName(uint8_t, const char*) { return false; }
    void updateReachability(uint8_t, bool, int32_t = INT32_MIN) {}
    void onSelectionChanged(SelectionChangedCallback) {}
    bool saveToNVS() { return false; }
    uint8_t loadFromNVS() { return 0; }
};

#endif // ENABLE_WIFI
