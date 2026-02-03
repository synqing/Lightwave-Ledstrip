# LightwaveOS iOS — API Reference

**This file is superseded by [API_REFERENCE.md](API_REFERENCE.md).**

For the full canonical API documentation (REST endpoints, WebSocket commands, request/response schemas, client usage matrix), see:

→ **[API_REFERENCE.md](API_REFERENCE.md)**

---

## iOS-Specific Gaps (Quick Reference)

When implementing the redesigned UI from `balanced.html`, ensure the iOS app correctly uses:

| Area | Firmware Provides | iOS Fix |
|------|-------------------|---------|
| **Effects** | `categoryId`, `isAudioReactive` | Decode in `EffectsResponse.Effect`, map to `EffectMetadata` |
| **Zones** | `segments[]`, `effectName`, `blendModeName` in zones | Add `segments` to `ZonesResponse`, map to `ZoneSegment`; add `effectName`/`blendModeName` to Zone |
| **Palettes** | `id`, `name`, `category` from API | Load from `GET /api/v1/palettes`, replace hardcoded `PaletteStore` |
| **Zone controls** | REST + WS for effect, palette, blend | Add per-zone dropdowns; use `zone.loadPreset` for preset selector |
| **Zone count** | Via layout or `zone.loadPreset` | Zone count from `segments.length`; presets 0-4 load built-in layouts |

All API details, request/response formats, and client usage are in [API_REFERENCE.md](API_REFERENCE.md).
