---
abstract: "Protocol contract files for K1 WebSocket and REST APIs. Read before modifying any network code in firmware-v3, tab5-encoder, or lightwave-ios-v2."
---

# LightwaveOS Protocol Contracts

This directory contains the **source-of-truth** protocol contracts for communication between K1 firmware, Tab5 encoder, and the iOS companion app.

## Files

| File | Purpose |
|------|---------|
| `k1-ws-contract.yaml` | Every WebSocket command, broadcast, and event — with request/response schemas, consumers, and known issues |
| `k1-rest-contract.yaml` | Every REST API endpoint — method, path, description, and auth requirements |
| `README.md` | This file |

## The Rule

**Update the contract FIRST, implement SECOND.**

When adding, modifying, or removing any network command or endpoint:

1. Edit the relevant `.yaml` contract file
2. Implement the change in firmware
3. Update the consumer (Tab5 / iOS) to match
4. Commit contract and implementation together

This prevents protocol drift — the single largest source of silent failures in the K1 ecosystem.

## How to Check for Drift

Until an automated lint script exists, use these manual checks:

### WebSocket commands registered on K1 but not in contract

```bash
grep -oP 'registerCommand\("\K[^"]+' firmware-v3/src/network/webserver/ws/*.cpp | sort > /tmp/k1_ws_commands.txt
grep -oP '^\s+\K[\w.]+(?=:)' docs/protocol/k1-ws-contract.yaml | grep -v '^#' | sort > /tmp/contract_ws_commands.txt
diff /tmp/k1_ws_commands.txt /tmp/contract_ws_commands.txt
```

### REST routes registered on K1 but not in contract

```bash
grep -oP 'registry\.on\w+\("\K[^"]+' firmware-v3/src/network/webserver/V1ApiRoutes.cpp | sort -u > /tmp/k1_rest_routes.txt
grep -oP '^\s+\K/api[^:]+' docs/protocol/k1-rest-contract.yaml | sort -u > /tmp/contract_rest_routes.txt
diff /tmp/k1_rest_routes.txt /tmp/contract_rest_routes.txt
```

### Commands Tab5 sends but K1 does not handle

```bash
grep -oP 'sendJSON\("\K[^"]+' tab5-encoder/src/network/WebSocketClient.cpp | sort -u > /tmp/tab5_sends.txt
grep -oP 'registerCommand\("\K[^"]+' firmware-v3/src/network/webserver/ws/*.cpp | sort -u > /tmp/k1_handles.txt
comm -23 /tmp/tab5_sends.txt /tmp/k1_handles.txt
```

## Contract Statistics (2026-03-21)

| Metric | Count |
|--------|-------|
| Active WS commands | 141 |
| WS broadcasts | 7 distinct types |
| REST endpoints | ~110 routes |
| Tab5 sent command types | 29 |
| Tab5 handled message types | 12 |
| Known deprecated commands | 3 |
| Known issues | 2 |

## Future Work

- Automated drift-detection script (`scripts/protocol-lint.sh`)
- CI check that rejects PRs modifying network code without contract updates
- JSON Schema generation from YAML contracts for runtime validation

---

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-03-21 | agent:network-api-engineer | Created initial protocol contracts from source extraction |
