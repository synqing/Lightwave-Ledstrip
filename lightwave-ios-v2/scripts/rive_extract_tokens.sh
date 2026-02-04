#!/usr/bin/env bash
set -euo pipefail

ASSET_ID="$1"; shift
OUTDIR="Docs/Rive/References/${ASSET_ID}"
mkdir -p "${OUTDIR}"

{
  echo "# Tokens for ${ASSET_ID}"
  echo ""
  echo "## Files scanned"
  for f in "$@"; do echo "- \`${f}\`"; done
  echo ""
  echo "## Font / typography"
  rg -n "\\.font\\(|Font\\." -S "$@" || true
  echo ""
  echo "## Color usage"
  rg -n "Color\\(|\\.foregroundStyle|\\.foregroundColor|\\.tint\\(|\\.background\\(|\\.fill\\(" -S "$@" || true
  echo ""
  echo "## Layout / shape"
  rg -n "\\.cornerRadius\\(|RoundedRectangle|Capsule\\(|Circle\\(|\\.shadow\\(|\\.blur\\(|\\.opacity\\(" -S "$@" || true
  echo ""
  echo "## Assets / images"
  rg -n "Image\\(|UIImage|SF Symbol|systemName:" -S "$@" || true
} > "${OUTDIR}/tokens.md"

echo "OK: wrote ${OUTDIR}/tokens.md"
