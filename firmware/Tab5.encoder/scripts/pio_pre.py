import json
import os
import time

from SCons.Script import Import  # type: ignore

Import("env")


def _ndjson(hypothesis_id: str, location: str, message: str, data: dict) -> None:
    try:
        log_path = "/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/.cursor/debug.log"
        rec = {
            "sessionId": "debug-session",
            "runId": "run2",
            "hypothesisId": hypothesis_id,
            "location": location,
            "message": message,
            "data": data,
            "timestamp": int(time.time() * 1000),
        }
        with open(log_path, "a") as f:
            f.write(json.dumps(rec) + "\n")
    except Exception:
        # Never fail the build due to logging.
        pass


# H1/H2: PlatformIO not injecting the correct toolchain bin directory into PATH.
toolchain_dir = None
try:
    # Prefer PlatformIO's resolved package directory when available.
    toolchain_dir = env.PioPlatform().get_package_dir("toolchain-riscv32-esp")
except Exception as e:
    _ndjson("H1", "pio_pre.py:35", "get_package_dir_failed", {"error": str(e)})

def _has_compiler(dir_path: str) -> bool:
    if not dir_path:
        return False
    return (
        os.path.isfile(os.path.join(dir_path, "bin", "riscv32-esp-elf-g++"))
        or os.path.isfile(os.path.join(dir_path, "riscv32-esp-elf", "bin", "riscv32-esp-elf-g++"))
    )

if toolchain_dir and not _has_compiler(toolchain_dir):
    # Some PlatformIO installs can leave a metadata-only toolchain directory around; fall back to scanning.
    toolchain_dir = None

# Fallback: search for toolchain package manually if get_package_dirs fails
if not toolchain_dir:
    pio_packages_dir = os.path.expanduser("~/.platformio/packages")
    # Search for a toolchain directory that actually contains the compiler binaries.
    if os.path.isdir(pio_packages_dir):
        for item in os.listdir(pio_packages_dir):
            if not item.startswith("toolchain-riscv32-esp"):
                continue
            candidate = os.path.join(pio_packages_dir, item)
            if not os.path.isdir(candidate):
                continue
            if _has_compiler(candidate):
                toolchain_dir = candidate
                break

_ndjson("H1", "pio_pre.py:37", "toolchain_dir", {"toolchainDir": toolchain_dir})

env_path_before = env["ENV"].get("PATH", "")

bin_candidates = []
if toolchain_dir:
    bin_candidates = [
        os.path.join(toolchain_dir, "bin"),
        os.path.join(toolchain_dir, "riscv32-esp-elf", "bin"),
    ]

selected_bin = None
for b in bin_candidates:
    if os.path.isfile(os.path.join(b, "riscv32-esp-elf-g++")):
        selected_bin = b
        break

_ndjson(
    "H2",
    "pio_pre.py:58",
    "bin_selection",
    {"binCandidates": bin_candidates, "selectedBin": selected_bin, "pathBeforeHead": env_path_before.split(":")[:6]},
)

if selected_bin:
    env.PrependENVPath("PATH", selected_bin)

env_path_after = env["ENV"].get("PATH", "")
_ndjson(
    "H2",
    "pio_pre.py:69",
    "path_after_prepend",
    {"pathAfterHead": env_path_after.split(":")[:8]},
)

# H5/H6: framework package layout differs (some variants place core headers or library headers
# in non-standard locations). Ensure key include dirs are present to avoid missing headers.
framework_dir = None
try:
    framework_dir = env.PioPlatform().get_package_dir("framework-arduinoespressif32")
except Exception as e:
    _ndjson("H5", "pio_pre.py:79", "framework_dir_failed", {"error": str(e)})

_ndjson("H5", "pio_pre.py:81", "framework_dir", {"frameworkDir": framework_dir})

include_dirs = []
if framework_dir:
    cores_esp32 = os.path.join(framework_dir, "cores", "esp32")
    if os.path.isdir(cores_esp32):
        include_dirs.append(cores_esp32)

    # Arduino core package variants sometimes ship builtin libraries without a src/ folder.
    # Add library root dirs so <NetworkInterface.h> etc resolve consistently.
    net_lib = os.path.join(framework_dir, "libraries", "Network")
    if os.path.isdir(net_lib):
        include_dirs.append(net_lib)

    eth_lib = os.path.join(framework_dir, "libraries", "Ethernet")
    if os.path.isdir(eth_lib):
        include_dirs.append(eth_lib)

if include_dirs:
    env.Append(CPPPATH=include_dirs)

_ndjson(
    "H6",
    "pio_pre.py:105",
    "extra_include_dirs",
    {"includeDirs": include_dirs},
)

# Suppress noisy upstream warnings from framework/IDF headers without changing vendor code.
# Keep this targeted to avoid masking warnings in our own sources.
env.Append(CCFLAGS=["-Wno-deprecated-declarations"])
env.Append(CXXFLAGS=["-Wno-literal-suffix"])

# LVGL Helium/NEON ASM Exclusion for RISC-V
# The ESP32-P4 is RISC-V and cannot link ARM Helium/NEON assembly files.
# Remove these files before compilation to prevent linker errors.
import glob
import shutil

project_dir = env.get("PROJECT_DIR", "")
lvgl_helium_patterns = [
    os.path.join(project_dir, ".pio", "libdeps", "*", "lvgl", "src", "draw", "sw", "blend", "helium"),
    os.path.join(project_dir, ".pio", "libdeps", "*", "lvgl", "src", "draw", "sw", "blend", "neon"),
]

removed_dirs = []
for pattern in lvgl_helium_patterns:
    for dir_path in glob.glob(pattern):
        if os.path.isdir(dir_path):
            try:
                shutil.rmtree(dir_path)
                removed_dirs.append(dir_path)
            except Exception as e:
                _ndjson("LVGL_ASM", "pio_pre.py", "remove_failed", {"dir": dir_path, "error": str(e)})

if removed_dirs:
    _ndjson("LVGL_ASM", "pio_pre.py", "removed_asm_dirs", {"removed": removed_dirs})
