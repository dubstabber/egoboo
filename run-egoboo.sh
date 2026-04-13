#!/bin/bash
# Get the absolute path to the project root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Define paths
BIN_PATH="$SCRIPT_DIR/build/products/x64/bin"
DATA_PATH="$SCRIPT_DIR/data"

# Check if the executable exists
if [ ! -f "$BIN_PATH/egoboo" ]; then
    echo "Error: Egoboo executable not found at $BIN_PATH/egoboo"
    echo "Please ensure the project is built correctly."
    exit 1
fi

# Run the game with necessary environment variables
# SDL_VIDEODRIVER=x11: Required for legacy OpenGL compatibility on Wayland
# EGOBOO_DATA_DIR: Point to the game data
# DRI_PRIME=1: Use the discrete GPU (dGPU)
echo "Launching Egoboo..."
cd "$BIN_PATH" || exit 1
SDL_VIDEODRIVER=x11 EGOBOO_DATA_DIR="$DATA_PATH" DRI_PRIME=1 ./egoboo "$@"
