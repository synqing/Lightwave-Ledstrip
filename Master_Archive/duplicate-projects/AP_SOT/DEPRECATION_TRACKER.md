# AP_SOT Deprecation Tracker
**Created**: January 2025  
**Purpose**: Track migration from legacy monolithic to pluggable pipeline architecture

## Migration Status: 🟡 IN PROGRESS

### Phase 1: Deprecation (Current)
- [x] Move backup files to DEPRECATED/
- [x] Add deprecation headers to legacy files
- [x] Document replacement mappings
- [ ] Update build system

### Phase 2: Parallel Operation
- [ ] Activate pluggable pipeline alongside legacy
- [ ] Verify feature parity
- [ ] Performance comparison
- [ ] Fix any regressions

### Phase 3: Switchover
- [ ] Make pluggable pipeline primary
- [ ] Move legacy to DEPRECATED/
- [ ] Update all documentation
- [ ] Final testing

### Phase 4: Cleanup
- [ ] Remove DEPRECATED folder
- [ ] Archive for historical reference
- [ ] Update all dependencies

---

## File Migration Mapping

### Core Audio Processing
| Legacy File | Status | Replacement | Notes |
|------------|--------|-------------|-------|
| `audio_processing.cpp` | 🟡 Active | `i2s_input_node.h` + `dc_offset_node.h` | Main audio capture |
| `audio_processing.h.bak` | 🔴 Backup | N/A | Move to DEPRECATED |
| `audio_features.cpp` | 🟡 Active | `zone_mapper_node.h` | Zone energy calculations |
| `goertzel_engine.cpp` | 🟡 Active | `goertzel_node.h` | Frequency analysis |
| `enhanced_beat_detector.cpp` | 🟡 Active | `beat_detector_node.h` | Beat detection |
| `dc_offset_calibrator.cpp` | 🟢 Shared | Same (used by both) | Common component |
| `optimized_math.cpp` | 🟢 Shared | Same (used by both) | Math utilities |

### Headers
| Legacy File | Status | Replacement | Notes |
|------------|--------|-------------|-------|
| `AudioPipeline.h` | 🔴 Duplicate | `audio_pipeline.h` | Case conflict, has TODOs |
| `audio_processing.h` | 🟡 Active | Node architecture | Multiple nodes replace this |
| `goertzel_engine_legacy.h` | 🔴 Old | `goertzel_engine.h` | Previous version |
| `simd_goertzel.h` | ❓ Unknown | Check usage | May be integrated |

### Architecture Files
| Component | Legacy | Pluggable | Status |
|-----------|--------|-----------|--------|
| Main Entry | `main.cpp` | `main_pluggable.cpp` | Ready to switch |
| Pipeline Manager | `AudioProcessor` class | `AudioPipeline` class | ✅ Implemented |
| Data Flow | Direct calls | Node graph | ✅ Implemented |
| Configuration | Hardcoded | JSON-based | ✅ Implemented |
| State Management | `audio_state` global | Node metadata | 🟡 Both supported |

---

## Deprecation Warnings to Add

### Template for Legacy Files:
```cpp
/*
 * DEPRECATION NOTICE
 * ==================
 * This file is part of the legacy monolithic audio pipeline.
 * It will be replaced by the pluggable node architecture.
 * 
 * Replacement: [specify node(s)]
 * Migration: See DEPRECATION_TRACKER.md
 * Target removal: After Phase 3 completion
 * 
 * DO NOT ADD NEW FEATURES TO THIS FILE
 */
```

---

## Files to Move Immediately

### To DEPRECATED/backup/:
- [x] `audio_processing.h.bak`
- [x] `audio_processing.cpp.bak`
- [x] `AudioPipeline.h` (uppercase version)
- [x] `goertzel_engine_legacy.h`

### To DEPRECATED/examples/:
- [x] `pipeline_example.cpp`
- [x] `dual_path_pipeline_example.cpp`

---

## Critical Path Items

1. **Zone Energy Bug**: Must be fixed in legacy system first
2. **Performance Baseline**: Measure legacy system performance
3. **Test Coverage**: Ensure pluggable matches legacy behavior
4. **Documentation**: Update all references during migration

---

## Success Criteria

- [ ] All legacy functionality available in pluggable system
- [ ] No performance regression (maintain <8ms latency)
- [ ] Clean separation of deprecated vs active code
- [ ] Clear migration path for users/developers