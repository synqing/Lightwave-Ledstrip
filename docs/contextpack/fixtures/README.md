# Context Pack Fixtures

**Purpose**: Data fixtures for LLM prompts, formatted for token efficiency.

---

## Format Selection

| Data Shape | Format | Extension | When to Use |
|------------|--------|-----------|-------------|
| **Large uniform arrays** | TOON | `.toon` | Effects lists, palettes, zones (40-80% fewer tokens) |
| **Purely flat tabular** | CSV | `.csv` | Simple key-value data, logs (slightly more efficient than TOON) |
| **Nested / non-uniform** | JSON | `.json` | Config objects, complex structures (TOON savings shrink) |

---

## Naming Convention

```
{domain}_{description}.{format}

Examples:
  effects_list.toon       # All effects with metadata
  zones_config.toon       # Zone configurations
  palettes_master.toon    # Master palette list
  build_errors.csv        # Build error log
  audio_config.json       # Nested audio configuration
```

---

## Auto-TOONification

When you run `python tools/contextpack.py`:

1. JSON files in this directory are scanned
2. Eligible files (uniform arrays of objects, ≥5 items) are converted to TOON
3. Original JSON is preserved (source of truth)
4. Token savings are reported in `token_report.md`

### Eligibility Criteria

JSON is converted to TOON only when:
- Top-level is an **array**
- Each element is an **object**
- Keys are **consistent** across all elements
- Array has **≥5 items** (configurable)

### Skip Conversion

To skip auto-conversion:
```bash
python tools/contextpack.py --no-toonify
```

---

## TOON Format Reference

TOON (Token-Oriented Object Notation) is a compact encoding for LLM prompts.

### Basic Syntax

```toon
[count]{key1,key2,key3}:
  value1,value2,value3
  value1,value2,value3
```

### Example

**JSON**:
```json
[
  {"id": 1, "name": "Breathing", "speed": 50},
  {"id": 2, "name": "Pulse", "speed": 75}
]
```

**TOON**:
```toon
[2]{id,name,speed}:
  1,Breathing,50
  2,Pulse,75
```

### When NOT to Use TOON

- Deeply nested structures
- Non-uniform objects (different keys per item)
- Small arrays (< 5 items)
- Complex values (nested objects, long strings with commas)

---

## Manual TOON Conversion

```bash
# Install CLI (one-time)
npm --prefix tools install

# Convert
npm --prefix tools exec -- toon input.json -o output.toon

# Get stats
npm --prefix tools exec -- toon input.json --stats
```

---

## See Also

- [Context Pack README](../README.md) - Full pipeline documentation
- [TOON Format Evaluation](../../architecture/TOON_FORMAT_EVALUATION.md) - Decision document
- [TOON Official Docs](https://toonformat.dev/) - Format specification
