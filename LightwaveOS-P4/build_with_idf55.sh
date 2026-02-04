#!/bin/bash
# Build script that enforces ESP-IDF v5.5.2

set -e

# Get project directory (default: where this script lives)
PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
# Allow override for external projects
if [ -n "$LW_PROJECT_DIR" ]; then
    PROJECT_DIR="$LW_PROJECT_DIR"
fi
cd "$PROJECT_DIR"

REQUIRED_VERSION="v5.5.2"

echo "=== ESP-IDF Version Check ==="

# Auto-detect v5.5.2 if IDF_PATH not set
if [ -z "$IDF_PATH" ] || [ "$IDF_PATH" = "/path/to/esp-idf-v5.5.2" ]; then
    if [ -d "$HOME/esp/esp-idf-v5.5.2" ]; then
        export IDF_PATH="$HOME/esp/esp-idf-v5.5.2"
        echo "Auto-detected IDF_PATH: $IDF_PATH"
    else
        echo "ERROR: IDF_PATH not set and v5.5.2 not found at ~/esp/esp-idf-v5.5.2"
        echo "Set it manually: export IDF_PATH=\$HOME/esp/esp-idf-v5.5.2"
        exit 1
    fi
fi

if [ ! -d "$IDF_PATH" ]; then
    echo "ERROR: IDF_PATH=$IDF_PATH does not exist"
    exit 1
fi

IDF_VERSION=$(cd "$IDF_PATH" && git describe --tags --exact-match 2>/dev/null || git describe --tags 2>/dev/null || echo "unknown")
echo "IDF_PATH: $IDF_PATH"
echo "Detected version: $IDF_VERSION"

if [[ "$IDF_VERSION" != *"5.5.2"* ]] && [[ "$IDF_VERSION" != *"v5.5.2"* ]]; then
    echo ""
    echo "ERROR: Wrong ESP-IDF version!"
    echo "Required: $REQUIRED_VERSION"
    echo "Detected: $IDF_VERSION"
    echo ""
    echo "Fix:"
    echo "  cd $IDF_PATH"
    echo "  git checkout $REQUIRED_VERSION"
    echo "  git submodule update --init --recursive"
    exit 1
fi

echo "Version OK: $IDF_VERSION"
echo ""

# Force v5.5.2 Python environment (prevent v6.0 env contamination)
echo "=== Setting up Python environment for v5.5.2 ==="

# Find v5.5.2 Python environment
PYTHON_ENV_V55=$(ls -d $HOME/.espressif/python_env/idf5.5* 2>/dev/null | head -1)
if [ -z "$PYTHON_ENV_V55" ]; then
    echo "Creating Python environment for v5.5.2..."
    cd "$IDF_PATH"
    ./install.sh
    if [ $? -ne 0 ]; then
        echo "ERROR: Failed to install Python environment"
        exit 1
    fi
    PYTHON_ENV_V55=$(ls -d $HOME/.espressif/python_env/idf5.5* 2>/dev/null | head -1)
fi

if [ -z "$PYTHON_ENV_V55" ]; then
    echo "ERROR: Could not find or create v5.5.2 Python environment"
    exit 1
fi

echo "Using Python env: $PYTHON_ENV_V55"

# CRITICAL: Set Python environment BEFORE sourcing export.sh
# This ensures idf.py and CMake use the correct Python
export IDF_PYTHON_ENV_PATH="$PYTHON_ENV_V55"
export PYTHON="$PYTHON_ENV_V55/bin/python"
export VIRTUAL_ENV="$PYTHON_ENV_V55"
# Remove ALL v6.0 Python paths from PATH, then prepend v5.5.2
CLEAN_PATH=$(echo "$PATH" | tr ':' '\n' | grep -v "idf6.0" | tr '\n' ':' | sed 's/:$//')
export PATH="$PYTHON_ENV_V55/bin:$CLEAN_PATH"
unset PYTHONPATH  # Clear any v6.0 Python paths

# CRITICAL: DO NOT source export.sh - it detects wrong Python environment
# Instead, manually set up the environment that export.sh would set
echo "=== Setting up ESP-IDF environment (manual, no export.sh) ==="
CORRECT_IDF_PATH="$IDF_PATH"
export IDF_PATH="$CORRECT_IDF_PATH"

# Force Python environment BEFORE any ESP-IDF tools run
export IDF_PYTHON_ENV_PATH="$PYTHON_ENV_V55"
export PYTHON="$PYTHON_ENV_V55/bin/python"
export VIRTUAL_ENV="$PYTHON_ENV_V55"

