#!/bin/bash
# Helper script to fetch commit SHAs for GitHub Actions and update workflows
# Usage: ./pin_action_shas.sh <action-repo> <tag>
# Example: ./pin_action_shas.sh actions/checkout v4

set -euo pipefail

if [ $# -lt 2 ]; then
    echo "Usage: $0 <action-repo> <tag>"
    echo "Example: $0 actions/checkout v4"
    exit 1
fi

REPO="$1"
TAG="$2"

# Fetch SHA from GitHub API
SHA=$(curl -s "https://api.github.com/repos/${REPO}/git/refs/tags/${TAG}" | \
      grep -o '"sha":"[^"]*"' | head -1 | cut -d'"' -f4)

if [ -z "$SHA" ]; then
    echo "Error: Could not fetch SHA for ${REPO}@${TAG}"
    exit 1
fi

# Try to get full SHA (40 chars) from commits API
FULL_SHA=$(curl -s "https://api.github.com/repos/${REPO}/commits/${SHA}" | \
           grep -o '"sha":"[^"]*"' | head -1 | cut -d'"' -f4)

if [ -n "$FULL_SHA" ]; then
    SHA="$FULL_SHA"
fi

echo "${SHA}"
