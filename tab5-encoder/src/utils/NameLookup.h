// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
#pragma once
// ============================================================================
// NameLookup - Global Effect/Palette Name Lookup Functions
// ============================================================================
// Forward declarations for global name lookup functions defined in main.cpp
// These allow ZoneComposerUI and other modules to access effect/palette names
// fetched from the LightwaveOS server via WebSocket.
// ============================================================================

#include <cstdint>

// Forward declarations for global name lookup functions
// effectId is uint16_t — K1 firmware uses 16-bit hex IDs (e.g. 0x1100)
extern const char* lookupEffectName(uint16_t id);
extern const char* lookupPaletteName(uint8_t id);