# Clean PATH: remove ALL v6.0 Python paths, then add v5.5.2 Python
CLEAN_PATH=$(echo "$PATH" | tr ':' '\n' | grep -v "idf6.0" | grep -v "idf6" | tr '\n' ':' | sed 's/:$//')
export PATH="$PYTHON_ENV_V55/bin:$IDF_PATH/tools:$CLEAN_PATH"

# Unset any v6.0 Python variables
unset PYTHONPATH

# Add toolchain to PATH (v5.5.2 uses esp-14.2.0_20251107)
TOOLCHAIN_PATH=$(find ~/.espressif/tools/riscv32-esp-elf -name "riscv32-esp-elf-gcc" -type f 2>/dev/null | grep "esp-14.2.0" | head -1)
if [ -n "$TOOLCHAIN_PATH" ]; then
    TOOLCHAIN_DIR=$(dirname "$TOOLCHAIN_PATH")
    export PATH="$TOOLCHAIN_DIR:$PATH"
    echo "Added toolchain to PATH: $TOOLCHAIN_DIR"
fi

# Set ESP_ROM_ELF_DIR for bootloader build (required by ESP-IDF v5.5.2)
if [ -d "$IDF_PATH/components/esp_rom/elf" ]; then
    export ESP_ROM_ELF_DIR="$IDF_PATH/components/esp_rom/elf/"
elif [ -d "$HOME/.espressif/tools/esp_rom_elfs" ]; then
    export ESP_ROM_ELF_DIR="$HOME/.espressif/tools/esp_rom_elfs/"
else
    # Use local placeholder to satisfy gdbinit generation
    mkdir -p "$PROJECT_DIR/esp_rom_elfs"
    export ESP_ROM_ELF_DIR="$PROJECT_DIR/esp_rom_elfs/"
fi

# Verify IDF_PATH is still correct
if [ "$IDF_PATH" != "$CORRECT_IDF_PATH" ]; then
    echo "ERROR: export.sh changed IDF_PATH from $CORRECT_IDF_PATH to $IDF_PATH"
    echo "Forcing back to v5.5.2"
    export IDF_PATH="$CORRECT_IDF_PATH"
fi

# Verify IDF_PATH points to v5.5.2
VERIFY_IDF_VERSION=$(cd "$IDF_PATH" && git describe --tags 2>/dev/null || echo "unknown")
if [[ "$VERIFY_IDF_VERSION" != *"5.5.2"* ]]; then
    echo "ERROR: IDF_PATH=$IDF_PATH reports version $VERIFY_IDF_VERSION, not v5.5.2"
    exit 1
fi

# Verify Python is from correct environment
ACTUAL_PYTHON=$(which python 2>/dev/null || echo "not found")
if echo "$ACTUAL_PYTHON" | grep -q "idf6.0"; then
    echo "ERROR: Python still pointing to v6.0: $ACTUAL_PYTHON"
    echo "Forcing to v5.5.2: $PYTHON_ENV_V55/bin/python"
    export PATH="$PYTHON_ENV_V55/bin:$PATH"
    export PYTHON="$PYTHON_ENV_V55/bin/python"
    ACTUAL_PYTHON=$(which python 2>/dev/null || echo "not found")
fi

# Verify after sourcing
VERIFY_VERSION=$(python "$IDF_PATH/tools/idf.py" --version 2>/dev/null | head -1 || echo "unknown")
echo "idf.py reports: $VERIFY_VERSION"
echo "Python env: $IDF_PYTHON_ENV_PATH"
echo "Python executable: $ACTUAL_PYTHON"
echo "IDF_PATH: $IDF_PATH"
echo "IDF_PATH version: $VERIFY_IDF_VERSION"
echo ""

# Clean build artifacts (ensure we're in project directory)
cd "$PROJECT_DIR"
echo "=== Aggressive Clean: Removing ALL build artifacts ==="
# Remove everything that could contain cached IDF version info
rm -rf build sdkconfig sdkconfig.old .sdkconfig
rm -rf managed_components
# Also remove any CMake cache files that might exist
find . -name "CMakeCache.txt" -delete 2>/dev/null || true
find . -name "CMakeFiles" -type d -exec rm -rf {} + 2>/dev/null || true
echo "Build directory and cache removed"
echo ""

