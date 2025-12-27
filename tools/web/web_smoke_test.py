#!/usr/bin/env python3
"""
LightwaveOS Web Smoke Test (manual)

Purpose:
- Quick sanity checks for the REST + WebSocket control plane after flashing WiFi builds.
- Designed to be run from a dev machine on the same network as the device.

Usage:
  python3 tools/web_smoke_test.py --host lightwaveos.local
  python3 tools/web_smoke_test.py --host 192.168.1.123

Notes:
- This script does not require ArduinoJson. It tests the cJSON-only API responses.
- Keep payloads small; the firmware enforces strict max request/body sizes.
"""

import argparse
import json
import sys
import urllib.request
import urllib.error


def http_get(url: str) -> dict:
    req = urllib.request.Request(url, method="GET", headers={"Accept": "application/json"})
    with urllib.request.urlopen(req, timeout=5) as resp:
        data = resp.read().decode("utf-8")
        return json.loads(data)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", required=True, help="Device host or IP (e.g. lightwaveos.local)")
    ap.add_argument("--port", type=int, default=80)
    args = ap.parse_args()

    base = f"http://{args.host}:{args.port}"

    endpoints = [
        "/api/v1/",
        "/api/v1/spec",
        "/api/v1/device/status",
        "/api/v1/effects",
        "/api/v1/effects/current",
    ]

    for ep in endpoints:
        url = base + ep
        try:
            obj = http_get(url)
        except urllib.error.HTTPError as e:
            print(f"FAIL {ep}: HTTP {e.code}")
            return 2
        except Exception as e:
            print(f"FAIL {ep}: {e}")
            return 2

        if "success" not in obj:
            print(f"FAIL {ep}: missing 'success' in response")
            return 2
        print(f"OK   {ep}: success={obj.get('success')}")

    print("All basic REST smoke tests passed.")
    print("WebSocket smoke: connect to ws://<host>/ws and send: {\"type\":\"ping\"}")
    return 0


if __name__ == "__main__":
    sys.exit(main())


