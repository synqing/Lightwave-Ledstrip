#!/usr/bin/env python3
"""Run the canonical native host harness matrix."""

from __future__ import annotations

import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path


FIRMWARE_ROOT = Path(__file__).resolve().parents[1]


@dataclass(frozen=True)
class HarnessStep:
    name: str
    commands: tuple[tuple[str, ...], ...]


MATRIX: tuple[HarnessStep, ...] = (
    HarnessStep("codec aggregate", (("pio", "test", "-e", "native_codec_test_ws"),)),
    HarnessStep("colour correction engine", (("pio", "test", "-e", "native_test_color_correction_engine"),)),
    HarnessStep("control bus bench toggles", (("pio", "test", "-e", "native_test_control_bus_bench_toggles"),)),
    HarnessStep("fade override bench helper", (("pio", "test", "-e", "native_test_fade_override"),)),
    HarnessStep("phase5 native substrate", (("pio", "test", "-e", "native_test_phase5"),)),
    HarnessStep("zone effect isolation", (("pio", "test", "-e", "native_test_zone_effect_isolation"),)),
    HarnessStep("reflective twin policy", (("pio", "test", "-e", "native_test_reflective_twin_policy"),)),
    HarnessStep("inter-strip phase delay", (("pio", "test", "-e", "native_test_interstrip_phase_delay"),)),
    HarnessStep("cross-strip wave interference", (("pio", "test", "-e", "native_test_cross_strip_wave_interference"),)),
    HarnessStep("audio benchmark", (("pio", "test", "-e", "native_audio_benchmark"),)),
    HarnessStep(
        "manifest codec standalone",
        (
            ("pio", "run", "-e", "native_codec_test_manifest"),
            (".pio/build/native_codec_test_manifest/program",),
        ),
    ),
    HarnessStep(
        "ws effects codec standalone",
        (
            ("pio", "run", "-e", "native_codec_test_ws_effects"),
            (".pio/build/native_codec_test_ws_effects/program",),
        ),
    ),
    HarnessStep(
        "ws router standalone",
        (
            ("pio", "run", "-e", "native_test_ws_router"),
            (".pio/build/native_test_ws_router/program",),
        ),
    ),
    HarnessStep(
        "ws router benchmark",
        (
            ("pio", "run", "-e", "native_benchmark_ws_routing"),
            (".pio/build/native_benchmark_ws_routing/program",),
        ),
    ),
)


def run_step(step: HarnessStep) -> tuple[bool, float]:
    start = time.monotonic()
    for command in step.commands:
        print(f"\n=== {step.name}: {' '.join(command)} ===", flush=True)
        result = subprocess.run(command, cwd=FIRMWARE_ROOT)
        if result.returncode != 0:
            return False, time.monotonic() - start
    return True, time.monotonic() - start


def main() -> int:
    results: list[tuple[str, bool, float]] = []
    for step in MATRIX:
        ok, elapsed = run_step(step)
        results.append((step.name, ok, elapsed))
        if not ok:
            break

    print("\n=== Native Harness Matrix Summary ===")
    for name, ok, elapsed in results:
        status = "PASS" if ok else "FAIL"
        print(f"{status:4} {elapsed:7.2f}s  {name}")

    return 0 if all(ok for _, ok, _ in results) and len(results) == len(MATRIX) else 1


if __name__ == "__main__":
    sys.exit(main())
