# Documentation Index

**LightwaveOS** - ESP32-S3 LED Control System for Dual 160-LED WS2812 Light Guide Plate

---

## Quick Links

| I want to... | Go to... |
|--------------|----------|
| Build and upload firmware | [README.md](../README.md) |
| Understand project rules for Claude | [CLAUDE.md](../CLAUDE.md) **MANDATORY** |
| Create a new visual effect | [pattern-development-guide.md](guides/pattern-development-guide.md) |
| Add audio-reactive features | [audio-visual-semantic-mapping.md](audio-visual/AUDIO_VISUAL_SEMANTIC_MAPPING.md) **MANDATORY** |
| Use the REST/WebSocket API | [api-v1.md](api/API_V1.md) |
| Design a light show narrative | [storytelling-framework.md](guides/storytelling-framework.md) |
| Understand system architecture | [00-lightwaveos-infrastructure-comprehensive.md](architecture/00_LIGHTWAVEOS_INFRASTRUCTURE_COMPREHENSIVE.md) |
| Prepare efficient LLM prompts | [LLM_CONTEXT.md](LLM_CONTEXT.md) + [contextpack/README.md](contextpack/README.md) |
| Set up CI/CD | [ci-cd/README.md](ci_cd/README.md) |
| Find archived/legacy docs | [archive/consolidation-manifest.md](archive/consolidation-manifest.md) |

---

## Core Documentation

| Document | Description | Status |
|----------|-------------|--------|
| [README.md](../README.md) | Project overview, quick start, build commands | Current |
| [CLAUDE.md](../CLAUDE.md) | **MANDATORY** - Claude Code project instructions, constraints, harness protocol | Current |
| [ARCHITECTURE.md](../ARCHITECTURE.md) | Extracted system facts for quick agent reference | Current |
| [CONSTRAINTS.md](../CONSTRAINTS.md) | Hard limits: timing, memory, power | Current |

---

## LLM / Prompting

Resources for efficient LLM interactions and delta-only prompting:

| Document | Description | Status |
|----------|-------------|--------|
| [LLM_CONTEXT.md](LLM_CONTEXT.md) | Stable LLM prefix file - include instead of re-explaining the project | Current |
| [contextpack/README.md](contextpack/README.md) | Context Pack pipeline - delta-only prompting discipline | Current |
| [contextpack/packet.md](contextpack/packet.md) | Prompt packet template (goal, symptom, acceptance checks) | Current |
| [contextpack/fixtures/README.md](contextpack/fixtures/README.md) | Fixture format selection (TOON/CSV/JSON) | Current |
| [architecture/TOON_FORMAT_EVALUATION.md](architecture/TOON_FORMAT_EVALUATION.md) | TOON decision document - prompt codec, not wire format | Current |

**Generator script**: `tools/contextpack.py` - generates prompt bundles with diff, logs, and fixtures.

---

## API Reference

| Document | Description | Version |
|----------|-------------|---------|
| [api-v1.md](api/API_V1.md) | REST + WebSocket API v1 (recommended) | 1.0 |
| [api-v2.md](api/API_V2.md) | API v2 specification | Draft |
| [api.md](api/API.md) | Legacy API documentation | Legacy |
| [enhancement-engine-api.md](api/ENHANCEMENT_ENGINE_API.md) | Enhancement engine endpoints | Current |

---

## Architecture Series

Numbered architecture documents covering all system aspects:

