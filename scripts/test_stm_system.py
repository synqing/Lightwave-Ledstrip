#!/usr/bin/env python3
"""
STM System Test Suite — validates Tier 1 (snapshot), Tier 2 (stream), Tier 3 (spectral map).

Zero external dependencies — uses only Python stdlib. No pip install required.
Run offline while connected to Lightwave-AP.

Usage:
    python3 scripts/test_stm_system.py              # Run all tests
    python3 scripts/test_stm_system.py snapshot      # Tier 1 only
    python3 scripts/test_stm_system.py stream        # Tier 2 only
    python3 scripts/test_stm_system.py spectralmap   # Tier 3 only
    python3 scripts/test_stm_system.py modes         # Cycle all EdgeMixer modes
"""

import base64
import hashlib
import json
import os
import socket
import struct
import sys
import time
import urllib.request
import urllib.error

K1_HOST = "192.168.4.1"
K1_PORT = 80
REST_BASE = f"http://{K1_HOST}/api/v1"

# Frame format constants (must match StmStreamConfig in firmware)
STM_MAGIC = 0xFD
SPECTRAL_BINS = 42
TEMPORAL_BANDS = 16
SCALAR_COUNT = 4
FRAME_SIZE = 1 + 1 + (SPECTRAL_BINS * 4) + (TEMPORAL_BANDS * 4) + (SCALAR_COUNT * 4)  # 250 bytes


# ============================================================================
# Minimal WebSocket client (stdlib only, RFC 6455)
# ============================================================================

