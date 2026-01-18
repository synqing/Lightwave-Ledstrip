# Plugin System Reference

Complete canonical reference for the LightwaveOS v2 plugin system, manifest schema, SPIFFS layout, and effect registration.

---

## Table of Contents

1. [Plugin System Architecture](#plugin-system-architecture)
2. [SPIFFS Filesystem Layout](#spiffs-filesystem-layout)
3. [Manifest Schema](#manifest-schema)
4. [Effect ID Reference](#effect-id-reference)
5. [Registration Modes](#registration-modes)
6. [Manifest Examples](#manifest-examples)
7. [SPIFFS Management](#spiffs-management)
8. [Built-in Effect Registry](#built-in-effect-registry)
9. [API Integration](#api-integration)
10. [Troubleshooting](#troubleshooting)

---

## Plugin System Architecture

The plugin system provides a unified mechanism for managing IEffect instances with dynamic registration from SPIFFS manifests.

### Components

**PluginManagerActor**:
- Central registry manager (Core 0, priority 2)
- Maintains up to 128 registered IEffect instances
- Loads plugins from SPIFFS on startup
- Coordinates effect registration with RendererActor

**BuiltinEffectRegistry**:
- Static storage for compiled-in effect instances
- Maps effect IDs to IEffect pointers
- Populated during `registerAllEffects()`

**IEffectRegistry Interface**:
- Unified registration API (`registerEffect(uint8_t id, IEffect* effect)`)
- Implemented by both `PluginManagerActor` and `RendererActor`

### Registration Flow

```
Built-in Effects → BuiltinEffectRegistry → PluginManagerActor → RendererActor
                                         ↑
                                    SPIFFS Manifests (optional)
```

1. `registerAllEffects()` registers all compiled effects
2. `BuiltinRegistryAdapter` stores effects in `BuiltinEffectRegistry`
3. Effects are registered with `PluginManagerActor` (or `RendererActor` directly if `FEATURE_PLUGIN_RUNTIME=0`)
4. `PluginManagerActor::loadPluginsFromSPIFFS()` scans SPIFFS for manifests
5. Manifests reference built-in effects by ID from `BuiltinEffectRegistry`
6. Plugin effects are forwarded to `RendererActor` for rendering

---

## SPIFFS Filesystem Layout

**Base Path**: `/` (SPIFFS root)

**Directory Structure**:
```
/
├── *.plugin.json          # Plugin manifest files (any name ending in .plugin.json)
├── /plugins/              # (Optional) Organised plugin directory
│   └── *.plugin.json      # Plugin manifests in subdirectory
└── (Web assets on LittleFS, not SPIFFS)
```

### Filesystem Notes

- **SPIFFS and LittleFS are separate filesystems** on ESP32-S3
- **SPIFFS**: Plugin manifests only (`.plugin.json` files)
- **LittleFS**: Web dashboard assets (`/index.html`, `/app.js`, `/styles.css`)
- Plugin manifests must end with `.plugin.json` (case-sensitive)
- Multiple manifests allowed (all are scanned and merged)
- Max path length: 64 characters (`PluginConfig::SPIFFS_PLUGIN_PATH_MAX`)

### Mounting

- SPIFFS mounted in `PluginManagerActor::loadPluginsFromSPIFFS()`
- Mount point: `/` (root)
- Auto-format: `false` (preserves existing files)

---

## Manifest Schema

**Version**: `1.0` (current)

**Complete Schema**:
```json
{
  "version": "1.0",              // Manifest format version (required)
  "plugin": {                    // Plugin metadata (required)
    "name": "string",            // Plugin name (required, max 64 chars)
    "version": "string",         // Plugin version (optional, semantic versioning)
    "author": "string",          // Author name (optional, max 64 chars)
    "description": "string"      // Plugin description (optional, max 256 chars)
  },
  "mode": "additive|override",   // Registration mode (optional, default: "additive")
  "effects": [                    // Effect list (required, non-empty)
    {
      "id": 42,                   // Effect ID 0-127 (required)
      "force": false              // Force registration if already registered (optional, default: false)
    }
  ]
}
```

### Schema Fields

| Field | Type | Required | Default | Description |
|-------|------|----------|---------|-------------|
| `version` | string | Yes | - | Manifest format version ("1.0") |
| `plugin.name` | string | Yes | - | Human-readable plugin name (max 64 chars) |
| `plugin.version` | string | No | "1.0" | Plugin version (semantic versioning) |
| `plugin.author` | string | No | null | Author name (max 64 chars) |
| `plugin.description` | string | No | null | Plugin description (max 256 chars) |
| `mode` | string | No | "additive" | Registration mode: "additive" (add to built-ins) or "override" (whitelist only) |
| `effects` | array | Yes | - | Non-empty array of effect definitions |
| `effects[].id` | integer | Yes | - | Effect ID 0-127 (must exist in BuiltinEffectRegistry) |
| `effects[].force` | boolean | No | false | Overwrite existing registration if true |

### Manifest Size

- Max manifest size: 2048 bytes (defined by `kManifestCapacity`)
- JSON parsing uses `StaticJsonDocument<2048>`

---

## Registration Modes

### Additive Mode (default)

**Behaviour**: Plugin effects are added to built-in effects. All built-ins remain available.

**Use Case**: Extend the default effect set with additional selections.

**Example**:
```json
{
  "version": "1.0",
  "plugin": {"name": "My Favourites"},
  "effects": [{"id": 0}, {"id": 42}]
}
```

**Result**: All 98 built-in effects remain registered. Effects 0 and 42 are explicitly listed but already registered.

### Override Mode

**Behaviour**: Plugin manifests act as allowlists. Only effects listed in manifests are registered. Built-ins not listed are disabled.

**Use Case**: Restrict the effect set to a minimal curated list for performance or UI simplicity.

**Example**:
```json
{
  "version": "1.0",
  "plugin": {"name": "Minimal Set"},
  "mode": "override",
  "effects": [{"id": 0}, {"id": 1}, {"id": 15}]
}
```

**Result**: Only effects 0, 1, and 15 are registered. All other built-ins (2-14, 16-97) are unregistered.

### Multiple Manifests

- All `.plugin.json` files are scanned
- Effects are merged (union) across manifests
- **Mode conflicts**: If ANY manifest uses `mode: "override"`, override mode is enabled globally
- **Effect ID conflicts**: Later files win (or `force: true` overwrites)

**Example**: Two manifests, one override:
```
/plugin-a.plugin.json:
{
  "version": "1.0",
  "plugin": {"name": "Set A"},
  "mode": "override",
  "effects": [{"id": 0}]
}

/plugin-b.plugin.json:
{
  "version": "1.0",
  "plugin": {"name": "Set B"},
  "mode": "additive",
  "effects": [{"id": 1}]
}

Result: Override mode enabled globally (Set A wins).
Only effect 0 registered. Effect 1 ignored.
```

---

## Effect ID Reference

**Current Effect Count**: 98 effects (IDs 0-97)

**Effect Categories**:

| Range | Category | Count | Effects |
|-------|----------|-------|---------|
| 0-12 | Core | 13 | Fire, Ocean, Plasma, Confetti, Sinelon, Juggle, BPM, Wave Ambient, Ripple, Heartbeat, Interference, Breathing, Pulse |
| 13-17 | LGP Interference | 5 | Box Wave, Holographic, Modal Resonance, Interference Scanner, Wave Collision |
| 18-25 | LGP Geometric | 8 | Diamond Lattice, Hexagonal Grid, Spiral Vortex, Sierpinski, Chevron Waves, Concentric Rings, Star Burst, Mesh Network |
| 26-33 | LGP Advanced | 8 | Moire Curtains, Radial Ripple, Holographic Vortex, Evanescent Drift, Chromatic Shear, Modal Cavity, Fresnel Zones, Photonic Crystal |
| 34-39 | LGP Organic | 6 | Aurora Borealis, Bioluminescent Waves, Plasma Membrane, Neural Network, Crystalline Growth, Fluid Dynamics |
| 40-49 | LGP Quantum | 10 | Quantum Tunneling, Gravitational Lensing, Time Crystal, Soliton Waves, Metamaterial Cloak, GRIN Cloak, Caustic Fan, Birefringent Shear, Anisotropic Cloak, Evanescent Skin |
| 50-59 | LGP Colour Mixing | 10 | Color Temperature, RGB Prism, Complementary Mixing, Quantum Colors, Doppler Shift, Color Accelerator, DNA Helix, Phase Transition, Chromatic Aberration, Perceptual Blend |
| 60-64 | LGP Novel Physics | 5 | Chladni Harmonics, Gravitational Wave Chirp, Quantum Entanglement, Mycelial Network, Riley Dissonance |
| 65-67 | LGP Chromatic | 3 | Chromatic Lens, Chromatic Pulse, Chromatic Interference |
| 68-87 | Audio-Reactive | 20 | Audio Test, Beat Pulse, Spectrum Bars, Bass Breath, Audio Waveform, Audio Bloom, Star Burst Narrative, Chord Glow, Wave Reactive, Perlin Veil, Perlin Shocklines, Perlin Caustics, Perlin Interference Weave, Perlin Veil Ambient, Perlin Shocklines Ambient, Perlin Caustics Ambient, Perlin Interference Weave Ambient, Perlin Backend Test A/B/C |
| 88-97 | Enhanced Audio-Reactive | 10 | BPM Enhanced, Breathing Enhanced, Chevron Waves Enhanced, Ripple Enhanced, Interference Scanner Enhanced, Photonic Crystal Enhanced, Star Burst Enhanced, Wave Collision Enhanced, Spectrum Detail, Spectrum Detail Enhanced |

**Note**: Effect IDs are stable - they match PatternRegistry indices and must not change.

---

## Manifest Examples

### Example 1: Minimal Manifest (Additive Mode)

```json
{
  "version": "1.0",
  "plugin": {
    "name": "My Custom Set"
  },
  "effects": [
    {"id": 0},
    {"id": 42},
    {"id": 99}
  ]
}
```

**Result**: All 98 built-ins remain registered. Effects 0, 42, and 99 are explicitly listed (99 does not exist in built-ins, so it will fail to register with a warning).

### Example 2: Override Mode (Whitelist)

```json
{
  "version": "1.0",
  "plugin": {
    "name": "Minimal Effect Set",
    "version": "1.0.0",
    "author": "User",
    "description": "Only essential effects enabled"
  },
  "mode": "override",
  "effects": [
    {"id": 0, "force": true},
    {"id": 1, "force": true},
    {"id": 15, "force": true}
  ]
}
```

**Result**: Only effects 0, 1, and 15 are registered. All other built-ins (2-14, 16-97) are unregistered.

### Example 3: Multi-Plugin Setup (Additive)

```
/plugin-a.plugin.json:
{
  "version": "1.0",
  "plugin": {"name": "Core Set"},
  "effects": [{"id": 0}, {"id": 1}]
}

/plugin-b.plugin.json:
{
  "version": "1.0",
  "plugin": {"name": "LGP Set"},
  "effects": [{"id": 13}, {"id": 14}]
}

Result: All four effects (0, 1, 13, 14) registered additively.
All 98 built-ins remain registered.
```

### Example 4: Override Conflict

```
/plugin-a.plugin.json:
{
  "version": "1.0",
  "plugin": {"name": "Set A"},
  "mode": "override",
  "effects": [{"id": 0}]
}

/plugin-b.plugin.json:
{
  "version": "1.0",
  "plugin": {"name": "Set B"},
  "mode": "additive",
  "effects": [{"id": 1}]
}

Result: Override mode enabled globally (Set A wins).
Only effect 0 registered. Effect 1 ignored.
```

---

## SPIFFS Management

### File Naming

- Must end with `.plugin.json`
- Case-sensitive
- Max path length: 64 characters

### Uploading Manifests

**Via PlatformIO**:
```bash
cd firmware/v2
pio run -e esp32dev_audio -t uploadfs  # Upload SPIFFS image
```

**Via Web UI**: (Future: OTA manifest upload endpoint)

**Via Serial Commands**: (Future: `SPIFFS.write` command)

### Scanning Order

Manifests are scanned in filesystem order (not guaranteed to be alphabetical). If order matters for effect registration, use `force: true` in later manifests.

---

## Built-in Effect Registry

### Purpose

Maps effect IDs to compiled IEffect instances for lookup during plugin manifest loading.

### Registration

Automatically populated during `registerAllEffects()` via `BuiltinRegistryAdapter`:

```cpp
BuiltinRegistryAdapter registryAdapter(registry);
registryAdapter.registerEffect(id, effect);  // Stores in BuiltinEffectRegistry
target->registerEffect(id, effect);          // Also forwards to PluginManagerActor
```

### Lookup

```cpp
IEffect* builtin = BuiltinEffectRegistry::getBuiltin(uint8_t id);
// Returns IEffect* or nullptr if not registered
```

### Limitations

- Only compiled-in effects available (no dynamic loading)
- Effect instances are static singletons (shared across registrations)
- Future: Support compiled plugin libraries (.so/.dll-style loading)

---

## API Integration

### REST API (Future Endpoints)

- `GET /api/v1/plugins` - List loaded plugins
- `GET /api/v1/plugins/manifests` - List manifest files
- `POST /api/v1/plugins/reload` - Reload manifests from SPIFFS

### WebSocket Events (Future)

- `plugin.loaded` - Plugin manifest loaded
- `plugin.error` - Plugin load error
- `effects.changed` - Effect list changed (override mode)

### Statistics

`PluginStats` struct tracks:
- `registeredCount`: Currently registered effects
- `loadedFromSPIFFS`: Effects loaded from SPIFFS at startup
- `registrationsFailed`: Failed registration attempts
- `unregistrations`: Total unregistration count
- `overrideModeEnabled`: Whether override mode is active (future)
- `disabledByOverride`: Count of effects disabled by override mode (future)

---

## Troubleshooting

### Common Issues

1. **SPIFFS mount fails**
   - **Symptom**: `LW_LOGW("SPIFFS mount failed - plugin loading skipped")`
   - **Cause**: SPIFFS partition not configured or corrupted
   - **Solution**: Check partition table, ensure SPIFFS partition exists (e.g., `platformio.ini` has `board_build.filesystem = spiffs`)

2. **Manifest parse error**
   - **Symptom**: `LW_LOGW("Plugin manifest parse failed (%s): %s", name, err.c_str())`
   - **Cause**: Invalid JSON syntax or encoding
   - **Solution**: Validate JSON syntax (use JSONLint), ensure UTF-8 encoding, check for missing commas or quotes

3. **Effect ID not found**
   - **Symptom**: `LW_LOGW("Plugin effect id %u not found in built-in registry", id)`
   - **Cause**: Effect ID out of range (≥98) or not registered in BuiltinEffectRegistry
   - **Solution**: Verify effect exists in built-in registry (IDs 0-97), check `CoreEffects.cpp` registration order

4. **Override mode not working**
   - **Symptom**: All built-ins still registered after override manifest
   - **Cause**: Mode spelling ("override", not "overwrite"), or multiple manifests with conflicting modes
   - **Solution**: Check mode spelling in manifest, verify only one manifest uses override mode (or ensure override wins)

5. **Manifests not scanned**
   - **Symptom**: No plugins loaded despite SPIFFS files present
   - **Cause**: File suffix mismatch (must be `.plugin.json`, case-sensitive)
   - **Solution**: Verify file names end with `.plugin.json` exactly (not `.Plugin.json` or `.plugin.JSON`)

6. **Effect ID out of range**
   - **Symptom**: `LW_LOGW("Plugin effect id out of range (%u): %s", idValue, name)`
   - **Cause**: Effect ID ≥ 128 (max is 127)
   - **Solution**: Use valid effect IDs (0-97 for current built-ins, 98-127 reserved for future)

7. **Empty effects array**
   - **Symptom**: `LW_LOGW("Plugin manifest missing effects array: %s", name)`
   - **Cause**: Manifest has empty or missing `effects` array
   - **Solution**: Ensure `effects` is a non-empty array with at least one `{"id": ...}` entry

### Debug Logging

Enable verbose logging to diagnose plugin loading:

```cpp
// In PluginManagerActor.cpp, loadPluginsFromSPIFFS():
LW_LOGD("Scanning SPIFFS for plugin manifests...");
LW_LOGD("Found manifest: %s", name);
LW_LOGD("Plugin mode: %s", mode.c_str());
LW_LOGI("Override mode ENABLED (manifest: %s)", name);
LW_LOGI("Override mode: %u effects enabled, %u disabled", enabled, disabled);
```

---

## Version History

- **v1.0** (current): Initial manifest format with additive and override modes

---

**Last Updated**: 2025-01-18
