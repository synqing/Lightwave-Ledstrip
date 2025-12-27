#!/usr/bin/env python3
"""
LightwaveOS Web Smoke Test v2 (REST + WebSocket)

Purpose:
- Comprehensive testing of API v2 REST endpoints
- WebSocket v2 command testing (flat format)
- Legacy WebSocket compatibility testing
- Enhancement and Palette endpoint validation

Usage:
  python3 tools/web_smoke_test_v2.py --host 192.168.0.16
"""

import argparse
import json
import sys
import time
import urllib.request
import urllib.error
import urllib.parse
import websocket
import threading

# Expected WebSocket response types per command type (per API v2 spec)
EXPECTED_RESPONSE_TYPES = {
    "device.getStatus": "device.status",
    "device.getInfo": "device.info",
    "effects.list": "effects.list",
    "effects.getCurrent": "effects.current",
    "effects.getMetadata": "effects.metadata",
    "parameters.get": "parameters",
    "transitions.list": "transitions.list",
    "zones.list": "zones.list",
    "zones.get": "zones.zone",
    "enhancements.get": "enhancements.summary",
    "enhancements.color.get": "enhancements.color",
    "enhancements.color.set": "enhancements.color",
    "enhancements.color.reset": "enhancements.color",
    "enhancements.motion.get": "enhancements.motion",
    "enhancements.motion.set": "enhancements.motion",
    "palettes.list": "palettes.list",
    "palettes.get": "palettes.get",
    "batch": "batch.result",
    "ping": "pong",
}


def http_get(url: str, headers: dict = None) -> dict:
    """GET request with JSON response parsing."""
    req_headers = {"Accept": "application/json"}
    if headers:
        req_headers.update(headers)
    req = urllib.request.Request(url, method="GET", headers=req_headers)
    with urllib.request.urlopen(req, timeout=5) as resp:
        data = resp.read().decode("utf-8")
        return json.loads(data), resp.status


def http_patch(url: str, payload: dict, headers: dict = None) -> dict:
    """PATCH request with JSON body."""
    req_headers = {"Content-Type": "application/json", "Accept": "application/json"}
    if headers:
        req_headers.update(headers)
    body = json.dumps(payload).encode("utf-8")
    req = urllib.request.Request(url, data=body, method="PATCH", headers=req_headers)
    with urllib.request.urlopen(req, timeout=5) as resp:
        data = resp.read().decode("utf-8")
        return json.loads(data), resp.status


def http_post(url: str, payload: dict, headers: dict = None) -> dict:
    """POST request with JSON body."""
    req_headers = {"Content-Type": "application/json", "Accept": "application/json"}
    if headers:
        req_headers.update(headers)
    body = json.dumps(payload).encode("utf-8")
    req = urllib.request.Request(url, data=body, method="POST", headers=req_headers)
    with urllib.request.urlopen(req, timeout=5) as resp:
        data = resp.read().decode("utf-8")
        return json.loads(data), resp.status


def test_v2_rest(base: str) -> int:
    """Test all v2 REST endpoints."""
    print("\n=== V2 REST Endpoints ===\n")
    errors = 0

    # Discovery
    endpoints = [
        ("GET", "/api/v2/", None),
        ("GET", "/api/v2/device/status", None),
        ("GET", "/api/v2/device/info", None),
        ("GET", "/api/v2/openapi.json", None),
        ("GET", "/api/v2/effects", None),
        ("GET", "/api/v2/effects/current", None),
        ("GET", "/api/v2/parameters", None),
        ("GET", "/api/v2/zones", None),
        ("GET", "/api/v2/zones/0", None),
        ("GET", "/api/v2/enhancements", None),
        ("GET", "/api/v2/enhancements/color", None),
        ("GET", "/api/v2/enhancements/motion", None),
        ("GET", "/api/v2/palettes", None),
    ]

    for method, ep, payload in endpoints:
        url = base + ep
        try:
            if method == "GET":
                obj, status = http_get(url)
            elif method == "PATCH":
                obj, status = http_patch(url, payload)
            elif method == "POST":
                obj, status = http_post(url, payload)
            else:
                print(f"SKIP {ep}: unsupported method {method}")
                continue

            if status != 200:
                print(f"FAIL {ep}: HTTP {status}")
                errors += 1
                continue

            # OpenAPI endpoints return raw JSON (not wrapped in success/version)
            if ep.endswith("openapi.json") or ep.endswith("/spec"):
                if "openapi" in obj or "swagger" in obj:
                    print(f"OK   {ep}: valid OpenAPI spec")
                else:
                    print(f"FAIL {ep}: invalid OpenAPI spec")
                    errors += 1
                continue

            if "success" not in obj:
                print(f"FAIL {ep}: missing 'success' field")
                errors += 1
                continue

            if "version" not in obj:
                print(f"WARN {ep}: missing 'version' field")
            elif obj.get("version") != "2.0.0":
                print(f"WARN {ep}: version mismatch: {obj.get('version')} != 2.0.0")

            print(f"OK   {ep}: success={obj.get('success')}, version={obj.get('version', 'N/A')}")
        except urllib.error.HTTPError as e:
            print(f"FAIL {ep}: HTTP {e.code}")
            errors += 1
        except Exception as e:
            print(f"FAIL {ep}: {e}")
            errors += 1

    # Test PATCH endpoints
    print("\n--- Testing PATCH endpoints ---\n")
    patch_tests = [
        ("/api/v2/enhancements/color", {"enabled": True, "crossBlendEnabled": True}),
        ("/api/v2/enhancements/color", {"enabled": False}),  # Reset
        ("/api/v2/enhancements/motion", {"enabled": True, "phaseVelocity": 0.5}),
        ("/api/v2/enhancements/motion", {"enabled": False}),  # Reset
    ]

    for ep, payload in patch_tests:
        url = base + ep
        try:
            obj, status = http_patch(url, payload)
            if status == 200 and obj.get("success"):
                print(f"OK   {ep}: PATCH success")
            else:
                print(f"FAIL {ep}: PATCH failed (status={status}, success={obj.get('success')})")
                errors += 1
        except Exception as e:
            print(f"FAIL {ep}: PATCH error: {e}")
            errors += 1

    return errors


