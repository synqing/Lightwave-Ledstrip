#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: tools/claude-transcript-archive-prune.sh [--archive] [--prune] [options]

Archive-first retention tool for Claude project transcript JSONL.
Default mode is dry-run: report eligible files only.

Options:
  --archive             Create archive, checksum, manifest, metadata, and verify files.
  --prune               Prune live originals after archive verification. Implies --archive.
  --days N              Archive files older than N days, default 60.
  --retention-days N    Alias for --days.
  --source-dir DIR      Claude project transcript directory.
  --source DIR          Alias for --source-dir.
  --archive-root DIR    Canonical archive root, default /Users/spectrasynq/00.Project.archieves.
  --project-name NAME   Archive project name, default Lightwave-Ledstrip.
  --date YYYY-MM-DD     Archive date stamp, default today.
  -h, --help            Show help.
EOF
}

source_dir="$HOME/.claude/projects/-Users-spectrasynq-Workspace-Management-Software-Lightwave-Ledstrip"
archive_root="/Users/spectrasynq/00.Project.archieves"
project_name="Lightwave-Ledstrip"
date_stamp="$(date +%Y-%m-%d)"
days=60
mode_archive=0
mode_prune=0

cleanup_lock() {
  if [[ -n "${lock_dir:-}" && -d "${lock_dir:-}" ]]; then
    rmdir "$lock_dir" 2>/dev/null || true
  fi
}

validate_manifest_paths() {
  local manifest_path="$1"
  awk '
    BEGIN { bad = 0 }
    $0 == "" { print "empty manifest path" > "/dev/stderr"; bad = 1; next }
    $0 ~ /^\// { print "absolute manifest path: " $0 > "/dev/stderr"; bad = 1 }
    $0 ~ /(^|\/)\.\.(\/|$)/ { print "parent traversal path: " $0 > "/dev/stderr"; bad = 1 }
    {
      for (i = 1; i <= length($0); i++) {
        c = substr($0, i, 1)
        if (c < " " || c == sprintf("%c", 127)) {
          print "control character in manifest path" > "/dev/stderr"
          bad = 1
          break
        }
      }
    }
    END { exit bad }
  ' "$manifest_path"
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --archive)
      mode_archive=1
      shift
      ;;
    --prune)
      mode_archive=1
      mode_prune=1
      shift
      ;;
    --days|--retention-days)
      days="${2:?--days requires a value}"
      shift 2
      ;;
    --source-dir|--source)
      source_dir="${2:?--source-dir requires a value}"
      shift 2
      ;;
    --archive-root)
      archive_root="${2:?--archive-root requires a value}"
      shift 2
      ;;
    --project-name)
      project_name="${2:?--project-name requires a value}"
      shift 2
      ;;
    --date)
      date_stamp="${2:?--date requires a value}"
      shift 2
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

if [[ ! -d "$source_dir" ]]; then
  echo "source directory missing: $source_dir" >&2
  exit 1
fi

if ! command -v zstd >/dev/null 2>&1; then
  echo "zstd is required" >&2
  exit 1
fi

if ! command -v shasum >/dev/null 2>&1; then
  echo "shasum is required" >&2
  exit 1
fi

archive_dir="$archive_root/$project_name/claude-project-transcripts/$date_stamp"
manifest="$archive_dir/lightwave-claude-jsonl-older-than-${days}-days.manifest"
manifest_stats="$archive_dir/lightwave-claude-jsonl-older-than-${days}-days.manifest-stats.tsv"
metadata="$archive_dir/lightwave-claude-jsonl-older-than-${days}-days.metadata.txt"
archive="$archive_dir/lightwave-claude-jsonl-older-than-${days}-days-${date_stamp}.tar.zst"
verify_file="$archive_dir/lightwave-claude-jsonl-older-than-${days}-days.verify.txt"
preflight_file="$archive_dir/lightwave-claude-jsonl-older-than-${days}-days.prune-preflight.txt"
prune_log="$archive_dir/lightwave-claude-jsonl-older-than-${days}-days.prune-log.txt"
post_prune_file="$archive_dir/lightwave-claude-jsonl-older-than-${days}-days.post-prune-verify.txt"

if [[ "$mode_archive" -eq 0 ]]; then
  count=$(find "$source_dir" -type f -name '*.jsonl' -mtime +"$days" | wc -l | tr -d ' ')
  echo "dry_run=true"
  echo "source_dir=$source_dir"
  echo "archive_root=$archive_root"
  echo "selection=find source -type f -name '*.jsonl' -mtime +$days"
  echo "eligible_files=$count"
  echo "No archive or prune performed. Pass --archive to archive or --prune to archive and prune."
  exit 0
fi

mkdir -p "$archive_dir"
lock_dir="$archive_dir/.archive-prune.lock"
if ! mkdir "$lock_dir" 2>/dev/null; then
  echo "another archive/prune run appears active: $lock_dir" >&2
  exit 1
fi
trap cleanup_lock EXIT

(cd "$source_dir" && find . -type f -name '*.jsonl' -mtime +"$days" | sed 's#^\./##' | sort) > "$manifest"
validate_manifest_paths "$manifest"