class SimpleWebSocket:
    """Minimal WebSocket client using raw sockets. No external dependencies."""

    def __init__(self, host: str, port: int, path: str = "/ws"):
        self._sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._sock.settimeout(5.0)
        self._sock.connect((host, port))
        self._handshake(host, path)

    def _handshake(self, host: str, path: str):
        key = base64.b64encode(os.urandom(16)).decode()
        request = (
            f"GET {path} HTTP/1.1\r\n"
            f"Host: {host}\r\n"
            f"Upgrade: websocket\r\n"
            f"Connection: Upgrade\r\n"
            f"Sec-WebSocket-Key: {key}\r\n"
            f"Sec-WebSocket-Version: 13\r\n"
            f"\r\n"
        )
        self._sock.sendall(request.encode())

        # Read response headers
        response = b""
        while b"\r\n\r\n" not in response:
            chunk = self._sock.recv(4096)
            if not chunk:
                raise ConnectionError("WebSocket handshake failed: connection closed")
            response += chunk

        header_block, _, remainder = response.partition(b"\r\n\r\n")
        status_line = header_block.split(b"\r\n")[0].decode()
        if "101" not in status_line:
            raise ConnectionError(f"WebSocket handshake failed: {status_line}")

        # Verify accept key (case-insensitive header search)
        expected_accept = base64.b64encode(
            hashlib.sha1((key + "258EAFA5-E914-47DA-95CA-5AB5AA10BE11").encode()).digest()
        ).decode()
        headers_lower = header_block.lower()
        if b"sec-websocket-accept" in headers_lower:
            # Find the accept header value
            for line in header_block.split(b"\r\n")[1:]:
                if line.lower().startswith(b"sec-websocket-accept"):
                    _, _, val = line.partition(b":")
                    if val.strip().decode() != expected_accept:
                        raise ConnectionError("WebSocket accept key mismatch")
                    break
        # If header missing, accept anyway — some ESP32 builds omit it

        # Any data after headers is buffered WebSocket frames
        self._buffer = remainder

    def send_text(self, text: str):
        """Send a text frame (masked, as required by RFC 6455 for clients)."""
        payload = text.encode("utf-8")
        self._send_frame(0x1, payload)

    def _send_frame(self, opcode: int, payload: bytes):
        header = bytearray()
        header.append(0x80 | opcode)  # FIN + opcode

        length = len(payload)
        if length < 126:
            header.append(0x80 | length)  # Mask bit set
        elif length < 65536:
            header.append(0x80 | 126)
            header.extend(struct.pack(">H", length))
        else:
            header.append(0x80 | 127)
            header.extend(struct.pack(">Q", length))

        # Masking key
        mask = os.urandom(4)
        header.extend(mask)

        # Apply mask to payload
        masked = bytearray(length)
        for i in range(length):
            masked[i] = payload[i] ^ mask[i % 4]

        self._sock.sendall(bytes(header) + bytes(masked))

    def recv(self, timeout: float = 2.0) -> tuple:
        """
        Receive one WebSocket frame.
        Returns (opcode, data) where opcode is 0x1 (text), 0x2 (binary), etc.
        Returns (None, None) on timeout.
        """
        self._sock.settimeout(timeout)
        try:
            # Ensure we have at least 2 bytes for the header
            while len(self._buffer) < 2:
                chunk = self._sock.recv(4096)
                if not chunk:
                    return (None, None)
                self._buffer += chunk

            b0 = self._buffer[0]
            b1 = self._buffer[1]
            opcode = b0 & 0x0F
            masked = (b1 & 0x80) != 0
            length = b1 & 0x7F

            offset = 2
            if length == 126:
                while len(self._buffer) < 4:
                    self._buffer += self._sock.recv(4096)
                length = struct.unpack(">H", self._buffer[2:4])[0]
                offset = 4
            elif length == 127:
                while len(self._buffer) < 10:
                    self._buffer += self._sock.recv(4096)
                length = struct.unpack(">Q", self._buffer[2:10])[0]
                offset = 10

            mask_key = None
            if masked:
                while len(self._buffer) < offset + 4:
                    self._buffer += self._sock.recv(4096)
                mask_key = self._buffer[offset:offset + 4]
                offset += 4

            # Read full payload
            total_needed = offset + length
            while len(self._buffer) < total_needed:
                chunk = self._sock.recv(4096)
                if not chunk:
                    return (None, None)
                self._buffer += chunk

            payload = self._buffer[offset:offset + length]
            self._buffer = self._buffer[total_needed:]

            if masked and mask_key:
                payload = bytearray(payload)
                for i in range(len(payload)):
                    payload[i] ^= mask_key[i % 4]
                payload = bytes(payload)

            # Handle control frames
            if opcode == 0x8:  # Close
                return (None, None)
            if opcode == 0x9:  # Ping — send pong
                self._send_frame(0xA, payload)
                return self.recv(timeout)

            return (opcode, payload)

        except socket.timeout:
            return (None, None)
        except OSError:
            return (None, None)

    def close(self):
        try:
            self._send_frame(0x8, b"")
        except Exception:
            pass
        try:
            self._sock.close()
        except Exception:
            pass


# ============================================================================
# Helpers
# ============================================================================

def rest_get(path: str) -> dict:
    url = f"{REST_BASE}/{path}"
    req = urllib.request.Request(url, headers={"Accept": "application/json"})
    with urllib.request.urlopen(req, timeout=5) as resp:
        return json.loads(resp.read())


def rest_post(path: str, data: dict) -> dict:
    url = f"{REST_BASE}/{path}"
    body = json.dumps(data).encode()
    req = urllib.request.Request(url, data=body, method="POST",
                                headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=5) as resp:
        return json.loads(resp.read())


def check(name: str, condition: bool, detail: str = "") -> bool:
    status = "PASS" if condition else "FAIL"
    msg = f"  {status}: {name}"
    if detail:
        msg += f" — {detail}"
    print(msg)
    return condition


def parse_stm_frame(data: bytes) -> dict | None:
    if len(data) < FRAME_SIZE:
        return None
    if data[0] != STM_MAGIC:
        return None

    offset = 0
    magic = data[offset]; offset += 1
    ready = data[offset] != 0; offset += 1

    spectral = list(struct.unpack_from(f"<{SPECTRAL_BINS}f", data, offset))
    offset += SPECTRAL_BINS * 4

    temporal = list(struct.unpack_from(f"<{TEMPORAL_BANDS}f", data, offset))
    offset += TEMPORAL_BANDS * 4

    scalars = list(struct.unpack_from(f"<{SCALAR_COUNT}f", data, offset))

    return {
        "magic": magic,
        "ready": ready,
        "spectral": spectral,
        "temporal": temporal,
        "spectralEnergy": scalars[0],
        "temporalEnergy": scalars[1],
        "centroid": scalars[2],
        "dominantBin": scalars[3],
    }


