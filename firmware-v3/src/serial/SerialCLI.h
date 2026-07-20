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
    // Phase 2.3: transition queue + staged effect migrated to ActorSystem
    // (ManualStaging). SerialCLI is now a transport layer over the actor
    // staging surface; the legacy m_queued* fields are gone.

    // ── Phase 1C dual-strip Independent mode — keystroke editing target ──
    // Which strip (0 or 1) is targeted when effect-cycle keys (space/n/N/L)
    // fire while RendererMode::Independent is active. Toggled by '|'.
    // Ephemeral: resets to 0 each boot. Serial-only — iOS/Tab5/web do not
    // share this field; future cross-stack independence uses separate state.
    uint8_t        m_activeStripEditing    = 0;

    // Phase 2.3 — Enter debounce. CRLF-sending terminals (PIO monitor on
    // Windows, some Mac configurations) deliver \r and \n in quick
    // succession. A simple ms-window guard prevents double-fire from a
    // single keypress. 50ms is well above CRLF arrival delta (~µs) and
    // well below user typing cadence (~100ms minimum).
    uint32_t       m_lastEnterFireMs       = 0;

    // ── Internal dispatch ──
    void initRegisters();
    void processCommand(const String& input, char firstChar);
    void handleMultiCharCommand(const String& input, const String& inputLower, bool& handled);
    void handleSingleCharCommand(char cmd);

    // Single source of truth for the immediate-hotkey character set.
    // Used both by the in-loop guard and by the end-of-tick lone-hotkey
    // safety net, so the two cannot drift apart.
    static bool isImmediateHotkeyChar(char c);

    // Phase 1C — fork point for effect-cycle keystrokes. In Unified mode
    // routes to actors.setEffect (legacy); in Independent mode routes to
    // renderer->setStripEffectId(m_activeStripEditing, ...).
    void dispatchEffect(EffectId eid);

    // Phase 2.3 — arm/fire status echo helpers. Emit one ARM line per
    // staging-mutating keystroke (a/A/t/T/f/F/Esc) and per ? query.
    // computeArmLeadMs returns 0 when no transition is queued (hard cut
    // semantics), otherwise delegates to ActorSystem::getRealLeadTime.
    void emitArmLine();
    uint16_t computeArmLeadMs() const;
};

} // namespace serial
} // namespace lightwaveos
