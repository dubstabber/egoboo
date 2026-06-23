# Build For Windows

This is the maintained Windows build-status document. The current supported
path is a Linux-hosted x64 cross-build with `mingw-w64`.

Native Windows builds remain a target, but they are not first-class yet. The
maintained direction is an open-source toolchain path, not Visual Studio-only
project generation. Wine is a compatibility/debugging aid, not the support goal.

## Status

- Linux-hosted `mingw-w64` x64 cross-build: builds.
- Running that artifact under Wine: useful for debugging, still unstable.
- Native Windows open-source build: future documented target.
- Visual Studio/MSVC instructions: legacy only under `doc/legacy/`.

The Wine helper still defaults mipmaps and audio off because those runtime paths
remain unstable in the current Windows artifact.

## Toolchain

Fedora package baseline:

```bash
sudo dnf install \
  ninja-build mingw64-gcc mingw64-gcc-c++ mingw64-binutils \
  mingw64-winpthreads-static
```

Verify:

```bash
which x86_64-w64-mingw32-gcc
which x86_64-w64-mingw32-g++
```

## Build

```bash
cmake -S . -B build-windows -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/toolchains/mingw-w64-x86_64.cmake"
cmake --build build-windows -j20
```

Output binaries:

- `build-windows/products/x64/bin/egoboo.exe`
- `build-windows/products/x64/bin/egoboo-content-validator.exe`

## Dependency Bundle

The cross-build uses Windows-target SDL libraries from `external/mingw` instead
of Linux `pkg-config` packages. Override the bundle only if needed:

```bash
cmake -S . -B build-windows \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/toolchains/mingw-w64-x86_64.cmake" \
  -DEGOBOO_MINGW_DEPENDENCY_ROOT="/path/to/windows-deps"
```

Expected bundle shape:

- `include/SDL2/`
- `lib/`
- `bin/`

The build copies discovered runtime DLLs into the output directory for the game
and validator.

## Run Through Wine

```bash
./run-egoboo-windows.sh
```

The helper starts from the repository `data/` directory, supplies MinGW runtime
DLL paths through `WINEPATH` when needed, and defaults:

- `EGOBOO_DISABLE_MIPMAPS=1`
- `EGOBOO_DISABLE_AUDIO=1`

To test the unstable paths explicitly:

```bash
EGOBOO_DISABLE_MIPMAPS=0 EGOBOO_DISABLE_AUDIO=0 ./run-egoboo-windows.sh
```

## Notes

- Linux-native builds are covered by `doc/build-linux.md`.
- The checked-in toolchain file is x64-specific. Add a separate
  `i686-w64-mingw32` toolchain file for 32-bit targets.
- Cross-builds disable runnable test discovery unless
  `CMAKE_CROSSCOMPILING_EMULATOR` is set.
- MinGW-specific system link differences such as `shlwapi` and `winmm` are
  handled by CMake.

## Troubleshooting

Check these first:

1. `git submodule update --init data external idlib idlib-game-engine`
2. `which x86_64-w64-mingw32-gcc`
3. `which x86_64-w64-mingw32-g++`
4. `ls external/mingw/include/SDL2/SDL.h`
5. `ls external/mingw/lib/libSDL2.a`
6. `ls build-windows/products/x64/bin`

Common failures:

- Missing `x86_64-w64-mingw32-g++`: install `mingw64-gcc-c++`.
- Missing SDL MinGW libraries: check `EGOBOO_MINGW_DEPENDENCY_ROOT`.
- CMake tries to run a Windows `.exe`: remove `build-windows/` and reconfigure
  with the current cross-build settings.
