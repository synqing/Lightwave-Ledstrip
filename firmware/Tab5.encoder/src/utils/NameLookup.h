#pragma once
// ============================================================================
// NameLookup - Global Effect/Palette Name Lookup and ID Translation
// ============================================================================
// Forward declarations for global name lookup and effectId translation
// functions defined in main.cpp. These allow ZoneComposerUI, WebSocketClient,
// and other modules to access effect/palette names and translate between
// position indices and hex-based effectIds.
// ============================================================================

#include <cstdint>

// Name lookup (takes position index, not hex effectId)
extern const char* lookupEffectName(uint8_t index);
extern const char* lookupPaletteName(uint8_t id);

// EffectId translation (hex effectId ↔ position index)
extern uint16_t effectIdFromIndex(uint8_t index);     // returns 0xFFFF if invalid
extern uint8_t indexFromEffectId(uint16_t effectId);   // returns 0xFF if not found
