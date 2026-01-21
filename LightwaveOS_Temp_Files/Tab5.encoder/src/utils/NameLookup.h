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
extern const char* lookupEffectName(uint8_t id);
extern const char* lookupPaletteName(uint8_t id);

