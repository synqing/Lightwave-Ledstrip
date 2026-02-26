# K1 Composer Telemetry Schema v1

## Envelope

```json
{
  "event": "control.lease.acquired",
  "ts_mono_ms": 1209000,
  "schemaVersion": "1.0.0",
  "leaseId": "cl_9f4a2137c1d6",
  "source": "ws",
  "ownerWsClientId": 12
}
```

Security rule:

- Telemetry must never include `leaseToken`, `X-Control-Lease`, or `X-Control-Lease-Id` values.
- Dashboard logs must redact secret fields before rendering or persistence.

## Event Set

1. `control.lease.acquired`
2. `control.lease.heartbeat`
3. `control.lease.released`
4. `control.lease.expired`
5. `control.lease.rejected_locked`
6. `control.command.blocked.ws`
7. `control.command.blocked.rest`
8. `control.command.blocked.local.encoder`
9. `control.command.blocked.local.serial`
10. `render.diagnostics.window`
11. `render.frame.drop_ratio_window`

## Required Blocked-Event Fields

| Field | Type |
|---|---|
| `command` | string |
| `source` | string (`ws`, `rest`, `encoder`, `serial`) |
| `ownerClientName` | string |
| `remainingMs` | uint32 |
| `reason` | string |

## Device Status Counters

`/api/v1/device/status` and WS `status` payloads expose:

- `controlCounters.blockedWsCommands`
- `controlCounters.blockedRestRequests`
- `controlCounters.blockedLocalEncoderInputs`
- `controlCounters.blockedLocalSerialInputs`
- `controlCounters.lastLeaseEventMs`

JSON schema file: `tools/k1-composer-dashboard/telemetry.schema.json`

## Diagnostics Metrics

`render.diagnostics.window` payload fields:

- `bufferedAmountNow`
- `bufferedAmountP95`
- `bufferedAmountMax`
- `sendCadenceHz`
- `sendJitterMs`
- `dropRatio`
- `attempted`, `sent`, `dropped`
- `windowMs`

`render.frame.drop_ratio_window` payload fields:

- `dropRatio10s`
- `dropRatio60s`
- `attempted10s`
- `attempted60s`
