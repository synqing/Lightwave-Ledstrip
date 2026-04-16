"""Overlay FastLED 3.10.0 RMT4 implementation after PlatformIO resolves lib_deps.

Upstream FastLED blocks inside showPixels() until RMT completes (~5 ms on K1).
LightwaveOS vendors a non-blocking variant: completion and gTX_sem release move
to doneOnChannel(). This script copies the tracked overlay into .pio/libdeps so
clean builds stay reproducible (do not hand-edit libdeps only).
"""
Import("env")  # noqa: N816 — SCons/PlatformIO convention

import shutil
from pathlib import Path


def _apply_patch() -> None:
    project_dir = Path(env["PROJECT_DIR"])
    pioenv = env["PIOENV"]
    src = project_dir / "patches" / "vendor" / "FastLED-3.10.0-rmt4" / "idf4_rmt_impl.cpp"
    dst = (
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
    if not src.is_file():
        print(f"[fastled-rmt4] skip: overlay missing ({src})")
        return
    if not dst.parent.is_dir():
        print(
            f"[fastled-rmt4] skip: libdeps path not ready yet ({dst.parent}); "
            "run pio run again after dependency install."
        )
        return
    shutil.copy2(src, dst)
    print(f"[fastled-rmt4] applied overlay -> {dst}")


_apply_patch()
