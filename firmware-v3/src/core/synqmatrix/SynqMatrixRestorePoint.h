#pragma once

#include <stdint.h>

namespace lightwaveos::synqmatrix {

enum class SynqMatrixRestoreScope : uint8_t {
    SerialCli = 0,
    SerialJson,
    Rest,
    WebSocket,
    Count,
};

void captureSynqMatrixRestorePoint(SynqMatrixRestoreScope scope);
bool hasSynqMatrixRestorePoint(SynqMatrixRestoreScope scope);
bool restoreSynqMatrixRestorePoint(SynqMatrixRestoreScope scope);
void clearSynqMatrixRestorePoint(SynqMatrixRestoreScope scope);
void clearAllSynqMatrixRestorePoints();

} // namespace lightwaveos::synqmatrix
