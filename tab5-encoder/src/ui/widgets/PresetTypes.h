// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
#pragma once
#include <stdint.h>

/// Lifecycle states for a single preset slot widget.
/// Defined once here to avoid an ODR violation when both
/// PresetSlotWidget.h and PresetBankWidget.h are included
/// in the same translation unit.
enum class PresetSlotState : uint8_t {
    EMPTY,     ///< No preset saved in this slot
    OCCUPIED,  ///< Preset saved but not currently active
    ACTIVE,    ///< This slot's preset is currently loaded on K1
    SAVING,    ///< Save feedback animation in progress (500 ms)
    DELETING,  ///< Delete feedback animation in progress (500 ms)
};
