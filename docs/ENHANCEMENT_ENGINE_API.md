# Enhancement Engine API Reference

**Version**: 1.0
**Last Updated**: 2025-12-12
**Target Firmware**: esp32dev_enhanced build

## Overview

The Enhancement Engine API provides REST endpoints for controlling advanced visual features in LightwaveOS. These features are only available when `FEATURE_ENHANCEMENT_ENGINES=1` is enabled in the build configuration.

The system consists of three independent engines:
- **ColorEngine**: Cross-palette blending, color diffusion, temporal rotation
- **MotionEngine**: Phase control, auto-rotation, momentum physics
- **BlendingEngine**: Zone blend modes, layer ordering (Week 5 - not yet implemented)

---

## Table of Contents

1. [ColorEngine API](#colorengine-api)
2. [MotionEngine API](#motionengine-api)
3. [BlendingEngine API](#blendingengine-api-coming-soon)
4. [Status Endpoints](#status-endpoints)
5. [Error Handling](#error-handling)
6. [Usage Examples](#usage-examples)

---

## ColorEngine API

The ColorEngine provides advanced color manipulation through palette blending, color diffusion, and temporal rotation.

### Enable Cross-Palette Blending

**Endpoint**: `POST /api/enhancement/color/blend`

Enables or disables cross-palette blending mode. When enabled, ColorEngine blends between 2-3 palettes for richer color depth.

**Request Body**:
```json
{
  "enabled": true
}
```

**Parameters**:
| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `enabled` | boolean | Yes | `true` to enable blending, `false` to disable |

**Response** (Success):
```json
{
  "status": "ok"
}
```

**Response** (Error):
```json
{
  "error": "Invalid request"
}
```

**Behavior**:
- When enabled, effects using ColorEngine will blend colors from multiple palettes
- Default blend factors are applied (can be configured per-effect)
- Disable to revert to single-palette mode

**Example**:
```bash
curl -X POST http://lightwaveos.local/api/enhancement/color/blend \
  -H "Content-Type: application/json" \
  -d '{"enabled": true}'
```

---

### Set Color Diffusion Amount

**Endpoint**: `POST /api/enhancement/color/diffusion`

Sets the amount of color diffusion (blur) applied between neighboring LEDs. Creates smoother, shimmer-like effects.

**Request Body**:
```json
{
  "amount": 80
}
```

**Parameters**:
| Field | Type | Required | Range | Description |
|-------|------|----------|-------|-------------|
| `amount` | integer | Yes | 0-255 | Diffusion strength (0 = off, 255 = maximum blur) |

**Response** (Success):
```json
{
  "status": "ok"
}
```

**Behavior**:
- Uses FastLED's `blur1d()` for Gaussian blur
- Automatically enables diffusion when amount > 0
- Automatically disables diffusion when amount = 0
- Higher values create more blur but cost more CPU time

**Performance**:
- 0-100: ~0.5ms for 320 LEDs
- 100-200: ~1.2ms for 320 LEDs
- 200-255: ~2.0ms for 320 LEDs

**Recommended Values**:
- **Subtle shimmer**: 20-50
- **Moderate glow**: 60-100
- **Strong diffusion**: 120-180
- **Maximum blur**: 200-255

**Example**:
```bash
curl -X POST http://lightwaveos.local/api/enhancement/color/diffusion \
  -H "Content-Type: application/json" \
  -d '{"amount": 80}'
```

---

### Set Rotation Speed

**Endpoint**: `POST /api/enhancement/color/rotation`

Sets the speed of palette rotation over time (degrees per frame).

**Request Body**:
```json
{
  "speed": 0.5
}
```

**Parameters**:
| Field | Type | Required | Range | Description |
|-------|------|----------|-------|-------------|
| `speed` | float | Yes | 0.0-10.0 | Rotation speed in degrees per frame |

**Response** (Success):
```json
{
  "status": "ok"
}
```

**Behavior**:
- Rotates the color palette index each frame
- 0.0 = no rotation (static palette)
- Higher values = faster rotation
- Automatically enables temporal rotation when speed > 0
- Automatically disables temporal rotation when speed = 0

**Frame Rate Impact**:
- At 120 FPS: 1.0 deg/frame = 120 deg/sec = full rotation every 3 seconds
- At 120 FPS: 0.3 deg/frame = 36 deg/sec = full rotation every 10 seconds

**Recommended Values**:
- **Very slow drift**: 0.1-0.3
- **Slow rotation**: 0.4-0.8
- **Medium rotation**: 1.0-2.0
- **Fast rotation**: 3.0-5.0
- **Strobe-like**: 6.0-10.0

**Example**:
```bash
curl -X POST http://lightwaveos.local/api/enhancement/color/rotation \
  -H "Content-Type: application/json" \
  -d '{"speed": 0.5}'
```

---

## MotionEngine API

The MotionEngine provides phase control, auto-rotation, easing curves, and momentum physics for advanced motion effects.

### Set Phase Offset

**Endpoint**: `POST /api/enhancement/motion/phase`

Sets the phase offset for strip effects (in degrees, 0-360).

**Request Body**:
```json
{
  "offset": 45
}
```

**Parameters**:
| Field | Type | Required | Range | Description |
|-------|------|----------|-------|-------------|
| `offset` | integer | Yes | 0-360 | Phase offset in degrees |

**Response** (Success):
```json
{
  "status": "ok"
}
```

**Behavior**:
- Sets the strip's phase offset for wave-based effects
- Phase wraps at 360° (0° = 360°)
- Converted internally to radians (offset * DEG_TO_RAD)
- Affects effects using `getPhaseController().getStripPhaseRadians()`

**Use Cases**:
- **Wave interference**: Offset by 90° or 180° for constructive/destructive interference
- **Dual-strip synchronization**: Align phase across strips
- **Visual alignment**: Fine-tune effect starting position

**Recommended Values**:
- **In-phase**: 0°
- **Quadrature**: 90°
- **Anti-phase**: 180°
- **Custom offset**: Any value 0-360°

**Example**:
```bash
curl -X POST http://lightwaveos.local/api/enhancement/motion/phase \
  -H "Content-Type: application/json" \
  -d '{"offset": 90}'
```

---

### Enable/Disable Auto-Rotation

**Endpoint**: `POST /api/enhancement/motion/auto-rotate`

Enables or disables automatic phase rotation with a specified speed (degrees per second).

**Request Body**:
```json
{
  "enabled": true,
  "speed": 10.0
}
```

**Parameters**:
| Field | Type | Required | Range | Description |
|-------|------|----------|-------|-------------|
| `enabled` | boolean | Yes | - | `true` to enable auto-rotation, `false` to disable |
| `speed` | float | Yes | 0.0-100.0 | Rotation speed in degrees per second |

**Response** (Success):
```json
{
  "status": "ok"
}
```

**Behavior**:
- When enabled, phase offset automatically increments by `speed * deltaTime` each frame
- Phase wraps at 360° automatically
- When disabled (or speed = 0), phase remains static
- Delta time is calculated using `millis()` for frame-rate-independent rotation

**Speed Calculation**:
- At 120 FPS: 10 deg/sec = 0.083 deg/frame
- At 120 FPS: 36 deg/sec = full rotation every 10 seconds
- At 120 FPS: 120 deg/sec = full rotation every 3 seconds

**Recommended Values**:
- **Very slow drift**: 1-5 deg/sec
- **Slow rotation**: 10-20 deg/sec
- **Medium rotation**: 30-50 deg/sec
- **Fast rotation**: 60-100 deg/sec

**Example**:
```bash
# Enable auto-rotation at 10 degrees per second
curl -X POST http://lightwaveos.local/api/enhancement/motion/auto-rotate \
  -H "Content-Type: application/json" \
  -d '{"enabled": true, "speed": 10.0}'

# Disable auto-rotation
curl -X POST http://lightwaveos.local/api/enhancement/motion/auto-rotate \
  -H "Content-Type: application/json" \
  -d '{"enabled": false, "speed": 0}'
```

---

## BlendingEngine API (Coming Soon)

**Status**: Week 5 implementation (not yet available)

The BlendingEngine will provide advanced blend modes, layer ordering, and zone interaction effects. The following endpoints are planned:

### Planned Endpoints

1. **Set Zone Blend Mode**
   - `POST /api/enhancement/blend/mode`
   - Blend modes: OVERWRITE, ADDITIVE, MULTIPLY, SCREEN, OVERLAY, ALPHA, LIGHTEN_ONLY, DARKEN_ONLY

2. **Set Zone Z-Order**
   - `POST /api/enhancement/blend/z-order`
   - Control layer stacking for zone compositing

3. **Set Zone Alpha**
   - `POST /api/enhancement/blend/alpha`
   - Control zone opacity (0-255)

4. **Enable Zone Interaction**
   - `POST /api/enhancement/blend/interaction`
   - Enable wave propagation/resonance between zones

**Expected Availability**: Week 5 of implementation plan

---

## Status Endpoints

### Get Enhancement Engine Status

**Endpoint**: `GET /api/enhancement/status`

Returns the current status of all enhancement engines, including which features are enabled and active.

**Request**: No body required (GET request)

**Response** (Success):
```json
{
  "status": "ok",
  "engines": {
    "colorEngine": {
      "active": true,
      "crossBlend": true,
      "diffusionAmount": 80,
      "rotationSpeed": 0.5
    },
    "motionEngine": {
      "active": true,
      "phaseOffset": 45,
      "autoRotate": true,
      "autoRotateSpeed": 10.0
    },
    "blendingEngine": {
      "active": false,
      "message": "Not yet implemented (Week 5)"
    }
  }
}
```

**Field Descriptions**:

**ColorEngine Status**:
- `active`: Whether ColorEngine has any features enabled
- `crossBlend`: Cross-palette blending enabled/disabled
- `diffusionAmount`: Current diffusion amount (0-255)
- `rotationSpeed`: Current rotation speed (degrees/frame)

**MotionEngine Status**:
- `active`: Whether MotionEngine has any features enabled
- `phaseOffset`: Current phase offset in degrees
- `autoRotate`: Auto-rotation enabled/disabled
- `autoRotateSpeed`: Auto-rotation speed (degrees/second)

**BlendingEngine Status**:
- `active`: Always `false` until Week 5 implementation
- `message`: Implementation status message

**Example**:
```bash
curl -X GET http://lightwaveos.local/api/enhancement/status
```

---

## Error Handling

All enhancement endpoints follow consistent error handling:

### Error Response Format

```json
{
  "error": "Error description"
}
```

### Common HTTP Status Codes

| Code | Meaning | Typical Cause |
|------|---------|---------------|
| `200` | OK | Request succeeded |
| `400` | Bad Request | Missing parameters, invalid JSON, or out-of-range values |
| `404` | Not Found | Enhancement engines not compiled (FEATURE_ENHANCEMENT_ENGINES=0) |
| `500` | Internal Server Error | Engine initialization failure |

### Common Error Messages

| Error Message | Cause | Solution |
|---------------|-------|----------|
| `"Invalid request"` | Missing required field or malformed JSON | Check JSON syntax and required fields |
| `"Enhancement engines not available"` | FEATURE_ENHANCEMENT_ENGINES=0 | Build with `esp32dev_enhanced` environment |
| `"Value out of range"` | Parameter exceeds min/max bounds | Check parameter ranges in docs |

---

## Usage Examples

### Basic ColorEngine Setup

```bash
# Enable cross-palette blending
curl -X POST http://lightwaveos.local/api/enhancement/color/blend \
  -H "Content-Type: application/json" \
  -d '{"enabled": true}'

# Add moderate diffusion for shimmer effect
curl -X POST http://lightwaveos.local/api/enhancement/color/diffusion \
  -H "Content-Type: application/json" \
  -d '{"amount": 80}'

# Enable slow palette rotation
curl -X POST http://lightwaveos.local/api/enhancement/color/rotation \
  -H "Content-Type: application/json" \
  -d '{"speed": 0.3}'
```

### Basic MotionEngine Setup

```bash
# Set phase offset to 90 degrees
curl -X POST http://lightwaveos.local/api/enhancement/motion/phase \
  -H "Content-Type: application/json" \
  -d '{"offset": 90}'

# Enable auto-rotation at 10 deg/sec
curl -X POST http://lightwaveos.local/api/enhancement/motion/auto-rotate \
  -H "Content-Type: application/json" \
  -d '{"enabled": true, "speed": 10.0}'
```

### Combined Effect Example

```bash
# Create a slowly rotating, diffused, dual-palette fire effect

# 1. Enable cross-palette blending (Fire+ uses HeatColors + LavaColors)
curl -X POST http://lightwaveos.local/api/enhancement/color/blend \
  -H "Content-Type: application/json" \
  -d '{"enabled": true}'

# 2. Add medium diffusion for smoother flames
curl -X POST http://lightwaveos.local/api/enhancement/color/diffusion \
  -H "Content-Type: application/json" \
  -d '{"amount": 60}'

# 3. Add very slow palette rotation for color drift
curl -X POST http://lightwaveos.local/api/enhancement/color/rotation \
  -H "Content-Type: application/json" \
  -d '{"speed": 0.2}'

# 4. Select Fire+ effect (assumes effect ID 46)
curl -X POST http://lightwaveos.local/api/effect \
  -H "Content-Type: application/json" \
  -d '{"effectId": 46}'
```

### JavaScript/Web UI Example

```javascript
// ColorEngine controls
async function setColorDiffusion(amount) {
    const response = await fetch('/api/enhancement/color/diffusion', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ amount })
    });
    return await response.json();
}

// MotionEngine controls
async function enableAutoRotate(speed) {
    const response = await fetch('/api/enhancement/motion/auto-rotate', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ enabled: true, speed })
    });
    return await response.json();
}

// Get current status
async function getEnhancementStatus() {
    const response = await fetch('/api/enhancement/status');
    return await response.json();
}

// Usage
setColorDiffusion(80);
enableAutoRotate(10.0);
const status = await getEnhancementStatus();
console.log('Enhancement status:', status);
```

---

## Performance Considerations

### CPU Budget (per frame @ 120 FPS = 8.33ms)

| Component | Time Budget | Notes |
|-----------|-------------|-------|
| ColorEngine (cross-blend) | < 1ms | 3-palette blend for 320 LEDs |
| ColorEngine (diffusion) | < 2ms | Gaussian blur, depends on amount |
| ColorEngine (rotation) | < 0.1ms | Simple phase increment |
| MotionEngine (phase update) | < 0.1ms | Minimal CPU cost |
| MotionEngine (particles) | < 0.3ms | 32 active particles max |
| **Total** | **~3ms** | Well under 8.33ms target |

### Memory Footprint

| Component | RAM Usage | Flash Usage |
|-----------|-----------|-------------|
| ColorEngine | ~350 bytes | ~2 KB |
| MotionEngine | ~1,340 bytes | ~1.5 KB |
| BlendingEngine | ~1,200 bytes | ~2 KB (Week 5) |
| **Total** | **~2,900 bytes** | **~5.5 KB** |

### Optimization Tips

1. **Diffusion**: Lower values (0-100) are significantly faster than high values (200-255)
2. **Auto-Rotation**: Negligible performance impact at any speed
3. **Cross-Blending**: Fixed cost regardless of blend factors
4. **Disable Unused Features**: Set amount=0 or enabled=false to completely disable

---

## Troubleshooting

### Enhancement Controls Not Appearing

**Symptom**: Web UI doesn't show Enhancement Engines card

**Causes**:
1. Old SPIFFS filesystem (web files not updated)
2. Browser cache serving old files

**Solutions**:
```bash
# Re-upload SPIFFS
pio run -e esp32dev_wifi -t uploadfs

# Clear browser cache (hard refresh)
# Chrome/Edge: Ctrl+Shift+R (Windows) or Cmd+Shift+R (Mac)
# Firefox: Ctrl+F5 (Windows) or Cmd+Shift+R (Mac)
```

### API Returns 404

**Symptom**: All enhancement endpoints return 404 Not Found

**Cause**: Firmware built without `FEATURE_ENHANCEMENT_ENGINES=1`

**Solution**:
```bash
# Build with enhanced environment
pio run -e esp32dev_enhanced -t upload
```

### Effects Not Using Enhancements

**Symptom**: Enhanced effects (Fire+, Ocean+, etc.) look the same as standard effects

**Causes**:
1. Wrong effect selected (selected Fire instead of Fire+)
2. Enhancement features disabled via API
3. Feature flag not enabled in build

**Solutions**:
1. Verify effect name ends with `+` symbol
2. Check enhancement status via `/api/enhancement/status`
3. Confirm build environment is `esp32dev_enhanced`

### Performance Degradation

**Symptom**: FPS drops below 120 when using enhancements

**Causes**:
1. Diffusion amount too high (> 200)
2. Multiple engines active simultaneously
3. Complex effect + multiple zones

**Solutions**:
1. Reduce diffusion amount to 100 or less
2. Disable unused enhancement features
3. Reduce zone count or use simpler effects

---

## Feature Compatibility Matrix

| Effect Type | ColorEngine | MotionEngine | BlendingEngine |
|-------------|-------------|--------------|----------------|
| Fire+ | ✓ (dual-palette) | - | - |
| Ocean+ | ✓ (triple-palette) | - | - |
| LGP Holographic+ | ✓ (diffusion) | - | - |
| Shockwave+ | - | ✓ (momentum) | - |
| Collision+ | - | ✓ (phase) | - |
| LGP Wave Collision+ | - | ✓ (auto-rotate) | - |
| Future Effects | ✓ | ✓ | ✓ (Week 5) |

**Legend**:
- ✓ = Supported and used by effect
- - = Not used by this effect
- (feature) = Specific engine feature utilized

---

## Version History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2025-12-12 | Initial release: ColorEngine + MotionEngine APIs |
| 1.1 | TBD | BlendingEngine API (Week 5) |
| 2.0 | TBD | Web UI integration, WebSocket support |

---

## Related Documentation

- [Enhanced Effects Guide](./ENHANCED_EFFECTS_GUIDE.md) - Documentation for all enhanced effects
- [Effect Migration Guide](./EFFECT_MIGRATION_GUIDE.md) - Template for creating new enhanced effects
- [Performance Benchmarks](./PERFORMANCE_BENCHMARKS.md) - Detailed performance metrics
- [Web Enhancement Controls](./WEB_ENHANCEMENT_CONTROLS.md) - Web UI integration guide

---

## Support

For issues, feature requests, or questions about the Enhancement Engine API:
- GitHub Issues: [LightwaveOS Issues](https://github.com/your-repo/issues)
- Documentation: [LightwaveOS Docs](https://github.com/your-repo/docs)

---

**© 2025 LightwaveOS - Enhancement Engine API v1.0**
