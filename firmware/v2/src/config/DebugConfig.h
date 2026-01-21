/**
 * @file DebugConfig.h
 * @brief Unified debug configuration system for LightwaveOS v2
 *
 * Provides a single configuration struct for controlling debug verbosity
 * across all domains (audio, render, network, actor, system).
 *
 * Features:
 * - Global verbosity level with per-domain overrides
 * - On-demand status printing (one-shot commands)
 * - Configurable periodic output intervals
 *
 * Levels:
 *   0 = OFF      - No debug output
 *   1 = ERROR    - Actual errors (failures, corruption)
 *   2 = WARN     - Errors + actionable warnings (default)
 *   3 = INFO     - Warn + significant events
 *   4 = DEBUG    - Info + diagnostic values
 *   5 = TRACE    - Everything (per-frame, raw samples)
 *
 * Serial Commands:
 *   dbg                    - Show all debug config
 *   dbg <0-5>              - Set global level
 *   dbg audio <0-5>        - Set audio domain level
 *   dbg render <0-5>       - Set render domain level
 *   dbg network <0-5>      - Set network domain level
 *   dbg actor <0-5>        - Set actor domain level
 *   dbg status             - Print health summary NOW (one-shot)
 *   dbg spectrum           - Print spectrum NOW (one-shot)
 *   dbg interval status <N>    - Auto-print status every N seconds (0=off)
 *   dbg interval spectrum <N>  - Auto-print spectrum every N seconds (0=off)
 *
 * REST API:
 *   GET  /api/v1/debug/config     - Get full debug config
 *   POST /api/v1/debug/config     - Update debug config
 *   POST /api/v1/debug/status     - Trigger one-shot status (returns JSON)
 *
 * @see docs/debugging/SERIAL_DEBUG_REDESIGN_PROPOSAL.md
 */

#pragma once

#include <cstdint>

namespace lightwaveos {
namespace config {

/**
 * @brief Debug domains for per-domain verbosity control
 */
enum class DebugDomain : uint8_t {
    AUDIO = 0,
    RENDER = 1,
    NETWORK = 2,
    ACTOR = 3,
    SYSTEM = 4,
    _COUNT = 5
};

/**
 * @brief Debug levels with clear semantics
 *
 * NOTE: We use VERBOSE instead of DEBUG to avoid macro collision
 * with the DEBUG macro defined in features.h
 */
enum class DebugLevel : uint8_t {
    OFF = 0,      ///< Nothing from this domain
    ERROR = 1,    ///< Actual failures (capture fail, DMA timeout)
    WARN = 2,     ///< Errors + actionable warnings (spike correction, low stack)
    INFO = 3,     ///< Warn + significant events (effect change, connection)
    VERBOSE = 4,  ///< Info + diagnostic values (timing, periodic status)
    TRACE = 5     ///< Everything (per-frame, raw samples, DMA)
};

/**
 * @brief Level name strings for serialization
 */
constexpr const char* DEBUG_LEVEL_NAMES[] = {
    "OFF",
    "ERROR",
    "WARN",
    "INFO",
    "VERBOSE",
    "TRACE"
};

/**
 * @brief Domain name strings for serialization
 */
constexpr const char* DEBUG_DOMAIN_NAMES[] = {
    "audio",
    "render",
    "network",
    "actor",
    "system"
};

/**
 * @brief Unified debug configuration
 *
 * Replaces fragmented debug systems (AudioDebugConfig, ESP_LOG, etc.)
 * with a single configurable struct.
 */
struct DebugConfig {
    /// Global verbosity level (affects all domains unless overridden)
    uint8_t globalLevel = static_cast<uint8_t>(DebugLevel::WARN);

    /// Domain-specific overrides (-1 = use global level)
    int8_t audioLevel = -1;
    int8_t renderLevel = -1;
    int8_t networkLevel = -1;
    int8_t actorLevel = -1;
    int8_t systemLevel = -1;

    /// Periodic output intervals in seconds (0 = disabled)
    uint16_t statusIntervalSec = 0;    ///< Auto-print status every N seconds
    uint16_t spectrumIntervalSec = 0;  ///< Auto-print spectrum every N seconds

    /**
     * @brief Get effective level for a domain
     * @param domain The debug domain
     * @return Effective level (domain override or global)
     */
    uint8_t effectiveLevel(DebugDomain domain) const {
        int8_t domainLevel = -1;
        switch (domain) {
            case DebugDomain::AUDIO:   domainLevel = audioLevel; break;
            case DebugDomain::RENDER:  domainLevel = renderLevel; break;
            case DebugDomain::NETWORK: domainLevel = networkLevel; break;
            case DebugDomain::ACTOR:   domainLevel = actorLevel; break;
            case DebugDomain::SYSTEM:  domainLevel = systemLevel; break;
            default: break;
        }
        return (domainLevel >= 0) ? static_cast<uint8_t>(domainLevel) : globalLevel;
    }

    /**
     * @brief Set domain-specific level
     * @param domain The debug domain
     * @param level Level to set (-1 to use global)
     */
    void setDomainLevel(DebugDomain domain, int8_t level) {
        switch (domain) {
            case DebugDomain::AUDIO:   audioLevel = level; break;
            case DebugDomain::RENDER:  renderLevel = level; break;
            case DebugDomain::NETWORK: networkLevel = level; break;
            case DebugDomain::ACTOR:   actorLevel = level; break;
            case DebugDomain::SYSTEM:  systemLevel = level; break;
            default: break;
        }
    }

    /**
     * @brief Get raw domain level setting
     * @param domain The debug domain
     * @return Raw level (-1 if using global)
     */
    int8_t getDomainLevel(DebugDomain domain) const {
        switch (domain) {
            case DebugDomain::AUDIO:   return audioLevel;
            case DebugDomain::RENDER:  return renderLevel;
            case DebugDomain::NETWORK: return networkLevel;
            case DebugDomain::ACTOR:   return actorLevel;
            case DebugDomain::SYSTEM:  return systemLevel;
            default: return -1;
        }
    }

    /**
     * @brief Check if logging should occur for domain/level
     * @param domain The debug domain
     * @param level The log level
     * @return true if logging is enabled for this domain/level
     */
    bool shouldLog(DebugDomain domain, DebugLevel level) const {
        return effectiveLevel(domain) >= static_cast<uint8_t>(level);
    }

    /**
     * @brief Get domain name as string
     * @param domain The domain enum value
     * @return Human-readable domain name
     */
    static const char* domainName(DebugDomain domain);

    /**
     * @brief Get level name as string
     * @param level The level enum value
     * @return Human-readable level name
     */
    static const char* levelName(DebugLevel level);

    /**
     * @brief Get level name as string (overload for raw uint8_t)
     * @param level The level as uint8_t (0-5)
     * @return Human-readable level name
     */
    static const char* levelName(uint8_t level);
};

/**
 * @brief Get the global debug configuration singleton
 * @return Reference to the singleton config instance
 */
DebugConfig& getDebugConfig();

/**
 * @brief Reset debug configuration to defaults
 */
void resetDebugConfig();

/**
 * @brief Print current debug configuration to serial
 *
 * Outputs the current configuration in a human-readable format:
 * - Global level
 * - Per-domain effective levels
 * - Periodic interval settings
 */
void printDebugConfig();

} // namespace config
} // namespace lightwaveos
