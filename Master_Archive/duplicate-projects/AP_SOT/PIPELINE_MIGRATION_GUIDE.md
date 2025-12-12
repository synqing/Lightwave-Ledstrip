# Pipeline Migration Guide
**From Legacy Monolithic to Pluggable Architecture**

## Overview

This guide documents the process of migrating from the legacy monolithic audio pipeline to the new pluggable node-based architecture. The migration is designed to be safe and reversible.

## Migration Status: ✅ READY

All critical components have been implemented and tested:
- ✅ Zone energy bug fixed in legacy system
- ✅ Pluggable pipeline tested with simulated audio
- ✅ Performance benchmarked (<8ms target achieved)
- ✅ Dual-path architecture verified
- ✅ All 4 Phase 1.2 fixes implemented

## Architecture Comparison

### Legacy Monolithic System
```
main.cpp → AudioProcessor → GoertzelEngine → AudioFeatures → audio_state
```
- Single rigid processing chain
- Direct function calls
- Global state management
- Hardcoded configuration

### Pluggable Node System
```
main_pluggable.cpp → AudioPipeline → [Node Chain] → audio_state
                          ↓
                   JSON Configuration
```
- Dynamic node-based pipeline
- Runtime reconfigurable
- Modular components
- JSON-driven configuration

## Migration Steps

### Step 1: Pre-Migration Testing (RECOMMENDED)

1. **Run Test Programs**
   ```bash
   # Test pluggable pipeline functionality
   pio run -t upload -e test_pluggable
   
   # Benchmark performance comparison
   pio run -t upload -e benchmark
   
   # Verify dual-path architecture
   pio run -t upload -e verify_dual_path
   ```

2. **Verify Results**
   - Pipeline processes audio correctly
   - Performance meets <8ms target
   - Dual-path separation works
   - Zone energies calculate properly

### Step 2: Backup Current System

```bash
# Create backup of current main
cp src/main.cpp src/main_legacy_backup.cpp

# Create git checkpoint
git add -A
git commit -m "Pre-migration checkpoint - legacy system working"
```

### Step 3: Activate Pluggable Pipeline

```bash
# Switch to pluggable main
mv src/main.cpp src/main_legacy.cpp
mv src/main_pluggable.cpp src/main.cpp

# Build and upload
pio run -t upload
```

### Step 4: Verify Operation

1. **Check Serial Output**
   - Pipeline initialization messages
   - Performance metrics (should show <8ms)
   - Zone energy values (should vary, not stuck at 1.0)
   - Beat detection events

2. **Monitor Health**
   ```
   Pipeline health: HEALTHY, failures: 0
   Average process time: X.XXX ms
   ```

3. **Visual Verification**
   - LEDs respond to audio
   - Beat synchronization works
   - Zone mapping appears correct

### Step 5: Configuration Tuning

The pluggable system supports runtime configuration via JSON:

```json
{
  "main_pipeline": {
    "nodes": [
      {
        "name": "DCOffset",
        "mode": "calibrate",
        "high_pass_enabled": true
      },
      {
        "name": "MultibandAGC",
        "attack_ms": [10, 20, 30, 40],
        "release_ms": [100, 200, 300, 400]
      },
      {
        "name": "ZoneMapper",
        "num_zones": 36,
        "mapping_mode": "logarithmic"
      }
    ]
  }
}
```

## Rollback Procedure

If issues occur, rollback is simple:

```bash
# Restore legacy main
mv src/main.cpp src/main_pluggable.cpp
mv src/main_legacy.cpp src/main.cpp

# Rebuild and upload
pio run -t upload
```

## Performance Expectations

Based on benchmarking:

| Metric | Legacy | Pluggable | Target |
|--------|--------|-----------|--------|
| Average latency | ~6.5ms | ~7.2ms | <8ms |
| 99th percentile | ~7.8ms | ~8.5ms | <10ms |
| Memory usage | ~32KB | ~38KB | <64KB |
| CPU usage | ~15% | ~17% | <25% |

The pluggable system adds ~10% overhead but remains well within performance targets.

## Common Issues and Solutions

### Issue: Pipeline won't initialize
**Solution**: Check serial output for specific node initialization failures. Usually caused by I2S pins conflict.

### Issue: No audio processing
**Solution**: Verify I2S microphone connections. Check that DCOffsetNode is configured correctly.

### Issue: Beat detection not working
**Solution**: Ensure beat detector receives RAW frequency data, not AGC processed. Check dual-path wiring.

### Issue: Performance degradation
**Solution**: Monitor individual node timings. Disable non-critical nodes if needed.

## Advanced Configuration

### Genre-Specific Profiles

```cpp
// Electronic/Dance Music
const char* EDM_CONFIG = R"({
  "beat_detector": {
    "onset_threshold": 0.3,
    "tempo_range": {"min": 120, "max": 140}
  },
  "zone_mapper": {
    "bass_boost": 2.5
  }
})";

// Classical Music
const char* CLASSICAL_CONFIG = R"({
  "beat_detector": {
    "onset_threshold": 0.1,
    "tempo_range": {"min": 60, "max": 120}
  },
  "zone_mapper": {
    "bass_boost": 1.0
  }
})";
```

### Adding Custom Nodes

1. Create new node class inheriting from `AudioNode`
2. Implement `process()` method
3. Add to `AudioNodeFactory`
4. Include in pipeline configuration

## Migration Benefits

1. **Flexibility**: Change processing chain without recompiling
2. **Modularity**: Easy to add/remove/reorder processing stages
3. **Debugging**: Per-node performance metrics
4. **Extensibility**: Community can create custom nodes
5. **Resilience**: Graceful failure handling

## Future Enhancements

- Web-based pipeline configurator
- Real-time parameter tuning
- Multi-device synchronization
- Cloud profile sharing
- Plugin marketplace

---

**Migration Support**: If you encounter issues during migration, check the diagnostic output and refer to the troubleshooting section. The architecture is designed for safe experimentation - you can always roll back to the legacy system.