def test_websocket_v2(host: str, port: int) -> int:
    """Test WebSocket v2 commands (flat format)."""
    print("\n=== WebSocket v2 Commands ===\n")
    errors = 0
    received = []
    ws = None

    def on_message(ws, message):
        received.append(json.loads(message))

    def on_error(ws, error):
        print(f"WS ERROR: {error}")

    def on_close(ws, close_status_code, close_msg):
        pass

    try:
        ws_url = f"ws://{host}:{port}/ws"
        ws = websocket.WebSocketApp(
            ws_url,
            on_message=on_message,
            on_error=on_error,
            on_close=on_close,
        )

        # Start WebSocket in background thread
        ws_thread = threading.Thread(target=ws.run_forever, daemon=True)
        ws_thread.start()
        time.sleep(1)  # Wait for connection

        # Test v2 commands (flat format, no "v" field, no nested "data")
        v2_commands = [
            {"type": "device.getStatus"},
            {"type": "device.getInfo"},
            {"type": "effects.list"},
            {"type": "effects.getCurrent"},
            {"type": "parameters.get"},
            {"type": "zones.list"},
            {"type": "zones.get", "zoneId": 0},
            {"type": "enhancements.get"},
            {"type": "enhancements.color.get"},
            {"type": "enhancements.motion.get"},
            {"type": "palettes.list"},
            {"type": "palettes.get", "id": 0},
        ]

        for cmd in v2_commands:
            received.clear()
            cmd_type = cmd["type"]
            ws.send(json.dumps(cmd))
            
            # Wait for response with timeout
            timeout = 0.5
            start = time.time()
            while len(received) == 0 and (time.time() - start) < timeout:
                time.sleep(0.05)
            
            if not received:
                print(f"FAIL {cmd_type}: no response after {timeout}s")
                errors += 1
                continue

            # Find matching response (handle out-of-order responses)
            resp = None
            expected_type = EXPECTED_RESPONSE_TYPES.get(cmd_type, cmd_type)
            for r in received:
                if r.get("type") == expected_type:
                    resp = r
                    break
            
            if not resp:
                # Fallback: use first response if no exact match
                resp = received[0]
                print(f"WARN {cmd_type}: using first response (type={resp.get('type')}) instead of expected {expected_type}")
            
            # Check response type against expected mapping
            expected_type = EXPECTED_RESPONSE_TYPES.get(cmd["type"])
            actual_type = resp.get("type")
            if expected_type:
                if actual_type != expected_type:
                    print(f"FAIL {cmd['type']}: response type mismatch - expected '{expected_type}', got '{actual_type}'")
                    errors += 1
                    continue
            elif actual_type != cmd["type"]:
                # For commands not in mapping, allow exact match or warn
                print(f"WARN {cmd['type']}: response type mismatch: {actual_type} (not in expected mapping)")
            
            if not resp.get("success"):
                print(f"FAIL {cmd['type']}: success=false")
                errors += 1
                continue
            if resp.get("version") != "2.0.0":
                print(f"WARN {cmd['type']}: version mismatch: {resp.get('version')}")

            print(f"OK   {cmd['type']}: success={resp.get('success')}, version={resp.get('version', 'N/A')}, type={actual_type}")

        # Test v2 write commands
        print("\n--- Testing v2 write commands ---\n")
        write_commands = [
            {"type": "enhancements.color.set", "enabled": True, "crossBlendEnabled": True},
            {"type": "enhancements.color.set", "enabled": False},  # Reset
            {"type": "enhancements.motion.set", "enabled": True, "phaseVelocity": 0.5},
            {"type": "enhancements.motion.set", "enabled": False},  # Reset
        ]

        for cmd in write_commands:
            received.clear()
            cmd_type = cmd["type"]
            ws.send(json.dumps(cmd))
            
            # Wait for response with timeout
            timeout = 0.5
            start = time.time()
            while len(received) == 0 and (time.time() - start) < timeout:
                time.sleep(0.05)
            
            if not received:
                print(f"FAIL {cmd_type}: no response after {timeout}s")
                errors += 1
                continue

            # Find matching response
            resp = None
            expected_type = EXPECTED_RESPONSE_TYPES.get(cmd_type, cmd_type)
            for r in received:
                if r.get("type") == expected_type:
                    resp = r
                    break
            
            if not resp:
                resp = received[0]
                print(f"WARN {cmd_type}: using first response (type={resp.get('type')}) instead of expected {expected_type}")
            
            # Check response type for write commands
            expected_type = EXPECTED_RESPONSE_TYPES.get(cmd["type"])
            actual_type = resp.get("type")
            if expected_type and actual_type != expected_type:
                print(f"WARN {cmd['type']}: response type mismatch - expected '{expected_type}', got '{actual_type}'")
            
            if resp.get("success"):
                print(f"OK   {cmd['type']}: success, type={actual_type}")
            else:
                error_info = resp.get('error', {})
                if isinstance(error_info, dict):
                    error_msg = error_info.get('message', str(error_info))
                else:
                    error_msg = str(error_info)
                print(f"FAIL {cmd['type']}: success=false, error={error_msg}")
                errors += 1

        # Test error paths
        print("\n--- Testing WebSocket error paths ---\n")
        error_commands = [
            {"type": "palettes.get", "id": 999},  # Invalid palette ID
            {"type": "zones.get", "zoneId": 99},  # Invalid zone ID
            {"type": "invalid.command"},  # Unknown command
        ]

        for cmd in error_commands:
            received.clear()
            cmd_type = cmd["type"]
            ws.send(json.dumps(cmd))
            
            timeout = 0.5
            start = time.time()
            while len(received) == 0 and (time.time() - start) < timeout:
                time.sleep(0.05)
            
            if received:
                resp = received[0]
                if not resp.get("success"):
                    print(f"OK   {cmd_type}: error response as expected (success=false)")
                else:
                    print(f"WARN {cmd_type}: expected error but got success=true")
            else:
                print(f"WARN {cmd_type}: no response (may be expected for invalid commands)")

        ws.close()
    except Exception as e:
        print(f"FAIL WebSocket: {e}")
        errors += 1
        if ws:
            ws.close()

    return errors


