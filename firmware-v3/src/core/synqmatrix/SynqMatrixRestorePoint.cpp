#include "SynqMatrixRestorePoint.h"

#include "SynqMatrix.h"

#if defined(ESP32)
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#endif

namespace lightwaveos::synqmatrix {
namespace {

constexpr uint8_t kRestoreScopeCount = static_cast<uint8_t>(SynqMatrixRestoreScope::Count);

SynqMatrixRuntimeState g_restorePoints[kRestoreScopeCount];
bool g_restorePointValid[kRestoreScopeCount] = {false};

#if defined(ESP32)
portMUX_TYPE g_restorePointMux = portMUX_INITIALIZER_UNLOCKED;
#endif

uint8_t scopeIndex(SynqMatrixRestoreScope scope) {
    const uint8_t index = static_cast<uint8_t>(scope);
    return (index < kRestoreScopeCount) ? index : 0;
}

void enterRestorePointCritical() {
#if defined(ESP32)
    portENTER_CRITICAL(&g_restorePointMux);
#endif
}

void exitRestorePointCritical() {
#if defined(ESP32)
    portEXIT_CRITICAL(&g_restorePointMux);
#endif
}

} // namespace

void captureSynqMatrixRestorePoint(SynqMatrixRestoreScope scope) {
    const SynqMatrixRuntimeState state = SynqMatrix::instance().exportRuntimeState();
    const uint8_t index = scopeIndex(scope);
    enterRestorePointCritical();
    g_restorePoints[index] = state;
    g_restorePointValid[index] = true;
    exitRestorePointCritical();
}

bool hasSynqMatrixRestorePoint(SynqMatrixRestoreScope scope) {
    const uint8_t index = scopeIndex(scope);
    enterRestorePointCritical();
    const bool valid = g_restorePointValid[index];
    exitRestorePointCritical();
    return valid;
}

bool restoreSynqMatrixRestorePoint(SynqMatrixRestoreScope scope) {
    const uint8_t index = scopeIndex(scope);
    SynqMatrixRuntimeState state;
    bool valid = false;
    enterRestorePointCritical();
    if (!g_restorePointValid[index]) {
        exitRestorePointCritical();
        return false;
    }
    state = g_restorePoints[index];
    valid = true;
    exitRestorePointCritical();
    if (!valid) {
        return false;
    }
    SynqMatrix::instance().restoreRuntimeState(state);
    return true;
}

void clearSynqMatrixRestorePoint(SynqMatrixRestoreScope scope) {
    const uint8_t index = scopeIndex(scope);
    enterRestorePointCritical();
    g_restorePoints[index] = SynqMatrixRuntimeState{};
    g_restorePointValid[index] = false;
    exitRestorePointCritical();
}

void clearAllSynqMatrixRestorePoints() {
    enterRestorePointCritical();
    for (uint8_t i = 0; i < kRestoreScopeCount; ++i) {
        g_restorePoints[i] = SynqMatrixRuntimeState{};
        g_restorePointValid[i] = false;
    }
    exitRestorePointCritical();
}

} // namespace lightwaveos::synqmatrix
