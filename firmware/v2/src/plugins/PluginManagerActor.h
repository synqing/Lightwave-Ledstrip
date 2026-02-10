/**
 * @file PluginManagerActor.h
 * @brief Plugin manager for dynamic effect registration from LittleFS manifests
 *
 * Manages IEffect registration with support for:
 * - Loading plugin manifests from LittleFS at startup
 * - Atomic reload of manifests at runtime
 * - Additive and override registration modes
 * - Validation with detailed error reporting
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#pragma once

#include "../config/features.h"
#include "../config/limits.h"
#include "api/IEffectRegistry.h"
#include "api/IEffect.h"
#include <stdint.h>
#include <cstddef>

namespace lightwaveos {
namespace actors {
    class RendererActor;
}
}

namespace lightwaveos {
namespace plugins {

/**
 * @brief Plugin configuration constants
 */
struct PluginConfig {
    static constexpr uint8_t MAX_EFFECTS = 155;
    static_assert(MAX_EFFECTS >= limits::MAX_EFFECTS,
                  "PluginConfig::MAX_EFFECTS must be >= limits::MAX_EFFECTS");
    static constexpr uint8_t MAX_MANIFESTS = 16;
    static constexpr size_t LITTLEFS_PLUGIN_PATH_MAX = 64;
    static constexpr size_t MANIFEST_CAPACITY = 2048;
    static constexpr size_t ERROR_MSG_MAX = 128;
    static constexpr size_t PLUGIN_NAME_MAX = 64;
};

/**
 * @brief Parsed manifest information
 */
struct ParsedManifest {
    char filePath[PluginConfig::LITTLEFS_PLUGIN_PATH_MAX];
    char pluginName[PluginConfig::PLUGIN_NAME_MAX];
    bool valid;
    char errorMsg[PluginConfig::ERROR_MSG_MAX];
    bool overrideMode;
    uint8_t effectIds[PluginConfig::MAX_EFFECTS];
    uint8_t effectCount;
};

/**
 * @brief Plugin system statistics
 */
struct PluginStats {
    uint8_t registeredCount;      // Currently registered effects
    uint8_t loadedFromLittleFS;   // Effects loaded from manifests
    uint8_t registrationsFailed;  // Failed registration attempts
    uint8_t unregistrations;      // Total unregistration count
    bool overrideModeEnabled;     // Whether override mode is active
    uint8_t disabledByOverride;   // Effects disabled by override mode

    // Reload status (Phase 2)
    uint32_t lastReloadMillis;    // Timestamp of last reload attempt
    bool lastReloadOk;            // Whether last reload succeeded
    uint8_t manifestCount;        // Number of manifest files found
    uint8_t errorCount;           // Number of manifests with errors
    char lastErrorSummary[PluginConfig::ERROR_MSG_MAX]; // Summary of last error
};

/**
 * @brief Plugin Manager Actor
 *
 * Central registry manager that:
 * - Maintains up to 140 registered IEffect instances
 * - Loads plugins from LittleFS on startup
 * - Supports atomic reload at runtime
 * - Coordinates effect registration with RendererActor
 */
class PluginManagerActor : public IEffectRegistry {
public:
    PluginManagerActor();
    ~PluginManagerActor() = default;

    /**
     * @brief Set the target registry (usually RendererActor)
     * @param target Registry to forward registrations to
     */
    void setTargetRegistry(IEffectRegistry* target) { m_targetRegistry = target; }

    /**
     * @brief Called when actor starts
     */
    void onStart();

    // ========================================================================
    // IEffectRegistry Implementation
    // ========================================================================

    bool registerEffect(uint8_t id, IEffect* effect) override;
    bool unregisterEffect(uint8_t id) override;
    bool isEffectRegistered(uint8_t id) const override;
    uint8_t getRegisteredCount() const override;

    // ========================================================================
    // Plugin Loading
    // ========================================================================

    /**
     * @brief Load plugins from LittleFS (called at startup)
     *
     * Scans LittleFS for *.plugin.json files and applies them.
     * This is the initial load - uses loadPluginsFromLittleFS internally.
     */
    void loadPluginsFromLittleFS();

    /**
     * @brief Reload plugins from LittleFS (atomic, safe)
     *
     * Phase 2 feature: Atomically reloads all manifests.
     * - Scans and validates all manifests first
     * - Only applies if ALL manifests are valid
     * - Preserves previous state on any error
     *
     * @return true if reload succeeded, false if any manifest invalid
     */
    bool reloadFromLittleFS();

    // ========================================================================
    // Statistics and Diagnostics
    // ========================================================================

    /**
     * @brief Get plugin statistics
     * @return Reference to stats struct
     */
    const PluginStats& getStats() const { return m_stats; }

    /**
     * @brief Get parsed manifest information
     * @param index Manifest index (0 to manifestCount-1)
     * @return Pointer to ParsedManifest or nullptr if out of range
     */
    const ParsedManifest* getManifest(uint8_t index) const;

    /**
     * @brief Get all parsed manifests
     * @return Pointer to manifest array
     */
    const ParsedManifest* getManifests() const { return m_manifests; }

    /**
     * @brief Get number of parsed manifests
     * @return Manifest count
     */
    uint8_t getManifestCount() const { return m_manifestCount; }

private:
    // ========================================================================
    // Internal Methods
    // ========================================================================

    /**
     * @brief Scan LittleFS for manifest files
     * @return Number of manifest files found
     */
    uint8_t scanManifestFiles();

    /**
     * @brief Parse a single manifest file
     * @param path Path to manifest file
     * @param manifest Output parsed manifest
     * @return true if parse succeeded
     */
    bool parseManifest(const char* path, ParsedManifest& manifest);

    /**
     * @brief Validate a parsed manifest
     * @param manifest Manifest to validate
     * @return true if valid
     */
    bool validateManifest(ParsedManifest& manifest);

    /**
     * @brief Apply manifests to effect registry
     * @return true if application succeeded
     */
    bool applyManifests();

    /**
     * @brief Clear all registrations (for reload)
     */
    void clearRegistrations();

    // ========================================================================
    // State
    // ========================================================================

    IEffectRegistry* m_targetRegistry;
    IEffect* m_effects[PluginConfig::MAX_EFFECTS];
    PluginStats m_stats;
    bool m_overrideMode;

    // Manifest storage
    ParsedManifest m_manifests[PluginConfig::MAX_MANIFESTS];
    uint8_t m_manifestCount;

    // Allowed effect IDs (for override mode)
    bool m_allowedEffects[PluginConfig::MAX_EFFECTS];
};

} // namespace plugins
} // namespace lightwaveos