# ============================================================================
# Tier 1: Snapshot Test
# ============================================================================

def test_snapshot():
    print("\n=== Tier 1: STM Snapshot (GET /api/v1/audio/stm) ===\n")
    all_pass = True

    try:
        d = rest_get("audio/stm")
    except Exception as e:
        print(f"  FAIL: Request failed — {e}")
        return False

    data = d.get("data", {})
    all_pass &= check("HTTP success", d.get("success") is True)
    all_pass &= check("STM ready", data.get("ready") is True,
                       "false = pipeline warming" if not data.get("ready") else "")

    sp = data.get("spectral", {})
    all_pass &= check("Spectral bins = 42", sp.get("bins") == 42, f"got {sp.get('bins')}")
    all_pass &= check("Spectral values length", len(sp.get("values", [])) == 42)
    all_pass &= check("Spectral energy present", sp.get("energy") is not None)

    tp = data.get("temporal", {})
    all_pass &= check("Temporal bands = 16", tp.get("bands") == 16, f"got {tp.get('bands')}")
    all_pass &= check("Temporal values length", len(tp.get("values", [])) == 16)

    dr = data.get("derived", {})
    all_pass &= check("Centroid present", dr.get("spectralCentroidBin") is not None,
                       f"{dr.get('spectralCentroidBin', 'MISSING'):.2f}")
    all_pass &= check("Dominant bin present", dr.get("spectralDominantBin") is not None)
    all_pass &= check("Energy max present", dr.get("spectralEnergyMax") is not None)
    all_pass &= check("Energy RMS present", dr.get("spectralEnergyRms") is not None)

    if data.get("ready"):
        sp_vals = sp.get("values", [])
        tp_vals = tp.get("values", [])
        all_pass &= check("Spectral has signal", any(v > 0.001 for v in sp_vals),
                           "non-zero" if any(v > 0.001 for v in sp_vals) else "ALL ZEROS")
        all_pass &= check("Temporal has signal", any(v > 0.001 for v in tp_vals),
                           "non-zero" if any(v > 0.001 for v in tp_vals) else "ALL ZEROS")

    return all_pass


# ============================================================================
# Tier 2: Stream Test
# ============================================================================

