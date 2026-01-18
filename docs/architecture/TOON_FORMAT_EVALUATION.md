# TOON Format Evaluation for Lightwave-Ledstrip

**Date**: 2026-01-18  
**Status**: Decision Document  
**Recommendation**: Not Recommended for Runtime Use

---

## Executive Summary

**Verdict**: TOON (Token-Oriented Object Notation) is **not recommended** for runtime API communication in Lightwave-Ledstrip firmware. The format's primary benefit (token efficiency for LLM input) does not apply to standard REST/WebSocket APIs consumed by React dashboards and encoder controllers.

**Recommended Use**: TOON is best treated as a **prompt codec** for LLM input, not a wire format. Use it upstream when preparing large uniform arrays (effects, zones, palettes) for Claude agent prompts via the TOON CLI tool, without changing firmware code.

---

## Prompt Codec, Not Wire Format

TOON's own documentation frames it as a **translation layer**: keep JSON for machines, encode as TOON for LLM input. This distinction is critical:

| Aspect | Wire Format | Prompt Codec |
|--------|-------------|--------------|
| **Purpose** | Machine-to-machine communication | LLM input optimisation |
| **Consumers** | Browsers, APIs, firmware | Claude, ChatGPT, Cursor agents |
| **Benefit** | Bandwidth, parsing speed | Token count, retrieval accuracy |
| **TOON fit** | Poor (adds complexity) | Excellent (40% fewer tokens) |

**Structure-dependent savings**: TOON's token efficiency varies by data shape:
- **Uniform arrays** (effects lists, palettes, zones): ~40% fewer tokens, best fit
- **Deeply nested / non-uniform structures**: Savings shrink; compact JSON may be more efficient
- **Flat tabular data**: CSV can be slightly more token-efficient than TOON, but lacks nesting support

**Rule of thumb**: Use TOON for large uniform arrays in LLM prompts. Use compact JSON for nested configs. Use CSV only for purely flat tables.

---

## What TOON Provides

### Core Capabilities
- **Token-efficient JSON encoding**: ~40% fewer tokens than standard JSON
- **Lossless conversion**: Round-trip compatible with JSON data model
- **Tabular array format**: CSV-like compactness for uniform arrays
- **Schema-aware**: Explicit `[N]` lengths and `{fields}` headers

### Benchmarks (from TOON repo)
- **73.9% accuracy** vs JSON's 69.7% (on LLM retrieval tasks)
- **39.6% fewer tokens** than standard JSON
- Best for: Uniform arrays of objects (effects lists, zones, palettes)

---

## Current Lightwave-Ledstrip JSON Usage

### 1. REST API Responses
**Location**: `firmware/v2/src/network/webserver/handlers/`
- **Effects list**: Paginated effects with metadata
- **Zones state**: Array of zone configurations
- **Palettes list**: Large arrays of palette data
- **Status updates**: Device state, FPS, CPU metrics
- **Format**: Standard JSON via ArduinoJson `serializeJson()`
- **Consumers**: React/TypeScript dashboard, encoder controllers

### 2. WebSocket Messages
**Location**: `firmware/v2/src/network/WebServer.cpp`
- **Status broadcasts**: Device state updates (`doBroadcastStatus()`)
- **Command responses**: Effect changes, parameter updates
- **Stream data**: Binary frames for LED/audio streams (not JSON)
- **Format**: Standard JSON via `StaticJsonDocument<512>`
- **Consumers**: Web dashboard WebSocket clients

### 3. Multi-Device Sync
**Location**: `firmware/v2/src/sync/`
- **State serialization**: `StateSerializer.cpp` - Manual `snprintf`-based compact JSON (~450 bytes)
- **Command serialization**: `CommandSerializer.cpp` - Single-char keys for compactness
- **Format**: Already manually optimized JSON (not standard ArduinoJson)
- **Consumers**: Other LightwaveOS devices over WebSocket

### 4. Development/Agent Workflows
**Location**: Documentation (`AGENTS.md`)
- **LLM usage**: Claude-Flow for code generation, documentation, design
- **Not runtime**: LLMs are used during development, not for firmware operations
- **TOON potential**: Could reduce tokens when sending API examples to LLMs in agent workflows

---

## Assessment: Use Cases

### ❌ NOT Suitable: Runtime API Communication

**Why**: TOON is optimized for LLM comprehension, but firmware APIs are consumed by:
- **React/TypeScript dashboard** - Expects standard JSON, has native `JSON.parse()`
- **WebSocket clients** - Standard JSON parsers in browsers
- **Encoder firmware** - ArduinoJson deserialization (no TOON support)

**Drawbacks**:
1. **No browser/TypeScript support** - Dashboard would need TOON decoder library
2. **ESP32 memory constraints** - Adding TOON encoder/decoder increases firmware size
3. **Complexity vs. benefit** - ArduinoJson is already lightweight and well-integrated
4. **Breaking change** - Would require updating all API consumers

**Example Comparison**:

JSON (current):
```json
{
  "success": true,
  "data": {
    "effects": [
      {"id": 0, "name": "Fire", "category": "Classic"},
      {"id": 1, "name": "Water", "category": "Wave"}
    ]
  }
}
```

TOON equivalent (saves ~40% tokens, but only matters for LLM input):
```
success: true
data:
  effects[2]{id,name,category}:
    0,Fire,Classic
    1,Water,Wave
```

---

### ⚠️ Marginal: Development/Agent Workflows

**Potential benefit**: When agents process API response examples:
- Sending `/api/v1/effects` response to Claude for code generation
- Documentation generation with API examples
- Test case generation from API responses

