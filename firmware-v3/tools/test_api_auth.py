#!/usr/bin/env python3
"""
API Authentication Test Script for Lightwave-Ledstrip

Tests the API authentication implementation by verifying:
- 401 Unauthorized for missing X-API-Key header
- 403 Forbidden for invalid API key
- 200 OK for correct API key
- 200 OK for public endpoints (no auth required)

Usage:
    python3 test_api_auth.py [host]

Examples:
    python3 test_api_auth.py                    # Uses lightwaveos.local
    python3 test_api_auth.py 192.168.1.100      # Uses specific IP
"""

import requests
import sys
import json
from typing import List, Tuple

# Configuration
DEFAULT_HOST = "lightwaveos.local"
API_KEY = "spectrasynq"
TIMEOUT = 5


def test_auth(host: str) -> bool:
    """Run API authentication tests against the specified host."""
    base_url = f"http://{host}"
    results: List[Tuple[str, int, int, bool, str]] = []

    print(f"\nTesting API authentication against {base_url}")
    print("-" * 60)

    # Test 1: No API key - expect 401 Unauthorized
    print("Test 1: Missing X-API-Key header...")
    try:
        r = requests.get(f"{base_url}/api/v1/device/status", timeout=TIMEOUT)
        passed = r.status_code == 401
        detail = ""
        if not passed:
            try:
                detail = r.json().get("error", {}).get("message", "")
            except:
                detail = r.text[:100] if r.text else ""
        results.append(("No API key (401 expected)", r.status_code, 401, passed, detail))
    except requests.exceptions.RequestException as e:
        results.append(("No API key (401 expected)", 0, 401, False, str(e)))

    # Test 2: Wrong API key - expect 403 Forbidden
    print("Test 2: Invalid X-API-Key header...")
    try:
        r = requests.get(
            f"{base_url}/api/v1/device/status",
            headers={"X-API-Key": "wrong-key"},
            timeout=TIMEOUT
        )
        passed = r.status_code == 403
        detail = ""
        if not passed:
            try:
                detail = r.json().get("error", {}).get("message", "")
            except:
                detail = r.text[:100] if r.text else ""
        results.append(("Wrong API key (403 expected)", r.status_code, 403, passed, detail))
    except requests.exceptions.RequestException as e:
        results.append(("Wrong API key (403 expected)", 0, 403, False, str(e)))

    # Test 3: Correct API key - expect 200 OK
    print("Test 3: Valid X-API-Key header...")
    try:
        r = requests.get(
            f"{base_url}/api/v1/device/status",
            headers={"X-API-Key": API_KEY},
            timeout=TIMEOUT
        )
        passed = r.status_code == 200
        detail = ""
        if passed:
            try:
                data = r.json()
                detail = f"success={data.get('success', 'N/A')}"
            except:
                pass
        else:
            try:
                detail = r.json().get("error", {}).get("message", "")
            except:
                detail = r.text[:100] if r.text else ""
        results.append(("Correct API key (200 expected)", r.status_code, 200, passed, detail))
    except requests.exceptions.RequestException as e:
        results.append(("Correct API key (200 expected)", 0, 200, False, str(e)))

    # Test 4: Public endpoint (ping) - expect 200 without auth
    print("Test 4: Public endpoint (no auth required)...")
    try:
        r = requests.get(f"{base_url}/api/v1/ping", timeout=TIMEOUT)
        passed = r.status_code == 200
        detail = ""
        if passed:
            try:
                data = r.json()
                detail = f"pong={data.get('data', {}).get('pong', 'N/A')}"
            except:
                pass
        results.append(("Public endpoint /ping (200 expected)", r.status_code, 200, passed, detail))
    except requests.exceptions.RequestException as e:
        results.append(("Public endpoint /ping (200 expected)", 0, 200, False, str(e)))

    # Test 5: Another protected endpoint with correct key
    print("Test 5: Protected endpoint /effects with valid key...")
    try:
        r = requests.get(
            f"{base_url}/api/v1/effects",
            headers={"X-API-Key": API_KEY},
            timeout=TIMEOUT
        )
        passed = r.status_code == 200
        detail = ""
        if passed:
            try:
                data = r.json()
                effects_count = len(data.get("data", {}).get("effects", []))
                detail = f"found {effects_count} effects"
            except:
                pass
        results.append(("Protected /effects (200 expected)", r.status_code, 200, passed, detail))
    except requests.exceptions.RequestException as e:
        results.append(("Protected /effects (200 expected)", 0, 200, False, str(e)))

    # Print results
    print("\n" + "=" * 60)
    print("API Authentication Test Results")
    print("=" * 60)

    all_pass = True
    for name, actual, expected, passed, detail in results:
        status = "PASS" if passed else "FAIL"
        icon = "✅" if passed else "❌"
        print(f"{icon} {status} {name}")
        print(f"         Got: {actual}, Expected: {expected}")
        if detail:
            print(f"         Detail: {detail}")
        if not passed:
            all_pass = False

    print("=" * 60)
    if all_pass:
        print("✅ ALL TESTS PASSED - API Authentication is working correctly")
    else:
        print("❌ SOME TESTS FAILED - Check results above")

    return all_pass


def main():
    host = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_HOST

    print(f"""
╔════════════════════════════════════════════════════════════╗
║     Lightwave-Ledstrip API Authentication Tester          ║
╠════════════════════════════════════════════════════════════╣
║  Host: {host:<50} ║
║  API Key: {API_KEY:<47} ║
╚════════════════════════════════════════════════════════════╝
""")

    try:
        success = test_auth(host)
        sys.exit(0 if success else 1)
    except KeyboardInterrupt:
        print("\n\nTest interrupted by user")
        sys.exit(130)
    except Exception as e:
        print(f"\n❌ Unexpected error: {e}")
        sys.exit(1)


if __name__ == "__main__":
    main()
