#!/bin/bash
set -e

ENV_NAME="pico2"
PROJECT_DIR=$(pwd)
PIO_DIR="$PROJECT_DIR/.pio"
LIBDEPS="$PIO_DIR/libdeps/$ENV_NAME"
FRAMEWORK_SRC="$HOME/.platformio/packages/framework-arduino-pico"
FRAMEWORK_DST="$PROJECT_DIR/frameworks/framework-arduino-pico"
PLATFORM_DIR="$PROJECT_DIR/platform-raspberrypi"

echo "=== Vendorizing PlatformIO project for Pico2 (offline mode) ==="

# Ensure lib/ exists
mkdir -p "$PROJECT_DIR/lib"

echo "-- Copying libraries from $LIBDEPS to lib/ ..."
if [ -d "$LIBDEPS" ]; then
    for LIB in "$LIBDEPS"/*; do
        NAME=$(basename "$LIB")
        echo "   -> $NAME"
        rm -rf "$PROJECT_DIR/lib/$NAME"
        cp -R "$LIB" "$PROJECT_DIR/lib/$NAME"
    done
else
    echo "ERROR: .pio/libdeps not found. Build once before running this script."
    exit 1
fi

echo "-- Copying Arduino-Pico framework ..."

if [ -d "$FRAMEWORK_SRC" ]; then
    mkdir -p "$PROJECT_DIR/frameworks"
    rm -rf "$FRAMEWORK_DST"
    cp -R "$FRAMEWORK_SRC" "$FRAMEWORK_DST"
    echo "   -> Copied Arduino-Pico core to frameworks/"
else
    echo "WARNING: framework-arduino-pico not found in ~/.platformio/packages/"
    echo "         Please run 'pio run' once to let PlatformIO download it."
fi

echo "-- Checking Raspberry Pi platform clone ..."
if [ ! -d "$PLATFORM_DIR" ]; then
    echo "   -> Cloning platform-raspberrypi ..."
    git clone https://github.com/maxgerhardt/platform-raspberrypi.git "$PLATFORM_DIR"
else
    echo "   -> platform-raspberrypi already exists. Skipping."
fi

echo "-- Updating platformio.ini ..."

INI="$PROJECT_DIR/platformio.ini"

# Remove any GitHub platform URL
sed -i.bak 's|platform = .*github.*|platform = platform-raspberrypi|' "$INI"

# Ensure local libs are used
if ! grep -q "micro_ros_platformio" "$INI"; then
    echo "WARNING: Your lib_deps section does not include micro_ros_platformio."
fi

# Add framework override if missing
if ! grep -q "board_build.framework_dir" "$INI"; then
    echo "board_build.framework_dir = frameworks/framework-arduino-pico" >> "$INI"
fi

echo "=== Done! Project is now fully offline-ready. ==="
echo "You can now build with:"
echo "    pio run"

