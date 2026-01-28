#!/bin/bash
# =============================================================================
# Commit Strategy Execution Script
# Branch: feature/emotiscope-tempo-replacement
# Generated: 2026-01-01
# =============================================================================
set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Helper functions
info() { echo -e "${CYAN}[INFO]${NC} $1"; }
success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
error() { echo -e "${RED}[ERROR]${NC} $1"; }

checkpoint() {
    echo ""
    echo -e "${YELLOW}═══════════════════════════════════════════════════════════════${NC}"
    echo -e "${YELLOW}  CHECKPOINT: $1${NC}"
    echo -e "${YELLOW}═══════════════════════════════════════════════════════════════${NC}"
    read -p "Press Enter to continue or Ctrl+C to abort..."
    echo ""
}

# =============================================================================
# COMMIT 1: Configuration and Feature Flags
# =============================================================================
echo ""
echo -e "${GREEN}╔═══════════════════════════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║  COMMIT 1/10: Configuration and Feature Flags                 ║${NC}"
echo -e "${GREEN}╚═══════════════════════════════════════════════════════════════╝${NC}"
echo ""

info "Staging configuration files..."
git add \
    v2/src/config/features.h \
    v2/src/config/audio_config.h \
    v2/platformio.ini \
    platformio.ini

git commit -m "$(cat <<'EOF'
refactor(config): update feature flags for TempoTracker migration

Remove K1 beat tracker feature flags and prepare build system for
TempoTracker replacement.

Changes:
- Remove FEATURE_K1_BEAT_TRACKER and related K1 flags
- Add FEATURE_STYLE_DETECTION (defaults to FEATURE_AUDIO_SYNC)
- Update audio_config.h with TempoTracker timing constants
- Add runtime protections: stack overflow check, heap corruption detection
- Enable FEATURE_HEAP_MONITORING and FEATURE_VALIDATION_PROFILING

Technical details:
- K1 pipeline will be replaced by simpler Goertzel-based TempoTracker
- Feature flags prepared for gradual migration
- Build flags added: -D configCHECK_FOR_STACK_OVERFLOW=2

🤖 Generated with [Claude Code](https://claude.ai/claude-code)

Co-Authored-By: Claude Opus 4.5 <noreply@anthropic.com>
EOF
)"

success "Commit 1 complete: Configuration and Feature Flags"
checkpoint "Verify compile: pio run -e esp32dev_audio"

# =============================================================================
# COMMIT 2: Add TempoTracker Implementation
# =============================================================================
echo ""
echo -e "${GREEN}╔═══════════════════════════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║  COMMIT 2/10: Add TempoTracker Implementation                 ║${NC}"
echo -e "${GREEN}╚═══════════════════════════════════════════════════════════════╝${NC}"
echo ""

info "Staging TempoTracker files..."
git add \
    v2/src/audio/tempo/TempoTracker.h \
    v2/src/audio/tempo/TempoTracker.cpp

git commit -m "$(cat <<'EOF'
feat(audio): add TempoTracker Goertzel-based beat detection

Implement TempoTracker as a lightweight replacement for K1 beat detection.
Uses Goertzel resonators for tempo tracking with ~40% less code and ~35%
less memory than K1.

Architecture:
- 96 bins covering 60-156 BPM at 1 BPM resolution
- Dual-rate novelty: spectral flux (31.25 Hz) + VU derivative (62.5 Hz)
- Adaptive block sizes per-bin for optimal frequency resolution
- Hysteresis winner selection (10% advantage for 5 frames)
- Window-free Goertzel (novelty already smoothed)
- Novelty decay multiplier (0.999/frame) for silent bin suppression