# Verify IDF_PATH one more time before running idf.py
if [ "$IDF_PATH" != "$CORRECT_IDF_PATH" ]; then
    echo "ERROR: IDF_PATH changed to $IDF_PATH, expected $CORRECT_IDF_PATH"
    export IDF_PATH="$CORRECT_IDF_PATH"
fi

# Helper function to verify IDF_PATH before each idf.py command
verify_idf_path() {
    if [ "$IDF_PATH" != "$CORRECT_IDF_PATH" ]; then
        echo "ERROR: IDF_PATH changed to $IDF_PATH, expected $CORRECT_IDF_PATH"
        export IDF_PATH="$CORRECT_IDF_PATH"
    fi
    VERIFY=$(cd "$IDF_PATH" && git describe --tags 2>/dev/null || echo "unknown")
    if [[ "$VERIFY" != *"5.5.2"* ]]; then
        echo "ERROR: IDF_PATH verification failed: $VERIFY"
        exit 1
    fi
}

# CRITICAL: Use explicit Python interpreter for all idf.py commands
# This ensures idf.py uses the correct Python environment
IDF_PY="$PYTHON_ENV_V55/bin/python"
IDF_PY_SCRIPT="$IDF_PATH/tools/idf.py"

# CRITICAL: Export environment variables that idf.py/CMake will use
export IDF_PATH="$CORRECT_IDF_PATH"
export PYTHON="$IDF_PY"
export IDF_PYTHON_ENV_PATH="$PYTHON_ENV_V55"
export VIRTUAL_ENV="$PYTHON_ENV_V55"

# CRITICAL: Ensure IDF_PATH/tools comes FIRST in PATH so idf.py wrapper isn't used
# The wrapper at $HOME/bin/idf.py hardcodes idf6.0_py3.11_env!
# By putting IDF_PATH/tools first, the real idf.py will be found before the wrapper
# But we're calling idf.py explicitly anyway, so this is just defensive
export PATH="$IDF_PATH/tools:$PATH"

# CRITICAL: Add toolchain to PATH if export.sh didn't add it
# export.sh might not add toolchain in non-interactive mode
if ! command -v riscv32-esp-elf-gcc >/dev/null 2>&1; then
    # Find the toolchain (v5.5.2 uses esp-14.2.0_20251107)
    TOOLCHAIN_PATH=$(find ~/.espressif/tools/riscv32-esp-elf -name "riscv32-esp-elf-gcc" -type f 2>/dev/null | grep "esp-14.2.0" | head -1)
    if [ -n "$TOOLCHAIN_PATH" ]; then
        TOOLCHAIN_DIR=$(dirname "$TOOLCHAIN_PATH")
        echo "Adding toolchain to PATH: $TOOLCHAIN_DIR"
        export PATH="$TOOLCHAIN_DIR:$PATH"
    else
        echo "WARNING: Toolchain not found, idf.py will install it if needed"
    fi
fi

# Verify we're using the correct idf.py (not the wrapper)
WHICH_IDF=$(which idf.py 2>/dev/null || echo "not found")
if [ "$WHICH_IDF" != "$IDF_PY_SCRIPT" ] && [ "$WHICH_IDF" != "$IDF_PATH/tools/idf.py" ]; then
    echo "WARNING: idf.py at $WHICH_IDF might be a wrapper"
    echo "Will use explicit path: $IDF_PY_SCRIPT"
fi

# Add esp-dsp dependency (must be in project directory)
echo "=== Adding esp-dsp dependency ==="
verify_idf_path
# Add dependency to manifest (if not already there)
"$IDF_PY" "$IDF_PY_SCRIPT" add-dependency "espressif/esp-dsp" 2>&1 | grep -v "already exists" || true
# Fetch/update managed components
"$IDF_PY" "$IDF_PY_SCRIPT" reconfigure 2>&1 | grep -E "(esp-dsp|Fetching|Component)" || true
echo ""

# Reconfigure can create a non-CMake build directory; remove to avoid fullclean failure
rm -rf build

# Set target and build
echo "=== Building ==="
verify_idf_path
echo "IDF_PATH: $IDF_PATH"
echo "PYTHON: $PYTHON"
echo "IDF_PYTHON_ENV_PATH: $IDF_PYTHON_ENV_PATH"
echo "Using idf.py: $IDF_PY_SCRIPT"
"$IDF_PY" "$IDF_PY_SCRIPT" set-target esp32p4
verify_idf_path
"$IDF_PY" "$IDF_PY_SCRIPT" build

echo ""
echo "=== Build complete ==="
echo "Flash with: idf.py -p <PORT> flash monitor"
