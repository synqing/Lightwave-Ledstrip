# K1 Front-End Performance Report

## CPU Budget Analysis

### Per-Hop Processing (125 Hz = 8.0 ms per hop)

#### RhythmBank (Every Hop)
- **Goertzel processing**: Group-by-N optimization
  - CopyLast + window: O(N) per unique N
  - Goertzel kernel: O(N) per bin
  - Estimated: **≤0.6 ms** (target)

#### HarmonyBank (Every 2 Hops)
- **Goertzel processing**: 64 bins, group-by-N
  - Estimated: **≤1.0 ms** (target, when runs)

#### Signal Conditioning
- **NoiseFloor update**: O(bins) = O(24) or O(64) = **negligible**
- **AGC process**: O(bins) = **negligible**
- **NoveltyFlux**: O(24) = **negligible**
- **ChromaExtractor**: O(64) = **negligible**
- **ChromaStability**: O(12) = **negligible**

### Total Per-Hop Budget
- **Rhythm-only hop**: ≤0.6 ms
- **Rhythm + Harmony hop**: ≤1.6 ms
- **Hard ceiling**: ≤2.0 ms per hop

### Overload Policy
- If processing exceeds 2.0 ms → drop harmony tick, set `overload=true`
- Never drop rhythm (critical for beat tracking)

## Hot Path Optimizations

### Group-by-N Processing
- ✅ CopyLast + window once per N (not per bin)
- ✅ Window lookup O(1) via WindowBank
- ✅ No per-hop trig/pow calls (only init-time)

### Memory Access Patterns
- ✅ Ring buffer wrap handling (no memmove)
- ✅ Scratch buffer reuse (single allocation)
- ✅ Window LUTs pre-computed

## Verification Checklist
- [ ] Rhythm tick ≤0.6ms (requires profiling)
- [ ] Harmony tick ≤1.0ms (requires profiling)
- [ ] Hard ceiling ≤2.0ms per hop (requires profiling)
- [ ] Overload drops harmony only (code verified)
- [x] Per-group copy (not per-bin) - code verified
- [x] Window lookup O(1) - code verified
- [x] No per-hop trig/pow calls - code verified

## Profiling Instructions
1. Add timing instrumentation to `K1AudioFrontEnd::processHop()`
2. Measure RhythmBank processing time
3. Measure HarmonyBank processing time
4. Verify overload policy triggers correctly

