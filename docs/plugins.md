# Plugin System Reference

Complete canonical reference for the LightwaveOS v2 plugin system, manifest schema, LittleFS layout, and effect registration.

---

## Table of Contents

1. [Plugin System Architecture](#plugin-system-architecture)
2. [LittleFS Filesystem Layout](#littlefs-filesystem-layout)
3. [Manifest Schema](#manifest-schema)
4. [Effect ID Reference](#effect-id-reference)
5. [Registration Modes](#registration-modes)
6. [Manifest Examples](#manifest-examples)
7. [LittleFS Management](#littlefs-management)
8. [Built-in Effect Registry](#built-in-effect-registry)
9. [API Integration](#api-integration)
10. [Atomic Reload](#atomic-reload)
11. [Troubleshooting](#troubleshooting)

---

## Plugin System Architecture

The plugin system provides a unified mechanism for managing IEffect instances with dynamic registration from LittleFS manifests.

### Components

**PluginManagerActor**:
- Central registry manager
- Maintains up to 128 registered IEffect instances
- Loads plugins from LittleFS on startup
- Supports atomic reload at runtime
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
                                    LittleFS Manifests (optional)
```

1. `registerAllEffects()` registers all compiled effects
2. `BuiltinRegistryAdapter` stores effects in `BuiltinEffectRegistry`
3. Effects are registered with `PluginManagerActor` (or `RendererActor` directly if `FEATURE_PLUGIN_RUNTIME=0`)
4. `PluginManagerActor::loadPluginsFromLittleFS()` scans LittleFS for manifests
5. Manifests reference built-in effects by ID from `BuiltinEffectRegistry`
6. Plugin effects are forwarded to `RendererActor` for rendering

---

## LittleFS Filesystem Layout

**Base Path**: `/` (LittleFS root)

**Directory Structure**:
```
/
├── *.plugin.json          # Plugin manifest files (any name ending in .plugin.json)
├── index.html             # Web dashboard
├── app.js                 # Dashboard JavaScript
├── styles.css             # Dashboard styles
└── (other web assets)
```

### Filesystem Notes

- **LittleFS** is used for both plugin manifests and web assets
- Plugin manifests must end with `.plugin.json` (case-sensitive)
- Multiple manifests allowed (all are scanned and merged)
- Max path length: 64 characters (`PluginConfig::LITTLEFS_PLUGIN_PATH_MAX`)
- Max manifest size: 2048 bytes (`PluginConfig::MANIFEST_CAPACITY`)

### Mounting

- LittleFS mounted in `PluginManagerActor::loadPluginsFromLittleFS()`
- Mount point: `/` (root)
- Auto-format: `false` (preserves existing files)

---

## Manifest Schema

**Version**: `1.0` (current)

### Naming Convention

- Files must end with `.plugin.json` (case-sensitive)
- Examples: `my-effects.plugin.json`, `override.plugin.json`, `test-additive.plugin.json`
- Invalid: `plugin.json`, `my-effects.PLUGIN.json`, `my-effects.plugin.JSON`

### Complete Schema

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
      "name": "Effect Name"       // Human-readable name (optional, for documentation)
    }
  ]
}
```

### Schema Fields

| Field | Type | Required | Default | Constraints | Description |
|-------|------|----------|---------|-------------|-------------|
| `version` | string | Yes | - | Must be "1.0" | Manifest format version |
| `plugin` | object | Yes | - | Must contain `name` | Plugin metadata container |
| `plugin.name` | string | Yes | - | Max 64 chars, non-empty | Human-readable plugin name |
| `plugin.version` | string | No | "1.0" | Semantic versioning | Plugin version |
| `plugin.author` | string | No | null | Max 64 chars | Author name |
| `plugin.description` | string | No | null | Max 256 chars | Plugin description |
| `mode` | string | No | "additive" | "additive" or "override" | Registration mode |
| `effects` | array | Yes | - | Non-empty, max 128 entries | Effect definitions |
| `effects[].id` | integer | Yes | - | 0-127, must exist in BuiltinEffectRegistry | Effect ID |
| `effects[].name` | string | No | null | For documentation only | Human-readable effect name |

### Validation Rules

1. **JSON Syntax**: Must be valid JSON (no trailing commas, proper quoting)
2. **Version Check**: `version` must be exactly `"1.0"`
3. **Plugin Object**: Must have a `plugin` object with a non-empty `name`
4. **Effects Array**: Must have a non-empty `effects` array
5. **Effect IDs**: Each effect must have an `id` in range 0-127
6. **Effect Existence**: Each effect ID must exist in `BuiltinEffectRegistry`

### Error Messages

| Error | Cause | Solution |
|-------|-------|----------|
| `JSON parse error: ...` | Invalid JSON syntax | Validate JSON with JSONLint |
| `Unsupported version: X` | Version not "1.0" | Change version to "1.0" |
| `Missing 'plugin' object` | No plugin metadata | Add `"plugin": {"name": "..."}` |
| `Missing plugin name` | Empty or missing name | Add non-empty name |
| `Missing or empty 'effects' array` | No effects defined | Add at least one effect |
| `Invalid effect ID: X` | ID < 0 or >= 128 | Use ID in range 0-127 |
| `Effect ID X not found in built-in registry` | Effect not compiled in | Use valid built-in effect ID |

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

**Result**: All built-in effects remain registered. Effects 0 and 42 are explicitly listed.

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

**Result**: Only effects 0, 1, and 15 are registered. All other built-ins are unregistered.

### Multiple Manifests

- All `.plugin.json` files are scanned
- Effects are merged (union) across manifests
- **Mode conflicts**: If ANY manifest uses `mode: "override"`, override mode is enabled globally
- **Effect ID conflicts**: All unique effect IDs are combined

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
    {"id": 0, "name": "Solid"},
    {"id": 42, "name": "Quantum Tunneling"}
  ]
}
```

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
    {"id": 0, "name": "Solid"},
    {"id": 1, "name": "Breathing"},
    {"id": 15, "name": "Interference Scanner"}
  ]
}
```

### Example 3: Full Metadata

```json
{
  "version": "1.0",
  "plugin": {
    "name": "LGP Showcase",
    "version": "2.1.0",
    "author": "LightwaveOS Team",
    "description": "Curated selection of Light Guide Plate interference effects"
  },
  "mode": "additive",
  "effects": [
    {"id": 13, "name": "Box Wave"},
    {"id": 14, "name": "Holographic"},
    {"id": 15, "name": "Modal Resonance"},
    {"id": 16, "name": "Interference Scanner"},
    {"id": 17, "name": "Wave Collision"}
  ]
}
```

---

## LittleFS Management

### File Naming

- Must end with `.plugin.json`
- Case-sensitive
- Max path length: 64 characters

### Uploading Manifests

**Device Port Mapping**:
- **ESP32-S3 (v2 firmware)**: `/dev/cu.usbmodem1101`
- **Tab5 (encoder firmware)**: `/dev/cu.usbmodem101`

**Via PlatformIO** (ESP32-S3 on usbmodem1101):
```bash
cd firmware/v2
# Place manifest files in data/ directory
pio run -e esp32dev_audio -t uploadfs --upload-port /dev/cu.usbmodem1101  # Upload LittleFS image
```

**Workflow**:
1. Create your `.plugin.json` file
2. Place it in `firmware/v2/data/` directory
3. Run `pio run -e esp32dev_audio -t uploadfs --upload-port /dev/cu.usbmodem1101` (ESP32-S3 on usbmodem1101)
4. Restart device or call reload API

### Scanning Order

Manifests are scanned in filesystem order (not guaranteed to be alphabetical). All valid manifests are merged.

---

## Built-in Effect Registry

### Purpose

Maps effect IDs to compiled IEffect instances for lookup during plugin manifest loading.

### Registration

Automatically populated during `registerAllEffects()`:

```cpp
BuiltinEffectRegistry::registerBuiltin(id, effect);  // Stores in static registry
```

### Lookup

```cpp
IEffect* builtin = BuiltinEffectRegistry::getBuiltin(uint8_t id);
// Returns IEffect* or nullptr if not registered
```

### Limitations

- Only compiled-in effects available (no dynamic loading)
- Effect instances are static singletons (shared across registrations)

---

## API Integration

### REST API

| Endpoint | Method | Description |
|----------|--------|-------------|
| `GET /api/v1/plugins` | GET | List plugin stats (registeredCount, loadedFromLittleFS, overrideModeEnabled, lastReloadOk, errorCount) |
| `GET /api/v1/plugins/manifests` | GET | List manifest files with validation status (`{file, valid, error?, name?, mode?}`) |
| `POST /api/v1/plugins/reload` | POST | Trigger atomic reload from LittleFS, returns stats + errors |

### WebSocket Commands

| Command | Response | Description |
|---------|----------|-------------|
| `plugins.list` | `plugins.list` | List registered effect IDs |
| `plugins.stats` | `plugins.stats` | Get plugin statistics including reload status |
| `plugins.reload` | `plugins.reload.result` | Trigger atomic reload, returns stats + errors |

### Statistics

`PluginStats` struct tracks:
- `registeredCount`: Currently registered effects
- `loadedFromLittleFS`: Effects loaded from manifests
- `registrationsFailed`: Failed registration attempts
- `unregistrations`: Total unregistration count
- `overrideModeEnabled`: Whether override mode is active
- `disabledByOverride`: Count of effects disabled by override mode
- `lastReloadMillis`: Timestamp of last reload attempt
- `lastReloadOk`: Whether last reload succeeded
- `manifestCount`: Number of manifest files found
- `errorCount`: Number of manifests with errors
- `lastErrorSummary`: Summary of last error (max 128 chars)

---

## Atomic Reload

### Why Atomic?

The plugin system uses atomic reload to ensure consistency:
- **All-or-nothing**: Either all manifests are valid and applied, or none are
- **No partial state**: Invalid manifests don't corrupt the active effect set
- **Safe rollback**: Previous state preserved on any error

### Reload Process

1. **Scan**: Find all `*.plugin.json` files in LittleFS root
2. **Parse**: Parse each manifest into `ParsedManifest` struct
3. **Validate**: Check JSON syntax, version, required fields, effect IDs
4. **Decision**: If ANY manifest is invalid, abort and keep previous state
5. **Apply**: If ALL valid, atomically swap active manifest set
6. **Stats**: Record reload timestamp, success/fail, error count

### Error Handling

- **JSON parse error**: Manifest marked invalid, error message stored
- **Missing fields**: Manifest marked invalid, specific field noted
- **Invalid effect ID**: Manifest marked invalid, ID noted
- **Effect not in registry**: Manifest marked invalid, ID noted

### API Response Example

**Success**:
```json
{
  "success": true,
  "data": {
    "reloadSuccess": true,
    "stats": {
      "registeredCount": 98,
      "loadedFromLittleFS": 3,
      "overrideModeEnabled": false,
      "lastReloadOk": true,
      "lastReloadMillis": 1705612800000,
      "manifestCount": 1,
      "errorCount": 0
    },
    "errors": []
  }
}
```

**Failure**:
```json
{
  "success": true,
  "data": {
    "reloadSuccess": false,
    "stats": {
      "registeredCount": 98,
      "lastReloadOk": false,
      "lastReloadMillis": 1705612800000,
      "manifestCount": 2,
      "errorCount": 1
    },
    "errors": [
      {"file": "/bad.plugin.json", "error": "Effect ID 999 not found in built-in registry"}
    ]
  }
}
```

---

## Troubleshooting

### Common Issues

1. **LittleFS mount fails**
   - **Symptom**: `LW_LOGW("LittleFS mount failed - plugin loading skipped")`
   - **Cause**: LittleFS partition not configured or corrupted
   - **Solution**: Check partition table, ensure LittleFS partition exists

2. **Manifest parse error**
   - **Symptom**: `JSON parse error: ...`
   - **Cause**: Invalid JSON syntax or encoding
   - **Solution**: Validate JSON syntax (use JSONLint), ensure UTF-8 encoding

3. **Effect ID not found**
   - **Symptom**: `Effect ID X not found in built-in registry`
   - **Cause**: Effect ID out of range (≥98) or not registered
   - **Solution**: Verify effect exists (IDs 0-97), check `CoreEffects.cpp`

4. **Override mode not working**
   - **Symptom**: All built-ins still registered after override manifest
   - **Cause**: Mode spelling ("override", not "overwrite")
   - **Solution**: Check mode spelling in manifest

5. **Manifests not scanned**
   - **Symptom**: No plugins loaded despite LittleFS files present
   - **Cause**: File suffix mismatch (must be `.plugin.json`, case-sensitive)
   - **Solution**: Verify file names end with `.plugin.json` exactly

6. **Reload fails with valid manifests**
   - **Symptom**: Reload returns false but manifests look correct
   - **Cause**: One manifest has an error that blocks all
   - **Solution**: Check `GET /api/v1/plugins/manifests` for per-file status

### Debug Logging

Enable verbose logging to diagnose plugin loading:

```cpp
// In PluginManagerActor.cpp:
LW_LOGD("Found manifest: %s", path);
LW_LOGI("Plugin mode: %s", mode);
LW_LOGI("Override mode: %u effects allowed, %u disabled", allowed, disabled);
```

---

## Version History

- **v1.0** (current): Initial manifest format with additive and override modes, atomic reload

---

**Last Updated**: 2025-01-18
