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
