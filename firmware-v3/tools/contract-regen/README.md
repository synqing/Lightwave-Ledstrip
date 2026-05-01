# contract-regen

Drift detectors that compare firmware source-of-truth against the YAML
protocol contracts under `docs/protocol/`. Failing CI here means the
contract and the implementation have diverged — fix one or the other.

## Tools

| Tool | Compares | Status |
|------|----------|--------|
| `regen_rest.py` | `firmware-v3/src/network/webserver/V1ApiRoutes.cpp` against `docs/protocol/k1-rest-contract.yaml` | active |
| `regen_ws.py`   | `firmware-v3/src/network/webserver/ws/Ws*Commands.cpp` against `docs/protocol/k1-ws-contract.yaml` | active |

## Tests

```sh
python3 -m unittest discover firmware-v3/tools/contract-regen/tests
```

## REST regenerator

`regen_rest.py` parses every `registry.on{Get,Post,Patch,Put,Delete}` and
matching `*Regex` call from `V1ApiRoutes.cpp`, normalises regex routes
(`^\/api\/v1\/zones\/([0-3])$` → `/api/v1/zones/{id}`,
`^\/api\/v1\/presets\/([^/]+)$` → `/api/v1/presets/{name}`), and compares
the result against the contract YAML. Single-line `//` comments are
honoured so commented-out registrations are not falsely counted.

### Usage

```sh
# Default — print a human-readable drift report. Always exits 0.
python3 firmware-v3/tools/contract-regen/regen_rest.py

# CI mode — exit non-zero if any drift is detected.
python3 firmware-v3/tools/contract-regen/regen_rest.py --strict

# Skeleton mode — write <contract>.regen.yaml with FIXME placeholders for
# every firmware-only route, ready for a human to fill in and merge.
python3 firmware-v3/tools/contract-regen/regen_rest.py --update-skeleton
```

Override the inputs explicitly with `--cpp <path>` and `--yaml <path>`
(useful for tests, CI matrices, and local experiments).

### Output categories

* **Firmware-only** — route registered in `V1ApiRoutes.cpp` but missing
  from the YAML. Either the contract needs an entry or the registration
  was a mistake. Regex-derived routes carry a `[from regex: …]` suffix
  so the original literal is recoverable.
* **Contract-only** — route declared in YAML but no matching `registry.on*`
  call. Either the implementation is missing or the contract entry is
  stale and should be removed.

### Path normalisation

Numeric capture groups (`[0-9]+`, `[0-3]`, etc.) are rewritten as `{id}`;
all other captures (notably `[^/]+`) are rewritten as `{name}`. This
matches the convention used in `k1-rest-contract.yaml` as of 2026-05-01.

## WS regenerator

`regen_ws.py` parses every `WsCommandRouter::registerCommand("…", handler)`
call across `firmware-v3/src/network/webserver/ws/Ws*Commands.cpp`, loads
the `commands:` map from `docs/protocol/k1-ws-contract.yaml`, and reports
drift in either direction. Block (`/* … */`) and line (`//`) comments are
honoured so commented-out registrations are not falsely counted.

### Usage

```sh
# Default — print a human-readable drift report. Always exits 0.
python3 firmware-v3/tools/contract-regen/regen_ws.py

# CI mode — exit non-zero if any drift is detected.
python3 firmware-v3/tools/contract-regen/regen_ws.py --strict

# Skeleton mode — write <contract>.regen.yaml with TODO placeholders for
# every firmware-only command, ready for a human to fill in and merge.
python3 firmware-v3/tools/contract-regen/regen_ws.py --update-skeleton

# Suppress the textual report (still respects --strict for the exit code).
python3 firmware-v3/tools/contract-regen/regen_ws.py --strict --quiet
```

### Output categories

* **Firmware-only** — command registered in C++ but missing from the YAML.
  Either the contract needs an entry or the registration was a mistake.
* **Contract-only** — command declared in YAML but no matching
  `registerCommand` call. Either the implementation is missing or the
  contract entry is stale and should be removed.

### Aliases

Some commands are deliberately registered under more than one name (for
example, both `setEffect` and `effects.setCurrent` route to the same
handler). Each registered name is treated independently — both must
appear in the contract or both will be flagged.

### Dependencies

Stdlib + PyYAML only. Install with `pip install pyyaml` if missing.
