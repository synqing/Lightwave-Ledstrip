#!/bin/bash
# LightwaveOS Simple Controller Server
# Serves webapp + auto-discovers device via mDNS

DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$DIR"

# Kill any existing server on port 8888
lsof -ti:8888 | xargs kill -9 2>/dev/null

# Start the server with discovery endpoint
python3 server.py

