#!/bin/bash
# Simple HTTP server for LightwaveOS Simple Controller
# Serves the webapp so you can access it from mobile devices

PORT=8888
DIR="$(cd "$(dirname "$0")" && pwd)"

echo "Starting LightwaveOS Simple Controller server..."
echo "Directory: $DIR"
echo "Port: $PORT"
echo ""

# Get local IP
LOCAL_IP=$(ifconfig | grep "inet " | grep -v 127.0.0.1 | head -1 | awk '{print $2}' | cut -d'/' -f1)

if [ -z "$LOCAL_IP" ]; then
    echo "⚠️  Could not detect local IP address"
    echo "   Access via: http://localhost:$PORT"
else
    echo "✅ Server starting..."
    echo ""
    echo "📱 To access from mobile:"
    echo "   1. Make sure your phone is on the same WiFi network"
    echo "   2. Open browser and go to: http://$LOCAL_IP:$PORT"
    echo "   3. Enter device IP: 192.168.0.16"
    echo "   4. Click CONNECT"
    echo ""
    echo "💻 To access from this computer:"
    echo "   http://localhost:$PORT"
    echo ""
fi

echo "Press Ctrl+C to stop the server"
echo ""

cd "$DIR"
python3 -m http.server $PORT

