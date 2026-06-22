# Build And Run On Linux

This is the canonical Linux build and run document for the current repository layout.

It matches the active CMake build, the current binary output paths, and the Linux/Fedora path handling already present in this workspace.

## Repository setup

Clone the repository and initialize submodules:

```bash
git clone https://github.com/dubstabber/egoboo
cd egoboo
git submodule update --init data external idlib idlib-game-engine
```

For the normal Egoboo superproject build, only these top-level submodules are
required. The root `CMakeLists.txt` passes `IDLIB_PATH` to
`idlib-game-engine`, so the nested `idlib-game-engine/idlib` checkout is not
needed unless you are working on `idlib-game-engine` as a standalone project.

## Linux dependencies

The current CMake build expects these development packages to be available through `pkg-config`:

- `sdl2`
- `SDL2_image`
- `SDL2_mixer`
- `SDL2_ttf`
- `OpenGL`
- standard C/C++ toolchain
- `cmake`
- `pkg-config`

`PhysFS` is currently fetched and built by CMake from the vendored `idlib-game-engine` setup, so it does not need to be installed separately for this build.

On Fedora, the practical package set is usually close to:

```bash
sudo dnf install \
  cmake ninja-build ccache gcc gcc-c++ make pkgconf-pkg-config \
  SDL2-devel SDL2_image-devel SDL2_mixer-devel SDL2_ttf-devel \
  mesa-libGL-devel
```

If your environment differs, inspect `idlib-game-engine/library/CMakeLists.txt` first. That file is the real dependency contract.

## Configure and build

Use the root CMake project from the repository root:

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_C_COMPILER_LAUNCHER=ccache \
  -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
cmake --build build -j20
```

The default runtime outputs land in:

```text
build/products/x64/bin/
```

Important binaries:

- game: `build/products/x64/bin/egoboo`
- validator: `build/products/x64/bin/egoboo-content-validator`

If you want a clean rebuild:

```bash
rm -rf build
cmake -S . -B build -G Ninja \
  -DCMAKE_C_COMPILER_LAUNCHER=ccache \
  -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
cmake --build build -j20
```

Run tests after a successful build:

```bash
ctest --test-dir build -j20 --output-on-failure
```

The test harness is parallel-safe. Each test process gets an isolated
`EGOBOO_USER_DIR`, so `ctest -j20` is appropriate on this 24-thread machine.

## Run the game from the source tree

The Linux port in this workspace supports `EGOBOO_DATA_DIR`, and the repository already includes a helper script that uses it.

Preferred:

```bash
./run-egoboo.sh
```

Equivalent manual invocation:

```bash
SDL_VIDEODRIVER=x11 \
EGOBOO_DATA_DIR="$PWD/data" \
./build/products/x64/bin/egoboo
```

Notes:

- `EGOBOO_DATA_DIR="$PWD/data"` points the game at the repository data checkout instead of an installed data directory.
- Generated user/config/debug data now stays inside the repository under `.egoboo-runtime/` instead of `~/.local/share/...`.
- `SDL_VIDEODRIVER=x11` is useful on Wayland systems when legacy OpenGL compatibility is problematic.
- The checked-in helper script also sets `DRI_PRIME=1`; keep or remove that depending on your GPU setup.

## Validate content without launching the full game

The content validator is intended for refactor work and CI-style smoke checks.

Validate all discovered modules:

```bash
./build/products/x64/bin/egoboo-content-validator --data-dir "$PWD/data"
```

Validate a single module:

```bash
./build/products/x64/bin/egoboo-content-validator \
  --data-dir "$PWD/data" \
  --module test.mod
```

Useful flags:

- `--verbose`
- `--skip-scripts`

Current validator scope:

- module profile discovery
- `menu.txt` presence through `ModuleProfile` loading
- `level.mpd` parsing
- `wawalite.txt` parsing
- `spawn.txt` parsing
- local object profile parsing via lightweight object loads
- spawn-referenced object existence checks
- object script compilation checks, including fallback detection

This is intentionally narrower than a full runtime playtest. It is meant to catch parser and content-shape regressions early.

## Known Linux-specific realities in this workspace

The current workspace already carries Linux/Fedora-oriented portability edits:

- `EGOBOO_DATA_DIR` support in `egolib/library/src/egolib/Platform/file_linux.c`
- symbolic link allowance in `egolib/library/src/egolib/vfs.c`
- explicit OpenGL 2.1 compatibility-context setup in the SDL window/context code

Treat those as current operational requirements, not accidental local trivia.

## If configuration or runtime fails

Check these first:

1. `git submodule update --init data external idlib idlib-game-engine`
2. `pkg-config --modversion sdl2 SDL2_image SDL2_mixer SDL2_ttf`
3. `echo "$EGOBOO_DATA_DIR"`
4. `ls build/products/x64/bin`
5. `.egoboo-runtime/` in the repository root for generated logs and debug output

If the game runs only with `./run-egoboo.sh` but not by direct invocation, your data path or SDL video driver assumptions are still incomplete.