| # | Document | Description |
|---|----------|-------------|
| 00 | [lightwaveos-infrastructure-comprehensive.md](architecture/00_LIGHTWAVEOS_INFRASTRUCTURE_COMPREHENSIVE.md) | Complete infrastructure overview |
| 01 | [preset-management-system.md](architecture/01_PRESET_MANAGEMENT_SYSTEM.md) | Preset save/load system |
| 02 | [memory-debugging-heap-tracer.md](architecture/02_MEMORY_DEBUGGING_HEAP_TRACER.md) | Memory debugging tools |
| 03 | [serial-menu-interface.md](architecture/03_SERIAL_MENU_INTERFACE.md) | Serial command interface |
| 04 | [dual-i2c-architecture.md](architecture/04_DUAL_I2C_ARCHITECTURE.md) | I2C bus architecture |
| 05 | [sync-propagation-modes.md](architecture/05_SYNC_PROPAGATION_MODES.md) | Sync mode documentation |
| 07 | [security-architecture.md](architecture/07_SECURITY_ARCHITECTURE.md) | Security considerations |
| 08 | [testing-debugging-strategies.md](architecture/08_TESTING_DEBUGGING_STRATEGIES.md) | Testing approaches |
| 09 | [configuration-settings-persistence.md](architecture/09_CONFIGURATION_SETTINGS_PERSISTENCE.md) | NVS persistence |
| 10 | [power-management-thermal-control.md](architecture/10_POWER_MANAGEMENT_THERMAL_CONTROL.md) | Power and thermal management |

Additional architecture documents:

| Document | Description |
|----------|-------------|
| [genesis-audio-sync-integration-plan.md](architecture/GENESIS_AUDIO_SYNC_INTEGRATION_PLAN.md) | Audio sync integration plans |
| [light-show-mechanics-comprehensive.md](architecture/LIGHT_SHOW_MECHANICS_COMPREHENSIVE.md) | Light show mechanics |
| [project-status.md](architecture/PROJECT_STATUS.md) | Current project status |

---

## Guides

### Pattern Development (Canonical)

These three guides form the complete reference for creating LGP patterns:

| Document | Description | Status |
|----------|-------------|--------|
| [pattern-development-guide.md](guides/pattern-development-guide.md) | LGP physics, design levers, pattern taxonomy (center origin, no rainbows) | **Canonical** |
| [storytelling-framework.md](guides/storytelling-framework.md) | Narrative construction: motifs, pacing, scene cards | **Canonical** |
| [implementation-playbook.md](guides/implementation-playbook.md) | Engineering specs, testing, safety constraints | **Canonical** |

### Other Guides

| Document | Description |
|----------|-------------|
| [zone-editor-user-guide.md](guides/ZONE_EDITOR_USER_GUIDE.md) | Zone editor web interface |
| [k1-lightwave.md](guides/K1-Lightwave.md) | K1 Lightwave hardware guide |
| [light-crystals-migration.md](guides/LIGHT_CRYSTALS_MIGRATION.md) | Light Crystals migration |

---

## Audio-Visual

**MANDATORY:** Read [audio-visual-semantic-mapping.md](audio-visual/AUDIO_VISUAL_SEMANTIC_MAPPING.md) before adding ANY audio-reactive features.

| Document | Description | Status |
|----------|-------------|--------|
| [audio-visual-semantic-mapping.md](audio-visual/AUDIO_VISUAL_SEMANTIC_MAPPING.md) | **MANDATORY** - Musical intelligence, layer audit protocol, style-adaptive response | Current |
| [audio-reactive-effects-analysis.md](audio-visual/AUDIO_REACTIVE_EFFECTS_ANALYSIS.md) | Analysis of audio-reactive effect patterns | Current |
| [audio-bloom-implementation.md](audio-visual/audio-bloom-implementation.md) | Audio bloom effect implementation details | Current |

---

## Effects & Patterns

| Document | Description |
|----------|-------------|
| [effects.md](effects/EFFECTS.md) | Effect catalog and descriptions |
| [enhanced-effects-guide.md](effects/ENHANCED_EFFECTS_GUIDE.md) | Enhanced effects usage guide |
| [light-show-mechanics-comprehensive.md](effects/LIGHT_SHOW_MECHANICS_COMPREHENSIVE.md) | Light show mechanics |
| [dual-strip-wave-engine-implementation.md](effects/DUAL_STRIP_WAVE_ENGINE_IMPLEMENTATION.md) | Wave engine for dual strips |
| [wave-engine-implementation-summary.md](effects/WAVE_ENGINE_IMPLEMENTATION_SUMMARY.md) | Wave engine summary |

### LGP-Specific Effects

