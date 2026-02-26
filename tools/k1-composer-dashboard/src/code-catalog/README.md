# Code Catalogue

This directory maps firmware effect sources to dashboard visualiser metadata.

## Generated pipeline

Primary artefacts are generated from real firmware source files:

- `generated/index.json`
- `generated/effects/<EID_...>.json`

Generate with:

```bash
cd tools/k1-composer-dashboard
python3 ./scripts/generate_effect_catalog.py
python3 ./scripts/validate_effect_catalog.py --strict
```

## Schema v2 fields

Each generated entry carries:

- `effectId`, `effectIdHex`, `className`, `displayName`
- `headerPath`, `sourcePath`
- `renderRange` (1-based line numbers)
- `phaseRanges` (`input/mapping/modulation/render/post/output`)
- `mappingConfidence`, `mappingWarnings`
- `sourceText` (full `.cpp` source)
- `firmwareSourceHash` for stale detection

## Seed fallback

`effects.json` remains as a compatibility fallback only when generated artefacts are unavailable.
