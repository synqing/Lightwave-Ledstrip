#!/usr/bin/env python3
"""Wave 2 OTA test harness for tab5-encoder.

Drives the four hardware verification tests from
tab5-encoder/docs/wave2-ota-handover-2026-04-18.md §5.3 against a
running tab5 device (ESP32-P4, MAC 30:ed:a0:e0:c1:a0).

Closes three P0 findings from forensic-audit-2026-04-18:
  * P0-02 rollback mark-valid   (observable after successful OTA + reboot)
  * P0-03 SHA-256 integrity     (happy path + mismatch + missing header)
  * P0-04 session timeout       (abort mid-upload, confirm watchdog clears state)

Prerequisites
-------------
* Tab5 reachable on the network (default 192.168.4.2 — DHCP client of K1 AP
  LightwaveOS-AP). Use --host tab5encoder.local for mDNS.
* Python 3.9+, `requests` installed.
* Built firmware at tab5-encoder/.pio/build/tab5/firmware.bin (run `pio run -e tab5`).

Usage
-----
    # Run everything (test 1 reboots the device and terminates the run):
    python3 wave2_ota_test.py

    # Run only a specific test:
    python3 wave2_ota_test.py --test 3

    # Skip the destructive happy-path test:
    python3 wave2_ota_test.py --skip-happy

Test map
--------
    0 = GET /api/v1/firmware/version  (sanity — tab5 reachable + running)
    1 = P0-03 happy path              (correct SHA-256 → 200 + reboot)
    2 = P0-03 SHA-256 mismatch        (wrong hex → 500 "SHA-256 mismatch")
    3 = P0-03 missing header          (no X-OTA-SHA256 → 500 "Integrity hash required")
    4 = P0-04 session timeout         (abort mid-upload, wait 35 s, retry succeeds)

Default order: 0, 3, 2, 4, 1 — non-destructive first, reboot-happy-path last.
After Test 1 success, Test 0 on the next invocation validates P0-02 (the tab5
serial monitor should show "[OTA] Current image marked valid — rollback cancelled"
around 30 s after the reboot triggered by Test 1).
"""

from __future__ import annotations

import argparse
import hashlib
import socket
import sys
import time
from pathlib import Path
from typing import Callable

import requests

# -- Defaults ---------------------------------------------------------------

DEFAULT_HOST = "192.168.4.2"  # DHCP-assigned from K1 AP; or use tab5encoder.local
DEFAULT_PORT = 80
DEFAULT_TOKEN = ""
DEFAULT_FIRMWARE = Path(__file__).resolve().parent.parent / ".pio" / "build" / "tab5" / "firmware.bin"

# P0-04 watchdog is 30 s; add 5 s margin so the test observes the timeout deterministically.
SESSION_TIMEOUT_S = 35

# Test 1 reboots the device; allow up to this long for it to re-appear on /version.
REBOOT_RECOVER_S = 60

# -- Helpers ----------------------------------------------------------------


def sha256_of(path: Path) -> str:
    """Return the lower-case 64-char hex SHA-256 of a file."""
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def get_version(host: str, port: int, timeout: float = 5.0) -> dict:
    url = f"http://{host}:{port}/api/v1/firmware/version"
    r = requests.get(url, timeout=timeout)
    r.raise_for_status()
    return r.json()


