# z_Claude-mem (handoff bundle)

This folder is a self-contained “agent handoff” bundle for backfilling **claude-mem** on an older Mac from historical chat logs (Claude Code, Cursor, and optionally Codex).

## Contents

- Recovery tools:
  - `tools/claude_mem_recovery/extract_embedded_files.py`
  - `tools/claude_mem_recovery/export_cursor_composer.py`
  - `tools/claude_mem_recovery/cursor_to_claude_mem_import.py`
  - `tools/claude_mem_recovery/codex_to_claude_mem_import.py`
  - `tools/claude_mem_recovery/vscode_extension_to_claude_mem_import.py`
  - `tools/claude_mem_recovery/README.md`
- Older-machine runbook:
  - `docs/claude-mem/OLDER_MACHINE_CHATLOG_IMPORT_HANDBOOK.md`
- Recovered claude-mem import modules (if present in your logs):
  - `recovered/claude-mem-import/`
- New-machine replay handoff:
  - `NEW_MACHINE_MERGE_HANDOFF/` (payloads + reports + replay instructions)

## Quick start (older machine)

1) Copy this folder to the older machine (any location is fine), e.g. `~/z_Claude-mem/`.

2) Confirm log source locations exist:

- Claude Code logs: `~/.claude/projects/**/*.jsonl`
- Cursor DB: `~/Library/Application Support/Cursor/User/globalStorage/state.vscdb`
- Codex logs (optional): `~/.codex/sessions/**/*.jsonl` and `~/.codex/history.jsonl`

3) Export Cursor composer data to JSONL (close Cursor first if you hit DB lock errors):

```bash
python3 tools/claude_mem_recovery/export_cursor_composer.py \
  --output ~/chatlog-exports/cursor_composer.jsonl \
  --emit-meta
```

4) (Optional) Recover embedded importer code from Claude Code logs:

```bash
python3 tools/claude_mem_recovery/extract_embedded_files.py \
  --input ~/.claude/projects \
  --output recovered/claude-mem-import \
  --include '/thedotmack/src/services/import/' \
  --include '/thedotmack/src/commands/import-cli\\.ts$'
```

5) Follow the full runbook:

- `docs/claude-mem/OLDER_MACHINE_CHATLOG_IMPORT_HANDBOOK.md`

6) For replaying old-machine payloads into a new machine:

- `NEW_MACHINE_MERGE_HANDOFF/README.md`

## Notes

- This bundle does not install claude-mem for you; it assumes you’ll install/enable claude-mem first on the older machine.
- None of these tools delete or modify source logs; they only read and export/reconstruct into new output files.
