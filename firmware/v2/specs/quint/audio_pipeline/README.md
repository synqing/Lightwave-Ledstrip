# Audio Pipeline Quint Specification

Safety invariants for the LightwaveOS audio processing pipeline.

## Overview

This specification models the core audio pipeline components (DC removal, AGC, noise floor adaptation) using scaled integers to enable bounded model checking with Quint/Apalache.

## Safety Invariants

The spec verifies:

1. **DCBounded**: DC estimate stays within reasonable bounds
2. **AGCGainValid**: AGC gain remains within configured min/max range
3. **RMSBounded**: RMS values (pre/post gain) stay bounded
4. **NoiseFloorValid**: Noise floor estimate stays within valid range
5. **AGCStable**: AGC gain changes are bounded (prevents oscillation)

## Usage

```bash
cd firmware/v2/specs/quint/tools

# Typecheck
quint typecheck ../audio_pipeline/audio_pipeline.qnt

# Bounded verification
quint verify --max-steps=100 --invariant=DCBounded ../audio_pipeline/audio_pipeline.qnt
quint verify --max-steps=100 --invariant=AGCGainValid ../audio_pipeline/audio_pipeline.qnt
quint verify --max-steps=100 --invariant=RMSBounded ../audio_pipeline/audio_pipeline.qnt
quint verify --max-steps=100 --invariant=NoiseFloorValid ../audio_pipeline/audio_pipeline.qnt
```

## Implementation Notes

- Uses **scaled integers** (multiply by 1,000,000) to represent floating-point values
- Simplified RMS calculation (uses absolute value as proxy)
- Nondeterministic input samples (bounded to int16 range)
- Focuses on **safety properties** rather than full DSP accuracy

## Related Files

- Firmware implementation: `firmware/v2/src/audio/AudioActor.cpp`
- Tuning parameters: `firmware/v2/src/audio/AudioTuning.h`
