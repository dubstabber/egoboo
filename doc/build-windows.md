# Build For Windows

This document covers the current Windows build paths in this repository.

It matches the active CMake build and the Windows dependency handling now present in this workspace.

## Supported paths

- Native Windows build with Visual Studio
- Fedora/Linux cross-build to Windows x64 with `mingw-w64`

## Native Windows build

The maintained native Windows path is CMake with a Visual Studio generator.

Prerequisites:

- Visual Studio 2022 with the Desktop development with C++ workload
- CMake
- Git submodules initialized with `git submodule update --init --recursive`

From a Developer PowerShell or Developer Command Prompt:

```bash
cmake -S . -B build-vs -G "Visual Studio 17 2022" -A x64
cmake --build build-vs --config Release -j4
```

Outputs land under:

```text
build-vs/products/release/x64/bin/
```

On Windows builds, the game and content validator now stage the required SDL runtime DLLs into the executable directory automatically.

Important binaries:

- game: `build-vs/products/release/x64/bin/egoboo.exe`
- validator: `build-vs/products/release/x64/bin/egoboo-content-validator.exe`

## Fedora cross-build to Windows x64

Install a MinGW toolchain first. On Fedora this is typically close to:

```bash
sudo dnf install \
  mingw64-gcc mingw64-gcc-c++ mingw64-binutils \
  mingw64-winpthreads-static
```

Verify the cross-compilers exist:

```bash
which x86_64-w64-mingw32-gcc
which x86_64-w64-mingw32-g++
```

Configure with the checked-in toolchain file:

```bash
cmake -S . -B build-windows \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/toolchains/mingw-w64-x86_64.cmake"
cmake --build build-windows -j4
```

If you want a clean rebuild:

```bash
rm -rf build-windows
cmake -S . -B build-windows \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/toolchains/mingw-w64-x86_64.cmake"
cmake --build build-windows -j4
```

Outputs land under:

```text
build-windows/products/x64/bin/
```

Important binaries:

- game: `build-windows/products/x64/bin/egoboo.exe`
- validator: `build-windows/products/x64/bin/egoboo-content-validator.exe`

## Run the Windows build from Linux with Wine

If you cross-built the Windows binary on Linux and want to launch that `.exe`
directly, use the checked-in helper script:

```bash
./run-egoboo-windows.sh
```

The script:

- prefers `build-windows/products/x64/bin/egoboo.exe`
- falls back to `build-vs/products/release/x64/bin/egoboo.exe`
- starts the game from the repository `data/` directory so the Windows build
  can resolve `basicdat/`
- adds the MinGW runtime DLL directory to `WINEPATH` automatically when the
  cross-built output is missing `libgcc_s_seh-1.dll`, `libstdc++-6.dll`, or
  `libwinpthread-1.dll`
- defaults `EGOBOO_DISABLE_MIPMAPS=1` for the current Wine path to avoid a
  texture-upload crash in the Windows build
- defaults `EGOBOO_DISABLE_AUDIO=1` for the current Wine path because startup
  sound loading still crashes in the Windows build under Wine
- uses `wine` by default, or `WINE_BIN=/path/to/wine` if you need a different
  runner

If you want to experiment with the unstable paths anyway, override them on the
command line:

```bash
EGOBOO_DISABLE_MIPMAPS=0 EGOBOO_DISABLE_AUDIO=0 ./run-egoboo-windows.sh
```

## Windows dependency bundle for MinGW

The MinGW path uses a Windows-target dependency bundle for SDL instead of the Linux `pkg-config` setup.

By default it uses:

```text
external/mingw
```

If your Windows SDL bundle lives elsewhere, override it at configure time:

```bash
cmake -S . -B build-windows \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/toolchains/mingw-w64-x86_64.cmake" \
  -DEGOBOO_MINGW_DEPENDENCY_ROOT="/path/to/windows-deps"
```

The bundle is expected to contain:

- `include/SDL2/`
- `lib/`
- `bin/`

The build copies any discovered runtime DLLs from that bundle into the output directory for `egoboo` and `egoboo-content-validator`.

Minimum expected bundle layout:

- `external/mingw/include/SDL2/SDL.h`
- `external/mingw/lib/libSDL2.a`
- `external/mingw/lib/libSDL2main.a`
- `external/mingw/lib/libSDL2_image.a`
- `external/mingw/lib/libSDL2_mixer.a`
- `external/mingw/lib/libSDL2_ttf.a`
- `external/mingw/bin/`

## Notes

- Linux-native builds still use `doc/build-linux.md`.
- The Fedora cross-build path is x64-focused. If you need 32-bit Windows targets, add a separate `i686-w64-mingw32` toolchain file rather than reusing the x64 one.
- Cross-builds disable runnable test targets by default unless `CMAKE_CROSSCOMPILING_EMULATOR` is set. This avoids CMake trying to execute Windows `.exe` test binaries on the Linux build host during `gtest_discover_tests()`.
- Fedora cross-builds now handle MinGW-specific system link differences such as `shlwapi` and `winmm` internally; you should not need to add them manually on the command line.

## If the Windows build fails

Check these first:

1. `git submodule update --init --recursive`
2. `which x86_64-w64-mingw32-gcc` and `which x86_64-w64-mingw32-g++` for Fedora cross-builds
3. `ls external/mingw/include/SDL2/SDL.h`
4. `ls external/mingw/lib/libSDL2.a`
5. `ls build-vs/products/release/x64/bin` or `ls build-windows/products/x64/bin`

Common failure patterns:

- `x86_64-w64-mingw32-g++ was not found`: install `mingw64-gcc-c++`
- `Failed to find MinGW library SDL2`: check `EGOBOO_MINGW_DEPENDENCY_ROOT`
- `GoogleTestAddTests.cmake` tries to run a `.exe` during cross-build: remove the build directory and reconfigure so the current cross-build test settings apply
