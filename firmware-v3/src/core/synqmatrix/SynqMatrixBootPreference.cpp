/**
 * @file SynqMatrixBootPreference.cpp
 * @brief NVS-backed boot preference for the SynqMatrix director.
 */

#include "SynqMatrixBootPreference.h"

#include "SynqMatrix.h"
#include "../../utils/Log.h"

#include <cstring>

#if defined(ESP32)
#include <Preferences.h>
#endif

namespace lightwaveos {
namespace synqmatrix {

namespace {

constexpr const char* kNvsNamespace = "lw_sm";
constexpr const char* kBootModeKey  = "boot_mode";

} // namespace

const char* synqMatrixBootModeName(SynqMatrixBootMode mode) {
    switch (mode) {
        case SynqMatrixBootMode::On:  return "on";
        case SynqMatrixBootMode::Off: return "off";
    }
    return "off";
}

SynqMatrixBootMode parseSynqMatrixBootMode(const char* value, bool* ok) {
    if (ok != nullptr) *ok = false;
    if (value == nullptr) return SynqMatrixBootMode::Off;
    if (std::strcmp(value, "on") == 0 || std::strcmp(value, "On") == 0 ||
        std::strcmp(value, "ON") == 0 || std::strcmp(value, "1") == 0 ||
        std::strcmp(value, "true") == 0) {
        if (ok != nullptr) *ok = true;
        return SynqMatrixBootMode::On;
    }
    if (std::strcmp(value, "off") == 0 || std::strcmp(value, "Off") == 0 ||
        std::strcmp(value, "OFF") == 0 || std::strcmp(value, "0") == 0 ||
        std::strcmp(value, "false") == 0) {
        if (ok != nullptr) *ok = true;
        return SynqMatrixBootMode::Off;
    }
    return SynqMatrixBootMode::Off;
}

SynqMatrixBootMode getSynqMatrixBootMode() {
#if defined(ESP32)
    Preferences prefs;
    if (!prefs.begin(kNvsNamespace, /*readOnly=*/true)) {
        return SynqMatrixBootMode::Off;
    }
    const uint8_t value = prefs.getUChar(kBootModeKey, static_cast<uint8_t>(SynqMatrixBootMode::Off));
    prefs.end();
    return (value == static_cast<uint8_t>(SynqMatrixBootMode::On))
               ? SynqMatrixBootMode::On
               : SynqMatrixBootMode::Off;
#else
    return SynqMatrixBootMode::Off;
#endif
}

bool setSynqMatrixBootMode(SynqMatrixBootMode mode) {
#if defined(ESP32)
    Preferences prefs;
    if (!prefs.begin(kNvsNamespace, /*readOnly=*/false)) {
        LW_LOGW("SynqMatrix: boot preference NVS open failed");
        return false;
    }
    const size_t written = prefs.putUChar(kBootModeKey, static_cast<uint8_t>(mode));
    prefs.end();
    if (written == 0) {
        LW_LOGW("SynqMatrix: boot preference NVS write failed");
        return false;
    }
    LW_LOGI("SynqMatrix: boot preference persisted (%s)", synqMatrixBootModeName(mode));
    return true;
#else
    (void)mode;
    return true;
#endif
}

void applySynqMatrixBootPreference() {
    const SynqMatrixBootMode mode = getSynqMatrixBootMode();
    auto& sm = SynqMatrix::instance();
    SynqMatrixConfig cfg = sm.getConfig();

    if (mode == SynqMatrixBootMode::On) {
        cfg.enabled = true;
        cfg.mode = SynqMatrixMode::Director;
        cfg.switchingEnabled = true;
        cfg.constrainedSwitching = true;
        sm.setConfig(cfg);
        // Director owns the boot baseline. User input still preempts this via
        // markManualControl; this only establishes the initial owner.
        sm.setOwner(SynqMatrixOwner::Director);
        LW_LOGI("SynqMatrix: boot preference applied (on) — Director engaged");
    } else {
        cfg.enabled = false;
        cfg.mode = SynqMatrixMode::Off;
        cfg.switchingEnabled = false;
        sm.setConfig(cfg);
        sm.setOwner(SynqMatrixOwner::None);
        LW_LOGI("SynqMatrix: boot preference applied (off) — Director dormant");
    }
}

void engageSynqMatrixDirector() {
    auto& sm = SynqMatrix::instance();
    SynqMatrixConfig cfg = sm.getConfig();
    cfg.enabled = true;
    cfg.mode = SynqMatrixMode::Director;
    cfg.switchingEnabled = true;
    cfg.constrainedSwitching = true;
    sm.setConfig(cfg);
    sm.setOwner(SynqMatrixOwner::Director);
    LW_LOGI("SynqMatrix: Director engaged via user request");
}

void releaseSynqMatrixDirector() {
    auto& sm = SynqMatrix::instance();
    SynqMatrixConfig cfg = sm.getConfig();
    cfg.switchingEnabled = false;
    // Leave mode/enabled untouched so the Director ticks still run for telemetry,
    // but it stops claiming ownership on its own.
    sm.setConfig(cfg);
    sm.setOwner(SynqMatrixOwner::None);
    LW_LOGI("SynqMatrix: Director released via user request");
}

} // namespace synqmatrix
} // namespace lightwaveos
