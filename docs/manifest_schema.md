# Manifest Schema Contract

**Version**: 2 (schema field: `schema: 2`)

This document defines the canonical schema contract for plugin manifest JSON files. It serves as both a human-readable reference and a machine-readable specification for codec implementations.

---

## Schema Versioning

### Version Field

Manifests MUST include a `schema` field (integer) to indicate the schema version:

```json
{
  "schema": 2,
  "version": "1.0",
  ...
}
```

### Version Handling

- **Missing `schema`**: Treated as schema 1 (legacy compatibility)
- **`schema: 1`**: Legacy format (no unknown-key rejection)
- **`schema: 2`**: Current format (strict unknown-key rejection)
- **`schema > 2`**: Rejected with error: `"Unsupported schema version: X"`

### Migration Path

- Schema 1 manifests remain valid but are treated with lenient validation (unknown keys allowed)
- Schema 2 manifests enforce strict validation (unknown keys rejected)
- New manifests SHOULD use `schema: 2`

---

## Schema 2: Complete Specification

### Field Reference

| Field | Type | Required | Default | Constraints | Description |
|-------|------|----------|---------|-------------|-------------|
| `schema` | integer | No | 1 (legacy) | 1 or 2 | Schema version |
| `version` | string | Yes | - | Must be "1.0" | Manifest format version |
| `plugin` | object | Yes | - | Must contain `name` | Plugin metadata container |
| `plugin.name` | string | Yes | - | Max 64 chars, non-empty | Human-readable plugin name |
| `plugin.version` | string | No | null | Semantic versioning | Plugin version |
| `plugin.author` | string | No | null | Max 64 chars | Author name |
| `plugin.description` | string | No | null | Max 256 chars | Plugin description |
| `mode` | string | No | "additive" | "additive" or "override" | Registration mode |
| `effects` | array | Yes | - | Non-empty, max 128 entries | Effect definitions |
| `effects[].id` | integer | Yes | - | 0-127, must exist in BuiltinEffectRegistry | Effect ID |
| `effects[].name` | string | No | null | For documentation only | Human-readable effect name |

### Allowed Keys (Schema 2)

**Root Level**:
- `schema` (integer)
- `version` (string, required)
- `plugin` (object, required)
- `mode` (string, optional)
- `effects` (array, required)

**Within `plugin` Object**:
- `name` (string, required)
- `version` (string, optional)
- `author` (string, optional)
- `description` (string, optional)

**Within `effects[]` Array Elements**:
- `id` (integer, required)
- `name` (string, optional)

### Unknown Key Rejection (Schema 2)

**Rule**: Any key at any nesting level that is not in the allowed keys table above MUST cause decode to fail with a clear error message.

**Examples**:

```json
// ❌ INVALID (unknown root key)
{
  "schema": 2,
  "version": "1.0",
  "plugin": {"name": "Test"},
  "typo": "should reject"
}
// Error: "Unknown key 'typo' at root level"
```

```json
// ❌ INVALID (unknown key in plugin object)
{
  "schema": 2,
  "version": "1.0",
  "plugin": {
    "name": "Test",
    "extra": "should reject"
  }
}
// Error: "Unknown key 'extra' in plugin object"
```

```json
// ✅ VALID (all keys allowed)
{
  "schema": 2,
  "version": "1.0",
  "plugin": {
    "name": "Test Additive Plugin",
    "description": "Sample manifest",
    "author": "LightwaveOS Team"
  },
  "mode": "additive",
  "effects": [
    { "id": 0, "name": "Solid" },
    { "id": 1, "name": "Breathing" }
  ]
}
```

---

## Schema 1: Legacy Format (Backwards Compatibility)

Schema 1 manifests are accepted but do NOT enforce unknown-key rejection. This allows existing manifests to continue working during migration.

### Schema 1 Handling

- Missing `schema` field → treated as schema 1
- `schema: 1` → explicit schema 1
- Unknown keys allowed (no rejection)
- All other validation rules apply (required fields, types, ranges)

---

## Validation Rules (All Schemas)

### Type Checking

- **Required fields**: Must exist AND be the correct type (use `is<T>()` checks)
- **Optional fields**: Use `operator|` on `JsonVariantConst` for defaults
- **Nested objects/arrays**: Read as `JsonObjectConst`/`JsonArrayConst`, explicitly reject null

### Field-Specific Rules

1. **`version`**: Must be exactly `"1.0"` (string)
2. **`plugin.name`**: Must be non-empty string, max 64 characters
3. **`mode`**: Must be `"additive"` or `"override"` (string)
4. **`effects`**: Must be non-empty array, max 128 entries
5. **`effects[].id`**: Must be integer in range 0-127, must exist in `BuiltinEffectRegistry`

### Error Messages

| Error Pattern | Cause | Schema |
|---------------|-------|--------|
| `Unsupported schema version: X` | `schema > 2` | All |
| `Missing required field 'version'` | `version` not present | All |
| `Field 'version' must be a string` | `version` wrong type | All |
| `Unsupported version: X` | `version` not "1.0" | All |
| `Missing required field 'plugin'` | `plugin` not present | All |
| `Field 'plugin' must be an object` | `plugin` wrong type | All |
| `Missing required field 'plugin.name'` | `plugin.name` not present | All |
| `Field 'plugin.name' must be a string` | `plugin.name` wrong type | All |
| `Plugin name too long (max 64 chars)` | `plugin.name` exceeds limit | All |
| `Missing required field 'effects'` | `effects` not present | All |
| `Field 'effects' must be an array` | `effects` wrong type | All |
| `Effects array must not be empty` | `effects` empty | All |
| `Unknown key 'X' at root level` | Unknown root key | 2 only |
| `Unknown key 'X' in plugin object` | Unknown key in plugin | 2 only |
| `Unknown key 'X' in effects array element` | Unknown key in effect | 2 only |
| `Invalid effect ID: X` | ID < 0 or >= 128 | All |
| `Effect ID X not found in built-in registry` | Effect not compiled in | All |

---

## Codec Implementation Contract

The codec layer (`ManifestCodec.h/.cpp`) MUST:

1. Accept `JsonObjectConst` (not `JsonObject`) to prevent accidental mutation
2. Return a typed `ManifestConfig` struct + error string
3. Use `is<T>()` for required fields (checks existence AND type)
4. Use `operator|` on `JsonVariantConst` for defaults
5. Parse nested objects/arrays as `JsonObjectConst`/`JsonArrayConst`, explicitly reject null
6. For schema 2: Iterate all keys at each nesting level and reject unknown keys

### Example Decode Signature

```cpp
struct ManifestDecodeResult {
    bool success;
    ManifestConfig config;
    char errorMsg[128];
};

ManifestDecodeResult ManifestCodec::decode(JsonObjectConst root);
```

---

## Testing Requirements

Test files SHOULD exist in `firmware/v2/test/testdata/`:

- `manifest_v1_valid.json` - Valid schema 1 manifest
- `manifest_v2_valid.json` - Valid schema 2 manifest
- `manifest_missing_schema.json` - Missing schema (should default to v1)
- `manifest_missing_required.json` - Missing required field
- `manifest_unknown_key_v1.json` - Unknown key in schema 1 (should pass)
- `manifest_unknown_key_v2.json` - Unknown key in schema 2 (should fail)
- `manifest_wrong_type.json` - Wrong type for required field
- `manifest_schema_3.json` - Future schema (should reject)

Tests MUST assert:
- Decode succeeds for valid inputs
- Decode fails for wrong types, missing required, unknown keys (schema 2)
- Defaults apply exactly as specified
- Schema version handling (missing → v1, >2 → reject)
