# New Machine claude-mem Merge Handoff

This package contains all artifacts required to merge/import recovered chat knowledge from the old machine into claude-mem on the new machine.

## Package Layout

- `artifacts/cursor_composer_full.jsonl`
  - Raw Cursor export (for importer-based chunked replay).
- `artifacts/cursor_import_payload.json`
- `artifacts/codex_import_payload.json`
- `artifacts/vscode_extension_import_payload.json`
- `artifacts/claude_code_import_payload.json`
  - Direct `/api/import` payloads ready to POST.
- `artifacts/cursor_recovery_audit.json`
  - Recoverability audit proving Cursor metadata-only vs transcript sessions.
- `artifacts/recovery_completion_report.json`
  - Final counts from old machine import validation.
- `artifacts/chat_sources_inventory.json`
  - Source discovery inventory from old machine.
- `tools/*.py`
  - Import tooling if you prefer script-based replay.

## Pre-flight on New Machine

1. Ensure claude-mem is installed and worker is running.
2. Confirm endpoint is reachable: `http://127.0.0.1:37777/api/import`.
3. Back up new-machine DB before merge:
   - `~/.claude-mem/claude-mem.db`
   - `~/.claude-mem/claude-mem.db-wal`
   - `~/.claude-mem/claude-mem.db-shm`

## Recommended Merge Method (direct payload replay)

Run from this handoff folder on the new machine:

```bash
python3 - <<'PY'
import json, urllib.request, pathlib
base = pathlib.Path('artifacts')
for name in [
  'cursor_import_payload.json',
  'codex_import_payload.json',
  'vscode_extension_import_payload.json',
  'claude_code_import_payload.json',
]:
    p = base / name
    payload = json.loads(p.read_text(encoding='utf-8'))
    req = urllib.request.Request(
      'http://127.0.0.1:37777/api/import',
      data=json.dumps(payload).encode('utf-8'),
      headers={'Content-Type': 'application/json'},
    )
    with urllib.request.urlopen(req, timeout=300) as resp:
      out = json.loads(resp.read())
    print(name, out.get('stats'))
PY
```

## Alternative Merge Method (script replay)

```bash
python3 tools/cursor_to_claude_mem_import.py --input artifacts/cursor_composer_full.jsonl --post
```

(Use script replay if you want chunked Cursor import behavior.)

## Post-merge validation

```bash
sqlite3 ~/.claude-mem/claude-mem.db "SELECT count(*) FROM sdk_sessions; SELECT count(*) FROM observations; SELECT count(*) FROM user_prompts;"
```

```bash
sqlite3 ~/.claude-mem/claude-mem.db "SELECT count(*) FROM observations_fts WHERE observations_fts MATCH 'LightwaveOS';"
```

## Key conclusion from old machine

- Cursor known IDs: 917
- Recoverable full transcript sessions: 42
- Metadata-only/non-recoverable sessions: 875

These figures are in `artifacts/cursor_recovery_audit.json`.
