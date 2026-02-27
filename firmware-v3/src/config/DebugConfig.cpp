/**
 * @file DebugConfig.cpp
 * @brief Unified debug configuration implementation
 */

#include "DebugConfig.h"

#ifdef ARDUINO
#include <Arduino.h>
#define DBG_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
#include <cstdio>
#define DBG_PRINTF(...) printf(__VA_ARGS__)
#endif

namespace lightwaveos {
namespace config {

namespace {
    /// Singleton instance
    DebugConfig s_debugConfig;
}

// ============================================================================
// Singleton Access
// ============================================================================

DebugConfig& getDebugConfig() {
    return s_debugConfig;
}

void resetDebugConfig() {
    s_debugConfig = DebugConfig();
}

// ============================================================================
// Static Name Methods
// ============================================================================

const char* DebugConfig::domainName(DebugDomain domain) {
    switch (domain) {
        case DebugDomain::AUDIO:   return "AUDIO";
        case DebugDomain::RENDER:  return "RENDER";
        case DebugDomain::NETWORK: return "NETWORK";
        case DebugDomain::ACTOR:   return "ACTOR";
        case DebugDomain::SYSTEM:  return "SYSTEM";
        default:                   return "UNKNOWN";
    }
}

const char* DebugConfig::levelName(DebugLevel level) {
    return levelName(static_cast<uint8_t>(level));
}

const char* DebugConfig::levelName(uint8_t level) {
    switch (level) {
        case 0:  return "OFF";
        case 1:  return "ERROR";
        case 2:  return "WARN";
        case 3:  return "INFO";
        case 4:  return "VERBOSE";
        case 5:  return "TRACE";
        default: return "INVALID";
    }
}

// ============================================================================
// Configuration Printing
// ============================================================================

void printDebugConfig() {
    auto& cfg = getDebugConfig();

    DBG_PRINTF("\n=== Debug Configuration ===\n");
    DBG_PRINTF("Global Level: %u (%s)\n", cfg.globalLevel, DebugConfig::levelName(cfg.globalLevel));
    DBG_PRINTF("\nDomain Levels:\n");

    for (uint8_t i = 0; i < static_cast<uint8_t>(DebugDomain::_COUNT); ++i) {
        DebugDomain domain = static_cast<DebugDomain>(i);
        int8_t rawLevel = cfg.getDomainLevel(domain);
        uint8_t effectiveLevel = cfg.effectiveLevel(domain);

        if (rawLevel >= 0) {
            DBG_PRINTF("  %-8s: %u (%s) [override]\n",
                       DebugConfig::domainName(domain),
                       effectiveLevel,
                       DebugConfig::levelName(effectiveLevel));
        } else {
            DBG_PRINTF("  %-8s: %u (%s) [global]\n",
                       DebugConfig::domainName(domain),
                       effectiveLevel,
                       DebugConfig::levelName(effectiveLevel));
        }
    }

    DBG_PRINTF("\nPeriodic Intervals:\n");
    if (cfg.statusIntervalSec > 0) {
        DBG_PRINTF("  Status:   every %u seconds\n", cfg.statusIntervalSec);
    } else {
        DBG_PRINTF("  Status:   disabled\n");
    }
    if (cfg.spectrumIntervalSec > 0) {
        DBG_PRINTF("  Spectrum: every %u seconds\n", cfg.spectrumIntervalSec);
    } else {
        DBG_PRINTF("  Spectrum: disabled\n");
    }
    DBG_PRINTF("===========================\n\n");
}

} // namespace config
} // namespace lightwaveos
