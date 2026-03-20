#pragma once

/**
 * @file runtime_state.h
 * @brief Extern declarations for shared runtime state defined in main.cpp.
 *
 * Variables declared here are DEFINED in main.cpp. Include this header
 * instead of writing ad-hoc extern declarations in individual .cpp files.
 */

#include <atomic>
#include <cstdint>

// Forward declarations
namespace lightwaveos {
namespace persistence {
class ZoneConfigManager;
}
}

/// Zone configuration manager instance (defined in main.cpp).
extern lightwaveos::persistence::ZoneConfigManager* zoneConfigMgr;

/// Active factory preset index (defined in main.cpp).
extern uint8_t g_factoryPresetIndex;

/// Cross-core NVS save trigger set by REST/WS handlers (defined in main.cpp).
extern std::atomic<bool> g_externalNvsSaveRequest;
