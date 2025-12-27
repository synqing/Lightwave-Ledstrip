#!/usr/bin/env python3
"""
LightwaveOS Simple Controller Server
Serves static files + provides device discovery endpoint
"""

import http.server
import json
import os
import socket
import socketserver

PORT = 8888
DEVICE_HOSTNAME = 'lightwaveos.local'


class ReuseAddrTCPServer(socketserver.TCPServer):
    """TCP server that allows port reuse"""
    allow_reuse_address = True


class DiscoveryHandler(http.server.SimpleHTTPRequestHandler):
    """HTTP handler with /api/discover endpoint for mDNS device lookup"""

    def do_GET(self):
        if self.path == '/api/discover':
            self.handle_discover()
        else:
            super().do_GET()

    def handle_discover(self):
        """Resolve lightwaveos.local via mDNS and return the IP"""
        try:
            ip = socket.gethostbyname(DEVICE_HOSTNAME)
            response = {
                'success': True,
                'ip': ip,
                'hostname': DEVICE_HOSTNAME
            }
            self.send_json(response)
        except socket.gaierror as e:
            response = {
                'success': False,
                'error': f'Device not found: {e}'
            }
            self.send_json(response, status=404)

    def send_json(self, data, status=200):
        """Send JSON response with CORS headers"""
        body = json.dumps(data).encode('utf-8')
        self.send_response(status)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Content-Length', len(body))
        self.send_header('Access-Control-Allow-Origin', '*')
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, format, *args):
        """Colorized logging"""
        if '/api/discover' in str(args):
            print(f"\033[36m[DISCOVER]\033[0m {args[0]}")
        else:
            print(f"[HTTP] {args[0]}")


def get_local_ip():
    """Get this machine's local IP address"""
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(('8.8.8.8', 80))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except Exception:
        return None


def main():
    # Change to script directory (where static files are)
    script_dir = os.path.dirname(os.path.abspath(__file__))
    os.chdir(script_dir)

    local_ip = get_local_ip()

    print("=" * 50)
    print("LightwaveOS Simple Controller Server")
    print("=" * 50)
    print(f"Port: {PORT}")
    print()

    if local_ip:
        print(f"📱 Mobile access: http://{local_ip}:{PORT}")
        print(f"💻 Local access:  http://localhost:{PORT}")
    else:
        print(f"💻 Access: http://localhost:{PORT}")

    print()
    print("Device discovery endpoint: /api/discover")
    print("Press Ctrl+C to stop")
    print("=" * 50)

    with ReuseAddrTCPServer(('', PORT), DiscoveryHandler) as httpd:
        httpd.serve_forever()


if __name__ == '__main__':
    main()
