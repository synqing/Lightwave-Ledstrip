# Custom Partition Table Optimization

## Why Custom Partition?

The default ESP32 partition wastes precious flash space on features we don't use:
- **6.25MB for OTA updates** - We don't use OTA
- **3.44MB for SPIFFS** - We only need 512KB for presets
- **Small app partition** - Only 1.3MB for code

## Our Custom Partition

```
Total Flash: 16MB
├── NVS:      20KB    (Non-volatile storage)
├── OTA Data: 8KB     (Required even without OTA)
├── App:      14.6MB  (MASSIVE! Was 1.3MB)
├── SPIFFS:   512KB   (Presets only)
├── UserData: 1MB     (Custom data partition)
└── CoreDump: 64KB    (Debug crashes)
```

## Benefits

### 1. Massive App Space (14.6MB)
- Store up to **5MB of pre-calculated LUTs in flash**
- Room for complex pre-rendered effects
- No need to compress or optimize code size

### 2. Flash-Based LUTs
Move large LUTs from RAM to Flash:
- **Trigonometric tables**: 16KB → Flash
- **Plasma textures**: 64KB → Flash  
- **Fire animations**: 32KB → Flash
- **Perlin noise**: 128KB → Flash
- **Transition frames**: 1MB → Flash

**Result**: Free up 200KB+ of RAM!

### 3. Performance Impact
ESP32-S3 has fast flash with cache:
- Flash read: ~40MHz (25ns per byte)
- With cache hit: Near RAM speed
- Sequential reads: Prefetch optimized

### 4. Future Expansion
With 14.6MB available:
- Store 1000+ pre-calculated effects
- Full animation sequences
- Complex particle systems
- Audio visualization data
- Bitmap/video frames

## Implementation

1. **partitions_custom.csv** - Already configured
2. **FlashLUTs.h** - Access flash-based tables
3. **Move existing LUTs** - Transfer from RAM to flash

## Memory Architecture

```
Before (Default Partition):
RAM:  [████████████████████░] 93.75% (300KB used)
Flash:[██░░░░░░░░░░░░░░░░░░] 28.3% (371KB of 1.3MB)

After (Custom Partition):  
RAM:  [████░░░░░░░░░░░░░░░░] 25% (80KB used)
Flash:[██░░░░░░░░░░░░░░░░░░] 10% (1.5MB of 14.6MB)
```

## The Philosophy

> "Why leave 10MB of flash unused when we could fill it with pre-calculated EVERYTHING? Every unused MB of flash is a missed opportunity for a massive lookup table. With 14.6MB, we can pre-calculate entire universes of LED patterns."

The custom partition transforms our ESP32-S3 into a LED pattern **supercomputer** with gigantic lookup tables!