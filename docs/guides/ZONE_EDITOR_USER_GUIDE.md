# Zone Editor User Guide

**Version:** 1.0.0  
**Last Updated:** 2025-01-XX

## Introduction

The Zone Editor is a powerful tool for creating custom multi-zone LED configurations on your LightwaveOS device. It allows you to:

- Define up to 4 independent zones with custom LED boundaries
- Assign different effects, palettes, and blend modes to each zone
- Visualise zone layouts in real-time
- Create complex layered lighting effects

This guide will walk you through using the Zone Editor interface in the LightwaveOS dashboard.

---

## Table of Contents

1. [Getting Started](#getting-started)
2. [Understanding Zones](#understanding-zones)
3. [Zone Layout Editor](#zone-layout-editor)
4. [Zone Configuration](#zone-configuration)
5. [Visualisation](#visualisation)
6. [Best Practices](#best-practices)
7. [Troubleshooting](#troubleshooting)

---

## Getting Started

### Accessing the Zone Editor

1. Open the LightwaveOS dashboard in your web browser
2. Navigate to the **Zone Editor** section
3. Ensure zone mode is enabled (toggle at the top of the zone editor)

### Prerequisites

- Zone mode must be enabled
- At least one zone must be configured
- All zones must follow centre-origin symmetry rules

---

## Understanding Zones

### What is a Zone?

A **zone** is a contiguous region of LEDs that can have its own:
- **Effect**: Visual pattern (e.g., Shockwave, Fire, Holographic Interference)
- **Palette**: Colour scheme (e.g., Sunset, Ocean, Aurora)
- **Blend Mode**: How the zone composites with other zones
- **Brightness & Speed**: Per-zone animation parameters

### Centre-Origin Architecture

LightwaveOS uses a **centre-origin** design where:
- **LEDs 79 and 80** are the centre pair (one on each strip)
- All effects originate from or converge toward the centre
- Zones must be **symmetrical** around the centre pair
- Zones are numbered from **inside-out**: Zone 0 is closest to centre, Zone 3 is at the edges

### LED Strip Layout

```
Left Strip:  0 ←──────────────────────────────→ 79 | 80 ←──────────────────────────────→ 159 Right Strip
              Edge                              Centre                              Edge
```

Each strip has 160 LEDs (0-159), with the centre pair at LEDs 79/80.

---

## Zone Layout Editor

### Zone Count Selector

At the top of the editor, you'll find a **Zone Count** dropdown:

- **1 Zone**: Single unified zone (all LEDs)
- **2 Zones**: Dual split (inner/outer)
- **3 Zones**: Triple rings (inner/middle/outer)
- **4 Zones**: Quad active (maximum zones)

**Note**: Changing the zone count will automatically generate a symmetric layout. You can then customise the boundaries manually.

### Segment Configuration

Each zone has two segments that must be configured:

#### Left Segment
- **Start LED**: Beginning of left segment (0-79)
- **End LED**: End of left segment (0-79, must be ≥ start)

#### Right Segment
- **Start LED**: Beginning of right segment (80-159)
- **End LED**: End of right segment (80-159, must be ≥ start)

**Auto-Mirror Feature**: When you enter left segment values, the right segment values are automatically calculated to maintain centre-origin symmetry.

### Visual LED Strip

The visualisation shows:
- **LED positions** (0-79 on left, 80-159 on right)
- **Zone boundaries** (coloured regions)
- **Centre divider** (yellow line at LEDs 79/80)
- **Zone labels** (Zone 0, Zone 1, etc.)

### Preset Layouts

Quick-select presets are available:
- **Unified**: Single zone covering all LEDs
- **Dual Split**: Two zones (inner/outer)
- **Triple Rings**: Three concentric zones
- **Quad Active**: Four zones
- **Heartbeat Focus**: Custom preset optimised for heartbeat effects

---

## Zone Configuration

### Per-Zone Controls

Each zone has its own configuration panel with:

#### Effect Selector
- Dropdown listing all available effects
- Effects are categorised (Classic, Shockwave, LGP Interference, etc.)
- Some effects are zone-aware and work better in multi-zone setups

#### Palette Selector
- Dropdown listing all available palettes
- **"Global"** option (palette ID 0) uses the system-wide palette
- Per-zone palettes allow different colour schemes in each zone

#### Blend Mode Selector
- **Overwrite** (default): Replace pixels in this zone
- **Additive**: Add to existing pixels (light accumulation)
- **Multiply**: Multiply with existing pixels
- **Screen**: Screen blend (lighten)
- **Overlay**: Overlay blend (multiply if dark, screen if light)
- **Alpha**: 50/50 blend
- **Lighten**: Take brighter pixel
- **Darken**: Take darker pixel

#### Brightness & Speed
- **Brightness**: 0-255 (per-zone brightness control)
- **Speed**: 1-100 (per-zone animation speed)

---

## Visualisation

### Reading the LED Strip Visualisation

The visualisation provides real-time feedback:

1. **Zone Colours**: Each zone is shown in a distinct colour
2. **Boundary Lines**: Clear separation between zones
3. **Centre Marker**: Yellow divider line at LEDs 79/80
4. **LED Numbers**: Position indicators along the strip

### Mobile Responsiveness

The visualisation is optimised for mobile viewing:
- Responsive layout that fits within viewport
- Touch-friendly controls
- Scrollable interface for smaller screens

---

## Best Practices

### Zone Layout Design

1. **Start Simple**: Begin with 2 zones to understand the system
2. **Maintain Symmetry**: Always keep left/right segments symmetric
3. **Consider Effect Types**: Some effects work better in larger zones
4. **Test Incrementally**: Make small changes and observe results

### Effect Selection

- **Zone-Aware Effects**: Look for effects marked as "zone-aware" in the effect list
- **Centre-Origin Effects**: All effects should work with centre-origin design
- **Layering**: Use different blend modes to create layered effects

### Palette Coordination

- **Contrast**: Use contrasting palettes for adjacent zones
- **Harmony**: Use complementary palettes for unified looks
- **Global vs Per-Zone**: Use global palette for consistency, per-zone for variety

### Blend Mode Selection

- **Overwrite**: Use for distinct zone separation
- **Additive**: Use for light accumulation effects
- **Screen/Overlay**: Use for soft blending between zones
- **Multiply**: Use for darkening/masking effects

### Performance Considerations

- **Zone Count**: More zones = more rendering overhead
- **Complex Effects**: Some effects are more computationally intensive
- **Blend Modes**: Additive and Multiply modes require additional processing

---

## Troubleshooting

### Common Issues

#### "Layout validation failed"
**Cause**: Zone boundaries violate constraints  
**Solution**: 
- Ensure zones are symmetric around centre
- Check for overlaps between zones
- Verify all LEDs (0-159) are covered
- Ensure zones are ordered centre-outward

#### "Zone not rendering"
**Cause**: Zone disabled or invalid configuration  
**Solution**:
- Check zone is enabled
- Verify effect ID is valid
- Check brightness is not 0
- Ensure zone boundaries are valid

#### "Right segment values incorrect"
**Cause**: Manual entry broke symmetry  
**Solution**:
- Use auto-mirror feature (enter left segment, right auto-fills)
- Or manually calculate: right start = 159 - left end, right end = 159 - left start

#### "Visualisation not updating"
**Cause**: WebSocket connection issue  
**Solution**:
- Refresh the page
- Check network connection
- Verify WebSocket endpoint is accessible

### Validation Rules

The system enforces these rules automatically:

1. **Centre-Origin Symmetry**: Left and right segments must be symmetric
2. **No Overlaps**: Zones cannot share LEDs
3. **Complete Coverage**: All LEDs (0-159) must be in exactly one zone
4. **Centre-Outward Ordering**: Zone 0 is innermost, Zone N-1 is outermost
5. **Minimum Size**: Each zone must have at least 2 LEDs total
6. **Centre Inclusion**: At least one zone must include LED 79 or 80

### Getting Help

If you encounter issues not covered here:

1. Check the API documentation for technical details
2. Review the validation error messages (they indicate which rule was violated)
3. Start with a preset layout and modify incrementally
4. Use the serial interface (`validate <zoneId>`) for detailed diagnostics

---

## Examples

### Example 1: Dual Split with Contrasting Effects

**Zone 0 (Inner)**:
- LEDs: 65-79 (left), 80-94 (right)
- Effect: Shockwave
- Palette: Sunset
- Blend: Overwrite

**Zone 1 (Outer)**:
- LEDs: 0-64 (left), 95-159 (right)
- Effect: Fire
- Palette: Ocean
- Blend: Additive

### Example 2: Triple Rings with Layered Blending

**Zone 0 (Innermost)**:
- LEDs: 70-79 (left), 80-89 (right)
- Effect: Holographic Interference
- Palette: Aurora
- Blend: Overwrite

**Zone 1 (Middle)**:
- LEDs: 40-69 (left), 90-119 (right)
- Effect: Standing Wave
- Palette: Sunset
- Blend: Screen

**Zone 2 (Outermost)**:
- LEDs: 0-39 (left), 120-159 (right)
- Effect: Fire
- Palette: Ocean
- Blend: Additive

### Example 3: Quad Active with Per-Zone Palettes

**Zone 0**: Centre focus (LEDs 75-79, 80-84), Shockwave, Custom Palette 1, Overwrite  
**Zone 1**: Inner ring (LEDs 60-74, 85-99), Standing Wave, Custom Palette 2, Screen  
**Zone 2**: Middle ring (LEDs 30-59, 100-129), Fire, Custom Palette 3, Additive  
**Zone 3**: Outer ring (LEDs 0-29, 130-159), Ocean, Custom Palette 4, Overwrite

---

## Advanced Topics

### Dynamic Zone Management

You can change the number of active zones at runtime:
1. Select new zone count from dropdown
2. System generates symmetric layout automatically
3. Customise boundaries as needed
4. Assign effects/palettes/blend modes per zone

### Zone State Persistence

Zone configurations are saved to non-volatile storage (NVS):
- Layouts persist across reboots
- Per-zone settings (effect, palette, blend) are saved
- Restore previous configurations automatically

### API Integration

The Zone Editor uses the LightwaveOS REST and WebSocket APIs:
- `GET /api/v1/zones` - Retrieve current zone configuration
- `POST /api/v1/zones/layout` - Set zone layout
- `POST /api/v1/zones/{id}/effect` - Set zone effect
- `POST /api/v1/zones/{id}/palette` - Set zone palette
- `POST /api/v1/zones/{id}/blend` - Set zone blend mode
- WebSocket: `zones.setLayout`, `zone.setEffect`, `zone.setPalette`, `zone.setBlend`

See the API documentation for complete endpoint details.

---

## Conclusion

The Zone Editor provides powerful tools for creating sophisticated multi-zone lighting effects. Start with simple configurations and gradually explore more complex setups as you become familiar with the system.

Remember:
- Always maintain centre-origin symmetry
- Test incrementally
- Use presets as starting points
- Experiment with blend modes for unique effects

Happy zone editing!

---

**Related Documentation:**
- [API Reference (v1)](/docs/api/API_V1.md)
- [API Reference (v2)](/docs/api/API_V2.md)
- [Effect Catalogue](/docs/effects/)
- [Architecture Overview](/docs/architecture/)