| Document | Description |
|----------|-------------|
| [lgp-metamaterial-cloak.md](effects/LGP_Metamaterial_Cloak.md) | Metamaterial cloak effect |
| [lgp-pulse-wave.md](effects/LGP_PULSE_WAVE.md) | Pulse wave effect |
| [lgp-quantum-resonance.md](effects/LGP_QUANTUM_RESONANCE.md) | Quantum resonance effect |
| [lgp-chromatic-aberration-analysis.md](effects/LGP_CHROMATIC_ABERRATION_ANALYSIS.md) | Chromatic aberration analysis |

---

## Analysis & Research

### Audio Analysis

| Document | Description |
|----------|-------------|
| [audio-reactive-analysis.md](analysis/audio-reactive-analysis.md) | Consolidated audio-reactive analysis |
| [dual-rate-audio-pipeline-technical-brief.md](analysis/DUAL_RATE_AUDIO_PIPELINE_TECHNICAL_BRIEF.md) | Dual-rate pipeline technical brief |
| [emotiscope-beat-tracking-comparative-analysis.md](analysis/Emotiscope_BEAT_TRACKING_COMPARATIVE_ANALYSIS.md) | Emotiscope beat tracking comparison |
| [bpm-effect-audit-report.md](analysis/BPM_EFFECT_AUDIT_REPORT.md) | BPM effect audit |
| [k1-tempo-mislock-analysis.md](analysis/K1_TEMPO_MISLOCK_ANALYSIS.md) | K1 tempo mislock investigation |

### LGP Research

| Document | Description |
|----------|-------------|
| [lgp-optical-physics-reference.md](analysis/lgp-optical-physics-reference.md) | LGP optical physics reference |
| [lgp-pattern-taxonomy.md](analysis/lgp-pattern-taxonomy.md) | LGP pattern taxonomy |
| [research-analysis-report.md](analysis/RESEARCH_ANALYSIS_REPORT.md) | General research report |

### Phased Analysis Series

| Document | Description |
|----------|-------------|
| [01-executive-summary-and-research.md](analysis/01_EXECUTIVE_SUMMARY_AND_RESEARCH.md) | Executive summary |
| [02-code-alignment-analysis.md](analysis/02_CODE_ALIGNMENT_ANALYSIS.md) | Code alignment analysis |
| [03-gap-analysis.md](analysis/03_GAP_ANALYSIS.md) | Gap analysis |
| [04-phased-action-plan.md](analysis/04_PHASED_ACTION_PLAN.md) | Phased action plan |
| [05-success-metrics-and-references.md](analysis/05_SUCCESS_METRICS_AND_REFERENCES.md) | Success metrics |

---

## CI/CD

| Document | Description |
|----------|-------------|
| [README.md](ci_cd/README.md) | CI/CD overview |
| [quick-start.md](ci_cd/QUICK_START.md) | Quick start guide |
| [audio-benchmark-ci.md](ci_cd/AUDIO_BENCHMARK_CI.md) | Audio benchmarking in CI |
| [implementation-summary.md](ci_cd/implementation-summary.md) | Implementation summary |

---

## Hardware

| Document | Description |
|----------|-------------|
| [light-guide-plate.md](hardware/LIGHT_GUIDE_PLATE.md) | LGP hardware design |
| [usb-setup.md](hardware/USB_SETUP.md) | USB connection setup |
| [wifi-antenna-guide.md](hardware/WIFI_ANTENNA_GUIDE.md) | WiFi antenna configuration |

---

## Optimization

| Document | Description |
|----------|-------------|
| [optimization-summary.md](optimization/OPTIMIZATION_SUMMARY.md) | Performance optimization summary |
| [fastled-optimizations-implemented.md](optimization/FASTLED_OPTIMIZATIONS_IMPLEMENTED.md) | FastLED optimizations |
| [heap-trace-analysis.md](optimization/HEAP_TRACE_ANALYSIS.md) | Heap trace analysis |
| [stability-analysis-and-fixes.md](optimization/STABILITY_ANALYSIS_AND_FIXES.md) | Stability analysis |

---

## Security

