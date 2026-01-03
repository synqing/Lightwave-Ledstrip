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
    toolchain_dir = env.PioPlatform().get_package_dir("toolchain-riscv32-esp")
except Exception as e:
    _ndjson("H1", "pio_pre.py:35", "get_package_dir_failed", {"error": str(e)})

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


