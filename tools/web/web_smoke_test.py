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


def validate_effects_shape(mode: str, obj: dict, endpoint: str) -> None:
    data = obj.get("data")
    if not isinstance(data, dict):
        raise ValueError(f"{endpoint}: missing object 'data'")

    if mode == "list":
        if not isinstance(data.get("effects"), list):
            raise ValueError(f"{endpoint}: expected data.effects[] list")
        has_pagination = isinstance(data.get("pagination"), dict)
        has_flat_paging = all(key in data for key in ("total", "offset", "limit"))
        if not (has_pagination or has_flat_paging):
            raise ValueError(f"{endpoint}: missing pagination metadata")
        if "effectId" in data and "effects" not in data:
            raise ValueError(f"{endpoint}: returned current-effect shape")
        return

    if mode == "current":
        if "effectId" not in data:
            raise ValueError(f"{endpoint}: missing data.effectId")
        if "effects" in data:
            raise ValueError(f"{endpoint}: returned list shape (data.effects present)")
        return

    raise ValueError(f"{endpoint}: unknown shape mode '{mode}'")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", required=True, help="Device host or IP (e.g. lightwaveos.local)")
    ap.add_argument("--port", type=int, default=80)
    args = ap.parse_args()

    base = f"http://{args.host}:{args.port}"

    endpoints = [
        ("/api/v1/", None),
        ("/api/v1/spec", None),
        ("/api/v1/device/status", None),
        ("/api/v1/effects", "list"),
        ("/api/v1/effects/current", "current"),
        ("/api/v1/effects/current?details=true", "current"),
        ("/api/v1/effects?limit=20", "list"),
    ]

    for ep, shape_mode in endpoints:
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

        if shape_mode:
            try:
                validate_effects_shape(shape_mode, obj, ep)
            except ValueError as e:
                print(f"FAIL {e}")
                return 2

        print(f"OK   {ep}: success={obj.get('success')}")

    print("All basic REST smoke tests passed.")
    print("WebSocket smoke: connect to ws://<host>/ws and send: {\"type\":\"ping\"}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