(cd "$source_dir" && while IFS= read -r rel; do
  if [[ -f "$rel" ]]; then
    stat -f '%N	%z	%m' "$rel"
  fi
done < "$manifest") > "$manifest_stats"

count=$(wc -l < "$manifest" | tr -d ' ')
stats_count=$(wc -l < "$manifest_stats" | tr -d ' ')
test "$count" = "$stats_count"

bytes=$(cd "$source_dir" && while IFS= read -r rel; do
  [[ -f "$rel" ]] && stat -f '%z' "$rel"
done < "$manifest" | awk '{s+=$1} END{printf "%.0f", s}')
mb=$(awk -v b="$bytes" 'BEGIN{printf "%.2f", b/1024/1024}')

{
  echo "label=$project_name Claude project transcript cold archive"
  echo "created_at=$(date '+%Y-%m-%d %H:%M:%S %z')"
  echo "source_dir=$source_dir"
  echo "selection=find . -type f -name '*.jsonl' -mtime +$days"
  echo "file_count=$count"
  echo "payload_bytes=$bytes"
  echo "payload_mib=$mb"
  echo "archive_policy=archive first; prune only with --prune and verified manifest/archive"
} > "$metadata"

if [[ "$count" = "0" ]]; then
  echo "archive_skipped=true"
  echo "reason=no eligible files"
  echo "manifest=$manifest"
  echo "metadata=$metadata"
  exit 0
fi

rm -f "$archive" "$archive.sha256"
tar -C "$source_dir" -cf - -T "$manifest" | zstd -10 -T0 -o "$archive"
shasum -a 256 "$archive" > "$archive.sha256"

expected="$count"
archive_count=$(zstd -dc "$archive" | tar -tf - | wc -l | tr -d ' ')
checksum_status=$(cd "$archive_dir" && shasum -a 256 -c "$(basename "$archive").sha256")

{
  echo "expected_files=$expected"
  echo "archive_files=$archive_count"
  echo "checksum_status=$checksum_status"
  echo "archive_size=$(du -h "$archive" | awk '{print $1}')"
  echo "archive_dir=$archive_dir"
} | tee "$verify_file"

test "$expected" = "$archive_count"

if [[ "$mode_prune" -eq 0 ]]; then
  echo "prune=false"
  echo "Archive verified. Pass --prune to delete manifest-listed live originals."
  exit 0
fi

missing_before=$(cd "$source_dir" && while IFS= read -r rel; do
  [[ -f "$rel" ]] || printf '%s\n' "$rel"
done < "$manifest" | wc -l | tr -d ' ')
changed_before=$(cd "$source_dir" && while IFS=$'\t' read -r rel size mtime; do
  if [[ ! -f "$rel" ]]; then
    printf '%s\n' "$rel"
    continue
  fi
  current_size=$(stat -f '%z' "$rel")
  current_mtime=$(stat -f '%m' "$rel")
  if [[ "$current_size" != "$size" || "$current_mtime" != "$mtime" ]]; then
    printf '%s\n' "$rel"
  fi
done < "$manifest_stats" | wc -l | tr -d ' ')

{
  echo "expected_manifest_files=$expected"
  echo "archive_listing_files=$archive_count"
  echo "missing_live_originals=$missing_before"
  echo "changed_live_originals=$changed_before"
  echo "checksum_status=$checksum_status"
  echo "preflight_at=$(date '+%Y-%m-%d %H:%M:%S %z')"
} | tee "$preflight_file"

test "$missing_before" = "0"
test "$changed_before" = "0"

{
  echo "prune_started_at=$(date '+%Y-%m-%d %H:%M:%S %z')"
  echo "manifest=$manifest"
  echo "archive=$archive"
  echo "expected_manifest_files=$expected"
  echo "archive_listing_files=$archive_count"
  echo "missing_before=$missing_before"
  echo "changed_before=$changed_before"
  echo "checksum_status=$checksum_status"
} > "$prune_log"

deleted=0
cd "$source_dir"
while IFS= read -r rel; do
  if [[ -f "$rel" ]]; then
    rm -- "$rel"
    deleted=$((deleted + 1))
  fi
done < "$manifest"

missing_after=$(while IFS= read -r rel; do
  [[ -f "$rel" ]] || printf '%s\n' "$rel"
done < "$manifest" | wc -l | tr -d ' ')

{
  echo "deleted_files=$deleted"
  echo "missing_after=$missing_after"
  echo "prune_completed_at=$(date '+%Y-%m-%d %H:%M:%S %z')"
} | tee -a "$prune_log"

{
  echo "post_prune_at=$(date '+%Y-%m-%d %H:%M:%S %z')"
  echo -n "live_project_du="
  du -sh "$source_dir" | awk '{print $1}'
  echo -n "live_jsonl_count="
  find "$source_dir" -type f -name '*.jsonl' | wc -l | tr -d ' '
  echo
  echo -n "live_jsonl_older_than_${days}_count="
  find "$source_dir" -type f -name '*.jsonl' -mtime +"$days" | wc -l | tr -d ' '
  echo
  echo -n "archive_dir_du="
  du -sh "$archive_dir" | awk '{print $1}'
} | tee "$post_prune_file"

test "$deleted" = "$expected"
test "$missing_after" = "$expected"
