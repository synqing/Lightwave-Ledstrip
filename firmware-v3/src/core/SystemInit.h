/**
 * @file SystemInit.h
 * @brief Init helper functions extracted from setup() in main.cpp.
 *
 * Each function encapsulates a logical phase of the LightwaveOS boot
 * sequence. They are called from setup() in EXACT order — do NOT
 * reorder or call from anywhere else. The ordering dependencies are
 * documented inline.
 *
 * Phase 4 of the main.cpp decomposition (Phases 1-3 extracted
 * CaptureStreamer, SerialCLI, and loop helpers).
 */

#pragma once

#include <cstdint>

// Forward declarations — avoids pulling heavy headers into every TU
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
namespace serial {
class CaptureStreamer;
}
namespace plugins {
class PluginManagerActor;
}
}

namespace lightwaveos::core {

// ─── Boot flag state shared between init phases ─────────────────────────

struct BootFlags {
    bool wdtSafeMode  = false;   ///< WDT recovery boot detected
    bool nvsCorrupted  = false;   ///< NVS checksum failure on load
};

// ─── Init helper functions — called from setup() in EXACT order ─────────
// DO NOT call these from anywhere else or reorder them.
//
// Ordering dependencies (arrows = "must precede"):
//   initSerial → everything (prints)
//   initPSRAMScratch → anything that allocates from PSRAM
//   initOtaAndWiFiReset → WiFi init
//   checkWdtSafeMode → state restore (in setup)
//   initSystemMonitoring → actors start
//   initActorSystem → registerAllEffects, zone composer, plugin manager
//   initNvsAndZones → state restore (in setup)
//   initStatusStripAndButton → actors start
//   startActorsAndPlugins → WiFi AP, state restore (in setup)
//   [state restore remains in setup — depends on applyFactoryPreset()]
//   initWiFiAP → WebServer
//   initWebServer → OTA health validation
//   postBootValidation → help banner

/// Phase 1: Serial HWCDC TX buffer + baud + telemetry heartbeat.
void initSerial();

/// Phase 2: PSRAM scratch buffers + capture streamer init.
void initPSRAMScratch(lightwaveos::serial::CaptureStreamer& captureStreamer);

/// Phase 3: OTA boot verification + WiFi deinit to clean state.
void initOtaAndWiFiReset();

/// Phase 4: NVS WDT flag check — sets BootFlags::wdtSafeMode if WDT reset.
void checkWdtSafeMode(BootFlags& flags);

/// Phase 5: StackMonitor, HeapMonitor, MemoryLeakDetector, ValidationProfiler.
void initSystemMonitoring();

/// Phase 6: ActorSystem::init(), registerAllEffects(), AudioMappingRegistry.
/// Sets renderer and wires captureStreamer.
void initActorSystem(
    lightwaveos::actors::ActorSystem& actors,
    lightwaveos::actors::RendererActor*& renderer,
    lightwaveos::serial::CaptureStreamer& captureStreamer);

/// Phase 7: NVS manager, OTA token, ZoneComposer, ZoneConfigManager.
void initNvsAndZones(
    lightwaveos::actors::RendererActor* renderer,
    lightwaveos::zones::ZoneComposer& zoneComposer,
    lightwaveos::persistence::ZoneConfigManager*& zoneConfigMgr);

/// Phase 8: Status strip + TTP223 capacitive touch button GPIO.
void initStatusStripAndButton();

/// Phase 9a: actors.start() + PluginManager creation + boot memory log.
void startActorsAndPlugins(
    lightwaveos::actors::ActorSystem& actors,
    lightwaveos::actors::RendererActor* renderer,
    lightwaveos::plugins::PluginManagerActor*& pluginManager);

/// Phase 10: WiFiManager AP-only boot + WiFiCredentialManager.
void initWiFiAP();

/// Phase 11: WebServer creation, route registration.
void initWebServer(
    lightwaveos::actors::ActorSystem& actors,
    lightwaveos::actors::RendererActor* renderer,
    lightwaveos::plugins::PluginManagerActor* pluginManager);

/// Phase 12: OTA health validation + boot-time degraded-mode signals.
void postBootValidation(const BootFlags& flags);

/// Phase 13: Print command help banner to Serial.
void printHelpBanner();

}  // namespace lightwaveos::core
