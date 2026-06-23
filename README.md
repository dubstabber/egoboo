# Egoboo

Egoboo is an open-source 3D dungeon crawler in the spirit of NetHack. The
current codebase is a mixed C/C++ SDL2/OpenGL 2.1 runtime that is being
modernized toward smaller subsystems, clearer ownership, and first-class Linux
and open-source Windows build paths.

## Current Direction

- Keep Linux native builds as the primary development path.
- Keep Linux-hosted Windows cross-compilation working with `mingw-w64`.
- Make native Windows builds first-class through an open-source toolchain, not a
  Visual Studio-only workflow.
- Reduce global coupling, warning noise, and portability debt while preserving
  shipped content behavior.
- Use `refactoring-documents/CODEBASE-HEALTH-STATUS.md` for live architecture,
  test, archive, and validator metrics.

## Maintained Docs

- Linux build/run: `doc/build-linux.md`
- Windows cross-build and status: `doc/build-windows.md`
- Error policy: `doc/error-handling-policy.md`
- Architecture and roadmap: `refactoring-documents/README.md`

Older platform-specific READMEs are historical. Prefer the files above for
current setup and refactor work.

## Quick Start

Initialize the top-level submodules:

```bash
git submodule update --init data external idlib idlib-game-engine
```

Configure, build, and test the Linux target:

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_C_COMPILER_LAUNCHER=ccache \
  -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
cmake --build build -j20
ctest --test-dir build -j20 --output-on-failure
```

Run from the source tree:

```bash
./run-egoboo.sh
```

Linux binaries are written to `build/products/x64/bin/`. The launcher uses the
repository `data/` checkout and writes runtime user data under
`.egoboo-runtime/`.

## Windows Cross-Build

The maintained Windows build path is currently a Linux-hosted `mingw-w64` x64
cross-build:

```bash
cmake -S . -B build-windows -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/toolchains/mingw-w64-x86_64.cmake"
cmake --build build-windows -j20
```

Windows binaries are written to `build-windows/products/x64/bin/`. The helper
below can run the cross-built binary through Wine for compatibility debugging,
but Wine is not the long-term Windows support target:

```bash
./run-egoboo-windows.sh
```

## Content Validation

Use the validator after changes to content loading, VFS behavior, module/object
data, script compilation, model loading, or related runtime code:

```bash
./build/products/x64/bin/egoboo-content-validator --data-dir "$PWD/data" --module test.mod
./build/products/x64/bin/egoboo-content-validator --data-dir "$PWD/data"
```

The full legacy content set has known pre-existing failures. Check
`refactoring-documents/06-validator-baseline.md` before treating full-validator
output as a new regression.

## License

Egoboo is licensed under GPLv3. See `LICENSE`.
