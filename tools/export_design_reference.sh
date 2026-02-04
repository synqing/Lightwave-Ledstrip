#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROJECT="$ROOT_DIR/lightwave-ios-v2/LightwaveOS.xcodeproj"
SCHEME="LightwaveOS"
DERIVED_DATA="$ROOT_DIR/lightwave-ios-v2/build/design-reference"
SIM_ID="6CFDDE75-B3AE-49D1-AA7C-DFE262F8B08A"
DESTINATION="platform=iOS Simulator,id=${SIM_ID}"
BUNDLE_ID="com.spectrasynq.lightwaveos2"

mkdir -p "$DERIVED_DATA"

# Ensure palette gradients are up to date before building
python3 "$ROOT_DIR/tools/generate_palette_swatches.py"

xcodebuild \
  -project "$PROJECT" \
  -scheme "$SCHEME" \
  -configuration Debug \
  -sdk iphonesimulator \
  -derivedDataPath "$DERIVED_DATA" \
  -destination "$DESTINATION" \
  build

APP_PATH="$DERIVED_DATA/Build/Products/Debug-iphonesimulator/LightwaveOS.app"

xcrun simctl bootstatus "$SIM_ID" -b
xcrun simctl install booted "$APP_PATH"

# Avoid --console to prevent blocking; we only need the app to render/export then exit.
xcrun simctl launch --terminate-running-process booted "$BUNDLE_ID" LW_EXPORT_DESIGN_PNGS

sleep 6

CONTAINER_PATH=$(xcrun simctl get_app_container booted "$BUNDLE_ID" data)
SOURCE_DIR="$CONTAINER_PATH/Documents/DesignReference"
TARGET_DIR="$ROOT_DIR/reports/rive-reference"

mkdir -p "$TARGET_DIR"
cp -R "$SOURCE_DIR/." "$TARGET_DIR/"

echo "Design reference PNGs copied to: $TARGET_DIR"
