/**
 * @file TransitionRuntimeConfig.cpp
 * @brief NVS-backed runtime controls for the transition engine.
 *
 * Phase 2.1 narrowed the scope of the `transitions.enabled` flag:
 * it now gates only the manual / show / recording paths. The Director
 * path performs hard cuts unconditionally — see
 * `RendererActor::processSynqMatrixTransition`. The REST GET
 * `/api/v1/transitions/config` response exposes a sibling `_scope`
 * field documenting this for clients.
 */

#include "TransitionRuntimeConfig.h"

#if defined(ESP32)
#include <Preferences.h>
#endif

namespace lightwaveos {
namespace transitions {

namespace {
constexpr const char* kNvsNamespace = "transitions";
constexpr const char* kEnabledKey = "enabled";

#if !defined(ESP32)
bool g_transitionsEnabled = true;
#endif
} // namespace

bool transitionsEnabled() {
#if defined(ESP32)
    Preferences prefs;
    if (!prefs.begin(kNvsNamespace, false)) {
        return true;
    }
    const bool enabled = prefs.getBool(kEnabledKey, true);
    prefs.end();
    return enabled;
#else
    return g_transitionsEnabled;
#endif
}

bool setTransitionsEnabled(bool enabled) {
#if defined(ESP32)
    Preferences prefs;
    if (!prefs.begin(kNvsNamespace, false)) {
        return false;
    }
    const size_t written = prefs.putBool(kEnabledKey, enabled);
    prefs.end();
    return written > 0;
#else
    g_transitionsEnabled = enabled;
    return true;
#endif
}

} // namespace transitions
} // namespace lightwaveos
