/**
 * @file SerialCLI.cpp
 * @brief Serial command-line interface for LightwaveOS.
 *
 * Extracted from main.cpp (Phase 3 decomposition).  Contains all serial
 * command handling: line buffering, single-char hotkeys, and multi-char
 * command dispatch.
 *
 * This code is byte-identical in runtime behaviour to the original
 * main.cpp serial handling.  No functional changes were made during
 * extraction.
 */

#include "SerialCLI.h"
#include "SerialJsonGateway.h"
#include "CaptureStreamer.h"

#include "config/features.h"
#include "config/Trace.h"
#include "config/display_order.h"
#include "config/DebugConfig.h"

#include "core/actors/ActorSystem.h"
#include "core/actors/RendererActor.h"
#include "core/narrative/NarrativeEngine.h"
#include "core/persistence/ZoneConfigManager.h"
#include "core/shows/DynamicShowStore.h"

#include "effects/enhancement/EdgeMixer.h"
#include "effects/zones/ZoneComposer.h"
#include "effects/zones/BlendMode.h"
#include "effects/PatternRegistry.h"
#include "effects/ieffect/BeatPulseBloomEffect.h"  // g_bloomDebugEnabled
#include "effects/ieffect/BloomParityEffect.h"     // Runtime PostFX tuning
#include "effects/ieffect/LGPFilmPost.h"           // Cinema post toggle

#include "plugins/api/IEffect.h"

#include "utils/Log.h"

#if FEATURE_TRANSITIONS
#include "effects/transitions/TransitionEngine.h"
#include "effects/transitions/TransitionTypes.h"
#endif

#if FEATURE_AUDIO_SYNC
#include "audio/AudioDebugConfig.h"
#include "audio/contracts/AudioEffectMapping.h"
#endif

#if FEATURE_STACK_PROFILING
#include "core/system/StackMonitor.h"
#endif

#if !defined(NATIVE_BUILD) && FEATURE_STATUS_STRIP_TOUCH
#include "hal/esp32s3/StatusStripTouch.h"
#endif

#if FEATURE_VALIDATION_PROFILING
#include "core/system/ValidationProfiler.h"
#endif

#if FEATURE_WEB_SERVER
#ifndef NATIVE_BUILD
#include <esp_wifi.h>
#endif
#include "network/WiFiManager.h"
#include "network/WiFiCredentialManager.h"
#include "network/WebServer.h"
#endif

#ifndef NATIVE_BUILD
#include <esp_heap_caps.h>
#endif

using namespace lightwaveos::actors;
using namespace lightwaveos::effects;
using namespace lightwaveos::zones;
using namespace lightwaveos::plugins;
#if FEATURE_TRANSITIONS
using namespace lightwaveos::transitions;
#endif
using namespace lightwaveos::narrative;

