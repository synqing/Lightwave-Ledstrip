RBDO label: GROUNDED

REST/WS was not the validation dependency.

REST:
- Existing `/api/v1/songAware/config` updated for set/reset/restore/counters reset.
- Existing `/api/v1/songAware/status` supports `view=status|health|debug|policy|allowlist`.
- No unrelated REST path was created for validation.

WS:
- SongAware command set added to existing WS command router.
- `songAware.countersReset` alias added.

Protocol:
- `docs/protocol/k1-rest-contract.yaml` updated.
- `docs/protocol/k1-ws-contract.yaml` updated.
- YAML parse passed for both files.

Classification:
- No REST/WS failure observed.