**Limitations**:
1. **Development-time only** - Not runtime benefit
2. **Agent-specific** - Only relevant for Claude/LLM workflows
3. **Implementation cost** - Would need C++ TOON implementation for firmware examples

**Alternative**: Use TOON CLI tool (`npx @toon-format/cli`) to convert JSON→TOON when preparing examples for agent prompts, **without changing firmware code**.

**Example workflow**:
```bash
# Generate API example for agent prompt
curl http://lightwave.local/api/v1/effects | npx @toon-format/cli > examples/effects.toon

# Use in agent prompt (reduces tokens when sending to Claude)
```

---

### ❌ NOT Suitable: Multi-Device Sync

**Why**: Already using manually optimized compact JSON:
- `StateSerializer.cpp` uses single-char keys (`"e"`, `"p"`, `"b"` instead of `"effectId"`, `"paletteId"`, `"brightness"`)
- Manual `snprintf` serialization (no library overhead)
- Target: ~450 bytes for full state sync

**Current optimization**:
```cpp
// StateSerializer.cpp - Already compact
written += snprintf(outBuffer + written, bufferSize - written,
    "\"e\":%u,\"p\":%u,\"b\":%u,\"sp\":%u,\"h\":%u,",
    state.currentEffectId, state.currentPaletteId, state.brightness, ...
);
```

**TOON wouldn't help**: TOON's benefit is token efficiency (for LLMs), not byte efficiency. Manual JSON is already byte-optimized.

---

## Implementation Considerations

### Required Changes (if adopted)

1. **Firmware (C++)**:
   - **C++ TOON library**: Would need to port/implement TOON encoder/decoder
   - **Current libraries**: TOON has official implementations for TypeScript, Python, Go, Rust, .NET - **no C++ yet**
   - **Memory cost**: Encoder/decoder code would add ~5-10KB to firmware
   - **Integration**: Replace ArduinoJson calls with TOON encoding

2. **Dashboard (TypeScript)**:
   - **TOON decoder**: Would need `@toon-format/toon` library
   - **API client changes**: Update `lightwave-dashboard/src/services/v2/api.ts` to decode TOON
   - **Type safety**: TOON decode still returns JSON-compatible objects (TypeScript types unchanged)

3. **Encoder Firmware**:
   - **Similar changes**: Update WebSocket client to decode TOON responses
   - **Memory impact**: Additional decoder code on constrained M5Stack AtomS3

### Memory Constraints

**ESP32-S3 Memory Budget** (from `constraints.md`):
- **Total SRAM**: 320KB (hardware limit)
- **Available after boot**: ~280KB
- **LED buffers**: 960 bytes (required)
- **JSON buffers**: Variable (`StaticJsonDocument<512>` in status broadcasts)

**Impact**: Adding TOON encoder/decoder (~5-10KB) is feasible but adds complexity without clear benefit for runtime use.

---

## Decision & Rationale

### Do NOT adopt TOON for runtime APIs

**Reasons**:
1. **Wrong use case**: TOON optimises for LLM token efficiency, but firmware APIs serve standard web clients
2. **C++ implementations exist but not worthwhile**: Community implementations (e.g., `ctoon`) make it *feasible*, but the integration + maintenance + breaking-change surface is not justified for runtime use
3. **Breaking changes**: Would require updating dashboard, encoder firmware, and all API consumers
4. **Minimal benefit**: Byte savings don't matter (WiFi bandwidth is sufficient), and token savings only matter for LLM input
5. **Complexity cost**: ArduinoJson is lightweight, well-integrated, and sufficient

### Recommended: Use TOON as a Prompt Codec

**Where TOON shines**: Upstream in the LLM prompt pipeline, not in runtime APIs.

**When useful**: Converting JSON API responses to TOON format when:
- Preparing examples for Claude agent prompts (effects lists, palettes, zones)
- Documenting API responses in markdown
- Generating test fixtures from real API responses
- Stuffing large uniform arrays into LLM context windows

**How**: Use TOON CLI tool (`npx @toon-format/cli`) to convert JSON→TOON for agent/documents, **without changing firmware code**.

**Format selection guide**:
| Data Shape | Recommended Format | Reason |
|------------|-------------------|--------|
| Large uniform arrays | TOON | ~40% fewer tokens, better LLM retrieval |
| Deeply nested / non-uniform | Compact JSON | TOON savings shrink; JSON may be more efficient |
| Purely flat tabular | CSV | Slightly more token-efficient than TOON |

**See also**: [`docs/contextpack/README.md`](../contextpack/README.md) for the Context Pack pipeline that integrates TOON fixtures into delta-only prompting workflows.

---

## Conclusion

TOON is an excellent format for **LLM input optimisation**, but Lightwave-Ledstrip's runtime JSON usage is for **standard web APIs**, not LLM prompts. The token efficiency benefit doesn't translate to runtime value, and adoption would add complexity without meaningful benefit.

**Final Verdict**:
- **Runtime APIs**: Not recommended. Keep JSON.
- **LLM prompts**: Recommended as a prompt codec. Use TOON for large uniform arrays (effects, zones, palettes) via the CLI tool or Context Pack pipeline.

---

## References

- TOON specification: https://github.com/toon-format/toon
- Current JSON serialization: `firmware/v2/src/network/webserver/handlers/EffectHandlers.cpp`
- Memory constraints: `constraints.md`
- API architecture: `firmware/v2/src/network/WebServer.cpp`
- Multi-device sync: `firmware/v2/src/sync/StateSerializer.cpp`
