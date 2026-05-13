"""
Zone Composer Cross-Transport Adapters (E5 skeleton).

Phase 0 status: SCAFFOLDING. Class outlines + interfaces defined; concrete
implementations land in Phase 1 as the test suite grows.

Each adapter exposes:
  - send(payload: dict) -> response: dict
  - snapshot_zones() -> ZoneState (defined in snapshot.py)
  - close()
"""

from typing import Optional


class RestTransport:
    """REST transport adapter for K1 zone commands."""

    def __init__(self, host: str, port: int = 80, timeout: float = 5.0):
        self.host = host
        self.port = port
        self.timeout = timeout
        # TODO_PHASE1: instantiate requests.Session() with appropriate headers/auth

    def send(self, method: str, path: str, payload: Optional[dict] = None) -> dict:
        """Send REST command, return parsed JSON response."""
        # TODO_PHASE1: use requests library; map HTTP errors to dict shape
        raise NotImplementedError("RestTransport.send — Phase 1")

    def snapshot_zones(self):
        """GET /api/v1/zones returning full zone state."""
        # TODO_PHASE1: GET /api/v1/zones, parse into ZoneState
        raise NotImplementedError("RestTransport.snapshot_zones — Phase 1")

    def close(self):
        pass


class WsTransport:
    """WebSocket transport adapter for K1 zone commands."""

    def __init__(self, host: str, port: int = 80, path: str = "/ws", timeout: float = 5.0):
        self.host = host
        self.port = port
        self.path = path
        self.timeout = timeout
        # TODO_PHASE1: instantiate websockets client

    def send(self, command: dict) -> dict:
        """Send WS command, await matching response by requestId."""
        # TODO_PHASE1: connect (or reuse), send JSON, read until matching requestId
        raise NotImplementedError("WsTransport.send — Phase 1")

    def snapshot_zones(self):
        """zones.list command returning full zone state."""
        # TODO_PHASE1: send {type: zones.list}, parse response into ZoneState
        raise NotImplementedError("WsTransport.snapshot_zones — Phase 1")

    def close(self):
        pass


class SerialJsonTransport:
    """
    SerialJSON transport adapter for K1 zone commands.

    Implemented 2026-05-02 (Phase 0 closeout step 5) to prove the harness loop.
    Cross-transport sister adapters (REST, WS) remain Phase 1 — wire them in when
    needed; their interface mirrors this one.
    """

    def __init__(self, port: str, baud: int = 115200, timeout: float = 2.0):
        import serial  # local import — keeps the module importable when pyserial absent
        self.port = port
        self.baud = baud
        self.timeout = timeout
        self.ser = serial.Serial(port, baud, timeout=timeout)
        import time
        time.sleep(0.5)
        self.ser.reset_input_buffer()
        self._counter = 0

    def _next_request_id(self) -> str:
        self._counter += 1
        return f"e5-{self._counter}"

    def send(self, command: dict) -> dict:
        """
        Send SerialJSON command, await matching response by requestId.

        If `command` does not include `requestId`, one is auto-generated.
        Returns the matching response dict, or raises TimeoutError on no match.
        """
        import json
        import time
        if "requestId" not in command:
            command["requestId"] = self._next_request_id()
        rid = command["requestId"]
        line = json.dumps(command) + "\n"
        self.ser.write(line.encode())
        self.ser.flush()
        deadline = time.time() + self.timeout
        buf = b""
        while time.time() < deadline:
            chunk = self.ser.read(4096)
            if not chunk:
                continue
            buf += chunk
            for raw in buf.decode(errors="replace").splitlines():
                raw = raw.strip()
                if not raw.startswith("{"):
                    continue
                try:
                    resp = json.loads(raw)
                except json.JSONDecodeError:
                    continue
                if resp.get("requestId", "") == rid:
                    return resp
            deadline = time.time() + 0.3  # extend window if data still arriving
        raise TimeoutError(
            f"SerialJsonTransport: no response with requestId={rid!r} "
            f"within {self.timeout}s for command {command.get('type')!r}"
        )

    def snapshot_zones(self):
        """zones.list command returning full zone state as ZoneState dataclass."""
        from .snapshot import ZoneState, ZoneEntry, ZoneSegment
        resp = self.send({"type": "zones.list"})
        if not resp.get("success"):
            raise RuntimeError(f"zones.list failed: {resp.get('error')}")
        data = resp.get("data", {})
        zones = []
        for z in data.get("zones", []):
            zones.append(ZoneEntry(
                id=z.get("zoneId", z.get("id", 0)),
                enabled=z.get("enabled", False),
                effectId=z.get("effectId", 0),
                effectName=z.get("effectName", ""),
                brightness=z.get("brightness", 0),
                speed=z.get("speed", 0),
                paletteId=z.get("paletteId", 0),
                paletteName=z.get("paletteName", ""),
                blendMode=z.get("blendMode", 0),
                blendModeName=z.get("blendModeName", ""),
            ))
        # Note: SerialJSON zones.list at SerialJsonGateway.cpp:320 currently does NOT
        # include segment geometry — that's a documented Phase 0 B5 Gap (#5
        # `getZoneConfig` / readback). For now segments[] stays empty for SerialJSON
        # snapshots. REST + WS adapters will populate this when implemented Phase 1.
        return ZoneState(
            enabled=data.get("enabled", False),
            zoneCount=data.get("zoneCount", 0),
            segments=[],
            zones=zones,
        )

    def close(self):
        try:
            self.ser.close()
        except Exception:
            pass


# ---------------------------------------------------------------------------
# Factory helper
# ---------------------------------------------------------------------------

def make_transports(rest_host: Optional[str] = None,
                    ws_host: Optional[str] = None,
                    serial_port: Optional[str] = None):
    """Build all three transports from CLI args. Returns dict {name: adapter}."""
    transports = {}
    if rest_host:
        transports["rest"] = RestTransport(rest_host)
    if ws_host:
        transports["ws"] = WsTransport(ws_host)
    if serial_port:
        transports["serial"] = SerialJsonTransport(serial_port)
    return transports