Memory: ~22 KB total (vs K1's ~35 KB)

API:
- updateNovelty(bands, num_bands, rms, bands_ready) - per audio hop
- updateTempo(delta_sec) - interleaved computation
- advancePhase(delta_sec) - called at 120 FPS render rate
- getOutput() - returns TempoOutput struct

🤖 Generated with [Claude Code](https://claude.ai/claude-code)

Co-Authored-By: Claude Opus 4.5 <noreply@anthropic.com>
EOF
)"

success "Commit 2 complete: TempoTracker Implementation"
checkpoint "Verify compile: pio run -e esp32dev_audio"

# =============================================================================
# COMMIT 3: Integrate TempoTracker into Audio Pipeline
# =============================================================================
echo ""
echo -e "${GREEN}╔═══════════════════════════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║  COMMIT 3/10: Integrate TempoTracker into Audio Pipeline      ║${NC}"
echo -e "${GREEN}╚═══════════════════════════════════════════════════════════════╝${NC}"
echo ""

info "Staging audio integration files..."
git add \
    v2/src/audio/AudioActor.cpp \
    v2/src/audio/AudioActor.h \
    v2/src/audio/AudioCapture.cpp \
    v2/src/audio/AudioCapture.h \
    v2/src/audio/GoertzelAnalyzer.cpp \
    v2/src/audio/contracts/ControlBus.cpp \
    v2/src/audio/contracts/ControlBus.h \
    v2/src/audio/contracts/SnapshotBuffer.h \
    v2/src/audio/contracts/AudioEffectMapping.h

[ -f "v2/src/audio/AudioMappingRegistry.h" ] && git add v2/src/audio/AudioMappingRegistry.h

git commit -m "$(cat <<'EOF'
refactor(audio): integrate TempoTracker into AudioActor pipeline

Wire TempoTracker into AudioActor, replacing K1 integration points.

AudioActor changes:
- Add TempoTracker member (m_tempo) replacing m_k1Pipeline
- Call updateNovelty() and updateTempo() per audio hop
- Expose getTempo()/getTempoMut() for cross-actor access
- Migrate PERCEPTUAL_BAND_WEIGHTS from K1Config.h
- Move large buffers from stack to class members (stack reduction)
- Add stack high water mark monitoring

Defensive bounds checking:
- GoertzelAnalyzer: Clamp count, validate indices
- AudioCapture: Clamp oversized I2S reads

🤖 Generated with [Claude Code](https://claude.ai/claude-code)

Co-Authored-By: Claude Opus 4.5 <noreply@anthropic.com>
EOF
)"

success "Commit 3 complete: Audio Pipeline Integration"
checkpoint "Verify compile: pio run -e esp32dev_audio"

# =============================================================================
# COMMIT 4: Delete K1 Beat Tracker
# =============================================================================
echo ""
echo -e "${GREEN}╔═══════════════════════════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║  COMMIT 4/10: Delete K1 Beat Tracker                          ║${NC}"
echo -e "${GREEN}╚═══════════════════════════════════════════════════════════════╝${NC}"
echo ""

info "Staging K1 deletions..."
git add \
    v2/src/audio/k1/K1BeatClock.cpp \
    v2/src/audio/k1/K1BeatClock.h \
    v2/src/audio/k1/K1Config.h \
    v2/src/audio/k1/K1DebugCli.cpp \
    v2/src/audio/k1/K1DebugCli.h \
    v2/src/audio/k1/K1DebugMacros.h \
    v2/src/audio/k1/K1DebugMetrics.h \
    v2/src/audio/k1/K1DebugRing.h \
    v2/src/audio/k1/K1Messages.h \
    v2/src/audio/k1/K1Pipeline.cpp \
    v2/src/audio/k1/K1Pipeline.h \
    v2/src/audio/k1/K1ResonatorBank.cpp \
    v2/src/audio/k1/K1ResonatorBank.h \
    v2/src/audio/k1/K1TactusResolver.cpp \
    v2/src/audio/k1/K1TactusResolver.h \
    v2/src/audio/k1/K1Types.h \
    v2/src/audio/k1/K1Utils.h \
    v2/test/test_audio/test_pipeline_benchmark.cpp

git commit -m "$(cat <<'EOF'
refactor(audio): remove obsolete K1 beat tracker

Delete K1 beat detection system (17 files, ~2,500 lines), replaced by
TempoTracker with 40% less code and 35% less memory.

Removed: K1Pipeline, K1ResonatorBank, K1TactusResolver, K1BeatClock,
K1DebugCli, K1DebugMacros/Metrics/Ring, K1Config, K1Types/Utils/Messages

Why K1 was replaced:
- Tempo mis-lock at 123-129 BPM instead of 138 BPM on reference tracks
- Overly complex 4-stage architecture
- TactusResolver biased toward 129.2 BPM
- Confidence always 1.0 (semantic bug)

🤖 Generated with [Claude Code](https://claude.ai/claude-code)

Co-Authored-By: Claude Opus 4.5 <noreply@anthropic.com>
EOF
)"

success "Commit 4 complete: K1 Deletion"
checkpoint "Verify clean compile: pio run -e esp32dev_audio"

# =============================================================================
# COMMIT 5: Wire TempoTracker to RendererActor
# =============================================================================
echo ""
echo -e "${GREEN}╔═══════════════════════════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║  COMMIT 5/10: Wire TempoTracker to RendererActor              ║${NC}"
echo -e "${GREEN}╚═══════════════════════════════════════════════════════════════╝${NC}"
echo ""

info "Staging core actor files..."
git add \
    v2/src/core/actors/RendererActor.cpp \
    v2/src/core/actors/RendererActor.h \
    v2/src/core/actors/ActorSystem.cpp \
    v2/src/core/actors/ActorSystem.h \
    v2/src/core/actors/Actor.h \
    v2/src/core/actors/ShowDirectorActor.cpp \
    v2/src/core/state/StateStore.cpp \
    v2/src/core/state/StateStore.h \
    v2/src/core/state/SystemState.h

[ -d "v2/src/core/system" ] && git add v2/src/core/system/

git commit -m "$(cat <<'EOF'
feat(core): complete TempoTracker integration in RendererActor

Wire TempoTracker into render loop for 120 FPS phase advancement.

RendererActor:
- Add setTempo() for cross-actor pointer handoff
- Call m_tempo->advancePhase() in onTick()
- Update effect context with tempo output
- Add validateEffectId() defensive check

Phase in render domain (not audio):
- Audio thread: 62.5 Hz - causes visible stepping
- Render thread: 120 FPS - smooth interpolation

ActorSystem:
- Connect AudioActor.getTempoMut() to RendererActor.setTempo()
- Add public startTransition() API

Effects access tempo via ctx.audio.bpm(), phase01(), beatTick()

🤖 Generated with [Claude Code](https://claude.ai/claude-code)

Co-Authored-By: Claude Opus 4.5 <noreply@anthropic.com>
EOF
)"

success "Commit 5 complete: RendererActor Integration"
checkpoint "UPLOAD: pio run -e esp32dev_audio -t upload - verify tempo in serial"

# =============================================================================
# COMMIT 6: Effects and Zone System Improvements
# =============================================================================
echo ""
echo -e "${GREEN}╔═══════════════════════════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║  COMMIT 6/10: Effects and Zone System Improvements            ║${NC}"
echo -e "${GREEN}╚═══════════════════════════════════════════════════════════════╝${NC}"
echo ""

info "Staging effect and zone files..."
git add \
    v2/src/effects/zones/ZoneComposer.cpp \
    v2/src/effects/zones/ZoneComposer.h \
    v2/src/core/persistence/ZoneConfigManager.cpp \
    v2/src/core/persistence/ZoneConfigManager.h \
    v2/src/effects/CoreEffects.cpp \
    v2/src/effects/CoreEffects.h \
    v2/src/effects/PatternRegistry.cpp \
    v2/src/effects/PatternRegistry.h \
    v2/src/effects/enhancement/SmoothingEngine.h \
    v2/src/effects/ieffect/BPMEffect.cpp \
    v2/src/effects/ieffect/BPMEffect.h \
    v2/src/effects/ieffect/LGPPhotonicCrystalEffect.cpp \
    v2/src/effects/ieffect/LGPPhotonicCrystalEffect.h \
    v2/src/palettes/Palettes_Master.h \
    v2/src/effects/ieffect/PerlinNoiseTypes.h \
    v2/src/effects/ieffect/LGPPerlinBackendEmotiscopeFullEffect.cpp \
    v2/src/effects/ieffect/LGPPerlinBackendEmotiscopeFullEffect.h \
    v2/src/effects/ieffect/LGPPerlinBackendFastLEDEffect.cpp \
    v2/src/effects/ieffect/LGPPerlinBackendFastLEDEffect.h \
    v2/src/effects/ieffect/LGPPerlinShocklinesEffect.cpp \
    v2/src/effects/ieffect/LGPPerlinShocklinesEffect.h

git commit -m "$(cat <<'EOF'
feat(effects): update effects for TempoTracker and fix zone buffers

ZONE FLICKERING FIX (CRITICAL):
- Added persistent per-zone buffers: m_zoneBuffers[MAX_ZONES][320]
- Prevents cross-zone contamination and strobing
- Clear buffer on zone enable

BPMEffect rewrite:
- Dual-layer (traveling wave + expanding rings)
- Uses TempoTracker phase01/beat_tick
- Center-origin compliant

New Perlin effects:
- LGPPerlinShocklinesEffect, LGPPerlinBackendEmotiscopeFullEffect
- LGPPerlinBackendFastLEDEffect, PerlinNoiseTypes.h

🤖 Generated with [Claude Code](https://claude.ai/claude-code)

Co-Authored-By: Claude Opus 4.5 <noreply@anthropic.com>
EOF
)"

success "Commit 6 complete: Effects and Zone System"
checkpoint "UPLOAD: pio run -e esp32dev_audio -t upload - test zone effects"

# =============================================================================
# COMMIT 7: WebServer Infrastructure Extraction
# =============================================================================
echo ""
echo -e "${GREEN}╔═══════════════════════════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║  COMMIT 7/10: WebServer Infrastructure Extraction             ║${NC}"
echo -e "${GREEN}╚═══════════════════════════════════════════════════════════════╝${NC}"
echo ""

info "Staging WebServer infrastructure files..."
git add \
    v2/src/network/webserver/WebServerContext.h \
    v2/src/network/webserver/StaticAssetRoutes.cpp \
    v2/src/network/webserver/StaticAssetRoutes.h \
    v2/src/network/webserver/V1ApiRoutes.cpp \
    v2/src/network/webserver/V1ApiRoutes.h \
    v2/src/network/webserver/WsGateway.cpp \
    v2/src/network/webserver/WsGateway.h \
    v2/src/network/webserver/HttpRouteRegistry.h \
    v2/src/network/webserver/WsCommandRouter.cpp \
    v2/src/network/webserver/WsCommandRouter.h \
    v2/src/network/ApiResponse.h \
    v2/src/network/RequestValidator.h

git commit -m "$(cat <<'EOF'
refactor(network): extract WebServer infrastructure into modules

WebServerContext.h: Shared context for dependency injection
StaticAssetRoutes: Serve static files from LittleFS
V1ApiRoutes: RESTful API v1 endpoint registration
WsGateway: WebSocket connection management

Updated: WsCommandRouter, ApiResponse (remove legacy), RequestValidator

🤖 Generated with [Claude Code](https://claude.ai/claude-code)

Co-Authored-By: Claude Opus 4.5 <noreply@anthropic.com>
EOF
)"

success "Commit 7 complete: WebServer Infrastructure"
checkpoint "Verify compile: pio run -e esp32dev_audio"

# =============================================================================
# COMMIT 8: Network Handlers and WiFi Fixes
# =============================================================================
echo ""
echo -e "${GREEN}╔═══════════════════════════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║  COMMIT 8/10: Network Handlers and WiFi Fixes                 ║${NC}"
echo -e "${GREEN}╚═══════════════════════════════════════════════════════════════╝${NC}"
echo ""

info "Staging handlers..."
git add \
    v2/src/network/webserver/handlers/AudioHandlers.cpp \
    v2/src/network/webserver/handlers/AudioHandlers.h \
    v2/src/network/webserver/handlers/BatchHandlers.cpp \
    v2/src/network/webserver/handlers/BatchHandlers.h \
    v2/src/network/webserver/handlers/NarrativeHandlers.cpp \
    v2/src/network/webserver/handlers/NarrativeHandlers.h \
    v2/src/network/webserver/handlers/PaletteHandlers.cpp \
    v2/src/network/webserver/handlers/PaletteHandlers.h \
    v2/src/network/webserver/handlers/ParameterHandlers.cpp \
    v2/src/network/webserver/handlers/ParameterHandlers.h \
    v2/src/network/webserver/handlers/SystemHandlers.cpp \
    v2/src/network/webserver/handlers/SystemHandlers.h \
    v2/src/network/webserver/handlers/TransitionHandlers.cpp \
    v2/src/network/webserver/handlers/TransitionHandlers.h \
    v2/src/network/webserver/handlers/EffectHandlers.cpp \
    v2/src/network/webserver/handlers/EffectHandlers.h \
    v2/src/network/webserver/handlers/ZoneHandlers.cpp \
    v2/src/network/webserver/handlers/ZoneHandlers.h

git add v2/src/network/webserver/ws/

git add \
    v2/src/network/WebServer.cpp \
    v2/src/network/WebServer.h \
    v2/src/network/WiFiManager.cpp \
    v2/src/network/WiFiManager.h \
    v2/src/main.cpp

git commit -m "$(cat <<'EOF'
feat(network): complete handler extraction and fix WiFi EventGroup bug

HTTP Handlers: Audio, Batch, Narrative, Palette, Parameter, System,
Transition, Effect, Zone handlers

WebSocket Commands: 11 handlers in ws/ directory

WebServer.cpp: 6433 -> ~546 lines (91% reduction)

WIFI FIX (CRITICAL):
- Clear EventGroup bits on entering STATE_WIFI_CONNECTING
- Bits: EVENT_CONNECTED | EVENT_GOT_IP | EVENT_CONNECTION_FAILED
- Fixes "Connected! IP: 0.0.0.0" false-positive bug

🤖 Generated with [Claude Code](https://claude.ai/claude-code)

Co-Authored-By: Claude Opus 4.5 <noreply@anthropic.com>
EOF
)"

success "Commit 8 complete: Handlers and WiFi Fix"
checkpoint "UPLOAD: pio run -e esp32dev_audio -t upload - verify WiFi works"

# =============================================================================
# COMMIT 9: UI and Dashboard Updates
# =============================================================================
echo ""
echo -e "${GREEN}╔═══════════════════════════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║  COMMIT 9/10: UI and Dashboard Updates                        ║${NC}"
echo -e "${GREEN}╚═══════════════════════════════════════════════════════════════╝${NC}"
echo ""

info "Staging UI files..."
git add \
    v2/data/app.js \
    v2/data/index.html \
    v2/data/styles.css \
    lightwave-simple/app.js \
    lightwave-simple/index.html \
    lightwave-simple/styles.css \
    lightwave-dashboard/src/components/DashboardShell.tsx \
    lightwave-dashboard/src/services/v2/api.ts \
    lightwave-dashboard/src/services/v2/types.ts \
    lightwave-dashboard/src/state/v2.tsx \
    lightwave-dashboard/src/components/ZoneEditor.tsx

git commit -m "$(cat <<'EOF'
feat(ui): add zone editor and update dashboard for API changes

v2/data/: Zone editor, palette filtering, WebSocket envelope handling
lightwave-simple/: Match zone functionality
lightwave-dashboard/: ZoneEditor.tsx, updated API types and state

Zone editor: Per-zone effect/brightness/speed, blend modes, presets

🤖 Generated with [Claude Code](https://claude.ai/claude-code)

Co-Authored-By: Claude Opus 4.5 <noreply@anthropic.com>
EOF
)"

success "Commit 9 complete: UI and Dashboard"
checkpoint "BROWSER: Open http://lightwaveos.local - test zone editor"

# =============================================================================
# COMMIT 10: Documentation and Tests
# =============================================================================
echo ""
echo -e "${GREEN}╔═══════════════════════════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║  COMMIT 10/10: Documentation and Tests                        ║${NC}"
echo -e "${GREEN}╚═══════════════════════════════════════════════════════════════╝${NC}"
echo ""

info "Staging documentation..."
git add \
    ARCHITECTURE.md \
    CLAUDE.md \
    docs/api/API.md \
    docs/api/API_V1.md \
    docs/api/API_V2.md \
    docs/architecture/06_ERROR_HANDLING_FAULT_TOLERANCE.md \
    docs/security/ARCHITECTURE_SECURITY_REVIEW.md \
    v2/docs/API_PARITY_ANALYSIS.md \
    docs/guides/ZONE_EDITOR_USER_GUIDE.md \
    v2/docs/EMOTISCOPE_2.0_ARCHITECTURE_REVIEW.md \
    v2/docs/EMOTISCOPE_2.0_PARITY_ANALYSIS.md \
    v2/docs/EMOTISCOPE_REFERENCES_AUDIT.md \
    v2/docs/EMOTISCOPE_TEMPO_COMPREHENSIVE_AUDIT.md \
    v2/docs/WIFI_AUDIT_FINDINGS.md \
    v2/docs/WIFI_FIX_PROPOSAL.md

[ -d "v2/docs/audio-visual" ] && git add v2/docs/audio-visual/
[ -d "v2/docs/architecture" ] && git add v2/docs/architecture/
[ -d "v2/docs/implementation" ] && git add v2/docs/implementation/
[ -d "v2/docs/migration" ] && git add v2/docs/migration/
[ -d "v2/docs/performance" ] && git add v2/docs/performance/

info "Staging tests..."
git add v2/test/test_audio/test_goertzel_basic.cpp
[ -d "v2/test/baseline" ] && git add v2/test/baseline/
[ -d "v2/test/test_native" ] && git add v2/test/test_native/
[ -d "v2/test/test_audio_benchmark" ] && git add v2/test/test_audio_benchmark/
[ -d "v2/tools" ] && git add v2/tools/

git add .gitignore

git commit -m "$(cat <<'EOF'
docs(all): comprehensive documentation for TempoTracker and WebServer refactor

Audio/Visual docs, Architecture docs, API updates, User guides
Tests: Goertzel, WebServer routes, WS gateway, benchmarks
Updated: ARCHITECTURE.md, CLAUDE.md, .gitignore

🤖 Generated with [Claude Code](https://claude.ai/claude-code)

Co-Authored-By: Claude Opus 4.5 <noreply@anthropic.com>
EOF
)"

success "Commit 10 complete: Documentation and Tests"

# =============================================================================
# FINAL SUMMARY
# =============================================================================
echo ""
echo -e "${GREEN}╔═══════════════════════════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║  ALL 10 COMMITS COMPLETE!                                     ║${NC}"
echo -e "${GREEN}╚═══════════════════════════════════════════════════════════════╝${NC}"
echo ""

info "Commit history:"
git log --oneline -10

echo ""
success "Ready to push and create PR:"
echo "  git push origin feature/emotiscope-tempo-replacement"
echo ""