def test_websocket_legacy(host: str, port: int) -> int:
    """Test legacy WebSocket commands (backward compatibility)."""
    print("\n=== WebSocket Legacy Commands ===\n")
    errors = 0
    received = []
    ws = None

    def on_message(ws, message):
        received.append(json.loads(message))

    def on_error(ws, error):
        print(f"WS ERROR: {error}")

    try:
        ws_url = f"ws://{host}:{port}/ws"
        ws = websocket.WebSocketApp(
            ws_url,
            on_message=on_message,
            on_error=on_error,
        )

        ws_thread = threading.Thread(target=ws.run_forever, daemon=True)
        ws_thread.start()
        time.sleep(1)

        # Test legacy commands (old format)
        legacy_commands = [
            {"type": "ping"},
            {"type": "getStatus"},
            {"type": "getZoneState", "zoneId": 0},
            {"type": "effects.getCurrent"},
            {"type": "parameters.get"},
        ]

        for cmd in legacy_commands:
            received.clear()
            ws.send(json.dumps(cmd))
            time.sleep(0.3)

            if not received:
                print(f"WARN {cmd['type']}: no response (may be broadcast-only)")
                continue

            resp = received[0]
            print(f"OK   {cmd['type']}: received response")

        ws.close()
    except Exception as e:
        print(f"FAIL Legacy WebSocket: {e}")
        errors += 1
        if ws:
            ws.close()

    return errors


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", required=True, help="Device host or IP")
    ap.add_argument("--port", type=int, default=80, help="HTTP port")
    ap.add_argument("--ws-port", type=int, default=80, help="WebSocket port (usually same as HTTP)")
    ap.add_argument("--skip-rest", action="store_true", help="Skip REST tests")
    ap.add_argument("--skip-ws", action="store_true", help="Skip WebSocket tests")
    args = ap.parse_args()

    base = f"http://{args.host}:{args.port}"

    total_errors = 0

    if not args.skip_rest:
        total_errors += test_v2_rest(base)

    if not args.skip_ws:
        total_errors += test_websocket_v2(args.host, args.ws_port)
        total_errors += test_websocket_legacy(args.host, args.ws_port)

    print(f"\n=== Summary ===")
    if total_errors == 0:
        print("All tests passed!")
        return 0
    else:
        print(f"Total errors: {total_errors}")
        return 1


if __name__ == "__main__":
    sys.exit(main())

