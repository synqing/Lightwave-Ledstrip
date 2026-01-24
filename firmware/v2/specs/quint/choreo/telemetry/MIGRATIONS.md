# Telemetry Schema Migrations

**Purpose:** Document schema version changes and migration rules for telemetry format evolution.

---

## Migration Policy

- **Schema versions** are required in all telemetry events (`schemaVersion` field).
- **Breaking changes** require a new schema version (e.g., `1.0.0` → `2.0.0`).
- **Non-breaking changes** (new optional fields) may increment minor version (e.g., `1.0.0` → `1.1.0`).
- **Validators** must support multiple schema versions during transition periods.

---

## Version History

### v1.0.0 (2026-01-19)

**Initial schema:**
- Required fields: `event`, `schemaVersion`
- Event-specific required fields: `ts_mono_ms`, `connEpoch`, `eventSeq`, `clientId`
- `msg.recv` specific: `msgType`, `result`, `reason`

**Field naming:**
- Custom names (`connEpoch`, `clientId`, `ts_mono_ms`) aligned with firmware conventions
- OTel semantic conventions documented for future migration

---

## Future Migrations (Planned)

### v2.0.0 (Future)

**Planned changes:**
- Align field names with OpenTelemetry semantic conventions:
  - `connEpoch` → `session.id` or `lw.conn.epoch`
  - `clientId` → `client.id`
  - `ts_mono_ms` → `monotonic.timestamp`
- Add `schemaUrl` field pointing to schema definition

**Migration strategy:**
- Validator supports both v1.0.0 and v2.0.0 during transition
- Firmware emits `schemaVersion: "1.0.0"` until migration complete
- Trace converters (JSONL→ITF) handle both versions

---

## Migration Checklist

When creating a new schema version:

1. **Document breaking changes** in this file
2. **Update schema JSON** (`schema_v1.json` → `schema_v2.json`)
3. **Update validator** (`validate_telemetry_schema.py`) to support both versions
4. **Update firmware** to emit new `schemaVersion`
5. **Update trace converters** (JSONL→ITF) if field mappings change
6. **Test** with curated traces (should pass validation with old version)

---

## Notes

- **Backward compatibility:** Validators should accept older schema versions during transition periods (minimum 2 versions).
- **Conformance checking:** ITF trace converter and conformance checker should handle schema evolution gracefully.
- **CI enforcement:** Schema validation fails if required fields are missing or `schemaVersion` is unrecognized.
