"""Overlay FastLED 3.10.0 RMT4 implementation after PlatformIO resolves lib_deps.

Upstream FastLED blocks inside showPixels() until RMT completes (~5 ms on K1).
LightwaveOS vendors a non-blocking variant: completion and gTX_sem release move
to doneOnChannel(). This script copies the tracked overlay into .pio/libdeps so
clean builds stay reproducible (do not hand-edit libdeps only).

Also overlays fastled_delay.h: CMinWait::mark() must not call micros() on ESP32
from the RMT ISR when micros() is in flash (default Arduino sdkconfig); use
esp_timer_get_time() instead to avoid cache-disabled panics during NVS/WiFi.
"""
Import("env")  # noqa: N816 — SCons/PlatformIO convention

import shutil
from pathlib import Path


def _apply_patch() -> None:
    project_dir = Path(env["PROJECT_DIR"])
    pioenv = env["PIOENV"]
    vendor = project_dir / "patches" / "vendor" / "FastLED-3.10.0-rmt4"

    rmt_src = vendor / "idf4_rmt_impl.cpp"
    rmt_dst = (
        project_dir
        / ".pio"
        / "libdeps"
        / pioenv
        / "FastLED"
        / "src"
        / "platforms"
        / "esp"
        / "32"
        / "rmt_4"
        / "idf4_rmt_impl.cpp"
    )
    delay_src = vendor / "fastled_delay.h"
    delay_dst = project_dir / ".pio" / "libdeps" / pioenv / "FastLED" / "src" / "fastled_delay.h"

    if not rmt_src.is_file():
        print(f"[fastled-rmt4] skip: RMT overlay missing ({rmt_src})")
        return
    if not rmt_dst.parent.is_dir():
        print(
            f"[fastled-rmt4] skip: libdeps path not ready yet ({rmt_dst.parent}); "
            "run pio run again after dependency install."
        )
        return
    shutil.copy2(rmt_src, rmt_dst)
    print(f"[fastled-rmt4] applied overlay -> {rmt_dst}")

    if delay_src.is_file():
        shutil.copy2(delay_src, delay_dst)
        print(f"[fastled-rmt4] applied overlay -> {delay_dst}")


_apply_patch()