def post_firmware(
    host: str,
    port: int,
    firmware_path: Path,
    token: str,
    sha256_hex: str | None,
    *,
    connect_timeout: float = 15.0,
    read_timeout: float = 180.0,
):
    """POST multipart firmware upload. Returns (status, body, elapsed_s).

    status is an int on a completed response, or one of:
      "CONNECT_TIMEOUT"   — TCP handshake didn't complete (network/routing issue)
      "READ_TIMEOUT"      — TCP connected but no response within read_timeout
      "CONNECTION_RESET"  — socket closed mid-transfer (expected on success-path reboot)
    """
    url = f"http://{host}:{port}/api/v1/firmware/update"
    body = firmware_path.read_bytes()
    headers = {"X-OTA-Token": token}
    if sha256_hex is not None:
        headers["X-OTA-SHA256"] = sha256_hex

    t0 = time.monotonic()
    try:
        r = requests.post(
            url,
            headers=headers,
            files={"firmware": (firmware_path.name, body, "application/octet-stream")},
            timeout=(connect_timeout, read_timeout),
        )
        return r.status_code, r.text, time.monotonic() - t0
    except requests.exceptions.ConnectTimeout as exc:
        return "CONNECT_TIMEOUT", str(exc), time.monotonic() - t0
    except requests.exceptions.ReadTimeout as exc:
        return "READ_TIMEOUT", str(exc), time.monotonic() - t0
    except (
        requests.exceptions.ConnectionError,
        requests.exceptions.ChunkedEncodingError,
    ) as exc:
        return "CONNECTION_RESET", str(exc), time.monotonic() - t0


def _banner(label: str) -> None:
    print(f"\n{'=' * 70}\n{label}\n{'=' * 70}")


# -- Tests ------------------------------------------------------------------


def test_0_version(args) -> bool:
    """Sanity: tab5 is reachable and serving /api/v1/firmware/version."""
    _banner("[TEST 0] Sanity — GET /api/v1/firmware/version")
    try:
        version = get_version(args.host, args.port)
        print(f"  OK — response: {version}")
        return True
    except Exception as exc:
        print(f"  FAIL — {exc}")
        print(f"  Hint: confirm tab5 is on the network and {args.host}:{args.port} is reachable.")
        return False


def test_1_happy(args) -> bool:
    """P0-03 happy path — correct X-OTA-SHA256 → 200 → device reboots."""
    _banner("[TEST 1] P0-03 happy path — correct X-OTA-SHA256")
    print(f"  Firmware: {args.firmware} ({args.firmware.stat().st_size} bytes)")
    sha = sha256_of(args.firmware)
    print(f"  SHA-256:  {sha}")
    print("  POST …")

    status, body, elapsed = post_firmware(
        args.host, args.port, args.firmware, args.token, sha
    )
    print(f"  Response: status={status}  elapsed={elapsed:.1f}s")
    snippet = (body or "")[:300].replace("\n", " ")
    print(f"  Body:     {snippet}")

    happy = (status == 200 and body and "rebooting" in body.lower())
    # Reboot race is only credible if the upload took real time; an instant
    # CONNECTION_RESET / CONNECT_TIMEOUT is a plain network failure.
    reboot_race = (status == "CONNECTION_RESET" and elapsed > 10.0)
    if status in ("CONNECT_TIMEOUT", "READ_TIMEOUT"):
        print(f"  FAIL — {status} (tab5 unreachable or not responding). See handover §5.4.")
        return False
    if not (happy or reboot_race):
        print(f"  FAIL — expected 200+rebooting or reboot-race; got {status} in {elapsed:.1f}s")
        return False

    print(f"  Waiting up to {REBOOT_RECOVER_S}s for tab5 to come back online …")
    deadline = time.monotonic() + REBOOT_RECOVER_S
    while time.monotonic() < deadline:
        time.sleep(2)
        try:
            version = get_version(args.host, args.port, timeout=2.0)
            dt = REBOOT_RECOVER_S - int(deadline - time.monotonic())
            print(f"  Tab5 back online after ~{dt}s — version {version}")
            print("  Note: the [OTA] mark-valid log is expected ~30 s after this reboot.")
            print("        Watch serial for \"[OTA] Current image marked valid — rollback cancelled\"")
            print("        → confirms P0-02 rollback mark-valid.")
            return True
        except Exception:
            pass
    print(f"  FAIL — tab5 did not return within {REBOOT_RECOVER_S}s (possible rollback or boot-loop)")
    return False