namespace lightwaveos {
namespace serial {

// ============================================================================
// Lifecycle
// ============================================================================

void SerialCLI::init(const SerialCLIDeps& deps) {
    m_deps = deps;
    m_currentEffect = lightwaveos::EID_FIRE;
}

// ============================================================================
// tick() — serial read + dispatch (called from loop())
// ============================================================================

void SerialCLI::tick() {
    // One-time initialisation of effect registers
    initRegisters();

    // Handle serial commands with proper line buffering.
    // This fixes the issue where typing "adbg 2" character-by-character
    // would trigger effect selection for '2' instead of the adbg command.
    while (Serial.available()) {
        char c = Serial.read();

        // Immediate single-char commands (no Enter needed, no buffering).
        // These work even mid-buffer — they are "hotkeys".
        if (m_cmdBuffer.length() == 0) {
            bool isImmediate = false;
            switch (c) {
                case ' ':   // Next effect
                case '+': case '=':  // Brightness up
                case '-': case '_':  // Brightness down
                case '[': case ']':  // Speed
                case ',': case '.':  // Palette
                case 'e':            // EdgeMixer mode cycle
                case 'w': case 'W':  // EdgeMixer spread +/-
                case '<': case '>':  // EdgeMixer strength -/+
                case 'y':            // EdgeMixer spatial toggle
                case 'Y':            // EdgeMixer temporal toggle
                case '}':            // EdgeMixer save to NVS
                case 'a':            // Audio debug toggle
                case 'p': case 'P':  // Bloom prism opacity
                case 'o': case 'O':  // Bloom bulb opacity
                case 'i': case 'I':  // Mood
                case 'f': case 'F':  // Bloom alpha (persistence)
                case 'h': case 'H':  // Bloom square iter (contrast)
                case 'j': case 'J':  // Bloom prism iterations
                case 'k': case 'K':  // Bloom gHue speed (palette sweep)
                case 'u': case 'U':  // Bloom spatial spread
                case 'v': case 'V':  // Bloom intensity coupling
                case 'b': case 'B':  // RD Triangle K +/-
                case 't': case 'T':  // RD Triangle F +/-
                case 'x': case 'X':  // Bands observability (one-shot dump)
                case 'q':            // Cinema post-processing toggle
                case '`':            // Status strip idle mode cycle
                    isImmediate = true;
                    break;
            }
            if (isImmediate) {
                // Check if more chars are pending — if so, this might be
                // the start of a multi-char command (e.g. 'a' in "adbg 5")
                if (Serial.available() > 0) {
                    // Buffer it instead of processing immediately
                    m_cmdBuffer += c;
                } else {
                    // Process immediately without buffering
                    m_cmdBuffer = String(c);
                    break; // Exit while loop to process
                }
                continue;
            }
        }

        if (c == '\n' || c == '\r') {
            // End of line — process buffered command
            break;
        } else if (c == 0x7F || c == 0x08) {
            // Backspace — remove last char
            if (m_cmdBuffer.length() > 0) {
                m_cmdBuffer.remove(m_cmdBuffer.length() - 1);
            }
        } else if (c >= 32 && c < 127) {
            // Printable ASCII — add to buffer
            m_cmdBuffer += c;
        }
    }

    // Process buffered command if we have one
    if (m_cmdBuffer.length() > 0) {
        String input = m_cmdBuffer;
        char firstChar = input[0]; // Save before trim (for space, etc.)
        m_cmdBuffer = ""; // Clear for next command
        processCommand(input, firstChar);
    }
}

// ============================================================================
// Register initialisation
// ============================================================================

void SerialCLI::initRegisters() {
    if (m_registersInitialised || !m_deps.renderer) return;

    uint16_t effectCount = m_deps.renderer->getEffectCount();
    const uint16_t cappedCount =
        (effectCount <= m_deps.effectIdScratchCap) ? effectCount : m_deps.effectIdScratchCap;

    // Build array of all registered EffectIds for ambient filtering
    for (uint16_t i = 0; i < cappedCount; i++) {
        m_deps.effectIdScratch[i] = m_deps.renderer->getEffectIdAt(i);
    }
    m_ambientEffectCount = PatternRegistry::buildAmbientEffectArray(
        m_ambientEffectIds, m_deps.effectIdScratchCap, m_deps.effectIdScratch, cappedCount);
    m_registersInitialised = true;
    LW_LOGI("Effect registers: %d reactive, %d ambient, %d total",
            PatternRegistry::getReactiveEffectCount(), m_ambientEffectCount, effectCount);
}

// ============================================================================
// Command dispatch (top-level)
// ============================================================================

void SerialCLI::processCommand(const String& rawInput, char firstChar) {
    String input = rawInput;
    input.trim();

    // Intercept VAL: commands -> forward to RendererActor validation mode
    if (input.startsWith("VAL:")) {
        auto* ren = m_deps.actors->getRenderer();
        if (ren) {
            ren->enqueueValidationCommand(input.c_str());
        }
        return;
    }

    // Use firstChar for single immediate commands, trimmed input for multi-char
    if (input.length() == 0 && firstChar != ' ' && firstChar != '+' && firstChar != '-' &&
        firstChar != '=' && firstChar != '[' && firstChar != ']' &&
        firstChar != ',' && firstChar != '.' && firstChar != '<' && firstChar != '>' &&
        firstChar != '}' && firstChar != 'x' && firstChar != 'X') {
        // Empty after trim and not an immediate command — ignore
        return;
    }

    // Restore single-char immediate commands that got trimmed
    if (input.length() == 0) {
        input = String(firstChar);
    }

    String inputLower = input;
    inputLower.toLowerCase();
    bool handledMulti = false;

    handleMultiCharCommand(input, inputLower, handledMulti);

    if (!handledMulti && input.length() == 1) {
        handleSingleCharCommand(input[0]);
    }
}

// ============================================================================
// Multi-char command handler
// ============================================================================

void SerialCLI::handleMultiCharCommand(const String& input, const String& inputLower,
                                        bool& handledMulti) {
    int peekChar = input[0];
    auto* renderer = m_deps.renderer;
    auto& actors = *m_deps.actors;
    auto& zoneComposer = *m_deps.zoneComposer;

    // Serial JSON command gateway (for PRISM Studio Web Serial).
    // Any line beginning with '{' is routed to the JSON handler and
    // does not fall through to the text command matching below.
    if (input.length() > 0 && input.charAt(0) == '{') {
        SerialJsonGatewayDeps jsonDeps{
            actors, renderer, zoneComposer, *m_deps.showStore};
        processSerialJsonCommand(input, jsonDeps);
        handledMulti = true;
    }

#if FEATURE_AUDIO_SYNC
    // Observability: single key "x" (or "bands") — must be top-level so
    // "x" and "bands" are not gated by peekChar == 'a'
    if (inputLower == "x" || inputLower == "bands") {
        handledMulti = true;
        RendererActor* ren = actors.getRenderer();
        if (ren) {
            RendererActor::BandsDebugSnapshot snap;
            ren->getBandsDebugSnapshot(snap);
            if (snap.valid) {
                Serial.println("\n=== Bands (renderer snapshot) ===");
                Serial.printf("  bands[0..7]: %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f\n",
                    snap.bands[0], snap.bands[1], snap.bands[2], snap.bands[3],
                    snap.bands[4], snap.bands[5], snap.bands[6], snap.bands[7]);
                Serial.printf("  bass=%.3f mid=%.3f treble=%.3f (ctx.audio accessors)\n",
                    snap.bass, snap.mid, snap.treble);
                Serial.printf("  rms=%.3f flux=%.3f hop_seq=%lu\n",
                    snap.rms, snap.flux, (unsigned long)snap.hop_seq);
            } else {
                Serial.println("Bands: no live snapshot (audio buffer or frame not ready)");
            }
        } else {
            Serial.println("Renderer not available");
        }
    }
    else
#endif
#if FEATURE_VRMS_METRICS
    if (inputLower == "vrms") {
        handledMulti = true;
        RendererActor* ren = actors.getRenderer();
        if (ren) {
            metrics::VRMSVector v = ren->getVrmsVector();
            Serial.println("\n=== VRMS Metrics ===");
            Serial.printf("  Dominant Hue:      %.1f\n", v.dominantHue);
            Serial.printf("  Colour Variance:   %.3f\n", v.colourVariance);
            Serial.printf("  Spatial Centroid:   %.1f\n", v.spatialCentroid);
            Serial.printf("  Symmetry Score:    %.3f\n", v.symmetryScore);
            Serial.printf("  Brightness Mean:   %.1f\n", v.brightnessMean);
            Serial.printf("  Brightness Var:    %.0f\n", v.brightnessVariance);
            Serial.printf("  Temporal Freq:     %.3f\n", v.temporalFreq);
            Serial.printf("  Audio-Visual Corr: %.3f\n", v.audioVisualCorr);
            Serial.println("====================");
        } else {
            Serial.println("[VRMS] Renderer not available");
        }
    }
    else
#endif
#if FEATURE_INPUT_MERGE_LAYER
    // merge <param> <value> [source]  — submit parameter to merge layer
    // merge clear [source]            — clear a source (marks all params unwritten)
    // merge status                    — show active sources and merged values
    if (inputLower.startsWith("merge")) {
        handledMulti = true;
        RendererActor* ren = actors.getRenderer();
        if (!ren) {
            Serial.println("[MERGE] Renderer not available");
        } else {
            String args = inputLower.substring(5);
            args.trim();

            if (args == "status" || args == "dump") {
                // Print all 10 merged parameter values + VRMS for comparison
                static const char* paramNames[] = {
                    "brightness", "speed", "intensity", "saturation",
                    "complexity", "variation", "hue", "mood",
                    "fadeAmount", "paletteIdx"
                };
                Serial.println("\n=== Merge Layer Dump ===");
                Serial.println("MERGED_PARAMS_START");
                for (uint8_t i = 0; i < 10; i++) {
                    Serial.printf("  %s=%d\n", paramNames[i], ren->getMergedParam(i));
                }
                Serial.println("MERGED_PARAMS_END");
                // Also print manual member state for comparison
                Serial.println("MANUAL_STATE_START");
                Serial.printf("  brightness=%d\n", ren->getBrightness());
                Serial.println("MANUAL_STATE_END");
#if FEATURE_VRMS_METRICS
                metrics::VRMSVector v = ren->getVrmsVector();
                Serial.println("VRMS_START");
                Serial.printf("  brightnessMean=%.1f\n", v.brightnessMean);
                Serial.printf("  dominantHue=%.1f\n", v.dominantHue);
                Serial.printf("  symmetryScore=%.3f\n", v.symmetryScore);
                Serial.printf("  temporalFreq=%.3f\n", v.temporalFreq);
                Serial.println("VRMS_END");
#endif
                Serial.println("========================");
            } else if (args.startsWith("clear")) {
                // Reset a source: "merge clear" or "merge clear 2"
                uint8_t srcId = 2;  // default AI_AGENT
                int spaceIdx = args.indexOf(' ', 6);
                if (spaceIdx > 0) {
                    srcId = args.substring(spaceIdx + 1).toInt();
                }
                if (srcId < 2 || srcId > 3) {
                    Serial.println("[MERGE] source must be 2 (ai_agent) or 3 (gesture)");
                } else {
                    // Send 10 messages to reset all params to 0 with "unwritten" semantics
                    // The staleness timeout will handle the rest
                    Serial.printf("[MERGE] Source %d will go stale (timeout)\n", srcId);
                }
            } else {
                // Parse: merge <param> <value> [source]
                // e.g.: merge brightness 200
                //        merge speed 50 3
                int sp1 = args.indexOf(' ');
                if (sp1 < 0) {
                    Serial.println("Usage: merge <param> <value> [source=2]");
                    Serial.println("       merge status");
                    Serial.println("Params: brightness speed intensity saturation");
                    Serial.println("        complexity variation hue mood fadeAmount paletteIndex");
                    Serial.println("Source: 2=ai_agent 3=gesture");
                } else {
                    String paramName = args.substring(0, sp1);
                    String rest = args.substring(sp1 + 1);
                    rest.trim();

                    int sp2 = rest.indexOf(' ');
                    uint8_t value;
                    uint8_t srcId = 2;  // default AI_AGENT
                    if (sp2 > 0) {
                        value = rest.substring(0, sp2).toInt();
                        srcId = rest.substring(sp2 + 1).toInt();
                    } else {
                        value = rest.toInt();
                    }

                    if (srcId < 2 || srcId > 3) {
                        Serial.println("[MERGE] source must be 2 (ai_agent) or 3 (gesture)");
                    } else {
                        // Map param name to index
                        int8_t paramIdx = -1;
                        if (paramName == "brightness") paramIdx = 0;
                        else if (paramName == "speed") paramIdx = 1;
                        else if (paramName == "intensity") paramIdx = 2;
                        else if (paramName == "saturation") paramIdx = 3;
                        else if (paramName == "complexity") paramIdx = 4;
                        else if (paramName == "variation") paramIdx = 5;
                        else if (paramName == "hue") paramIdx = 6;
                        else if (paramName == "mood") paramIdx = 7;
                        else if (paramName == "fadeamount") paramIdx = 8;
                        else if (paramName == "paletteindex") paramIdx = 9;

                        if (paramIdx < 0) {
                            Serial.printf("[MERGE] Unknown param: %s\n", paramName.c_str());
                        } else {
                            Message msg;
                            msg.type = MessageType::MERGE_SUBMIT;
                            msg.param1 = srcId;
                            msg.param2 = static_cast<uint8_t>(paramIdx);
                            msg.param3 = value;
                            msg.timestamp = millis();
                            ren->send(msg);
                            Serial.printf("[MERGE] src=%d %s=%d (idx=%d)\n",
                                srcId, paramName.c_str(), value, paramIdx);
                        }
                    }
                }
            }
        }
    }
    else
#endif
    // Colour correction commands (cc, ae, gamma, brown) and capture commands
    if (peekChar == 'c' && input.length() > 1) {

        // Capture commands — delegated to CaptureStreamer (Phase 2 extraction).
        if (inputLower.startsWith("capture")) {
            handledMulti = true;
            m_deps.captureStreamer->handleCommand(input);
        }

        if (input == "c") {
            // Single 'c' — let single-char handler process it (don't set handledMulti)
            // But we've already consumed it, so handle it here
            handledMulti = true;
            auto& engine = lightwaveos::enhancement::ColorCorrectionEngine::getInstance();
            auto currentMode = engine.getMode();
            uint8_t modeInt = (uint8_t)currentMode;
            modeInt = (modeInt + 1) % 4;  // Cycle: 0->1->2->3->0
            engine.setMode((lightwaveos::enhancement::CorrectionMode)modeInt);
            const char* modeNames[] = {"OFF", "HSV", "RGB", "BOTH"};
            Serial.printf("Color correction mode: %d (%s)\n", modeInt, modeNames[modeInt]);
        } else if (input.startsWith("cc")) {
            handledMulti = true;
            auto& engine = lightwaveos::enhancement::ColorCorrectionEngine::getInstance();

            if (input == "cc") {
                // Show current mode
                auto mode = engine.getMode();
                Serial.printf("Color correction mode: %d\n", (int)mode);
                Serial.println("  0=OFF, 1=HSV, 2=RGB, 3=BOTH");
            } else if (input.length() > 2) {
                // Set mode — handle both "cc1" and "cc 1" formats
                // Find first digit after "cc" (skip whitespace)
                int startIdx = 2;
                while (startIdx < (int)input.length() && (input[startIdx] == ' ' || input[startIdx] == '\t')) {
                    startIdx++;
                }
                if (startIdx < (int)input.length()) {
                    uint8_t mode = input.substring(startIdx).toInt();
                    if (mode <= 3) {
                        engine.setMode((lightwaveos::enhancement::CorrectionMode)mode);
                        Serial.printf("Color correction mode set to: %d\n", mode);
                    } else {
                        Serial.println("Invalid mode. Use 0-3");
                    }
                }
            }
        }
        // If input doesn't start with 'c' or 'cc', let it fall through
    }
    // -----------------------------------------------------------------
    // Effect selection command (multi-digit safe): "effect <id>"
    // -----------------------------------------------------------------
    else if (peekChar == 'e' && input.length() > 1) {
        if (inputLower.startsWith("effect ")) {
            handledMulti = true;

            // Parse as EffectId — accepts both namespaced IDs (0x0100) and display indices (0-161)
            // Supports hex input with 0x/0X prefix (e.g. "effect 0x1403")
            String idStr = input.substring(7);
            idStr.trim();
            long rawId;
            if (idStr.startsWith("0x") || idStr.startsWith("0X")) {
                rawId = strtol(idStr.c_str(), nullptr, 16);
            } else {
                rawId = idStr.toInt();
            }
            EffectId effectId;
            uint16_t effectCount = renderer->getEffectCount();
            if (rawId >= 0 && rawId < effectCount) {
                // Treat small numbers as display-order index
                effectId = renderer->getEffectIdAt(rawId);
            } else {
                // Treat as direct EffectId
                effectId = static_cast<EffectId>(rawId);
            }
            if (!renderer->isEffectRegistered(effectId)) {
                Serial.printf("ERROR: Effect 0x%04X not registered\n", effectId);
            } else {
                m_currentEffect = effectId;
                actors.setEffect(effectId);
                Serial.printf("Effect 0x%04X: " LW_CLR_GREEN "%s" LW_ANSI_RESET "\n", effectId, renderer->getEffectName(effectId));
            }
        }
    }
    else if (peekChar == 'a' && input.length() > 1) {
#if FEATURE_VALIDATION_PROFILING
        if (inputLower.startsWith("validation_stats") || inputLower == "val_stats") {
            lightwaveos::core::system::ValidationProfiler::generateReport();
            handledMulti = true;
        } else
#endif
#if FEATURE_STACK_PROFILING
        if (inputLower.startsWith("stack_usage") || inputLower == "stack_profile") {
            lightwaveos::core::system::StackMonitor::generateProfileReport();
            handledMulti = true;
        } else
#endif
        if (input.startsWith("ae")) {
            handledMulti = true;
            auto& engine = lightwaveos::enhancement::ColorCorrectionEngine::getInstance();
            auto& cfg = engine.getConfig();

            if (input == "ae") {
                Serial.printf("Auto-exposure: %s, target=%d\n",
                              cfg.autoExposureEnabled ? "ON" : "OFF",
                              cfg.autoExposureTarget);
            } else if (input.length() > 2) {
                // Handle both "ae0"/"ae1" and "ae 0"/"ae 1" formats
                int startIdx = 2;
                while (startIdx < (int)input.length() && (input[startIdx] == ' ' || input[startIdx] == '\t')) {
                    startIdx++;
                }
                if (startIdx < (int)input.length()) {
                    String arg = input.substring(startIdx);
                    if (arg == "0") {
                        cfg.autoExposureEnabled = false;
                        Serial.println("Auto-exposure: OFF");
                    } else if (arg == "1") {
                        cfg.autoExposureEnabled = true;
                        Serial.println("Auto-exposure: ON");
                    } else {
                        int target = arg.toInt();
                        if (target > 0 && target <= 255) {
                            cfg.autoExposureTarget = target;
                            Serial.printf("Auto-exposure target: %d\n", target);
                        }
                    }
                }
            }
        }
#if FEATURE_AUDIO_SYNC
        // adbg -> show audio debug config
        // (x | bands handled at top level so "x" and "bands" are not gated by peekChar == 'a')
        if (inputLower.startsWith("adbg")) {
            handledMulti = true;
            auto& dbgCfg = lightwaveos::audio::getAudioDebugConfig();
            auto& unifiedCfg = lightwaveos::config::getDebugConfig();

            if (inputLower == "adbg") {
                // Show current settings
                Serial.printf("Audio debug: level=%d interval=%d frames\n",
                              dbgCfg.verbosity, dbgCfg.baseInterval);
                Serial.println("Levels: 0=off, 1=errors, 2=warnings, 3=info, 4=debug, 5=trace");
                Serial.println("One-shot: adbg status | adbg spectrum | adbg beat");
            } else if (inputLower == "adbg status") {
                // One-shot status via AudioActor method
                auto* audio = actors.getAudio();
                if (audio) {
                    audio->printStatus();
                } else {
                    Serial.println("Audio not available");
                }
            } else if (inputLower == "adbg spectrum") {
                // One-shot spectrum via AudioActor method
                auto* audio = actors.getAudio();
                if (audio) {
                    audio->printSpectrum();
                } else {
                    Serial.println("Audio not available");
                }
            } else if (inputLower == "adbg beat") {
                // One-shot beat tracking via AudioActor method
                auto* audio = actors.getAudio();
                if (audio) {
                    audio->printBeat();
                } else {
                    Serial.println("Audio not available");
                }
            } else if (inputLower.startsWith("adbg interval ")) {
                int val = inputLower.substring(14).toInt();
                if (val > 0 && val < 1000) {
                    dbgCfg.baseInterval = val;
                    Serial.printf("Base interval set to %d frames (~%.1fs)\n", val, val / 62.5f);
                } else {
                    Serial.println("Invalid interval (1-999)");
                }
            } else {
                // Parse level: adbg0, adbg1, ..., adbg5 or adbg 0, adbg 1, etc.
                int level = -1;
                if (inputLower.length() == 5 && inputLower[4] >= '0' && inputLower[4] <= '5') {
                    level = inputLower[4] - '0';
                } else if (inputLower.length() > 5) {
                    level = inputLower.substring(5).toInt();
                    if (inputLower.substring(4, 5) != " " && inputLower[4] < '0') level = -1;
                }
                if (level >= 0 && level <= 5) {
                    dbgCfg.verbosity = level;
                    // Also sync with unified DebugConfig
                    unifiedCfg.setDomainLevel(lightwaveos::config::DebugDomain::AUDIO, level);
                    const char* names[] = {"off", "errors", "warnings", "info", "debug", "trace"};
                    Serial.printf("Audio debug level: %d (%s)\n", level, names[level]);
                } else {
                    Serial.println("Usage: adbg [0-5] | adbg status | adbg spectrum | adbg beat | adbg interval <N>");
                }
            }
        }
#endif
    }
#if FEATURE_AUDIO_SYNC && FEATURE_AUDIO_BACKEND_ESV11
    // Runtime tempo parameter tuning: "tempo" shows current, "tempo <param> <value>" sets.
    else if (peekChar == 't' && inputLower.startsWith("tempo")) {
        handledMulti = true;
        auto* aa = actors.getAudio();
        if (aa) {
            auto& tp = aa->esBackend().tempoParams();
            String subcmd = input.substring(5);
            subcmd.trim();
            if (subcmd.length() == 0) {
                Serial.printf("tempo params:\n"
                    "  gate_base=%.2f gate_scale=%.2f gate_tau=%.1f\n"
                    "  conf_floor=%.2f valid_thr=%.2f stab_tau=%.1f\n"
                    "  hold_us=%lu oct_runs=%u decay_floor=%.4f\n"
                    "  oct_ratio_lo=%.2f oct_ratio_hi=%.2f\n"
                    "  ws_sep_floor=%.4f conf_decay=%.4f\n"
                    "  generic_persist_us=%lu\n",
                    tp.gateBase, tp.gateScale, tp.gateTau,
                    tp.confFloor, tp.validationThr, tp.stabilityTau,
                    (unsigned long)tp.holdUs, (unsigned)tp.octaveRuns, tp.decayFloor,
                    tp.octRatioLo, tp.octRatioHi,
                    tp.wsSepFloor, tp.confDecay,
                    (unsigned long)tp.genericPersistUs);
            } else {
                int sp = subcmd.indexOf(' ');
                if (sp > 0) {
                    String key = subcmd.substring(0, sp); key.trim();
                    float val = subcmd.substring(sp + 1).toFloat();
                    if (key == "gate_base") tp.gateBase = val;
                    else if (key == "gate_scale") tp.gateScale = val;
                    else if (key == "gate_tau") tp.gateTau = val;
                    else if (key == "conf_floor") tp.confFloor = val;
                    else if (key == "valid_thr") tp.validationThr = val;
                    else if (key == "stab_tau") tp.stabilityTau = val;
                    else if (key == "hold_us") tp.holdUs = static_cast<uint32_t>(val);
                    else if (key == "oct_runs") tp.octaveRuns = static_cast<uint8_t>(val);
                    else if (key == "decay_floor") tp.decayFloor = val;
                    else if (key == "oct_ratio_lo") tp.octRatioLo = val;
                    else if (key == "oct_ratio_hi") tp.octRatioHi = val;
                    else if (key == "ws_sep_floor") tp.wsSepFloor = val;
                    else if (key == "conf_decay") tp.confDecay = val;
                    else if (key == "generic_persist_us") tp.genericPersistUs = static_cast<uint32_t>(val);
                    else { Serial.printf("Unknown param: %s\n", key.c_str()); }
                    Serial.printf("tempo %s = %.4f\n", key.c_str(), val);
                } else {
                    Serial.println("Usage: tempo <param> <value>");
                    Serial.println("Params: gate_base gate_scale gate_tau conf_floor valid_thr");
                    Serial.println("        stab_tau hold_us oct_runs decay_floor");
                    Serial.println("        oct_ratio_lo oct_ratio_hi ws_sep_floor conf_decay");
                    Serial.println("        generic_persist_us");
                }
            }
        } else {
            Serial.println("AudioActor not available");
        }
    }
#endif
    else if (peekChar == 'g' && input.length() > 1) {
        if (input.startsWith("gamma")) {
            handledMulti = true;
            auto& engine = lightwaveos::enhancement::ColorCorrectionEngine::getInstance();
            auto& cfg = engine.getConfig();

            if (input == "gamma") {
                Serial.printf("Gamma: %s, value=%.1f\n",
                              cfg.gammaEnabled ? "ON" : "OFF",
                              cfg.gammaValue);
            } else if (input.length() > 5) {
                // Handle both "gamma1.5" and "gamma 1.5" formats
                int startIdx = 5;
                while (startIdx < (int)input.length() && (input[startIdx] == ' ' || input[startIdx] == '\t')) {
                    startIdx++;
                }
                if (startIdx < (int)input.length()) {
                    float val = input.substring(startIdx).toFloat();
                    if (val == 0) {
                        cfg.gammaEnabled = false;
                        Serial.println("Gamma: OFF");
                    } else if (val >= 1.0f && val <= 3.0f) {
                        cfg.gammaEnabled = true;
                        cfg.gammaValue = val;
                        Serial.printf("Gamma set to: %.1f\n", val);
                    } else {
                        Serial.println("Invalid gamma. Use 0 (off) or 1.0-3.0");
                    }
                }
            }
        }
    }
    // -----------------------------------------------------------------
    // Debug Commands: dbg
    // -----------------------------------------------------------------
    else if (peekChar == 'd' && input.length() >= 3) {
        if (inputLower.startsWith("dbg")) {
            handledMulti = true;
            auto& cfg = lightwaveos::config::getDebugConfig();
            String subcmd = inputLower.substring(3);
            subcmd.trim();

            if (subcmd.length() == 0) {
                // dbg — show config
                lightwaveos::config::printDebugConfig();
            }
            else if (subcmd.length() == 1 && subcmd[0] >= '0' && subcmd[0] <= '5') {
                // dbg <0-5> — set global level
                cfg.globalLevel = subcmd[0] - '0';
                Serial.printf("Global debug level: %d (%s)\n",
                              cfg.globalLevel,
                              lightwaveos::config::DebugConfig::levelName(cfg.globalLevel));
            }
            else if (subcmd.startsWith("audio ")) {
                // dbg audio <0-5>
                int level = subcmd.substring(6).toInt();
                if (level >= 0 && level <= 5) {
                    cfg.setDomainLevel(lightwaveos::config::DebugDomain::AUDIO, level);
                    Serial.printf("Audio debug level: %d (%s)\n", level,
                                  lightwaveos::config::DebugConfig::levelName((uint8_t)level));
#if FEATURE_AUDIO_SYNC
                    // Also sync with legacy AudioDebugConfig for backwards compatibility
                    lightwaveos::audio::getAudioDebugConfig().verbosity = level;
#endif
                } else {
                    Serial.println("Invalid level. Use 0-5.");
                }
            }
            else if (subcmd.startsWith("render ")) {
                // dbg render <0-5>
                int level = subcmd.substring(7).toInt();
                if (level >= 0 && level <= 5) {
                    cfg.setDomainLevel(lightwaveos::config::DebugDomain::RENDER, level);
                    Serial.printf("Render debug level: %d (%s)\n", level,
                                  lightwaveos::config::DebugConfig::levelName((uint8_t)level));
                } else {
                    Serial.println("Invalid level. Use 0-5.");
                }
            }
            else if (subcmd.startsWith("network ")) {
                // dbg network <0-5>
                int level = subcmd.substring(8).toInt();
                if (level >= 0 && level <= 5) {
                    cfg.setDomainLevel(lightwaveos::config::DebugDomain::NETWORK, level);
                    Serial.printf("Network debug level: %d (%s)\n", level,
                                  lightwaveos::config::DebugConfig::levelName((uint8_t)level));
                } else {
                    Serial.println("Invalid level. Use 0-5.");
                }
            }
            else if (subcmd.startsWith("actor ")) {
                // dbg actor <0-5>
                int level = subcmd.substring(6).toInt();
                if (level >= 0 && level <= 5) {
                    cfg.setDomainLevel(lightwaveos::config::DebugDomain::ACTOR, level);
                    Serial.printf("Actor debug level: %d (%s)\n", level,
                                  lightwaveos::config::DebugConfig::levelName((uint8_t)level));
                } else {
                    Serial.println("Invalid level. Use 0-5.");
                }
            }
            else if (subcmd.startsWith("motion ")) {
                // dbg motion <0-5>
                int level = subcmd.substring(7).toInt();
                if (level >= 0 && level <= 5) {
                    cfg.setDomainLevel(lightwaveos::config::DebugDomain::MOTION, level);
                    Serial.printf("Motion debug level: %d (%s)\n", level,
                                  lightwaveos::config::DebugConfig::levelName(static_cast<uint8_t>(level)));
                } else {
                    Serial.println("Invalid level (0-5)");
                }
            }
            else if (subcmd == "motion") {
                // One-shot motion-semantic field print
#if FEATURE_AUDIO_SYNC
                auto* audio = actors.getAudio();
                if (audio) {
                    lightwaveos::audio::ControlBusFrame bus;
                    audio->getControlBusBuffer().ReadLatest(bus);
                    Serial.println("\n=== Motion-Semantic Fields ===");
                    Serial.println("  --- Layer 1 (raw DSP) ---");
                    Serial.printf("  timing_jitter:    %.4f\n", bus.timing_jitter);
                    Serial.printf("  syncopation_level:%.4f\n", bus.syncopation_level);
                    Serial.printf("  pitch_contour_dir:%.4f\n", bus.pitch_contour_dir);
                    auto* ren = actors.getRenderer();
                    if (ren) {
                        const auto& mf = ren->getMotionFrame();
                        Serial.println("  --- Layer 2 (Laban 6-axis) ---");
                        Serial.printf("  Weight:    %.3f\n", mf.weight);
                        Serial.printf("  Time:      %.3f\n", mf.time_quality);
                        Serial.printf("  Space:     %.3f\n", mf.space);
                        Serial.printf("  Flow:      %.3f\n", mf.flow);
                        Serial.printf("  Fluidity:  %.3f\n", mf.fluidity);
                        Serial.printf("  Impulse:   %.3f\n", mf.impulse_strength);
                        Serial.printf("  Confidence:%u\n", mf.confidence_min);
                        const auto& ms = ren->getMotionShaping();
                        Serial.println("  --- Layer 3 (temporal shaping) ---");
                        Serial.printf("  Intensity: %.3f\n", ms.intensity);
                        Serial.printf("  DecayMs:   %.0f\n", ms.decayMs);
                        Serial.printf("  Accent:    %.2f\n", ms.accentScale);
                        Serial.printf("  EnvType:   %u\n", ms.envType);
                        Serial.printf("  Active:    %s\n", ms.active ? "yes" : "no");
                    }
                    Serial.println();
                } else {
                    Serial.println("[DBG] Audio not available");
                }
#else
                Serial.println("[DBG] Audio not enabled in this build");
#endif
            }
            else if (subcmd == "status") {
                // One-shot status print — use AudioActor's printStatus() method
#if FEATURE_AUDIO_SYNC
                auto* audio = actors.getAudio();
                if (audio) {
                    audio->printStatus();
                } else {
                    Serial.println("[DBG] Audio not available");
                }
#else
                Serial.println("[DBG] Audio not enabled in this build");
#endif
            }
            else if (subcmd == "spectrum") {
                // One-shot spectrum print — use AudioActor's printSpectrum() method
#if FEATURE_AUDIO_SYNC
                auto* audio = actors.getAudio();
                if (audio) {
                    audio->printSpectrum();
                } else {
                    Serial.println("[DBG] Audio not available");
                }
#else
                Serial.println("[DBG] Audio not enabled in this build");
#endif
            }
            else if (subcmd == "beat") {
                // One-shot beat print — use AudioActor's printBeat() method
#if FEATURE_AUDIO_SYNC
                auto* audio = actors.getAudio();
                if (audio) {
                    audio->printBeat();
                } else {
                    Serial.println("[DBG] Audio not available");
                }
#else
                Serial.println("[DBG] Audio not enabled in this build");
#endif
            }
            else if (subcmd == "memory") {
                // One-shot memory print
                Serial.println("\n=== Memory Status ===");
                Serial.printf("  Free heap: %lu bytes\n", ESP.getFreeHeap());
                Serial.printf("  Min free heap: %lu bytes\n", ESP.getMinFreeHeap());
                Serial.printf("  Max alloc heap: %lu bytes\n", ESP.getMaxAllocHeap());
#ifdef CONFIG_SPIRAM_SUPPORT
                Serial.printf("  Free PSRAM: %lu bytes\n", ESP.getFreePsram());
#endif
                Serial.println();
            }
            else if (subcmd.startsWith("interval ")) {
                // dbg interval status <N> or dbg interval spectrum <N>
                String rest = subcmd.substring(9);
                rest.trim();
                if (rest.startsWith("status ")) {
                    cfg.statusIntervalSec = rest.substring(7).toInt();
                    if (cfg.statusIntervalSec > 0) {
                        Serial.printf("Status interval: %u seconds\n", cfg.statusIntervalSec);
                    } else {
                        Serial.println("Status interval: disabled");
                    }
                }
                else if (rest.startsWith("spectrum ")) {
                    cfg.spectrumIntervalSec = rest.substring(9).toInt();
                    if (cfg.spectrumIntervalSec > 0) {
                        Serial.printf("Spectrum interval: %u seconds\n", cfg.spectrumIntervalSec);
                    } else {
                        Serial.println("Spectrum interval: disabled");
                    }
                }
                else {
                    Serial.println("Usage: dbg interval <status|spectrum> <seconds>");
                }
            }
            else {
                Serial.println("Usage: dbg [0-5|audio|render|network|actor|motion|status|spectrum|beat|memory|interval]");
                Serial.println("  dbg           - Show debug config");
                Serial.println("  dbg <0-5>     - Set global level");
                Serial.println("  dbg audio <0-5>   - Set audio domain level");
                Serial.println("  dbg render <0-5>  - Set render domain level");
                Serial.println("  dbg network <0-5> - Set network domain level");
                Serial.println("  dbg actor <0-5>   - Set actor domain level");
                Serial.println("  dbg motion <0-5>  - Set motion-semantic domain level");
                Serial.println("  dbg motion        - Print motion-semantic fields NOW");
                Serial.println("  dbg status    - Print audio health NOW");
                Serial.println("  dbg spectrum  - Print spectrum NOW");
                Serial.println("  dbg beat      - Print beat tracking NOW");
                Serial.println("  dbg memory    - Print heap/stack NOW");
                Serial.println("  dbg interval status <N>   - Auto status every N sec (0=off)");
                Serial.println("  dbg interval spectrum <N> - Auto spectrum every N sec (0=off)");
            }
        }
    }
    else if (peekChar == 'b' && input.length() > 1) {
        if (input.startsWith("brown")) {
            handledMulti = true;
            auto& engine = lightwaveos::enhancement::ColorCorrectionEngine::getInstance();
            auto& cfg = engine.getConfig();

            if (input == "brown") {
                Serial.printf("Brown guardrail: %s\n",
                              cfg.brownGuardrailEnabled ? "ON" : "OFF");
                Serial.printf("  Max green: %d%% of red\n", cfg.maxGreenPercentOfRed);
                Serial.printf("  Max blue: %d%% of red\n", cfg.maxBluePercentOfRed);
            } else if (input.length() > 5) {
                // Handle both "brown0"/"brown1" and "brown 0"/"brown 1" formats
                int startIdx = 5;
                while (startIdx < (int)input.length() && (input[startIdx] == ' ' || input[startIdx] == '\t')) {
                    startIdx++;
                }
                if (startIdx < (int)input.length()) {
                    String arg = input.substring(startIdx);
                    if (arg == "0") {
                        cfg.brownGuardrailEnabled = false;
                        Serial.println("Brown guardrail: OFF");
                    } else if (arg == "1") {
                        cfg.brownGuardrailEnabled = true;
                        Serial.println("Brown guardrail: ON");
                    }
                }
            }
        }
    }
    else if (peekChar == 'C' && input.length() > 1) {
        if (input == "Csave") {
            handledMulti = true;
            lightwaveos::enhancement::ColorCorrectionEngine::getInstance().saveToNVS();
            Serial.println("Color correction settings saved to NVS");
        }
    }
    else if ((peekChar == 'v' || peekChar == 'V') && input.length() > 1) {
        if (inputLower.startsWith("validate ")) {
            handledMulti = true;

            // Parse effect ID — accepts display index or direct EffectId
            int rawId = input.substring(9).toInt();
            uint16_t effectCount = renderer->getEffectCount();
            EffectId effectId;
            if (rawId >= 0 && rawId < effectCount) {
                effectId = renderer->getEffectIdAt(rawId);
            } else {
                effectId = static_cast<EffectId>(rawId);
            }

            if (!renderer->isEffectRegistered(effectId)) {
                Serial.printf("ERROR: Effect 0x%04X not registered\n", effectId);
            } else {
                // Save current effect
                EffectId savedEffect = renderer->getCurrentEffect();
                const char* effectName = renderer->getEffectName(effectId);

                // Memory baseline
                uint32_t heapBefore = ESP.getFreeHeap();

                Serial.printf("\n=== Validating Effect: %s (ID %d) ===\n", effectName, effectId);

                // Switch to effect temporarily
                actors.setEffect(effectId);
                delay(100);  // Allow effect to initialise

                // Validation checks
                bool centreOriginPass = false;
                bool hueSpanPass = false;
                bool frameRatePass = false;
                bool memoryPass = false;

                // Centre-origin check: Measure brightness at LEDs 79-80 vs edges
                uint32_t centreBrightnessSum = 0;
                uint32_t edgeBrightnessSum = 0;
                uint16_t centreSamples = 0;
                uint16_t edgeSamples = 0;

                // Hue span tracking
                float minHue = 360.0f;
                float maxHue = 0.0f;
                bool hueInitialized = false;

                // Capture LED buffer without heap allocations
                constexpr uint16_t TOTAL_LEDS = 320;  // 2 strips x 160 LEDs
                CRGB* ledBuffer = m_deps.validationScratch;

                // Capture for 2 seconds total:
                // - first 1 second: brightness + hue sampling
                // - full 2 seconds: FPS stabilisation
                uint32_t validationStart = millis();
                uint32_t validationEnd = validationStart + 2000;
                uint16_t samplesCollected = 0;

                while (millis() < validationEnd) {
                    delay(8);  // ~120 FPS pacing

                    // Snapshot the LED buffer
                    renderer->getBufferCopy(ledBuffer);

                    // Only sample brightness/hue for first second to limit compute cost
                    if (millis() - validationStart < 1000 && samplesCollected < 120) {
                        samplesCollected++;

                        // Centre-origin: LEDs 79-80 vs edges (0-10, 150-159)
                        for (int i = 79; i <= 80; i++) {
                            uint16_t brightness = (uint16_t)ledBuffer[i].r + ledBuffer[i].g + ledBuffer[i].b;
                            centreBrightnessSum += brightness;
                            centreSamples++;
                        }

                        for (int i = 0; i <= 10; i++) {
                            uint16_t brightness = (uint16_t)ledBuffer[i].r + ledBuffer[i].g + ledBuffer[i].b;
                            edgeBrightnessSum += brightness;
                            edgeSamples++;
                        }
                        for (int i = 150; i <= 159; i++) {
                            uint16_t brightness = (uint16_t)ledBuffer[i].r + ledBuffer[i].g + ledBuffer[i].b;
                            edgeBrightnessSum += brightness;
                            edgeSamples++;
                        }

                        // Hue span: convert RGB to hue (degrees), track min/max
                        for (int i = 0; i < TOTAL_LEDS; i++) {
                            CRGB rgb = ledBuffer[i];
                            if (rgb.r == 0 && rgb.g == 0 && rgb.b == 0) continue;  // Skip black

                            uint8_t maxVal = max(max(rgb.r, rgb.g), rgb.b);
                            uint8_t minVal = min(min(rgb.r, rgb.g), rgb.b);
                            if (maxVal == 0 || maxVal == minVal) continue;

                            float delta = (maxVal - minVal) / 255.0f;
                            float hue = 0.0f;

                            if (maxVal == rgb.r) {
                                hue = 60.0f * fmod((((rgb.g - rgb.b) / 255.0f) / delta), 6.0f);
                            } else if (maxVal == rgb.g) {
                                hue = 60.0f * (((rgb.b - rgb.r) / 255.0f) / delta + 2.0f);
                            } else {
                                hue = 60.0f * (((rgb.r - rgb.g) / 255.0f) / delta + 4.0f);
                            }
                            if (hue < 0) hue += 360.0f;

                            if (!hueInitialized) {
                                minHue = maxHue = hue;
                                hueInitialized = true;
                            } else {
                                if (hue < minHue) minHue = hue;
                                if (hue > maxHue) maxHue = hue;
                            }
                        }
                    }
                }

                // Frame rate check (after 2s run)
                const RenderStats& finalStats = renderer->getStats();
                frameRatePass = (finalStats.currentFPS >= 120);

                // Centre-origin validation
                float centreAvg = 0.0f;
                float edgeAvg = 0.0f;
                float ratio = 0.0f;
                if (centreSamples > 0 && edgeSamples > 0) {
                    centreAvg = centreBrightnessSum / (float)centreSamples;
                    edgeAvg = edgeBrightnessSum / (float)edgeSamples;
                    ratio = (edgeAvg > 0.0f) ? (centreAvg / edgeAvg) : 0.0f;
                    centreOriginPass = (centreAvg > edgeAvg * 1.2f);
                }

                // Hue span validation
                float hueSpan = 0.0f;
                if (hueInitialized) {
                    hueSpan = maxHue - minHue;
                    if (hueSpan > 180.0f) hueSpan = 360.0f - hueSpan;  // wrap-around
                    hueSpanPass = (hueSpan < 60.0f);
                }

                // Memory check (heap delta should be near-zero; allow small drift)
                uint32_t heapAfter = ESP.getFreeHeap();
                int32_t heapDelta = (int32_t)heapAfter - (int32_t)heapBefore;
                memoryPass = (abs(heapDelta) <= 256);

                // Output validation report
                Serial.println("\n--- Validation Results ---");
                Serial.printf("[%s] Centre-origin: %s (centre: %.0f, edge: %.0f, ratio: %.2fx)\n",
                              centreOriginPass ? "✓" : "✗",
                              centreOriginPass ? "PASS" : "FAIL",
                              centreAvg, edgeAvg, ratio);

                Serial.printf("[%s] Hue span: %s (hue range: %.1f°, limit: <60°)\n",
                              hueSpanPass ? "✓" : "✗",
                              hueSpanPass ? "PASS" : "FAIL",
                              hueSpan);

                Serial.printf("[%s] Frame rate: %s (%d FPS, target: ≥120 FPS)\n",
                              frameRatePass ? "✓" : "✗",
                              frameRatePass ? "PASS" : "FAIL",
                              finalStats.currentFPS);

                Serial.printf("[%s] Memory: %s (free heap Δ %ld bytes)\n",
                              memoryPass ? "✓" : "✗",
                              memoryPass ? "PASS" : "FAIL",
                              (long)heapDelta);

                int passCount = (centreOriginPass ? 1 : 0) + (hueSpanPass ? 1 : 0) +
                                (frameRatePass ? 1 : 0) + (memoryPass ? 1 : 0);
                int totalChecks = 4;

                Serial.printf("\nOverall: %s (%d/%d checks passed)\n",
                              (passCount == totalChecks) ? "PASS" : "PARTIAL",
                              passCount, totalChecks);

                // Restore original effect
                actors.setEffect(savedEffect);
                delay(50);

                Serial.println("========================================\n");
            }
        } else {
            // Treat non-validate 'v...' input as handled to avoid consuming
            // and then reading a stale single-char command
            handledMulti = true;
            Serial.println("Unknown command. Use: validate <effect_id>");
        }
    }

    // -----------------------------------------------------------------
    // Trace Commands: trace (MabuTrace Perfetto dump)
    // -----------------------------------------------------------------
#if FEATURE_MABUTRACE
    else if (inputLower == "trace") {
        handledMulti = true;
        Serial.println(F("[TRACE] Flushing trace buffer..."));
        TRACE_FLUSH();
        get_json_trace_chunked(nullptr, [](void* /*ctx*/, const char* chunk, size_t len) {
            Serial.write(chunk, len);
        });
        Serial.println();
        Serial.println(F("[TRACE] Done."));
    }
#endif

    // -----------------------------------------------------------------
    // Tempo Debug Commands: tempo (non-ESV11 backend)
    // -----------------------------------------------------------------
#if FEATURE_AUDIO_SYNC && !FEATURE_AUDIO_BACKEND_ESV11 && !FEATURE_AUDIO_BACKEND_PIPELINECORE
    else if (inputLower.startsWith("tempo")) {
        handledMulti = true;

        // Get TempoTracker from AudioActor
        auto* audio = actors.getAudio();
        if (audio) {
            const auto& tempo = audio->getTempo();
            auto output = tempo.getOutput();
            Serial.println("=== TempoTracker Status ===");
            Serial.printf("  BPM: %.1f\n", output.bpm);
            Serial.printf("  Phase: %.3f\n", output.phase01);
            Serial.printf("  Confidence: %.2f\n", output.confidence);
            Serial.printf("  Locked: %s\n", output.locked ? "YES" : "NO");
            Serial.printf("  Beat Strength: %.2f\n", output.beat_strength);
            Serial.printf("  Winner Bin: %u\n", tempo.getWinnerBin());
        } else {
            Serial.println("TempoTracker not available (audio not enabled)");
        }
    }
#endif

    // -----------------------------------------------------------------
    // Zone Speed Commands: zs
    // -----------------------------------------------------------------
    else if (inputLower.startsWith("zs ")) {
        handledMulti = true;

        if (!zoneComposer.isEnabled()) {
            Serial.println("ERROR: Zone mode not enabled. Press 'z' to enable.");
        } else {
            // Parse: "zs <zoneId> <speed>" or "zs <speed0> <speed1> <speed2>"
            String args = input.substring(3); // After "zs "
            args.trim();

            int firstSpace = args.indexOf(' ');
            if (firstSpace > 0) {
                String first = args.substring(0, firstSpace);
                String rest = args.substring(firstSpace + 1);
                rest.trim();

                int parsedZone = first.toInt();
                int parsedSpeed = rest.toInt();

                if (parsedZone >= 0 && parsedZone < zoneComposer.getZoneCount() &&
                    parsedSpeed >= 1 && parsedSpeed <= 50) {
                    zoneComposer.setZoneSpeed(parsedZone, parsedSpeed);
                    Serial.printf("Zone %d speed set to %d\n", parsedZone, parsedSpeed);
                } else {
                    Serial.println("ERROR: Usage: zs <zoneId> <speed> (zoneId: 0-2, speed: 1-50)");
                }
            } else {
                // Try parsing as "zs <speed0> <speed1> <speed2>"
                int space1 = args.indexOf(' ');
                if (space1 > 0) {
                    String s0 = args.substring(0, space1);
                    String rest1 = args.substring(space1 + 1);
                    rest1.trim();
                    int space2 = rest1.indexOf(' ');
                    if (space2 > 0) {
                        String s1 = rest1.substring(0, space2);
                        String s2 = rest1.substring(space2 + 1);
                        s2.trim();

                        int speed0 = s0.toInt();
                        int speed1 = s1.toInt();
                        int speed2 = s2.toInt();

                        if (speed0 >= 1 && speed0 <= 50 &&
                            speed1 >= 1 && speed1 <= 50 &&
                            speed2 >= 1 && speed2 <= 50 &&
                            zoneComposer.getZoneCount() >= 3) {
                            zoneComposer.setZoneSpeed(0, speed0);
                            zoneComposer.setZoneSpeed(1, speed1);
                            zoneComposer.setZoneSpeed(2, speed2);
                            Serial.printf("Zone speeds set: Zone 0=%d, Zone 1=%d, Zone 2=%d\n",
                                        speed0, speed1, speed2);
                        } else {
                            Serial.println("ERROR: Usage: zs <speed0> <speed1> <speed2> (speeds: 1-50)");
                        }
                    } else {
                        Serial.println("ERROR: Usage: zs <zoneId> <speed> OR zs <speed0> <speed1> <speed2>");
                    }
                } else {
                    Serial.println("ERROR: Usage: zs <zoneId> <speed> OR zs <speed0> <speed1> <speed2>");
                }
            }
        }
    }

    // -----------------------------------------------------------------
    // WiFi commands: wifi, wifi connect SSID PASS, wifi ap, wifi scan
    // Uses original `input` (not inputLower) to preserve SSID case.
    //
    // WARNING: K1 is AP-ONLY. STA mode (`wifi connect`) is an escape hatch
    // for development/debugging. It is KNOWN UNRELIABLE — see CLAUDE.md
    // WiFi Architecture section. Do NOT add automatic STA connection logic.
    // -----------------------------------------------------------------
#if FEATURE_WEB_SERVER
    else if (inputLower.startsWith("wifi")) {
        handledMulti = true;
        // Use original input to preserve SSID/password case
        String args = input.substring(4);
        args.trim();
        String argsLower = args;
        argsLower.toLowerCase();

        if (args.length() == 0 || argsLower == "status") {
            // Print WiFi status
            Serial.println("\n=== WiFi Status ===");
            Serial.printf("  Mode: %s\n",
                WiFi.getMode() == WIFI_MODE_AP ? "AP" :
                WiFi.getMode() == WIFI_MODE_STA ? "STA" :
                WiFi.getMode() == WIFI_MODE_APSTA ? "AP+STA" : "OFF");
            if (WiFi.isConnected()) {
                Serial.printf("  Connected: YES\n");
                Serial.printf("  SSID: %s\n", WiFi.SSID().c_str());
                Serial.printf("  IP: %s\n", WiFi.localIP().toString().c_str());
                Serial.printf("  RSSI: %d dBm\n", WiFi.RSSI());
                Serial.printf("  Channel: %d\n", WiFi.channel());
            } else {
                Serial.printf("  Connected: NO\n");
            }
            Serial.printf("  AP IP: %s\n", WiFi.softAPIP().toString().c_str());
            Serial.printf("  AP Clients: %d\n", WiFi.softAPgetStationNum());
            // List saved networks (NVS)
            lightwaveos::network::WiFiCredentialsStorage::NetworkCredential nets[8];
            uint8_t count = WIFI_MANAGER.getSavedNetworks(nets, 8);
            Serial.printf("  Saved Networks (NVS): %d\n", count);
            for (uint8_t i = 0; i < count; i++) {
                Serial.printf("    [%d] %s\n", i, nets[i].ssid.c_str());
            }
            // Show compile-time config SSIDs
            using namespace lightwaveos::config;
            Serial.printf("  Config SSID 1: %s\n", NetworkConfig::WIFI_SSID_VALUE);
            if (strlen(NetworkConfig::WIFI_SSID_2_VALUE) > 0)
                Serial.printf("  Config SSID 2: %s\n", NetworkConfig::WIFI_SSID_2_VALUE);
            Serial.println();
        }
        else if (argsLower.startsWith("connect")) {
            // Parse: "connect SSID PASSWORD" or "connect SSID" (for saved) or "connect" (auto)
            String connectArgs = args.substring(7); // after "connect"
            connectArgs.trim();

            // Split into SSID and PASSWORD by first space
            String ssid = "";
            String password = "";
            int spaceIdx = connectArgs.indexOf(' ');
            if (spaceIdx > 0) {
                ssid = connectArgs.substring(0, spaceIdx);
                password = connectArgs.substring(spaceIdx + 1);
                password.trim();
            } else {
                ssid = connectArgs;
            }

            // No SSID given — try last connected or first saved
            if (ssid.length() == 0) {
                String lastSSID = WIFI_MANAGER.getLastConnectedSSID();
                if (lastSSID.length() > 0) {
                    ssid = lastSSID;
                } else {
                    lightwaveos::network::WiFiCredentialsStorage::NetworkCredential nets[8];
                    uint8_t count = WIFI_MANAGER.getSavedNetworks(nets, 8);
                    if (count > 0) ssid = nets[0].ssid;
                }
            }

            if (ssid.length() == 0) {
                Serial.println("ERROR: No SSID specified and no saved networks");
                Serial.println("Usage: wifi connect SSID PASSWORD");
            } else if (password.length() > 0) {
                // Have both SSID and password — connect directly and save to NVS
                Serial.printf("Connecting to '%s' (saving to NVS)...\n", ssid.c_str());
                bool ok = WIFI_MANAGER.connectToNetwork(ssid, password);
                if (ok) {
                    Serial.println("Connection initiated (AP+STA). Check 'wifi' in ~10s.");
                } else {
                    Serial.println("Connect request failed");
                }
            } else {
                // SSID only — try saved credentials (NVS)
                Serial.printf("Connecting to saved network: '%s'...\n", ssid.c_str());
                bool ok = WIFI_MANAGER.connectToSavedNetwork(ssid);
                if (ok) {
                    Serial.println("Connection initiated (AP+STA). Check 'wifi' in ~10s.");
                } else {
                    Serial.printf("'%s' not in NVS. Use: wifi connect %s YOUR_PASSWORD\n", ssid.c_str(), ssid.c_str());
                }
            }
        }
        else if (argsLower == "ap") {
            Serial.println("Switching to AP-only mode...");
            WIFI_MANAGER.requestAPOnly();
            Serial.println("AP-only mode active");
        }
        else if (argsLower == "scan") {
            Serial.println("Triggering WiFi scan...");
            WIFI_MANAGER.scanNetworks();
            Serial.println("Scan initiated. Check 'wifi' in ~5s for results.");
        }
        else {
            Serial.println("WiFi commands:");
            Serial.println("  wifi                        -- show status");
            Serial.println("  wifi connect SSID PASSWORD  -- connect + save to NVS");
            Serial.println("  wifi connect SSID           -- connect using saved NVS creds");
            Serial.println("  wifi connect                -- reconnect to last/first saved");
            Serial.println("  wifi ap                     -- switch to AP-only mode");
            Serial.println("  wifi scan                   -- trigger network scan");
        }
    }
#endif
}

// ============================================================================
// Single-char hotkey handler
// ============================================================================

void SerialCLI::handleSingleCharCommand(char cmd) {
    auto* renderer = m_deps.renderer;
    auto& actors = *m_deps.actors;
    auto& zoneComposer = *m_deps.zoneComposer;

    // Check if in zone mode for special handling
    bool inZoneMode = zoneComposer.isEnabled();

    // Zone mode: 1-5 selects presets
    if (inZoneMode && cmd >= '1' && cmd <= '5') {
        uint8_t preset = cmd - '1';
        zoneComposer.loadPreset(preset);
        Serial.printf("Zone Preset: %s\n", ZoneComposer::getPresetName(preset));
    }
    // Numeric and alpha effect selection (single mode)
    else if (!inZoneMode && cmd >= '0' && cmd <= '9') {
        uint8_t e = cmd - '0';

        // Special case: '6' cycles through audio effects (72=Waveform, 73=Bloom)
        if (e == 6) {
            const EffectId audioEffects[] = {lightwaveos::EID_AUDIO_WAVEFORM, lightwaveos::EID_AUDIO_BLOOM};
            const uint8_t audioEffectCount = sizeof(audioEffects) / sizeof(audioEffects[0]);

            // Cycle to next audio effect
            m_lastAudioEffectIndex = (m_lastAudioEffectIndex + 1) % audioEffectCount;
            EffectId audioEffectId = audioEffects[m_lastAudioEffectIndex];

            if (renderer->isEffectRegistered(audioEffectId)) {
                m_currentEffect = audioEffectId;
                actors.setEffect(audioEffectId);
                Serial.printf("Audio Effect 0x%04X: " LW_CLR_GREEN "%s" LW_ANSI_RESET "\n", audioEffectId, renderer->getEffectName(audioEffectId));
            } else {
                Serial.printf("ERROR: Audio effect 0x%04X not registered\n", audioEffectId);
            }
        } else {
            // Normal numeric effect selection (0-5, 7-9) — map to display order
            EffectId eid = (e < lightwaveos::DISPLAY_COUNT) ? lightwaveos::DISPLAY_ORDER[e] : lightwaveos::INVALID_EFFECT_ID;
            if (renderer->isEffectRegistered(eid)) {
                m_currentEffect = eid;
                actors.setEffect(eid);
                Serial.printf("Effect [%d] 0x%04X: " LW_CLR_GREEN "%s" LW_ANSI_RESET "\n", e, eid, renderer->getEffectName(eid));
            }
        }
    } else {
        // Check if this is an effect selection key (a-k = effects 10-20, excludes command letters)
        // Command letters: c, e, g, n, l, p, s, t, z are handled in switch below
        bool isEffectKey = false;
        if (!inZoneMode && cmd >= 'a' && cmd <= 'k' &&
            cmd != 'a' && cmd != 'b' && cmd != 'c' && cmd != 'd' && cmd != 'e' && cmd != 'f' && cmd != 'g' &&
            cmd != 'h' && cmd != 'i' && cmd != 'j' && cmd != 'k' &&
            cmd != 'n' && cmd != 'l' && cmd != 'p' && cmd != 's' && cmd != 't' && cmd != 'z') {
            uint16_t displayIdx = 10 + (cmd - 'a');
            EffectId eid = (displayIdx < lightwaveos::DISPLAY_COUNT) ? lightwaveos::DISPLAY_ORDER[displayIdx] : lightwaveos::INVALID_EFFECT_ID;
            if (renderer->isEffectRegistered(eid)) {
                m_currentEffect = eid;
                actors.setEffect(eid);
                Serial.printf("Effect [%d] 0x%04X: " LW_CLR_GREEN "%s" LW_ANSI_RESET "\n", displayIdx, eid, renderer->getEffectName(eid));
                isEffectKey = true;
            }
        }

        if (!isEffectKey)
        switch (cmd) {
#if FEATURE_AUDIO_SYNC
        case 'x': case 'X':
            // Bands observability (same as top-level "x" / "bands")
            {
                RendererActor* ren = actors.getRenderer();
                if (ren) {
                    RendererActor::BandsDebugSnapshot snap;
                    ren->getBandsDebugSnapshot(snap);
                    if (snap.valid) {
                        Serial.println("\n=== Bands (renderer snapshot) ===");
                        Serial.printf("  bands[0..7]: %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f\n",
                            snap.bands[0], snap.bands[1], snap.bands[2], snap.bands[3],
                            snap.bands[4], snap.bands[5], snap.bands[6], snap.bands[7]);
                        Serial.printf("  bass=%.3f mid=%.3f treble=%.3f (ctx.audio accessors)\n",
                            snap.bass, snap.mid, snap.treble);
                        Serial.printf("  rms=%.3f flux=%.3f hop_seq=%lu\n",
                            snap.rms, snap.flux, (unsigned long)snap.hop_seq);
                    } else {
                        Serial.println("Bands: no live snapshot (audio buffer or frame not ready)");
                    }
                } else {
                    Serial.println("Renderer not available");
                }
            }
            break;
#endif
        case 'z':
            // Toggle zone mode
            zoneComposer.setEnabled(!zoneComposer.isEnabled());
            Serial.printf("Zone Mode: %s\n",
                          zoneComposer.isEnabled() ? "ENABLED" : "DISABLED");
            if (zoneComposer.isEnabled()) {
                Serial.println("  Press 1-5 to load presets");
            }
            break;

        case 'Z':
            // Print zone status
            zoneComposer.printStatus();
            break;

        case 'S':
            // Save all settings to NVS
            if (m_deps.zoneConfigMgr) {
                Serial.println("Saving settings to NVS...");
                bool zoneOk = m_deps.zoneConfigMgr->saveToNVS();
                bool sysOk = m_deps.zoneConfigMgr->saveSystemState(
                    renderer->getCurrentEffect(),
                    renderer->getBrightness(),
                    renderer->getSpeed(),
                    renderer->getPaletteIndex()
                );
                if (zoneOk && sysOk) {
                    Serial.println("  All settings saved!");
                } else {
                    Serial.printf("  Save result: zones=%s, system=%s\n",
                                  zoneOk ? "OK" : "FAIL",
                                  sysOk ? "OK" : "FAIL");
                }
            } else {
                Serial.println("ERROR: Config manager not initialized");
            }
            break;

        case '`':
#if !defined(NATIVE_BUILD) && FEATURE_STATUS_STRIP_TOUCH
            statusStripNextIdleMode();
#endif
            break;

        case ' ':  // Spacebar — quick next effect (no Enter needed)
        case 'n':
            if (!inZoneMode) {
                EffectId newEffectId = m_currentEffect;

                switch (m_currentRegister) {
                    case EffectRegister::ALL:
                        newEffectId = lightwaveos::getNextDisplay(m_currentEffect);
                        break;

                    case EffectRegister::REACTIVE:
                        if (PatternRegistry::getReactiveEffectCount() > 0) {
                            m_reactiveRegisterIndex = (m_reactiveRegisterIndex + 1) %
                                                      PatternRegistry::getReactiveEffectCount();
                            newEffectId = PatternRegistry::getReactiveEffectId(m_reactiveRegisterIndex);
                        }
                        break;

                    case EffectRegister::AMBIENT:
                        if (m_ambientEffectCount > 0) {
                            m_ambientRegisterIndex = (m_ambientRegisterIndex + 1) % m_ambientEffectCount;
                            newEffectId = m_ambientEffectIds[m_ambientRegisterIndex];
                        }
                        break;
                }

                if (newEffectId != lightwaveos::INVALID_EFFECT_ID) {
                    m_currentEffect = newEffectId;
                    actors.setEffect(m_currentEffect);
                    const char* suffix = (m_currentRegister == EffectRegister::REACTIVE) ? "[R]" :
                                         (m_currentRegister == EffectRegister::AMBIENT) ? "[M]" : "";
                    Serial.printf("Effect 0x%04X%s: " LW_CLR_GREEN "%s" LW_ANSI_RESET "\n",
                                  m_currentEffect, suffix, renderer->getEffectName(m_currentEffect));
                }
            }
            break;

        case 'N':
            if (!inZoneMode) {
                EffectId newEffectId = m_currentEffect;

                switch (m_currentRegister) {
                    case EffectRegister::ALL:
                        newEffectId = lightwaveos::getPrevDisplay(m_currentEffect);
                        break;

                    case EffectRegister::REACTIVE: {
                        uint8_t count = PatternRegistry::getReactiveEffectCount();
                        if (count > 0) {
                            m_reactiveRegisterIndex = (m_reactiveRegisterIndex + count - 1) % count;
                            newEffectId = PatternRegistry::getReactiveEffectId(m_reactiveRegisterIndex);
                        }
                        break;
                    }

                    case EffectRegister::AMBIENT:
                        if (m_ambientEffectCount > 0) {
                            m_ambientRegisterIndex = (m_ambientRegisterIndex + m_ambientEffectCount - 1) %
                                                     m_ambientEffectCount;
                            newEffectId = m_ambientEffectIds[m_ambientRegisterIndex];
                        }
                        break;
                }

                if (newEffectId != lightwaveos::INVALID_EFFECT_ID) {
                    m_currentEffect = newEffectId;
                    actors.setEffect(m_currentEffect);
                    const char* suffix = (m_currentRegister == EffectRegister::REACTIVE) ? "[R]" :
                                         (m_currentRegister == EffectRegister::AMBIENT) ? "[M]" : "";
                    Serial.printf("Effect 0x%04X%s: " LW_CLR_GREEN "%s" LW_ANSI_RESET "\n",
                                  m_currentEffect, suffix, renderer->getEffectName(m_currentEffect));
                }
            }
            break;

        case 'r':  // Switch to Reactive register (audio-reactive effects)
            m_currentRegister = EffectRegister::REACTIVE;
            Serial.println("Switched to " LW_CLR_CYAN "Reactive" LW_ANSI_RESET " register");
            Serial.printf("  %d audio-reactive effects available\n",
                          PatternRegistry::getReactiveEffectCount());
            // Switch to current reactive effect
            if (PatternRegistry::getReactiveEffectCount() > 0) {
                EffectId reactiveId = PatternRegistry::getReactiveEffectId(m_reactiveRegisterIndex);
                if (reactiveId != lightwaveos::INVALID_EFFECT_ID) {
                    m_currentEffect = reactiveId;
                    actors.setEffect(reactiveId);
                    Serial.printf("  Current: %s (ID 0x%04X)\n",
                                  renderer->getEffectName(reactiveId), reactiveId);
                }
            }
            break;

        case 'm':  // Switch to aMbient register (time-based effects)
            m_currentRegister = EffectRegister::AMBIENT;
            Serial.println("Switched to " LW_CLR_MAGENTA "Ambient" LW_ANSI_RESET " register");
            Serial.printf("  %d ambient effects available\n", m_ambientEffectCount);
            // Switch to current ambient effect
            if (m_ambientEffectCount > 0 && m_ambientRegisterIndex < m_ambientEffectCount) {
                EffectId ambientId = m_ambientEffectIds[m_ambientRegisterIndex];
                if (ambientId != lightwaveos::INVALID_EFFECT_ID) {
                    m_currentEffect = ambientId;
                    actors.setEffect(ambientId);
                    Serial.printf("  Current: %s (ID 0x%04X)\n",
                                  renderer->getEffectName(ambientId), ambientId);
                }
            }
            break;

        case '*':  // Switch back to All effects register (default)
            m_currentRegister = EffectRegister::ALL;
            Serial.println("Switched to " LW_CLR_GREEN "All Effects" LW_ANSI_RESET " register");
            Serial.printf("  %d effects available\n", renderer->getEffectCount());
            Serial.printf("  Current: %s (ID 0x%04X)\n",
                          renderer->getEffectName(m_currentEffect), m_currentEffect);
            break;

        case 'L': {  // Jump to last effect in current register
            EffectId newEffectId = lightwaveos::INVALID_EFFECT_ID;

            switch (m_currentRegister) {
                case EffectRegister::ALL: {
                    // Last effect in display order
                    uint16_t dc = lightwaveos::DISPLAY_COUNT;
                    if (dc > 0) {
                        newEffectId = lightwaveos::DISPLAY_ORDER[dc - 1];
                    }
                    break;
                }

                case EffectRegister::REACTIVE: {
                    uint8_t count = PatternRegistry::getReactiveEffectCount();
                    if (count > 0) {
                        m_reactiveRegisterIndex = count - 1;
                        newEffectId = PatternRegistry::getReactiveEffectId(m_reactiveRegisterIndex);
                    }
                    break;
                }

                case EffectRegister::AMBIENT:
                    if (m_ambientEffectCount > 0) {
                        m_ambientRegisterIndex = m_ambientEffectCount - 1;
                        newEffectId = m_ambientEffectIds[m_ambientRegisterIndex];
                    }
                    break;
            }

            if (newEffectId != lightwaveos::INVALID_EFFECT_ID) {
                m_currentEffect = newEffectId;
                actors.setEffect(m_currentEffect);
                const char* suffix = (m_currentRegister == EffectRegister::REACTIVE) ? "[R]" :
                                     (m_currentRegister == EffectRegister::AMBIENT) ? "[M]" : "";
                Serial.printf("Last effect 0x%04X%s: " LW_CLR_GREEN "%s" LW_ANSI_RESET "\n",
                              m_currentEffect, suffix, renderer->getEffectName(m_currentEffect));
            }
            break;
        }

        case '+':
        case '=':
            {
                uint8_t b = renderer->getBrightness();
                if (b < 255) {
                    b = min((int)b + 16, 255);
                    actors.setBrightness(b);
                    Serial.printf("Brightness: %d\n", b);
                }
            }
            break;

        case '-':
            {
                uint8_t b = renderer->getBrightness();
                if (b > 16) {
                    b = max((int)b - 16, 16);
                    actors.setBrightness(b);
                    Serial.printf("Brightness: %d\n", b);
                }
            }
            break;

        case '[':
            {
                uint8_t s = renderer->getSpeed();
                if (s > 1) {
                    s = max((int)s - 1, 1);
                    actors.setSpeed(s);
                    Serial.printf("Speed: %d\n", s);
                }
            }
            break;

        case ']':
            {
                uint8_t s = renderer->getSpeed();
                if (s < 100) {
                    s = min((int)s + 1, 100);
                    actors.setSpeed(s);
                    Serial.printf("Speed: %d\n", s);
                }
            }
            break;

        case '.':  // Next palette (quick key)
            {
                uint8_t paletteCount = renderer->getPaletteCount();
                uint8_t pal = (renderer->getPaletteIndex() + 1) % paletteCount;
                actors.setPalette(pal);
                Serial.printf("Palette %d/%d: %s\n", pal, paletteCount, renderer->getPaletteName(pal));
            }
            break;

        case ',':  // Previous palette
            {
                uint8_t paletteCount = renderer->getPaletteCount();
                uint8_t current = renderer->getPaletteIndex();
                uint8_t p = (current + paletteCount - 1) % paletteCount;
                actors.setPalette(p);
                Serial.printf("Palette %d/%d: %s\n", p, paletteCount, renderer->getPaletteName(p));
            }
            break;

        case 'l':
            {
                uint16_t effectCount = renderer->getEffectCount();
                Serial.printf("\n=== Effects (%d total) ===\n", effectCount);
                for (uint16_t i = 0; i < effectCount; i++) {
                    EffectId eid = renderer->getEffectIdAt(i);
                    char key = (i < 10) ? ('0' + i) : ('a' + i - 10);
                    const char* type = (renderer->getEffectInstance(eid) != nullptr) ? " [IEffect]" : " [Legacy]";
                    Serial.printf("  %3d [%c] 0x%04X: %s%s%s\n", i, key, eid, renderer->getEffectName(eid), type,
                                  (!inZoneMode && eid == m_currentEffect) ? " <--" : "");
                }
                Serial.println();
            }
            break;

        case 'p':
            // Bloom prism opacity +
            {
                using BP = lightwaveos::effects::ieffect::BloomParityEffect;
                BP::setPrismOpacity(BP::getPrismOpacity() + 0.05f);
                Serial.printf("Bloom Prism: %.2f\n", BP::getPrismOpacity());
            }
            break;

        case 'P':
            // Bloom prism opacity -
            {
                using BP = lightwaveos::effects::ieffect::BloomParityEffect;
                BP::setPrismOpacity(BP::getPrismOpacity() - 0.05f);
                Serial.printf("Bloom Prism: %.2f\n", BP::getPrismOpacity());
            }
            break;

        case 'o':
            // Bloom bulb opacity +
            {
                using BP = lightwaveos::effects::ieffect::BloomParityEffect;
                BP::setBulbOpacity(BP::getBulbOpacity() + 0.05f);
                Serial.printf("Bloom Bulb: %.2f\n", BP::getBulbOpacity());
            }
            break;

        case 'O':
            // Bloom bulb opacity -
            {
                using BP = lightwaveos::effects::ieffect::BloomParityEffect;
                BP::setBulbOpacity(BP::getBulbOpacity() - 0.05f);
                Serial.printf("Bloom Bulb: %.2f\n", BP::getBulbOpacity());
            }
            break;

        case 'a':
            // Toggle audio debug logging
            {
                bool enabled = !renderer->isAudioDebugEnabled();
                renderer->setAudioDebugEnabled(enabled);
                Serial.printf("Audio debug: %s\n", enabled ? "ON" : "OFF");
            }
            break;

        case 'i':
            // Mood +
            {
                uint8_t m = renderer->getMood();
                m = static_cast<uint8_t>(min(static_cast<int>(m) + 16, 255));
                actors.setMood(m);
                Serial.printf("Mood: %d (%.0f%%)\n", m, m * 100.0f / 255.0f);
            }
            break;

        case 's':
            actors.printStatus();
            if (zoneComposer.isEnabled()) {
                zoneComposer.printStatus();
            }
            if (renderer->isTransitionActive()) {
                Serial.println("  Transition: ACTIVE");
            }
            // Show IEffect status for current effect
            {
                EffectId current = renderer->getCurrentEffect();
                IEffect* effect = renderer->getEffectInstance(current);
                if (effect != nullptr) {
                    Serial.printf("  Current effect type: IEffect (native)\n");
                    const EffectMetadata& meta = effect->getMetadata();
                    Serial.printf("  Metadata: %s - %s\n", meta.name, meta.description);
                } else {
                    Serial.printf("  Current effect type: Legacy (function pointer)\n");
                }
            }
            break;

        case 'd':
            // Toggle Bloom effect debug output
            lightwaveos::effects::ieffect::g_bloomDebugEnabled =
                !lightwaveos::effects::ieffect::g_bloomDebugEnabled;
            Serial.printf("[BLOOM DEBUG] %s\n",
                lightwaveos::effects::ieffect::g_bloomDebugEnabled ? "ENABLED (select effect 120)" : "DISABLED");
            break;

        case 't':
        case 'T':
            // RD Triangle: F - / +
            {
                EffectId rdTriangleId = lightwaveos::INVALID_EFFECT_ID;
                uint16_t effectCount = renderer->getEffectCount();
                for (uint16_t i = 0; i < effectCount; i++) {
                    EffectId eid = renderer->getEffectIdAt(i);
                    const char* name = renderer->getEffectName(eid);
                    if (name && strcmp(name, "LGP RD Triangle") == 0) {
                        rdTriangleId = eid;
                        break;
                    }
                }
                if (rdTriangleId == lightwaveos::INVALID_EFFECT_ID) {
                    Serial.println("RD Triangle not found");
                    break;
                }
                if (renderer->getCurrentEffect() != rdTriangleId) {
                    m_currentEffect = rdTriangleId;
                    actors.setEffect(rdTriangleId);
                }
                IEffect* effect = renderer->getEffectInstance(rdTriangleId);
                if (!effect) {
                    Serial.println("RD Triangle not available");
                    break;
                }
                float f = effect->getParameter("F");
                if (f <= 0.0f) f = 0.0380f;
                if (cmd == 't') f -= 0.0010f;
                else f += 0.0010f;
                if (f < 0.0300f) f = 0.0300f;
                if (f > 0.0500f) f = 0.0500f;
                effect->setParameter("F", f);
                Serial.printf("RD Triangle F: %.4f\n", f);
            }
            break;

        case '!':
#if FEATURE_TRANSITIONS
            // List transition types
            Serial.println("\n=== Transition Types ===");
            for (uint8_t i = 0; i < static_cast<uint8_t>(TransitionType::TYPE_COUNT); i++) {
                Serial.printf("  %2d: %s (%dms)\n", i,
                              getTransitionName(static_cast<TransitionType>(i)),
                              getDefaultDuration(static_cast<TransitionType>(i)));
            }
            Serial.println();
#else
            Serial.println("Transitions disabled (FH4 build)");
#endif
            break;

        case 'A':
            // Toggle auto-play (narrative) mode
            if (NARRATIVE.isEnabled()) {
                NARRATIVE.disable();
                Serial.println("Auto-play: DISABLED");
            } else {
                NARRATIVE.enable();
                Serial.println("Auto-play: ENABLED (4s cycle)");
            }
            break;

        case '@':
            // Print narrative status
            NARRATIVE.printStatus();
            break;

        // ========== EdgeMixer Commands ==========

        case 'e':
            // Cycle EdgeMixer mode: mirror -> analogous -> complementary -> split_comp -> sat_veil -> mirror
            {
                auto& mixer = lightwaveos::enhancement::EdgeMixer::getInstance();
                uint8_t next = (static_cast<uint8_t>(mixer.getMode()) + 1) % 5;
                actors.setEdgeMixerMode(next);
                Serial.printf("EdgeMixer mode: " LW_CLR_CYAN "%s" LW_ANSI_RESET "\n",
                              lightwaveos::enhancement::EdgeMixer::modeName(
                                  static_cast<lightwaveos::enhancement::EdgeMixerMode>(next)));
            }
            break;

        case 'w':
            // EdgeMixer spread +
            {
                auto& mixer = lightwaveos::enhancement::EdgeMixer::getInstance();
                uint8_t spread = mixer.getSpread();
                spread = (spread + 5 > 60) ? 60 : spread + 5;
                actors.setEdgeMixerSpread(spread);
                Serial.printf("EdgeMixer spread: %d\n", spread);
            }
            break;

        case 'W':
            // EdgeMixer spread -
            {
                auto& mixer = lightwaveos::enhancement::EdgeMixer::getInstance();
                uint8_t spread = mixer.getSpread();
                spread = (spread < 5) ? 0 : spread - 5;
                actors.setEdgeMixerSpread(spread);
                Serial.printf("EdgeMixer spread: %d\n", spread);
            }
            break;

        case '<':
            // EdgeMixer strength -
            {
                auto& mixer = lightwaveos::enhancement::EdgeMixer::getInstance();
                uint8_t strength = mixer.getStrength();
                strength = (strength < 15) ? 0 : strength - 15;
                actors.setEdgeMixerStrength(strength);
                Serial.printf("EdgeMixer strength: %d\n", strength);
            }
            break;

        case '>':
            // EdgeMixer strength +
            {
                auto& mixer = lightwaveos::enhancement::EdgeMixer::getInstance();
                uint8_t strength = mixer.getStrength();
                strength = (strength + 15 > 255) ? 255 : strength + 15;
                actors.setEdgeMixerStrength(strength);
                Serial.printf("EdgeMixer strength: %d\n", strength);
            }
            break;

        case 'y':
            // Toggle EdgeMixer spatial: uniform <-> centre_gradient
            {
                auto& mixer = lightwaveos::enhancement::EdgeMixer::getInstance();
                uint8_t next = (static_cast<uint8_t>(mixer.getSpatial()) == 0) ? 1 : 0;
                actors.setEdgeMixerSpatial(next);
                Serial.printf("EdgeMixer spatial: " LW_CLR_CYAN "%s" LW_ANSI_RESET "\n",
                              lightwaveos::enhancement::EdgeMixer::spatialName(
                                  static_cast<lightwaveos::enhancement::EdgeMixerSpatial>(next)));
            }
            break;

        case 'Y':
            // Toggle EdgeMixer temporal: static <-> rms_gate
            {
                auto& mixer = lightwaveos::enhancement::EdgeMixer::getInstance();
                uint8_t next = (static_cast<uint8_t>(mixer.getTemporal()) == 0) ? 1 : 0;
                actors.setEdgeMixerTemporal(next);
                Serial.printf("EdgeMixer temporal: " LW_CLR_CYAN "%s" LW_ANSI_RESET "\n",
                              lightwaveos::enhancement::EdgeMixer::temporalName(
                                  static_cast<lightwaveos::enhancement::EdgeMixerTemporal>(next)));
            }
            break;

        case '}':
            // Save EdgeMixer to NVS
            {
                actors.saveEdgeMixerToNVS();
                Serial.println("EdgeMixer: " LW_CLR_GREEN "saved to NVS" LW_ANSI_RESET);
            }
            break;

        case '#':
            // Print EdgeMixer status
            {
                auto& mixer = lightwaveos::enhancement::EdgeMixer::getInstance();
                Serial.println("\n=== EdgeMixer Status ===");
                Serial.printf("  Mode:     %s\n", mixer.modeName());
                Serial.printf("  Spread:   %d\n", mixer.getSpread());
                Serial.printf("  Strength: %d\n", mixer.getStrength());
                Serial.printf("  Spatial:  %s\n", mixer.spatialName());
                Serial.printf("  Temporal: %s\n", mixer.temporalName());
                Serial.println();
            }
            break;

        // ========== Colour Correction Keystrokes ==========

        case 'c':
            // Cycle colour correction mode (OFF -> HSV -> RGB -> BOTH -> OFF)
            {
                auto& engine = lightwaveos::enhancement::ColorCorrectionEngine::getInstance();
                auto currentMode = engine.getMode();
                uint8_t modeInt = (uint8_t)currentMode;
                modeInt = (modeInt + 1) % 4;  // Cycle: 0->1->2->3->0
                engine.setMode((lightwaveos::enhancement::CorrectionMode)modeInt);
                const char* modeNames[] = {"OFF", "HSV", "RGB", "BOTH"};
                Serial.printf("Color correction mode: %d (%s)\n", modeInt, modeNames[modeInt]);
            }
            break;

        case 'C':
            // Show colour correction status
            {
                auto& engine = lightwaveos::enhancement::ColorCorrectionEngine::getInstance();
                auto mode = engine.getMode();
                auto& cfg = engine.getConfig();
                const char* modeNames[] = {"OFF", "HSV", "RGB", "BOTH"};
                Serial.println("\n=== Color Correction Status ===");
                Serial.printf("  Mode: %d (%s)\n", (int)mode, modeNames[(int)mode]);
                Serial.printf("  Auto-exposure: %s, target=%d\n",
                              cfg.autoExposureEnabled ? "ON" : "OFF",
                              cfg.autoExposureTarget);
                Serial.printf("  Gamma: %s, value=%.1f\n",
                              cfg.gammaEnabled ? "ON" : "OFF",
                              cfg.gammaValue);
                Serial.printf("  Brown guardrail: %s\n",
                              cfg.brownGuardrailEnabled ? "ON" : "OFF");
                Serial.println();
            }
            break;

        case 'E':
            // Toggle auto-exposure
            {
                auto& engine = lightwaveos::enhancement::ColorCorrectionEngine::getInstance();
                auto& cfg = engine.getConfig();
                cfg.autoExposureEnabled = !cfg.autoExposureEnabled;
                Serial.printf("Auto-exposure: %s\n", cfg.autoExposureEnabled ? "ON" : "OFF");
            }
            break;

        case 'g':
            // Toggle gamma or cycle common values
            {
                auto& engine = lightwaveos::enhancement::ColorCorrectionEngine::getInstance();
                auto& cfg = engine.getConfig();
                if (!cfg.gammaEnabled) {
                    // Enable with default 2.2
                    cfg.gammaEnabled = true;
                    cfg.gammaValue = 2.2f;
                    Serial.printf("Gamma: ON (%.1f)\n", cfg.gammaValue);
                } else {
                    // Cycle through common values: 2.2 -> 2.5 -> 2.8 -> off
                    if (cfg.gammaValue < 2.3f) {
                        cfg.gammaValue = 2.5f;
                        Serial.printf("Gamma: %.1f\n", cfg.gammaValue);
                    } else if (cfg.gammaValue < 2.6f) {
                        cfg.gammaValue = 2.8f;
                        Serial.printf("Gamma: %.1f\n", cfg.gammaValue);
                    } else {
                        cfg.gammaEnabled = false;
                        Serial.println("Gamma: OFF");
                    }
                }
            }
            break;

        case 'q':
            // Toggle cinema post-processing (A/B visual comparison)
            {
                namespace cine = lightwaveos::effects::cinema;
                cine::setEnabled(!cine::isEnabled());
                Serial.printf("Cinema post: %s\n", cine::isEnabled() ? "ON" : "OFF");
            }
            break;

        case 'b':
        case 'B':
            // RD Triangle: K - / +
            {
                EffectId rdTriangleId = lightwaveos::INVALID_EFFECT_ID;
                uint16_t effectCount = renderer->getEffectCount();
                for (uint16_t i = 0; i < effectCount; i++) {
                    EffectId eid = renderer->getEffectIdAt(i);
                    const char* name = renderer->getEffectName(eid);
                    if (name && strcmp(name, "LGP RD Triangle") == 0) {
                        rdTriangleId = eid;
                        break;
                    }
                }
                if (rdTriangleId == lightwaveos::INVALID_EFFECT_ID) {
                    Serial.println("RD Triangle not found");
                    break;
                }
                if (renderer->getCurrentEffect() != rdTriangleId) {
                    m_currentEffect = rdTriangleId;
                    actors.setEffect(rdTriangleId);
                }
                IEffect* effect = renderer->getEffectInstance(rdTriangleId);
                if (!effect) {
                    Serial.println("RD Triangle not available");
                    break;
                }
                float k = effect->getParameter("K");
                if (k <= 0.0f) k = 0.0630f;
                if (cmd == 'b') k -= 0.0010f;
                else k += 0.0010f;
                if (k < 0.0550f) k = 0.0550f;
                if (k > 0.0750f) k = 0.0750f;
                effect->setParameter("K", k);
                Serial.printf("RD Triangle K: %.4f\n", k);
            }
            break;

        case 'I':
            // Mood -
            {
                uint8_t m = renderer->getMood();
                m = static_cast<uint8_t>(max(static_cast<int>(m) - 16, 0));
                actors.setMood(m);
                Serial.printf("Mood: %d (%.0f%%)\n", m, m * 100.0f / 255.0f);
            }
            break;

        case 'f':
            // Alpha (persistence) +
            {
                using BP = lightwaveos::effects::ieffect::BloomParityEffect;
                BP::setAlpha(BP::getAlpha() + 0.01f);
                Serial.printf("Bloom Alpha: %.3f\n", BP::getAlpha());
            }
            break;

        case 'F':
            // Alpha (persistence) -
            {
                using BP = lightwaveos::effects::ieffect::BloomParityEffect;
                BP::setAlpha(BP::getAlpha() - 0.01f);
                Serial.printf("Bloom Alpha: %.3f\n", BP::getAlpha());
            }
            break;

        case 'h':
            // Square iterations (contrast) +
            {
                using BP = lightwaveos::effects::ieffect::BloomParityEffect;
                BP::setSquareIter(BP::getSquareIter() + 1);
                Serial.printf("Bloom Square Iter: %d\n", BP::getSquareIter());
            }
            break;

        case 'H':
            // Square iterations (contrast) -
            {
                using BP = lightwaveos::effects::ieffect::BloomParityEffect;
                uint8_t si = BP::getSquareIter();
                BP::setSquareIter(si > 0 ? si - 1 : 0);
                Serial.printf("Bloom Square Iter: %d\n", BP::getSquareIter());
            }
            break;

        case 'j':
            // Prism iterations +
            {
                using BP = lightwaveos::effects::ieffect::BloomParityEffect;
                BP::setPrismIterations(BP::getPrismIterations() + 1);
                Serial.printf("Bloom Prism Iter: %d\n", BP::getPrismIterations());
            }
            break;

        case 'J':
            // Prism iterations -
            {
                using BP = lightwaveos::effects::ieffect::BloomParityEffect;
                uint8_t pi = BP::getPrismIterations();
                BP::setPrismIterations(pi > 0 ? pi - 1 : 0);
                Serial.printf("Bloom Prism Iter: %d\n", BP::getPrismIterations());
            }
            break;

        case 'k':
            // gHue speed +
            {
                using BP = lightwaveos::effects::ieffect::BloomParityEffect;
                BP::setGHueSpeed(BP::getGHueSpeed() + 0.25f);
                Serial.printf("Bloom gHue Speed: %.2f\n", BP::getGHueSpeed());
            }
            break;

        case 'K':
            // gHue speed -
            {
                using BP = lightwaveos::effects::ieffect::BloomParityEffect;
                BP::setGHueSpeed(BP::getGHueSpeed() - 0.25f);
                Serial.printf("Bloom gHue Speed: %.2f\n", BP::getGHueSpeed());
            }
            break;

        case 'u':
            // Spatial spread +
            {
                using BP = lightwaveos::effects::ieffect::BloomParityEffect;
                BP::setSpatialSpread(BP::getSpatialSpread() + 16.0f);
                Serial.printf("Bloom Spatial Spread: %.0f\n", BP::getSpatialSpread());
            }
            break;

        case 'U':
            // Spatial spread -
            {
                using BP = lightwaveos::effects::ieffect::BloomParityEffect;
                float ss = BP::getSpatialSpread();
                BP::setSpatialSpread(ss > 16.0f ? ss - 16.0f : 0.0f);
                Serial.printf("Bloom Spatial Spread: %.0f\n", BP::getSpatialSpread());
            }
            break;

        case 'v':
            // Intensity coupling +
            {
                using BP = lightwaveos::effects::ieffect::BloomParityEffect;
                BP::setIntensityCoupling(BP::getIntensityCoupling() + 0.1f);
                Serial.printf("Bloom Intensity Coupling: %.1f\n", BP::getIntensityCoupling());
            }
            break;

        case 'V':
            // Intensity coupling -
            {
                using BP = lightwaveos::effects::ieffect::BloomParityEffect;
                float ic = BP::getIntensityCoupling();
                BP::setIntensityCoupling(ic > 0.1f ? ic - 0.1f : 0.0f);
                Serial.printf("Bloom Intensity Coupling: %.1f\n", BP::getIntensityCoupling());
            }
            break;
        }
    }
}

} // namespace serial
} // namespace lightwaveos
