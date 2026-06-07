#!/bin/bash
# Launch the Cartman map editor (the full standalone editor: tile / vertex / passage / fx views).
#
# Cartman is gated behind the CMake option EGOBOO_BUILD_CARTMAN (OFF by default, so it is NOT
# part of the normal `cmake --build build`). This script configures + builds it on demand the
# first time, then launches it on a module.
#
# Usage:
#   ./run-cartman.sh [module-name]
#     module-name : a module under data/modules/, WITHOUT the .mod suffix (default: advent)
#   e.g.  ./run-cartman.sh advent      edits data/modules/advent.mod
#         ./run-cartman.sh             edits data/modules/advent.mod (default)
#
# Controls (legacy editor): ESC or F1 to quit. It grabs the mouse/keyboard while running.

# Get the absolute path to the project root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Define paths
BIN_PATH="$SCRIPT_DIR/build/products/x64/bin"
DATA_PATH="$SCRIPT_DIR/data"
MODULE="${1:-advent}"   # module name WITHOUT the .mod suffix

# Build Cartman on demand if it has not been built yet (option is OFF by default).
if [ ! -f "$BIN_PATH/cartman" ]; then
    echo "Cartman editor is not built yet — configuring and building it now"
    echo "(EGOBOO_BUILD_CARTMAN=ON; this is gated OFF in the default build)..."
    cmake -S "$SCRIPT_DIR" -B "$SCRIPT_DIR/build" \
        -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DEGOBOO_BUILD_CARTMAN=ON || {
        echo "Error: failed to configure the build for Cartman."; exit 1; }
    cmake --build "$SCRIPT_DIR/build" --target cartman -j4 || {
        echo "Error: failed to build Cartman."; exit 1; }
fi

# Validate the module exists. Cartman has a known legacy bug where exiting on bad
# arguments (before the VFS is initialized) crashes in its atexit cleanup, so we
# check here and always pass a valid module.
if [ ! -d "$DATA_PATH/modules/${MODULE}.mod" ]; then
    echo "Error: module '${MODULE}.mod' not found at $DATA_PATH/modules/${MODULE}.mod"
    echo "Usage: $0 [module-name-without-.mod]   (e.g. $0 advent)"
    echo "Available modules:"
    ls -1 "$DATA_PATH/modules" 2>/dev/null | sed 's/\.mod$//' | sed 's/^/  /'
    exit 1
fi

# Launch the editor.
# SDL_VIDEODRIVER=x11 : required for legacy OpenGL compatibility on Wayland
# EGOBOO_DATA_DIR     : point the VFS at the game data
# DRI_PRIME=1         : use the discrete GPU (dGPU), if present
# Args: <egoboo_path> <module> -- cartman uses egoboo_path to locate the project root.
echo "Launching Cartman editor on module '${MODULE}.mod'..."
SDL_VIDEODRIVER=x11 EGOBOO_DATA_DIR="$DATA_PATH" DRI_PRIME=1 \
    "$BIN_PATH/cartman" "$DATA_PATH" "$MODULE"
