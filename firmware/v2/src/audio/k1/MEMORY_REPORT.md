# K1 Front-End Memory Report

## Memory Budget Analysis

### Ring Buffer
- **AudioRingBuffer**: 4096 samples × 2 bytes = **8,192 bytes** (8 KB)

### Window Bank
- **WindowBank**: Per-N Hann LUTs
  - Unique N values: {256, 512, 1024, 1536} + low harmony bins
  - Estimated: ~6 unique N values
  - Per N: N samples × 2 bytes (Q15)
  - Total: ~(256+512+1024+1536+...)×2 ≈ **6,000 bytes** (6 KB)

### Scratch Buffers
- **GoertzelBank scratch**: N_MAX × 2 bytes = 1536 × 2 = **3,072 bytes** (3 KB)

### State Variables
- **K1AudioFrontEnd state**:
  - rhythmMags[24] × 4 = 96 bytes
  - harmonyMags[64] × 4 = 256 bytes
  - rhythmMagsRaw[24] × 4 = 96 bytes
  - harmonyMagsRaw[64] × 4 = 256 bytes
  - chroma12[12] × 4 = 48 bytes
  - AudioFeatureFrame = ~500 bytes
  - Total: ~**1,252 bytes** (1.2 KB)

### Module State
- **NoiseFloor**: 2 instances × (24+64 bins × 4 bytes) = **704 bytes**
- **AGC**: 2 instances × (24+64 bins × 4 bytes) = **704 bytes**
- **NoveltyFlux**: 24 bins × 4 bytes + state = **~100 bytes**
- **ChromaExtractor**: Minimal state = **~50 bytes**
- **ChromaStability**: 8 frames × 12 × 4 = **384 bytes**

### Total Estimated Memory
- Ring Buffer: 8 KB
- Window Bank: 6 KB
- Scratch: 3 KB
- Front-end state: 1.2 KB
- Module state: ~2 KB
- **Total: ~20.2 KB** (well under 32 KB budget)

### Hot Path Allocations
- **No heap allocations in hot path**: All buffers pre-allocated at init
- **No memmove operations**: Ring buffer uses wrap handling
- **No dynamic growth**: Fixed-size arrays only

## Verification
- [x] Total RAM < 32 KB
- [x] No heap allocs in hot path
- [x] All buffers pre-allocated

