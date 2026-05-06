#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
workspace="${repo_root}/firmware-v3"

pids="$(
  ps -axo pid=,command= |
    awk -v ws="${workspace}" '
      index($0, "mcp-language-server") && index($0, " --workspace " ws) { print $1; next }
      index($0, "clangd --compile-commands-dir=" ws) { print $1; next }
    '
)"

if [[ -z "${pids}" ]]; then
  echo "No Codex clangd MCP child processes found for ${workspace}"
  exit 0
fi

echo "Stopping Codex clangd MCP child processes for ${workspace}:"
echo "${pids}"
kill ${pids}

sleep 1

remaining="$(
  ps -axo pid=,command= |
    awk -v ws="${workspace}" '
      index($0, "mcp-language-server") && index($0, " --workspace " ws) { print $1; next }
      index($0, "clangd --compile-commands-dir=" ws) { print $1; next }
    '
)"

if [[ -n "${remaining}" ]]; then
  echo "Processes still running after SIGTERM:"
  echo "${remaining}"
  exit 1
fi

echo "Codex clangd MCP child process reset complete."