def test_stream():
    print("\n=== Tier 2: STM Stream (WebSocket stm.subscribe) ===\n")
    all_pass = True
    frames_received = []
    subscribe_response = None

    try:
        ws = SimpleWebSocket(K1_HOST, K1_PORT, "/ws")
    except Exception as e:
        print(f"  FAIL: WebSocket connect failed — {e}")
        return False

    try:
        # Subscribe
        ws.send_text(json.dumps({
            "type": "stm.subscribe",
            "requestId": "test-stm-sub"
        }))

        # Collect for 3 seconds
        deadline = time.time() + 3.0
        while time.time() < deadline:
            remaining = deadline - time.time()
            opcode, data = ws.recv(timeout=min(0.5, remaining))
            if opcode is None:
                continue

            if opcode == 0x1:  # Text
                try:
                    parsed = json.loads(data.decode("utf-8"))
                    if parsed.get("type") == "stm.subscribed":
                        subscribe_response = parsed
                except (json.JSONDecodeError, UnicodeDecodeError):
                    pass
            elif opcode == 0x2:  # Binary
                frame = parse_stm_frame(data)
                if frame:
                    frames_received.append(frame)

        # Unsubscribe
        ws.send_text(json.dumps({
            "type": "stm.unsubscribe",
            "requestId": "test-stm-unsub"
        }))
        time.sleep(0.2)

    except Exception as e:
        print(f"  WARN: Error during stream test — {e}")
    finally:
        ws.close()

    # Validate subscription response
    all_pass &= check("Subscribe response received", subscribe_response is not None)
    if subscribe_response:
        sr_data = subscribe_response.get("data", {})
        all_pass &= check("Frame size in response", sr_data.get("frameSize") == FRAME_SIZE,
                           f"got {sr_data.get('frameSize')}, expected {FRAME_SIZE}")
        all_pass &= check("Target FPS in response", sr_data.get("targetFps") == 30,
                           f"got {sr_data.get('targetFps')}")

    # Validate frames
    all_pass &= check("Binary frames received", len(frames_received) > 0,
                       f"{len(frames_received)} frames in 3s")

    if frames_received:
        f0 = frames_received[0]
        all_pass &= check("Magic byte = 0xFD", f0["magic"] == STM_MAGIC)
        all_pass &= check("STM ready in frame", f0["ready"] is True)
        all_pass &= check("Spectral length = 42", len(f0["spectral"]) == SPECTRAL_BINS)
        all_pass &= check("Temporal length = 16", len(f0["temporal"]) == TEMPORAL_BANDS)
        all_pass &= check("Spectral values in [0,1]",
                           all(0.0 <= v <= 1.01 for v in f0["spectral"]),
                           f"range [{min(f0['spectral']):.3f}, {max(f0['spectral']):.3f}]")
        all_pass &= check("Temporal values in [0,1]",
                           all(0.0 <= v <= 1.01 for v in f0["temporal"]),
                           f"range [{min(f0['temporal']):.3f}, {max(f0['temporal']):.3f}]")
        all_pass &= check("Centroid plausible", 0.0 <= f0["centroid"] <= 41.0,
                           f"{f0['centroid']:.2f}")

        # FPS estimate
        if len(frames_received) >= 5:
            fps = len(frames_received) / 3.0
            all_pass &= check("Stream FPS ≥ 15", fps >= 15.0,
                               f"{fps:.1f} FPS ({len(frames_received)} frames / 3s)")

        # Print first frame summary
        print(f"\n  First frame:")
        print(f"    Spectral energy: {f0['spectralEnergy']:.4f}")
        print(f"    Temporal energy: {f0['temporalEnergy']:.4f}")
        print(f"    Centroid: {f0['centroid']:.2f}")
        print(f"    Dominant bin: {int(f0['dominantBin'])}")
        print(f"    Spectral peak: {max(f0['spectral']):.4f}")
    else:
        print("\n  No frames received — possible causes:")
        print("    1. STM broadcaster not wired in WebServer update loop")
        print("    2. ControlBusFrame not being copied to broadcaster")
        print("    3. Frame size mismatch (check StmStreamConfig::FRAME_SIZE)")

    return all_pass


# ============================================================================
# Tier 3: Spectral Map Mode Test
# ============================================================================

def test_spectral_map():
    print("\n=== Tier 3: STM_SPECTRAL_MAP Mode (EdgeMixer mode 8) ===\n")
    all_pass = True

    # Save current mode
    try:
        current = rest_get("edgeMixer")
        orig_mode = current.get("data", {}).get("mode", 0)
        print(f"  Current mode: {orig_mode} ({current.get('data', {}).get('modeName', '?')})")
    except Exception as e:
        print(f"  FAIL: Cannot read current mode — {e}")
        return False

    # Set STM_SPECTRAL_MAP (8)
    try:
        result = rest_post("edgeMixer", {"mode": 8})
        data = result.get("data", {})
        all_pass &= check("Set mode 8 accepted", result.get("success") is True)
        all_pass &= check("Mode name = stm_spectral_map",
                           data.get("modeName") == "stm_spectral_map",
                           f"got '{data.get('modeName')}'")
    except Exception as e:
        print(f"  FAIL: Set mode 8 — {e}")
        return False

    # Readback
    try:
        rb = rest_get("edgeMixer")
        all_pass &= check("Readback mode = 8", rb.get("data", {}).get("mode") == 8)
    except Exception:
        pass

    print("\n  >>> VISUAL CHECK:")
    print("  >>> Centre LEDs (79/80) = low-frequency modulation (slow spectral change)")
    print("  >>> Edge LEDs = high-frequency modulation (fast spectral change)")
    print("  >>> Both strips show the SAME pattern")
    print()

    try:
        input("  Press Enter after observing (or Ctrl+C to skip)... ")
    except (KeyboardInterrupt, EOFError):
        print()

    # Compare with STM_DUAL
    try:
        rest_post("edgeMixer", {"mode": 7})
        print("  Switched to STM_DUAL (mode 7) for comparison")
        print("  >>> Strip 1 = uniform temporal brightness, Strip 2 = uniform spectral brightness")
        try:
            input("  Press Enter after comparing (or Ctrl+C to skip)... ")
        except (KeyboardInterrupt, EOFError):
            print()
    except Exception:
        pass

    # Restore
    try:
        rest_post("edgeMixer", {"mode": orig_mode})
        print(f"  Restored mode: {orig_mode}")
    except Exception:
        print(f"  WARN: Could not restore mode {orig_mode}")

    return all_pass


