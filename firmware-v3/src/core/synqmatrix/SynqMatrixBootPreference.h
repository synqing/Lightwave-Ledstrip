/**
 * @file SynqMatrixBootPreference.h
 * @brief NVS-backed boot preference for the SynqMatrix director.
 *
 * Captain controls whether the Director engages automatically on boot. Default
 * is `Off` — Director is dormant until the user explicitly engages it via REST,
 * WebSocket, or serial. When set to `On`, the Director boots in Director mode
 * with owner=Director, ready to dispatch autonomous switches once the audio
 * pipeline reports usable confidence.
 *
 * The preference is persisted under NVS namespace `lw_sm` (Lightwave SynqMatrix)
 * key `boot_mode`. Single byte payload: 0 = Off, 1 = On. Any other value reads
 * as Off (conservative default). The preference is read once at boot via
 * `applySynqMatrixBootPreference()` and on every write via `setSynqMatrixBootPreference()`.
 *
 * Authority structure (user > show > director) still applies after boot: setting
 * the preference to `On` does not pin the owner to Director — the first
 * markManualControl/markShowControl call rotates ownership normally.
 */

#pragma once

#include <cstdint>

namespace lightwaveos {
namespace synqmatrix {

enum class SynqMatrixBootMode : uint8_t {
    Off = 0,  // Director boots dormant (default). User must engage explicitly.
    On = 1,   // Director boots active (mode=Director, owner=Director).
};

// Persisted read. Returns Off if the key is absent, NVS read fails, or the
// stored value is unrecognised — never throws and never blocks the boot path.
SynqMatrixBootMode getSynqMatrixBootMode();

// Persisted write. Returns true on NVS success, false on failure. Safe to call
// from Core 0 (NVS internal mutex). Writes are commit-flushed before returning.
bool setSynqMatrixBootMode(SynqMatrixBootMode mode);

// Apply the persisted preference to the live SynqMatrix singleton. Called once
// during ShowDirectorActor::onStart(). Idempotent: subsequent calls re-read the
// preference and re-apply it.
void applySynqMatrixBootPreference();

// Engage Director on demand (REST/WS/serial). Sets mode=Director, owner=Director,
// switchingEnabled=true. Does NOT persist (use setSynqMatrixBootMode for that).
void engageSynqMatrixDirector();

// Release Director on demand (REST/WS/serial). Sets owner=None and disables
// autonomous switching. The Director continues to tick (so audio classification
// keeps running) but defers all action plans until re-engaged.
void releaseSynqMatrixDirector();

// Human-readable name for logging/JSON.
const char* synqMatrixBootModeName(SynqMatrixBootMode mode);
SynqMatrixBootMode parseSynqMatrixBootMode(const char* value, bool* ok = nullptr);

} // namespace synqmatrix
} // namespace lightwaveos