| Document | Description |
|----------|-------------|
| [security-findings-summary.md](security/SECURITY_FINDINGS_SUMMARY.md) | Security audit findings |
| [architecture-security-review.md](security/ARCHITECTURE_SECURITY_REVIEW.md) | Architecture security review |

---

## Planning

| Document | Description |
|----------|-------------|
| [project-evolution-timeline.md](planning/PROJECT_EVOLUTION_TIMELINE.md) | Project history and evolution |
| [phase3-operating-plan.md](planning/phase3_operating_plan.md) | Phase 3 operating plan |

---

## Templates

| Document | Description |
|----------|-------------|
| [pattern-story-template.md](templates/Pattern_Story_Template.md) | Template for pattern stories |
| [led-simulator-design.md](templates/LED_Simulator_Design.md) | LED simulator specifications |
| [instructions.md](templates/instructions.md) | Template instructions |

---

## Features

| Document | Description |
|----------|-------------|
| [web-interface-guide.md](features/WEB_INTERFACE_GUIDE.md) | Web interface usage |
| [ota-multi-level-support.md](features/OTA_Multi_Level_Support.md) | OTA firmware updates |
| [advanced-features-implementation-guide.md](features/ADVANCED_FEATURES_IMPLEMENTATION_GUIDE.md) | Advanced features guide |
| [fastled-advanced-features-analysis.md](features/FASTLED_ADVANCED_FEATURES_ANALYSIS.md) | FastLED features analysis |
| [fastled-implementation-plan.md](features/FASTLED_IMPLEMENTATION_PLAN.md) | FastLED implementation plan |

### LGP Features

| Document | Description |
|----------|-------------|
| [lgp-advanced-patterns.md](features/201.%20LGP-Advanced-Patterns.md) | LGP advanced patterns |
| [lgp-interference-discovery.md](features/202.%20LGP.Interference.Discovery.md) | LGP interference discovery |

---

## Audits

Recent code and pattern audits in `audits/`:

| Document | Description |
|----------|-------------|
| Pattern registry audit | Pattern registration analysis |
| Center distance refactor | LED center distance calculations |
| Color processing algorithm | Color pipeline analysis |
| Visual regression audit | Visual regression testing |
| Effects fixes | Effect bug fixes documentation |

---

## Implementation Notes

Historical implementation documentation in `implementation/`:

| Document | Description |
|----------|-------------|
| [README.md](implementation/README.md) | Implementation overview |
| [audio-synq-implementation-summary.md](implementation/AUDIO_SYNQ_IMPLEMENTATION_SUMMARY.md) | Audio sync implementation |
| [i2c-mutex-fix.md](implementation/I2C_MUTEX_FIX.md) | I2C mutex fix |
| [structural-fixes-implemented.md](implementation/STRUCTURAL_FIXES_IMPLEMENTED.md) | Structural fixes |
| [wifi-control.md](implementation/WiFi_CONTROL.md) | WiFi control implementation |

---

## Archive

Legacy, deprecated, and historical documentation has been consolidated into `archive/`.

**See:** [consolidation-manifest.md](archive/consolidation-manifest.md) for the complete archive inventory and migration history.

### Archive Structure

| Directory | Contents |
|-----------|----------|
| `archive/analysis-originals/` | Original pattern guide versions (b-series, c-series) |
| `archive/audio-analysis/` | Audio-reactive failure analysis versions |
| `archive/experimental/` | Experimental projects (ridge-plot-r3f) |
| `archive/session-2025-01-01/` | Session artifacts and commit history |
| `archive/misplaced-source/` | Source files incorrectly placed in docs |

---

## Document Naming Convention

This project uses **kebab-case** for all new documentation:
- `pattern-development-guide.md` (correct)
- `PATTERN_DEVELOPMENT_GUIDE.md` (legacy, being migrated)

Legacy SCREAMING_SNAKE_CASE files are being renamed during documentation reorganization.

---

## Contributing

When adding new documentation:
1. Use kebab-case filenames
2. Place in the appropriate directory
3. Update this INDEX.md
4. Follow the pattern guide structure for effect documentation
5. Mark MANDATORY documents clearly

---

*Last updated: 2026-01-02*
