# Older machine chat log import handbook (claude-mem)

This is a runbook for an agent to **backfill claude-mem** on an older Mac from a year+ of chat logs (Claude Code, Cursor, and (optionally) Codex/Codex CLI).

## Goal

- Build a repeatable pipeline that:
  - inventories available log sources,
  - exports them into a normalised JSONL format,
  - imports them into claude-mem’s SQLite database (or via whatever claude-mem import API/CLI is available),
  - validates searchability (FTS queries return expected hits).

## Inputs (expected locations on macOS)

### Claude Code (Claude CLI) logs

- `~/.claude/projects/**/*.jsonl`

Each `.jsonl` file is a session log containing events with metadata (`cwd`, `sessionId`, timestamps, etc).

### Cursor composer/chat data

- `~/Library/Application Support/Cursor/User/globalStorage/state.vscdb`

This is a SQLite DB. The large content lives primarily in `cursorDiskKV` under keys like:

- `composerData:<composerId>`
- `bubbleId:<composerId>:<bubbleId>`

### Codex / Codex CLI logs (if present)

- `~/.codex/sessions/**/*.jsonl` (often the richest source)
- `~/.codex/history.jsonl` (may be prompts only; treat as supplementary)

## Pre-flight checklist

1. **Back up** the raw log folders before any processing:
   - `~/.claude/projects/`
   - `~/Library/Application Support/Cursor/User/globalStorage/state.vscdb`
   - `~/.codex/`
2. Confirm free disk space (Cursor DBs can be multiple GB).
3. Decide where exports will live (example: `~/chatlog-exports/`).

## Step 1 — Verify claude-mem is installed and working

On the older machine:

1. Ensure the claude-mem plugin is installed/enabled.
2. Confirm the database exists (common location):
   - `~/.claude-mem/claude-mem.db`
3. If the agent needs schema details, inspect the installed plugin source:
   - `~/.claude/plugins/cache/thedotmack/claude-mem/<version>/`
   - and/or any marketplace checkout:
     - `~/.claude/plugins/marketplaces/thedotmack/`

Minimum validation:

- Run a basic query against the DB with `sqlite3` and confirm tables exist.
- Confirm claude-mem MCP tools can search (if running inside an MCP-capable host).

## Step 2 — Recover any missing importer code (if needed)

If the older machine’s claude-mem installation does not include historical chat-import code, recover what exists from Claude Code logs.

From this repo, use:

- `tools/claude_mem_recovery/extract_embedded_files.py`

Example (recover thedotmack import modules into `recovered/claude-mem-import/`):

```bash
python3 tools/claude_mem_recovery/extract_embedded_files.py \
  --input ~/.claude/projects \
  --output recovered/claude-mem-import \
  --include '/thedotmack/src/services/import/'
```

Notes:

- This works only if your historical logs contain embedded `<outcome>...</outcome>` payloads or structured `toolUseResult` objects with file contents.
- The recovered set may be partial. Treat it as a bootstrap, not a guarantee.

## Step 3 — Export Cursor composer data to JSONL

Use the exporter:

- `tools/claude_mem_recovery/export_cursor_composer.py`

Example:

```bash
python3 tools/claude_mem_recovery/export_cursor_composer.py \
  --output ~/chatlog-exports/cursor_composer.jsonl \
  --emit-meta
```

If you need to target a specific composer conversation while iterating on parsing:

```bash
python3 tools/claude_mem_recovery/export_cursor_composer.py \
  --output ~/chatlog-exports/cursor_one.jsonl \
  --composer-id <composerId> \
  --emit-meta
```

### Cursor DB inspection (manual sanity checks)

List largest Cursor K/V entries (helps confirm where the payloads are):

```bash
sqlite3 "$HOME/Library/Application Support/Cursor/User/globalStorage/state.vscdb" \
  "SELECT key, length(value) FROM cursorDiskKV ORDER BY length(value) DESC LIMIT 30;"
```

Peek at one composer record:

```bash
sqlite3 "$HOME/Library/Application Support/Cursor/User/globalStorage/state.vscdb" \
  "SELECT substr(CAST(value AS TEXT),1,200) FROM cursorDiskKV WHERE key='composerData:<composerId>';"
```

## Step 4 — Decide the import strategy into claude-mem

There are two viable paths; pick one based on what claude-mem exposes on the older machine.

### Option A: Use claude-mem’s own import API/CLI (preferred)

If claude-mem already provides an import endpoint/command, use it. Benefits:

- Schema changes are handled by the project that owns the DB.
- Better deduplication and session mapping (in theory).

Action for the agent:

- Locate existing commands (search under the installed plugin folders for “import”, “scan”, “jsonl”, “cursorDiskKV”, “Cursor”).
- If a CLI exists, run it in `--dry-run`/preview mode first.

### Option B: Write directly to the SQLite DB (fallback)

Only do this if the project has no supported importer.

Agent actions:

1. Inspect DB schema (`.schema`, migrations, FTS triggers).
2. Create a small loader that maps normalised message records to:
   - sessions
   - transcript events
   - observations / prompts (depending on schema)
3. Use transactions + batching (avoid per-row commits).
4. Validate by searching imported content using the DB’s FTS tables.

## Step 5 — Validation (must-have checks)

- Cursor export file exists and is non-empty:
  - `wc -l ~/chatlog-exports/cursor_composer.jsonl`
- Spot-check a few JSONL lines:
  - `head -5 ~/chatlog-exports/cursor_composer.jsonl`
- After import into claude-mem:
  - query counts of sessions / observations / prompts
  - run an FTS search for a known phrase from a chat and confirm it is returned

## Handoff notes for the next agent

When handing off, include:

- Exact log source paths (and whether they were copied or used in-place).
- claude-mem version and DB path.
- Export command(s) used and output locations.
- Any discovered Cursor key formats beyond `composerData:*` and `bubbleId:*`.
- Import method chosen (A or B) and validation queries executed.