def test_2_mismatch(args) -> bool:
    """P0-03 rejection — well-formed but wrong SHA-256 → 500 "SHA-256 mismatch"."""
    _banner("[TEST 2] P0-03 rejection — mismatched X-OTA-SHA256")
    bad_sha = "0" * 64  # valid shape (64 hex), wrong digest
    print(f"  Sending bad SHA: {bad_sha}")
    status, body, elapsed = post_firmware(
        args.host, args.port, args.firmware, args.token, bad_sha
    )
    print(f"  Response: status={status}  elapsed={elapsed:.1f}s")
    snippet = (body or "")[:400].replace("\n", " ")
    print(f"  Body:     {snippet}")
    if status == 500 and "SHA-256 mismatch" in (body or ""):
        print("  OK — tab5 returned 500 \"SHA-256 mismatch\" (image rejected before partition swap)")
        return True
    print("  FAIL — expected 500 with \"SHA-256 mismatch\" in body")
    return False


def test_3_missing_header(args) -> bool:
    """P0-03 hard-reject — missing X-OTA-SHA256 → 500 "Integrity hash required"."""
    _banner("[TEST 3] P0-03 hard-reject — missing X-OTA-SHA256")
    status, body, elapsed = post_firmware(
        args.host, args.port, args.firmware, args.token, None  # no SHA
    )
    print(f"  Response: status={status}  elapsed={elapsed:.1f}s")
    snippet = (body or "")[:400].replace("\n", " ")
    print(f"  Body:     {snippet}")
    if status == 500 and "Integrity hash required" in (body or ""):
        print("  OK — tab5 returned 500 \"Integrity hash required\"")
        return True
    print("  FAIL — expected 500 with \"Integrity hash required\" in body")
    return False


def test_4_timeout(args) -> bool:
    """P0-04 — start upload, abort mid-stream, wait 35 s, next upload must succeed.

    Uses a raw socket to send the HTTP request headers + ~1 KB of body, then closes
    the connection. The device's 30 s time-since-last-data watchdog must then abort
    the stale session so the subsequent happy-path upload is accepted.
    """
    _banner("[TEST 4] P0-04 — mid-upload abort → 30 s watchdog → retry")

    sha = sha256_of(args.firmware)
    body = args.firmware.read_bytes()
    boundary = "----Wave2TestBoundary"
    header_part = (
        f"--{boundary}\r\n"
        f'Content-Disposition: form-data; name="firmware"; filename="firmware.bin"\r\n'
        f"Content-Type: application/octet-stream\r\n\r\n"
    ).encode()
    footer_part = f"\r\n--{boundary}--\r\n".encode()
    total_len = len(header_part) + len(body) + len(footer_part)

    request_line = (
        f"POST /api/v1/firmware/update HTTP/1.1\r\n"
        f"Host: {args.host}\r\n"
        f"Content-Type: multipart/form-data; boundary={boundary}\r\n"
        f"Content-Length: {total_len}\r\n"
        f"X-OTA-Token: {args.token}\r\n"
        f"X-OTA-SHA256: {sha}\r\n"
        f"Connection: close\r\n"
        f"\r\n"
    ).encode()

    print(f"  Opening socket to {args.host}:{args.port} …")
    sock = socket.create_connection((args.host, args.port), timeout=10.0)
    try:
        sock.sendall(request_line + header_part)
        # Send ~1 KB of firmware, then drop the connection (simulate client crash).
        sock.sendall(body[:1024])
        time.sleep(0.5)
    finally:
        sock.close()
    print("  Aborted after ~1 KB. Session is now wedged.")

    print(f"  Waiting {SESSION_TIMEOUT_S}s for OtaHandler::loop() watchdog to fire …")
    print("  Watch serial for \"[OTA] Session timed out after 30 s — aborted\".")
    time.sleep(SESSION_TIMEOUT_S)

    # First prove tab5 is still alive and the session cleared: probe with a bad-SHA POST.
    # If the watchdog fired: handler runs, rejects with 500 "SHA-256 mismatch".
    # If the watchdog didn't fire: Update.begin() fails with "already running",
    # giving a different error string. CONNECT_TIMEOUT means tab5 is unreachable.
    print("  Probing session state with a bad-SHA POST (expects 500 \"SHA-256 mismatch\" if watchdog cleared)…")
    bad_sha = "1" * 64
    status, body, elapsed = post_firmware(
        args.host, args.port, args.firmware, args.token, bad_sha
    )
    print(f"  Probe response: status={status}  elapsed={elapsed:.1f}s")
    snippet = (body or "")[:300].replace("\n", " ")
    print(f"  Probe body:     {snippet}")

    if status in ("CONNECT_TIMEOUT", "READ_TIMEOUT"):
        print(f"  FAIL — tab5 unreachable after watchdog window ({status}).")
        return False
    if status == 500 and "SHA-256 mismatch" in (body or ""):
        print("  OK — watchdog cleared the wedged session (got mismatch rejection on probe).")
        return True
    if status == 500 and ("already" in (body or "").lower() or "in progress" in (body or "").lower()):
        print("  FAIL — session still wedged (got \"already in progress\" on probe).")
        return False
    print(f"  AMBIGUOUS — got {status}. Check serial for [OTA] Session timed out log to confirm.")
    return False


