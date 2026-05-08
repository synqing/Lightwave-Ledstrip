#!/usr/bin/env python3
"""Run the Liquid Light palette sequence on existing 0x0201 firmware.

This deliberately does not add a new effect. It drives the current serial CLI:

  effect 0x0201
  . / ,        palette next / previous
  + / -        brightness up / down
  [ / ]        speed down / up

The palette story is:
  Abyss -> Ocean -> Ocean Breeze 036 -> Ocean Breeze 068 -> Seafloor -> Rivendell
"""

from __future__ import annotations

import argparse
import glob
import re
import sys
import time
from dataclasses import dataclass
from pathlib import Path

try:
    import serial  # type: ignore[import-not-found]
except ImportError:
    print(
        "ERROR: pyserial is required. Install with `pip install pyserial` "
        "or use PlatformIO's Python environment.",
        file=sys.stderr,
    )
    sys.exit(1)


EFFECT_ID = "0x0201"
DEFAULT_BAUD = 115200
PALETTE_COUNT = 75


@dataclass(frozen=True)
class PaletteStop:
    palette_id: int
    name: str
    intent: str


LIQUID_LIGHT_SEQUENCE: tuple[PaletteStop, ...] = (
    PaletteStop(62, "Abyss", "deep blue base"),
    PaletteStop(64, "Ocean", "broader blue field"),
    PaletteStop(2, "Ocean Breeze 036", "richer cool blue"),
    PaletteStop(8, "Ocean Breeze 068", "teal shift"),
    PaletteStop(66, "Seafloor", "marine blue-green"),
    PaletteStop(1, "Rivendell", "soft green resolve"),
)


@dataclass
class DeviceState:
    palette_id: int | None = None
    brightness: int | None = None
    speed: int | None = None


def auto_port() -> str | None:
    candidates: list[str] = []
    for pattern in (
        "/dev/cu.usbmodem*",
        "/dev/cu.SLAB_USBtoUART*",
        "/dev/cu.usbserial*",
    ):
        candidates.extend(glob.glob(pattern))
    return sorted(candidates)[0] if candidates else None


def send_line(port: serial.Serial, line: str) -> None:
    port.write((line + "\n").encode("utf-8"))
    port.flush()


def read_for(port: serial.Serial, seconds: float, *, echo: bool) -> list[str]:
    lines: list[str] = []
    buf = bytearray()
    deadline = time.time() + seconds
    while time.time() < deadline:
        chunk = port.read(4096)
        if not chunk:
            time.sleep(0.02)
            continue
        buf.extend(chunk)
        while True:
            nl = buf.find(b"\n")
            if nl < 0:
                break
            raw = bytes(buf[:nl]).decode("utf-8", errors="replace").rstrip("\r")
            buf = buf[nl + 1 :]
            lines.append(raw)
            if echo and raw:
                print(f"rx | {raw}")
    if buf:
        raw = bytes(buf).decode("utf-8", errors="replace").rstrip("\r")
        if raw:
            lines.append(raw)
            if echo:
                print(f"rx | {raw}")
    return lines


def query_state(port: serial.Serial, *, timeout: float, echo: bool) -> DeviceState:
    port.reset_input_buffer()
    send_line(port, "vp stack")
    lines = read_for(port, timeout, echo=echo)

    state = DeviceState()
    for line in lines:
        palette_match = re.search(r"\bpalette:\s+(\d+)\b", line)
        if palette_match:
            state.palette_id = int(palette_match.group(1))

        controls_match = re.search(
            r"\bcontrols:\s+brightness=(\d+)\s+speed=(\d+)\b",
            line,
        )
        if controls_match:
            state.brightness = int(controls_match.group(1))
            state.speed = int(controls_match.group(2))

    return state


def nearest_palette_steps(current: int, target: int) -> tuple[str, int]:
    forward = (target - current) % PALETTE_COUNT
    backward = (current - target) % PALETTE_COUNT
    if forward <= backward:
        return ".", forward
    return ",", backward


def move_palette(
    port: serial.Serial,
    *,
    current: int,
    target: int,
    step_delay: float,
    echo: bool,
) -> int:
    command, steps = nearest_palette_steps(current, target)
    if steps == 0:
        return target

    for _ in range(steps):
        send_line(port, command)
        read_for(port, step_delay, echo=echo)
    return target


def adjust_brightness(
    port: serial.Serial,
    *,
    current: int | None,
    target: int | None,
    step_delay: float,
    echo: bool,
) -> None:
    if current is None or target is None:
        return

    target = max(16, min(255, target))
    while abs(current - target) > 8:
        if current < target:
            send_line(port, "+")
            current = min(current + 16, 255)
        else:
            send_line(port, "-")
            current = max(current - 16, 16)
        read_for(port, step_delay, echo=echo)


