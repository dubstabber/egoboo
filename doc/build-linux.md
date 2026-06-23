# Build And Run On Linux

This is the maintained Linux build and run guide for the current CMake layout.

## Setup

Initialize the top-level submodules:

```bash
git submodule update --init data external idlib idlib-game-engine
```

The nested `idlib-game-engine/idlib` checkout is not needed for the normal
superproject build; the root CMake project passes the top-level `idlib/` path
into `idlib-game-engine`.

Install a C/C++ toolchain, CMake, `pkg-config`, SDL2 development packages, and
OpenGL development headers. `PhysFS` is vendored through `idlib-game-engine`.

Fedora package baseline:

```bash
sudo dnf install \
  cmake ninja-build ccache gcc gcc-c++ make pkgconf-pkg-config \
  SDL2-devel SDL2_image-devel SDL2_mixer-devel SDL2_ttf-devel \
  mesa-libGL-devel
```

For other distributions, use `idlib-game-engine/library/CMakeLists.txt` as the
dependency contract.

## Build

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_C_COMPILER_LAUNCHER=ccache \
  -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
cmake --build build -j20
```

Output binaries:

- `build/products/x64/bin/egoboo`
- `build/products/x64/bin/egoboo-content-validator`

Run the test suite:

```bash
ctest --test-dir build -j20 --output-on-failure
```

The test harness is parallel-safe; each test process gets its own
`EGOBOO_USER_DIR`.

## Run

Preferred source-tree launch:

```bash
./run-egoboo.sh
```

The helper points the game at `data/` and writes runtime user data under
`.egoboo-runtime/`. For manual launches, set at least:

```bash
EGOBOO_DATA_DIR="$PWD/data" ./build/products/x64/bin/egoboo
```

`SDL_VIDEODRIVER=x11` can help on Wayland systems with legacy OpenGL
compatibility issues. The helper also sets `DRI_PRIME=1`; adjust that locally if
it does not match your GPU setup.

## Validate Content

Fast smoke check:

```bash
./build/products/x64/bin/egoboo-content-validator --data-dir "$PWD/data" --module test.mod
```

Full known-baseline validation:

```bash
./build/products/x64/bin/egoboo-content-validator --data-dir "$PWD/data"
```

The full legacy content set has known pre-existing failures. See
`refactoring-documents/06-validator-baseline.md` for the current baseline and
validator scope.

## Current Linux Assumptions

- `EGOBOO_DATA_DIR` can point at the source-tree `data/` checkout.
- Source-tree launches intentionally allow PhysFS symlinks.
- SDL requests an OpenGL 2.1 compatibility context.
- Runtime logs and generated user data should stay under `.egoboo-runtime/`
  when using `run-egoboo.sh`.

## Troubleshooting

Check these first:

1. `git submodule update --init data external idlib idlib-game-engine`
2. `pkg-config --modversion sdl2 SDL2_image SDL2_mixer SDL2_ttf`
3. `ls build/products/x64/bin`
4. `.egoboo-runtime/` for generated logs and debug output
