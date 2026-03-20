#pragma once

/**
 * @file SerialCLI.h
 * @brief Serial command-line interface for LightwaveOS.
 *
 * Extracted from main.cpp (Phase 3 decomposition).  Owns the serial line
 * buffer, the single-char hotkey dispatch, and the multi-char command parser.
 * Call tick() from loop() to read serial input and dispatch commands.
 *
 * All static locals that were previously in loop() now live as private
 * members of this class, preserving the same storage duration behaviour.
 */

#include <Arduino.h>
#include <FastLED.h>  // CRGB, EffectId

#include "effects/PatternRegistry.h"  // EffectRegister, PatternRegistry

// Forward declarations — avoid pulling heavy headers into every includer.
namespace lightwaveos {
namespace actors {
class ActorSystem;
class RendererActor;
}
namespace zones {
class ZoneComposer;
}
namespace persistence {
class ZoneConfigManager;
}
}

namespace prism {
class DynamicShowStore;
}

namespace lightwaveos {
namespace serial {

class CaptureStreamer;

/**
 * @brief Dependencies required by the Serial CLI.
 *
 * All pointers/references must remain valid for the lifetime of the CLI.
 * Set once via init() before the first tick() call.
 */
struct SerialCLIDeps {
    lightwaveos::actors::ActorSystem*    actors      = nullptr;
    lightwaveos::actors::RendererActor*  renderer    = nullptr;
    lightwaveos::zones::ZoneComposer*    zoneComposer = nullptr;
    lightwaveos::persistence::ZoneConfigManager* zoneConfigMgr = nullptr;
    CaptureStreamer*                     captureStreamer = nullptr;
    ::prism::DynamicShowStore*           showStore   = nullptr;

    // Scratch buffers owned by main.cpp (PSRAM-backed when available).
    EffectId*  effectIdScratch   = nullptr;
    CRGB*      validationScratch = nullptr;
    uint16_t   effectIdScratchCap = 0;
};

class SerialCLI {
public:
    /// Set dependencies.  Must be called before the first tick().
    void init(const SerialCLIDeps& deps);

    /// Call from loop() — reads serial, dispatches commands.
    void tick();

private:
    // ── Dependencies ──
    SerialCLIDeps m_deps{};

    // ── Serial line buffer ──
    String m_cmdBuffer;

    // ── Effect register state (previously static locals in loop()) ──
    EffectId       m_currentEffect        = 0;      // lightwaveos::EID_FIRE
    uint8_t        m_lastAudioEffectIndex  = 0;

    EffectRegister m_currentRegister       = EffectRegister::ALL;
    uint8_t        m_reactiveRegisterIndex = 0;
    uint16_t       m_ambientRegisterIndex  = 0;
    EffectId       m_ambientEffectIds[170] = {};     // 340 bytes, static storage duration
    uint16_t       m_ambientEffectCount    = 0;
    bool           m_registersInitialised  = false;

    // ── Internal dispatch ──
    void initRegisters();
    void processCommand(const String& input, char firstChar);
    void handleMultiCharCommand(const String& input, const String& inputLower, bool& handled);
    void handleSingleCharCommand(char cmd);
};

} // namespace serial
} // namespace lightwaveos
