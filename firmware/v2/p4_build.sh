#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$SCRIPT_DIR/esp-idf-p4"

export LW_PROJECT_DIR="$PROJECT_DIR"

"$SCRIPT_DIR/../../LightwaveOS-P4/build_with_idf55.sh"
