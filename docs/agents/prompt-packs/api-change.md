# Prompt Pack: API Change

Use this when adding, removing, or consuming REST, WebSocket, or SerialJSON commands.

## First Contact Prompt

```text
GROUNDED:

You are changing or consuming K1 protocol behaviour. Start by reading:
- CLAUDE.md top RBDO gate and Protocol Contract section.
- docs/protocol/k1-rest-contract.yaml if REST is in scope.
- docs/protocol/k1-ws-contract.yaml if WS is in scope.
- docs/protocol/README.md.
- firmware-v3/tools/contract-regen/README.md if regeneration or drift tooling is in scope.

Task:
1. Verify branch, HEAD, and dirty tree.
2. Identify the protocol owner: firmware runtime, YAML contract, iOS, Tab5, dashboard, or tool.
3. Update the protocol YAML before implementation when adding/modifying REST or WS commands.
4. Preserve backwards compatibility unless Captain explicitly approves breakage.
5. Keep AP-only constraints in mind; do not assume LAN/STA reachability.
6. Add or update tests/harnesses for codec, route, or client behaviour.
7. Document consumers.

Forbidden:
- adding WS/REST routes that are absent from YAML;
- changing field names without consumer audit;
- treating YAML drift as runtime truth without checking firmware;
- introducing STA validation or WiFi-mode work by accident.

Return:
- contract diff;
- implementation diff;
- consumer impact;
- validation commands;
- unresolved drift.
```
