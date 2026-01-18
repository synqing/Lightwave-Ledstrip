#!/bin/bash
# Context Pack Wrapper for Claude-Flow Integration
# Generates a context pack and prints a compact summary
#
# Usage: ./tools/contextpack_wrapper.sh [options]
# Options are passed through to contextpack.py

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(dirname "$SCRIPT_DIR")"

cd "$REPO_ROOT"

# Run contextpack.py and capture output
echo "=== Context Pack Generator ==="
echo ""

python tools/contextpack.py "$@"

EXIT_CODE=$?

if [ $EXIT_CODE -eq 0 ]; then
    echo ""
    echo "=== Ready for Agent ==="
    echo "Hand the agent the output directory path above."
    echo "The agent can immediately start work using:"
    echo "  - prompt.md (cache-friendly assembled prompt)"
    echo "  - diff.patch (filtered, sorted, size-budgeted)"
    echo "  - fixtures/*.toon (token-efficient data)"
fi

exit $EXIT_CODE
