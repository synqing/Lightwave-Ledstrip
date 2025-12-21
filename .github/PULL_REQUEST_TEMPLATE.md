# Pattern Effect Pull Request

## Pattern Specification

### 1) Overview
- **Name**: [Pattern name]
- **Category**: [standing / travelling / moiré / depth / spectral split]
- **One-sentence story**: [Brief narrative description]
- **Primary medium**: [LGP (dual edge) / strips]

### 2) Optical Intent (LGP)
- **Which optical levers are used**: [phase / spatial frequency / edge balance / diffusion / extraction softness]
- **Film-stack assumptions**: [prism/BEF present; diffusion layer present; extraction features present]
- **Expected signature**: [What the plate should do that raw LEDs cannot]

### 3) Control Mapping (Encoders)
- **Speed** → [Description]
- **Intensity** → [Description]
- **Saturation** → [Description]
- **Complexity** → [Description]
- **Variation** → [Description]

### 4) Performance Budget
- **Target frame rate**: 120 FPS minimum (8.33 ms budget)
- **Worst-case compute path**: [Description]
- **Memory strategy**: [Static buffers preferred; avoid large stack allocations]

### 5) Compliance Checklist
- [ ] **Centre origin**: Pattern originates from LEDs 79/80 (centre of strip)
- [ ] **No rainbows**: Hue range < 60° (no full-spectrum cycling)
- [ ] **No unsafe strobe**: No rapid flashing that could trigger photosensitivity
- [ ] **Performance validated**: Frame rate ≥ 120 FPS measured
- [ ] **Memory safe**: No large stack allocations; static buffers used
- [ ] **Visual verification**: Pattern tested on hardware LGP

## Implementation Details

### Code Changes
- [ ] Effect function implemented
- [ ] Effect registered in `effects[]` array in `main.cpp`
- [ ] Header declaration added to appropriate effects header file
- [ ] FastLED optimization utilities used where applicable (`FastLEDOptim.h`)

### Testing
- [ ] Visual verification on hardware completed
- [ ] Performance profiling shows ≥ 120 FPS
- [ ] Memory usage within budget (< 10KB per pattern)
- [ ] No compiler warnings

### Documentation
- [ ] Pattern story documented
- [ ] Encoder mappings documented
- [ ] Performance characteristics documented

## Definition of Done

- [ ] Pattern has clear one-sentence story
- [ ] Fits taxonomy category (see `docs/creative/LGP_PATTERN_TAXONOMY.md`)
- [ ] Encoder mappings documented
- [ ] Centre origin rule satisfied (LEDs 79/80)
- [ ] No-rainbows rule satisfied (hue range < 60°)
- [ ] Visual verification steps completed
- [ ] Performance validation passed (≥ 120 FPS)
- [ ] No new stability risks introduced
- [ ] Code follows FastLED optimization patterns
- [ ] Memory safety verified (no large stack allocations)

## Additional Notes

[Any additional context, known issues, or follow-up work needed]

