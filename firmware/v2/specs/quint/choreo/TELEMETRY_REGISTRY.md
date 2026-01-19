# Telemetry Attribute Registry

**Purpose:** Define telemetry schema for LightwaveOS v2 WebSocket protocol traces, aligned with OpenTelemetry semantic conventions.

**Schema Version:** 1.0.0  
**Last Updated:** 2026-01-19

---

## Schema Versioning

Telemetry events include `schemaVersion` (or `schemaUrl`) to support controlled evolution:

- **v1.0.0** (current): Initial schema with required fields
- **Migration rules:** See [`telemetry/MIGRATIONS.md`](telemetry/MIGRATIONS.md)

---

## Event Types

### `ws.connect`

**Description:** WebSocket connection attempt.

**Required Attributes:**
- `event` (string): `"ws.connect"`
- `ts_mono_ms` (number): Monotonic timestamp (ms since boot)
- `connEpoch` (number): Connection epoch (increments on reconnect)
- `eventSeq` (number): Monotonic event sequence number
- `clientId` (number): WebSocket client ID
- `schemaVersion` (string): Schema version (e.g., `"1.0.0"`)

**Optional Attributes:**
- `ip` (string): Client IP address

**OpenTelemetry Alignment:**
- `clientId` → `client.id` (custom, but OTel-aligned naming)
- `ts_mono_ms` → `monotonic.timestamp` (custom monotonic clock)

---

### `ws.connected`

**Description:** WebSocket handshake completed.

**Required Attributes:**
- `event` (string): `"ws.connected"`
- `ts_mono_ms` (number)
- `connEpoch` (number)
- `eventSeq` (number)
- `clientId` (number)
- `schemaVersion` (string)

---

### `ws.disconnect`

**Description:** WebSocket connection closed.

**Required Attributes:**
- `event` (string): `"ws.disconnect"`
- `ts_mono_ms` (number)
- `connEpoch` (number)
- `eventSeq` (number)
- `clientId` (number)
- `schemaVersion` (string)

---

### `msg.recv`

**Description:** Message received from client.

**Required Attributes:**
- `event` (string): `"msg.recv"`
- `ts_mono_ms` (number)
- `connEpoch` (number)
- `eventSeq` (number)
- `clientId` (number)
- `msgType` (string): Message type (empty string `""` if unknown/parse-error)
- `result` (string): `"ok"` or `"rejected"`
- `reason` (string): Rejection reason (empty string `""` if `result == "ok"`)

**Rejection Reasons:**
- `"rate_limit"`: Message rejected due to rate limiting
- `"size_limit"`: Message rejected due to size limit
- `"parse_error"`: Message rejected due to JSON parse error
- `"auth_failed"`: Message rejected due to authentication failure
- `""` (empty): No rejection (only used when `result == "ok"`)

**Optional Attributes:**
- `payloadSummary` (string): Truncated payload summary (~100 chars max)

**Schema Version:** Must include `schemaVersion` (string)

---

### `msg.send`

**Description:** Message sent to client.

**Required Attributes:**
- `event` (string): `"msg.send"`
- `ts_mono_ms` (number)
- `connEpoch` (number)
- `eventSeq` (number)
- `clientId` (number)
- `msgType` (string): Message type
- `schemaVersion` (string)

**Optional Attributes:**
- `payloadSummary` (string)

---

## OpenTelemetry Alignment

**Current Status:** Telemetry uses custom field names (`connEpoch`, `clientId`, `ts_mono_ms`) aligned with existing firmware conventions.

**Future Migration:** Consider aligning with OTel semantic conventions:
- `connEpoch` → `session.id` or `lw.conn.epoch` (custom namespace)
- `clientId` → `client.id`
- `ts_mono_ms` → `monotonic.timestamp` (custom, since OTel uses wall-clock by default)

**Recommendation:** Keep custom names for now (v1.0.0), but document OTel mappings for future schema versions.

---

## Validation

Telemetry traces are validated against this schema in CI:
- **Validator:** [`tools/validate_telemetry_schema.py`](tools/validate_telemetry_schema.py)
- **CI Workflow:** [`.github/workflows/telemetry_schema_check.yml`](.github/workflows/telemetry_schema_check.yml)

---

## Notes

- **Monotonic timestamps:** `ts_mono_ms` uses device boot time (not wall-clock), which is appropriate for embedded systems without NTP.
- **Empty strings:** `msgType` and `reason` may be empty strings (`""`) when not applicable (e.g., parse errors).
- **Schema versioning:** All events must include `schemaVersion` for forward compatibility.
