#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: tools/cc-audit-collector.sh [--output-dir DIR] [--project-dir DIR] [--days N] [--dry-run]

Collect aggregate-only Claude/Codex audit metrics for Lightwave-Ledstrip.
This script never prints raw prompts, tool bodies, settings values, environment values, or transcript rows.

Options:
  --output-dir DIR   Output directory for aggregate files.
  --out DIR          Alias for --output-dir.
  --project-dir DIR  Claude project transcript directory.
  --days N           Recent Codex session window, default 30.
  --dry-run          Alias for normal aggregate collection; no settings are mutated either way.
  -h, --help         Show help.
EOF
}

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
project_dir="$HOME/.claude/projects/-Users-spectrasynq-Workspace-Management-Software-Lightwave-Ledstrip"
output_dir="/tmp/lightwave-cc-audit-$(date +%Y-%m-%d)"
recent_days=30

while [[ $# -gt 0 ]]; do
  case "$1" in
    --output-dir|--out)
      output_dir="${2:?--output-dir requires a value}"
      shift 2
      ;;
    --project-dir)
      project_dir="${2:?--project-dir requires a value}"
      shift 2
      ;;
    --days)
      recent_days="${2:?--days requires a value}"
      shift 2
      ;;
    --dry-run)
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown option: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
done

if ! command -v jq >/dev/null 2>&1; then
  echo "jq is required" >&2
  exit 1
fi

if ! command -v rg >/dev/null 2>&1; then
  echo "rg is required" >&2
  exit 1
fi

if [[ ! -d "$project_dir" ]]; then
  echo "Claude project transcript directory missing: $project_dir" >&2
  exit 1
fi

mkdir -p "$output_dir"

preflight="$output_dir/preflight-post-prune.txt"
usage_tsv="$output_dir/token-cache-live-post-prune.tsv"
hook_cache="$output_dir/hook-cache-live-post-prune.txt"

{
  echo "=== preflight post-prune ==="
  date '+%Y-%m-%d %H:%M:%S %z'
  cd "$repo_root"
  pwd
  git status --short
  command -v jq
  jq --version

  echo
  echo "=== instruction counts ==="
  wc -l -w "$HOME/.claude/CLAUDE.md" CLAUDE.md .claude/CLAUDE.md AGENTS.md docs/WORKFLOW_ROUTING.md 2>/dev/null || true

  echo
  echo "=== claude hooks ==="
  jq '.hooks // {} | to_entries | map({key, count:(.value|length)})' "$HOME/.claude/settings.json" .claude/settings.json 2>/dev/null || true

  echo
  echo "=== claude plugins ==="
  jq '.enabledPlugins // {} | keys' "$HOME/.claude/settings.json" .claude/settings.json 2>/dev/null || true

  echo
  echo "=== claude mcps ==="
  jq '.mcpServers // {} | keys' "$HOME/.claude/settings.json" .claude/mcp-config.json "$HOME/.codex/mcp.json" 2>/dev/null || true

  echo
  echo "=== codex mcps ==="
  rg -n '^\[mcp_servers\.' "$HOME/.codex/config.toml" 2>/dev/null || true

  echo
  echo "=== settings summary ==="
  jq '{effortLevel, autoCompactEnabled, awaySummaryEnabled}' "$HOME/.claude/settings.json" 2>/dev/null || true
  rg -n '^model_reasoning_effort|^model = ' "$HOME/.codex/config.toml" 2>/dev/null || true

  echo
  echo "=== skill files ==="
  find "$HOME/.claude/skills" .claude/skills "$HOME/.codex/skills" "$HOME/.agents/skills" "$HOME/.codex/plugins/cache" "$HOME/.claude/plugins/cache" -name SKILL.md 2>/dev/null | sort | wc -l

  echo
  echo "=== live transcript inventory ==="
  du -sh "$project_dir" 2>/dev/null || true
  find "$project_dir" -type f -name '*.jsonl' 2>/dev/null | wc -l
  find "$project_dir" -type f -name '*.jsonl' -mtime +60 2>/dev/null | wc -l
  find "$HOME/.codex/sessions" -type f -name '*.jsonl' -mtime "-$recent_days" 2>/dev/null | wc -l
} | tee "$preflight"

find "$project_dir" -type f -name '*.jsonl' -print0 |
while IFS= read -r -d '' file; do
  jq -r '
    select(.type == "assistant" and .message.usage?)
    | .message.usage
    | [
        (.input_tokens // 0),
        (.cache_creation_input_tokens // 0),
        (.cache_read_input_tokens // 0),
        (.cache_creation_input_tokens_5m // 0),
        (.cache_creation_input_tokens_1h // 0),
        (.output_tokens // 0)
      ]
    | @tsv
  ' "$file"
done |
awk '
  {
    input += $1
    cache_create += $2
    cache_read += $3
    cache_5m += $4
    cache_1h += $5
    output += $6
    count++
  }
  END {
    if (!count) { print "No usage records found"; exit 1 }
    printf "usage_records\t%d\n", count
    printf "input_tokens\t%d\n", input
    printf "cache_creation_input_tokens\t%d\n", cache_create
    printf "cache_read_input_tokens\t%d\n", cache_read
    printf "cache_creation_input_tokens_5m\t%d\n", cache_5m
    printf "cache_creation_input_tokens_1h\t%d\n", cache_1h
    printf "output_tokens\t%d\n", output
    printf "avg_input_tokens\t%.0f\n", input / count
  }' | tee "$usage_tsv"

{
  echo "=== hook events fallback corrected ==="
  find "$project_dir" -type f -name '*.jsonl' -print0 |
  xargs -0 rg --no-filename -o '"(SessionStart|UserPromptSubmit|PreToolUse|PostToolUse|Stop)"' 2>/dev/null |
  tr -d '"' | sort | uniq -c | sort -rn || true

  echo
  echo "=== cache miss reasons ==="
  find "$project_dir" -type f -name '*.jsonl' -print0 |
  xargs -0 jq -r 'select(.message.usage.cache_miss_reason?) | .message.usage.cache_miss_reason' 2>/dev/null |
  sort | uniq -c | sort -rn || true

  echo
  echo "=== cache missed input token total ==="
  find "$project_dir" -type f -name '*.jsonl' -print0 |
  xargs -0 jq -r 'select(.message.usage.cache_missed_input_tokens?) | .message.usage.cache_missed_input_tokens' 2>/dev/null |
  awk '{s+=$1; c++} END {printf "records\t%d\ncache_missed_input_tokens\t%d\n", c, s}'
} | tee "$hook_cache"

cat <<EOF
cc-audit collector complete
output_dir=$output_dir
preflight=$preflight
usage=$usage_tsv
hook_cache=$hook_cache
EOF
