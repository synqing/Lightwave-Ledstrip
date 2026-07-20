# MarkItDown Profile: Lightwave-Ledstrip

Profile status: GROUNDED. This profile is a documentation/tooling surface only; it does not change firmware behaviour, build profiles, runtime contracts, or WiFi policy.

Purpose: convert firmware architecture docs, NotebookLM bundles, project trackers, and selected Office artefacts into reviewed Markdown packets for agents.

Use the global tool. Do not install MarkItDown inside this repo.

```bash
markitdown --version
```

## Priority Sources

- `firmware-v3/docs/`
- `docs/tooling/notebooklm-bundles/`
- `_archive/K1_Technical_Architecture_V1.2.docx`
- `_archive/K1-Master-Tracker.xlsx`
- `tab5-encoder/docs/`
- `zone-mixer/docs/`

## Default Workflow

Use `.markitdown/cache/` for disposable conversion output:

```bash
mkdir -p .markitdown/cache
markitdown _archive/K1_Technical_Architecture_V1.2.docx -o .markitdown/cache/K1_Technical_Architecture_V1.2.markitdown.md
sed -n '1,120p' .markitdown/cache/K1_Technical_Architecture_V1.2.markitdown.md
```

Promote only reviewed, useful outputs into `docs/markitdown/generated/`.

## Guardrails

- This profile is for documentation and ingestion only. It must not be used as authority over current source, contracts, or runtime state.
- Read `CLAUDE.md`, `AGENTS.md`, and relevant reference docs before using converted content for tactical firmware work.
- Keep British English in generated or curated summaries.
- Reject empty output and flag malformed tables before using converted Markdown.
- Keep `.claude/worktrees`, `.embedder`, build artefacts, captures, and local semantic caches out of routine conversion.
- Do not enable OCR, plugins, or cloud-backed extraction without explicit approval.