def adjust_speed(
    port: serial.Serial,
    *,
    current: int | None,
    target: int | None,
    step_delay: float,
    echo: bool,
) -> None:
    if current is None or target is None:
        return

    target = max(1, min(100, target))
    while current != target:
        if current < target:
            send_line(port, "]")
            current += 1
        else:
            send_line(port, "[")
            current -= 1
        read_for(port, step_delay, echo=echo)


def sleep_with_optional_rx(port: serial.Serial, seconds: float, *, echo: bool) -> None:
    if not echo:
        time.sleep(seconds)
        return

    deadline = time.time() + seconds
    while time.time() < deadline:
        read_for(port, min(0.5, max(0.0, deadline - time.time())), echo=True)


def print_sequence() -> None:
    print("Liquid Light palette sequence:")
    for index, stop in enumerate(LIQUID_LIGHT_SEQUENCE, start=1):
        print(f"  {index}. {stop.palette_id:02d} {stop.name} - {stop.intent}")


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Drive the 0x0201 Liquid Light palette sequence over serial.",
    )
    parser.add_argument("--port", help="Serial port, for example /dev/cu.usbmodem2101.")
    parser.add_argument("--baud", type=int, default=DEFAULT_BAUD)
    parser.add_argument("--dwell", type=float, default=8.0, help="Seconds per palette stop.")
    parser.add_argument("--loops", type=int, default=1, help="0 loops forever.")
    parser.add_argument("--brightness", type=int, default=208, help="Best-effort brightness target.")
    parser.add_argument("--speed", type=int, default=14, help="Speed target.")
    parser.add_argument("--read-timeout", type=float, default=1.2)
    parser.add_argument("--step-delay", type=float, default=0.08)
    parser.add_argument(
        "--assume-palette",
        type=int,
        help="Fallback current palette ID if vp stack cannot be parsed.",
    )
    parser.add_argument("--echo", action="store_true", help="Print serial output while running.")
    parser.add_argument("--dry-run", action="store_true", help="Print the sequence and exit.")
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    print_sequence()

    if args.dry_run:
        return 0

    port_name = args.port or auto_port()
    if not port_name:
        print("ERROR: no serial port found. Pass --port /dev/cu.usbmodemXXXX.", file=sys.stderr)
        return 2
    if not Path(port_name).exists():
        print(f"ERROR: serial port does not exist: {port_name}", file=sys.stderr)
        return 2

    print(f"Opening {port_name} @ {args.baud} baud")
    try:
        port = serial.Serial(
            port=port_name,
            baudrate=args.baud,
            timeout=0.1,
            write_timeout=2.0,
            exclusive=True,
        )
    except serial.SerialException as exc:
        print(f"ERROR: cannot open {port_name}: {exc}", file=sys.stderr)
        return 2

    with port:
        port.reset_input_buffer()
        print(f"Selecting effect {EFFECT_ID} LGP Holographic")
        send_line(port, f"effect {EFFECT_ID}")
        read_for(port, 0.4, echo=args.echo)

        state = query_state(port, timeout=args.read_timeout, echo=args.echo)
        if state.palette_id is None and args.assume_palette is not None:
            state.palette_id = args.assume_palette % PALETTE_COUNT

        if state.palette_id is None:
            print(
                "ERROR: could not parse current palette from `vp stack`; "
                "retry with --echo or pass --assume-palette.",
                file=sys.stderr,
            )
            return 3

        adjust_brightness(
            port,
            current=state.brightness,
            target=args.brightness,
            step_delay=args.step_delay,
            echo=args.echo,
        )
        adjust_speed(
            port,
            current=state.speed,
            target=args.speed,
            step_delay=args.step_delay,
            echo=args.echo,
        )

        loop_index = 0
        current_palette = state.palette_id
        while args.loops == 0 or loop_index < args.loops:
            loop_index += 1
            print(f"Loop {loop_index}" if args.loops else f"Loop {loop_index} (Ctrl-C to stop)")
            for stop in LIQUID_LIGHT_SEQUENCE:
                current_palette = move_palette(
                    port,
                    current=current_palette,
                    target=stop.palette_id,
                    step_delay=args.step_delay,
                    echo=args.echo,
                )
                print(f"  palette {stop.palette_id:02d}: {stop.name} - {stop.intent}")
                sleep_with_optional_rx(port, args.dwell, echo=args.echo)

    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
