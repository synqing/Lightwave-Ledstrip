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

## Canon Pack extraction (quality-gated)

Build an evidence-first Canon Pack from global claude-mem without modifying DB contents:

```bash
python3 z_Claude-mem/tools/claude_mem_recovery/build_canon_pack.py \
  --out-dir z_Claude-mem/canon_pack_obs_only \
  --min-chars 100 \
  --min-evidence-per-claim 2 \
  --min-sessions-per-claim 2
```

Outputs:

- `metadata.json` (quality gate settings + run metadata)
- `evidence.jsonl` (excerpts with IDs/timestamps/projects)
- `alias_map.json`
- `claims.json` (typed/statused/confidence-scored claims + citations)
- `canon_pack.md` (human-readable pack)

Quality contract enforced by the extractor:

- no DB writes (read-only analysis)
- claims require citation support threshold
- claims require multi-session evidence
- low-signal prompt/tool noise is filtered
- if support is missing, sections state `No supporting excerpts found`

## Canon phase compilers (citation-first)

Compile the Canon Pack using strict phased outputs:

1. Alias Map
2. Constitution
3. Product Spec
4. AS-IS Architecture Atlas
5. Decision Register
6. Canon readiness report (pass/fail gates)

```bash
python3 z_Claude-mem/tools/claude_mem_recovery/compile_canon_phases.py \
  --out-dir z_Claude-mem/canon_pack_pipeline
```

Key behaviour:

- session-local context stitching for short observations (default: `<120` chars, `±2` window)
- primary evidence required for claims (supplemental evidence is context only)
- claim gates enforce citation quality (`>=180` chars or explicit identifiers)
- `first_seen` / `last_seen` timestamps on claims and decisions
- explicit `current` / `contested` / `superseded` statuses
- missing support is emitted as `No supporting excerpts found.` (no gap-filling)
- decision register defaults to strict curated themes (coverage-oriented dynamic clustering is opt-in)

To include dynamic clustered decisions in addition to strict themes:

```bash
python3 z_Claude-mem/tools/claude_mem_recovery/compile_canon_phases.py \
  --out-dir z_Claude-mem/canon_pack_pipeline \
  --include-dynamic-decisions
```

Outputs:

- `metadata.json`
- `phase_index.json`
- `alias_map.json`, `alias_map.md`
- `constitution_claims.json`, `constitution.md`
- `product_spec_claims.json`, `product_spec.md`
- `architecture_claims.json`, `architecture_atlas.md`
- `decision_register.json`, `decision_register.md`
- `canon_readiness.json`, `canon_readiness.md`
- `evidence_candidates.jsonl`