# ============================================================================
# Mode Cycling Test
# ============================================================================

def test_mode_cycling():
    print("\n=== EdgeMixer Mode Cycling (0-8) ===\n")
    all_pass = True

    modes = [
        (0, "mirror"), (1, "analogous"), (2, "complementary"),
        (3, "split_complementary"), (4, "saturation_veil"),
        (5, "triadic"), (6, "tetradic"),
        (7, "stm_dual"), (8, "stm_spectral_map"),
    ]

    for mode_id, expected_name in modes:
        try:
            result = rest_post("edgeMixer", {"mode": mode_id})
            data = result.get("data", {})
            ok = data.get("modeName") == expected_name
            all_pass &= check(f"Mode {mode_id}", ok,
                               data.get("modeName") if ok else
                               f"expected '{expected_name}', got '{data.get('modeName')}'")
        except Exception as e:
            all_pass &= check(f"Mode {mode_id}", False, str(e))
        time.sleep(0.3)  # K1 single-core HTTP needs breathing room

    time.sleep(0.3)

    # Out-of-range rejection
    try:
        result = rest_post("edgeMixer", {"mode": 9})
        rejected = not result.get("success", True)
        all_pass &= check("Mode 9 rejected", rejected, "out-of-range")
    except urllib.error.HTTPError as e:
        all_pass &= check("Mode 9 rejected", e.code == 400, f"HTTP {e.code}")
    except Exception:
        all_pass &= check("Mode 9 rejected", True, "request failed (expected)")

    time.sleep(0.3)

    # Restore mirror
    try:
        rest_post("edgeMixer", {"mode": 0})
    except Exception:
        pass

    return all_pass


# ============================================================================
# Main
# ============================================================================

def main():
    tests = sys.argv[1:] if len(sys.argv) > 1 else ["all"]

    print("=" * 60)
    print("  STM System Test Suite (stdlib only — no pip required)")
    print(f"  Target: {K1_HOST}")
    print("=" * 60)

    # Connectivity
    try:
        rest_get("device/status")
        print(f"\n  K1 reachable at {K1_HOST}")
    except Exception:
        print(f"\n  FAIL: Cannot reach K1 at {K1_HOST}")
        print("  Are you connected to the Lightwave-AP WiFi?")
        sys.exit(1)

    results = {}

    if "all" in tests or "snapshot" in tests:
        results["Tier 1 (Snapshot)"] = test_snapshot()

    if "all" in tests or "stream" in tests:
        results["Tier 2 (Stream)"] = test_stream()

    if "all" in tests or "modes" in tests:
        results["Mode Cycling"] = test_mode_cycling()

    if "all" in tests or "spectralmap" in tests:
        results["Tier 3 (Spectral Map)"] = test_spectral_map()

    # Summary
    print("\n" + "=" * 60)
    print("  RESULTS")
    print("=" * 60)
    for name, result in results.items():
        status = "PASS" if result else "FAIL"
        print(f"  {status}: {name}")

    all_pass = all(results.values())
    print()
    sys.exit(0 if all_pass else 1)


if __name__ == "__main__":
    main()
