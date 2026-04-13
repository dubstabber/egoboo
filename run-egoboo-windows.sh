#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DATA_PATH="$SCRIPT_DIR/data"
REQUIRED_DLLS=(
    "libgcc_s_seh-1.dll"
    "libstdc++-6.dll"
    "libwinpthread-1.dll"
)

has_required_dlls() {
    local directory="$1"
    local dll
    for dll in "${REQUIRED_DLLS[@]}"; do
        if [ ! -f "$directory/$dll" ]; then
            return 1
        fi
    done
    return 0
}

find_executable() {
    local candidate
    for candidate in \
        "$SCRIPT_DIR/build-windows/products/x64/bin/egoboo.exe" \
        "$SCRIPT_DIR/build-vs/products/release/x64/bin/egoboo.exe"
    do
        if [ -f "$candidate" ]; then
            printf '%s\n' "$candidate"
            return 0
        fi
    done

    return 1
}

find_mingw_runtime_dir() {
    local candidate=""
    local sysroot=""

    for candidate in \
        "$SCRIPT_DIR/external/mingw/bin" \
        "/usr/x86_64-w64-mingw32/sys-root/mingw/bin"
    do
        if [ -d "$candidate" ] && has_required_dlls "$candidate"; then
            printf '%s\n' "$candidate"
            return 0
        fi
    done

    if command -v x86_64-w64-mingw32-g++ >/dev/null 2>&1; then
        sysroot="$(x86_64-w64-mingw32-g++ -print-sysroot 2>/dev/null || true)"
        candidate="$sysroot/mingw/bin"
        if [ -n "$sysroot" ] && [ -d "$candidate" ] && has_required_dlls "$candidate"; then
            printf '%s\n' "$candidate"
            return 0
        fi
    fi

    return 1
}

if [ ! -d "$DATA_PATH/basicdat" ]; then
    echo "Error: Egoboo data directory not found at $DATA_PATH/basicdat"
    exit 1
fi

if ! EXE_PATH="$(find_executable)"; then
    echo "Error: Egoboo Windows executable not found."
    echo "Checked:"
    echo "  $SCRIPT_DIR/build-windows/products/x64/bin/egoboo.exe"
    echo "  $SCRIPT_DIR/build-vs/products/release/x64/bin/egoboo.exe"
    exit 1
fi

EXE_DIR="$(dirname "$EXE_PATH")"

WINE_BIN="${WINE_BIN:-}"
if [ -z "$WINE_BIN" ]; then
    if command -v wine >/dev/null 2>&1; then
        WINE_BIN="wine"
    elif command -v wine64 >/dev/null 2>&1; then
        WINE_BIN="wine64"
    else
        echo "Error: wine was not found in PATH."
        echo "Set WINE_BIN to a custom runner if needed."
        exit 1
    fi
fi

MINGW_RUNTIME_DIR=""
if ! has_required_dlls "$EXE_DIR"; then
    if MINGW_RUNTIME_DIR="$(find_mingw_runtime_dir)"; then
        if ! command -v winepath >/dev/null 2>&1; then
            echo "Error: winepath was not found in PATH."
            echo "Wine can start the executable, but the launcher cannot expose the MinGW runtime DLL directory without winepath."
            exit 1
        fi

        WINEPATH_EXTRA="$(winepath -w "$MINGW_RUNTIME_DIR")"
        export WINEPATH="${WINEPATH_EXTRA}${WINEPATH:+;$WINEPATH}"
    else
        echo "Error: required MinGW runtime DLLs were not found next to egoboo.exe."
        echo "Missing runtime: ${REQUIRED_DLLS[*]}"
        echo "Checked the executable directory and common MinGW runtime locations."
        exit 1
    fi
fi

if [ -z "${EGOBOO_DISABLE_MIPMAPS:-}" ]; then
    export EGOBOO_DISABLE_MIPMAPS=1
fi

if [ -z "${EGOBOO_DISABLE_AUDIO:-}" ]; then
    export EGOBOO_DISABLE_AUDIO=1
fi

echo "Launching Egoboo Windows build with $WINE_BIN..."
if [ "${EGOBOO_DISABLE_MIPMAPS}" = "1" ] || [ "${EGOBOO_DISABLE_AUDIO}" = "1" ]; then
    echo "Using Wine compatibility defaults: EGOBOO_DISABLE_MIPMAPS=${EGOBOO_DISABLE_MIPMAPS} EGOBOO_DISABLE_AUDIO=${EGOBOO_DISABLE_AUDIO}"
fi
cd "$DATA_PATH"
exec "$WINE_BIN" "$EXE_PATH" "$@"
