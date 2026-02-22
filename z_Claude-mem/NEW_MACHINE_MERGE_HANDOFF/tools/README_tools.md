# claude-mem recovery tools

Recover missing scripts/modules that were embedded in historical Claude Code chat logs.

## What this does

Some older Claude Code sessions include tool payloads wrapped in `<outcome>...</outcome>` tags. Those payloads can contain **full file contents** for `create`/`update` operations.

`extract_embedded_files.py` scans JSONL chat logs (usually `~/.claude/projects/**/*.jsonl`), decodes embedded outcomes, and writes recovered files into an output directory (plus a `manifest.json`).

## Quick start (local machine)

Recover the claude-mem chat-import modules (thedotmack marketplace) into `recovered/claude-mem-import/`:

```bash
python3 tools/claude_mem_recovery/extract_embedded_files.py \
  --input ~/.claude/projects \
  --output recovered/claude-mem-import \
  --include '/thedotmack/src/services/import/' \
  --include '/thedotmack/src/commands/import-cli\\.ts$'
```

If you want to see what it would recover without writing files:

```bash
python3 tools/claude_mem_recovery/extract_embedded_files.py \
  --input ~/.claude/projects \
  --output recovered/claude-mem-import \
  --include '/thedotmack/src/services/import/' \
  --dry-run
```

## Output layout

Recovered files are written under a portable layout:

- `.../thedotmack/...` paths become `recovered/.../thedotmack/...`
- unknown paths fall back to `recovered/.../abs/...`

`manifest.json` records:

- original path
- output path written (including conflict names)
- SHA-256 hash + size
- JSONL source file + line number + outcome index

## Safety notes

- This tool never deletes anything.
- If the same file path is recovered with different content, it writes a `*.conflict-<hash>` sibling file rather than overwriting.

## Cursor export

Cursor stores composer/chat state in a SQLite DB:

- `~/Library/Application Support/Cursor/User/globalStorage/state.vscdb`

You can export composer bubbles as JSONL:

```bash
python3 tools/claude_mem_recovery/export_cursor_composer.py \
  --output recovered/cursor_composer.jsonl \
  --emit-meta
```

Tip: Use `--composer-id <uuid>` to export a specific conversation if you want to iterate on parsing.

## Cursor import into claude-mem

Convert exported Cursor JSONL into claude-mem import payloads:

```bash
python3 tools/claude_mem_recovery/cursor_to_claude_mem_import.py \
  --input ~/chatlog-exports/cursor_composer_full.jsonl
```

Actually ingest into a running claude-mem worker (`/api/import`):

```bash
python3 tools/claude_mem_recovery/cursor_to_claude_mem_import.py \
  --input ~/chatlog-exports/cursor_composer_full.jsonl \
  --post
```

## Cursor recovery audit

To measure what is truly recoverable vs metadata-only:

```bash
python3 tools/claude_mem_recovery/audit_cursor_recovery.py \
  --out-json ~/chatlog-exports/cursor_recovery_audit.json
```

This audit reports:

- total known session IDs (global + workspace metadata),
- sessions with recoverable transcript content,
- metadata-only sessions,
- first-pass vs second-pass export recovery delta.

## Codex import into claude-mem

Convert Codex session logs into claude-mem import payloads:

```bash
python3 tools/claude_mem_recovery/codex_to_claude_mem_import.py \
  --input-glob '~/.codex/sessions/**/*.jsonl'
```

Actually ingest into a running claude-mem worker (`/api/import`):

```bash
python3 tools/claude_mem_recovery/codex_to_claude_mem_import.py \
  --input-glob '~/.codex/sessions/**/*.jsonl' \
  --post
```

## VS Code extension chat import

Import historical VS Code extension chats (Claude-Dev and Blackbox):

```bash
python3 tools/claude_mem_recovery/vscode_extension_to_claude_mem_import.py \
  --post
```

This reads:

- `~/Library/Application Support/Code/User/globalStorage/saoudrizwan.claude-dev/tasks/*/api_conversation_history.json`
- `~/Library/Application Support/Code/User/globalStorage/blackboxapp.blackboxagent/tasks/*/api_conversation_history.json`

## Source inventory

Scan known IDE/CLI locations and generate an inventory report:

```bash
python3 tools/claude_mem_recovery/discover_chat_sources.py \
  --output ~/chatlog-exports/chat_sources_inventory.json
```