# -- Driver -----------------------------------------------------------------


TESTS: dict[int, Callable[[argparse.Namespace], bool]] = {
    0: test_0_version,
    1: test_1_happy,
    2: test_2_mismatch,
    3: test_3_missing_header,
    4: test_4_timeout,
}


def main() -> int:
    p = argparse.ArgumentParser(
        description="Wave 2 OTA test harness for tab5-encoder",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    p.add_argument("--host", default=DEFAULT_HOST, help=f"tab5 host/IP (default: {DEFAULT_HOST})")
    p.add_argument("--port", type=int, default=DEFAULT_PORT, help=f"HTTP port (default: {DEFAULT_PORT})")
    p.add_argument("--token", default=DEFAULT_TOKEN, help="X-OTA-Token value")
    p.add_argument(
        "--firmware",
        type=Path,
        default=DEFAULT_FIRMWARE,
        help=f"Path to firmware.bin (default: {DEFAULT_FIRMWARE})",
    )
    p.add_argument(
        "--test",
        type=int,
        choices=sorted(TESTS.keys()),
        help="Run only this test (default: all, in the safe order 0→3→2→4→1)",
    )
    p.add_argument(
        "--skip-happy",
        action="store_true",
        help="Skip Test 1 (the happy path reboots the device and ends the run)",
    )
    args = p.parse_args()

    if not args.token:
        print("OTA token is required: pass --token or set a local wrapper.", file=sys.stderr)
        return 2

    if not args.firmware.exists():
        print(f"Firmware not found: {args.firmware}", file=sys.stderr)
        print("Hint: run `pio run -e tab5` from tab5-encoder/ first.", file=sys.stderr)
        return 2

    if args.test is not None:
        order = [args.test]
    else:
        order = [0, 3, 2, 4, 1]
        if args.skip_happy:
            order = [t for t in order if t != 1]

    print(f"Tab5 OTA harness — target {args.host}:{args.port}  firmware {args.firmware}")
    print(f"Running tests: {order}")

    results: dict[int, bool] = {}
    for tid in order:
        try:
            ok = TESTS[tid](args)
        except KeyboardInterrupt:
            print("\nInterrupted by user.")
            return 130
        results[tid] = ok
        if tid == 1 and ok:
            print("\n  (Test 1 rebooted the device — ending run.)")
            break
        if not ok:
            print(f"\n  Test {tid} failed. Continuing with the next test …")

    _banner("SUMMARY")
    for tid, ok in results.items():
        flag = "PASS" if ok else "FAIL"
        print(f"  Test {tid}: {flag}  —  {TESTS[tid].__doc__.splitlines()[0]}")
    return 0 if all(results.values()) else 1


if __name__ == "__main__":
    sys.exit(main())
