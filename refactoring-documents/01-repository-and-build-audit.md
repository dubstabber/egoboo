# Repository And Build Audit

Current audit date: 2026-06-23. Volatile quantitative metrics live in
`CODEBASE-HEALTH-STATUS.md`; this document records repository shape and build
policy.

## Active Areas

| Path | Role | Notes |
| --- | --- | --- |
| `egoboo/` | Executable wrapper | `egoboo/src/game/Main.cpp` installs runtime systems and starts `GameEngine` |
| `egolib/` | Main runtime library | Primary source area; nine static archives under one CMake package |
| `tools/` | Active tools | Builds `egoboo-content-validator` |
| `cartman/` | Map editor | In CMake behind `EGOBOO_BUILD_CARTMAN=OFF`; not part of default build |
| `data/` | Game content | Submodule |
| `idlib/` | Foundation library | Submodule |
| `idlib-game-engine/` | Engine support | Submodule; root CMake passes top-level `idlib/` into it |
| `external/` | Third-party bundle | Submodule; includes googletest and Windows dependency bundle |

Do not treat generated or archival directories as source:

| Path | Policy |
| --- | --- |
| `build/`, `build-windows/` | Generated output; never manually edit |
| `backup-copy/` | Read-only reference snapshot; never modify or clean up |
| `doc/legacy/` | Deprecated historical docs, not active build guidance |

## Build Graph

The root `CMakeLists.txt` adds:

1. `idlib/`
2. `idlib-game-engine/`
3. `egolib/`
4. `egoboo/`
5. `cartman/` behind `option(EGOBOO_BUILD_CARTMAN OFF)`
6. `tools/`

`egolib` is no longer one monolithic archive. It currently builds as nine static
archives:

```text
egolib-foundation-base <- {egolib-physics, egolib-renderer <- egolib-gui}
  <- egolib-library <- egolib-game-graphics <- egolib-hud-widgets
  <- {egolib-scriptvm, egolib-gamestates}
```

The live archive member counts and source-size metrics are maintained in
`CODEBASE-HEALTH-STATUS.md`. When moving source files between archives, verify
against the built `.a` archives with `nm`; do not use stale object directories
as dependency evidence.

## Canonical Build Docs

- Linux native: `doc/build-linux.md`
- Windows cross-build from Linux: `doc/build-windows.md`
- Top-level quick start: `README.md`

Legacy platform READMEs were quarantined under `doc/legacy/` and should not be
used as active setup instructions.

Preferred local build on this machine:

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_C_COMPILER_LAUNCHER=ccache \
  -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
cmake --build build -j20
ctest --test-dir build -j20 --output-on-failure
```

The test runner is parallel-safe in the current harness; each test process gets
an isolated `EGOBOO_USER_DIR`.

## Linux Portability Behavior

These are intentional current behavior, not incidental local patches:

- `EGOBOO_DATA_DIR` can point the runtime at the source-tree `data/` checkout.
- `run-egoboo.sh` keeps generated runtime data under `.egoboo-runtime/`.
- PhysFS symbolic links are permitted for source-tree data use.
- SDL requests an OpenGL 2.1 compatibility context.
- `SDL_VIDEODRIVER=x11` remains useful on Wayland systems with legacy OpenGL
  compatibility issues.

Preserve these unless explicitly revisiting the Linux runtime policy.

## Windows Reality

The maintained Windows build path is currently the Linux-hosted mingw-w64 x64
cross-build. It builds, but the Wine runtime path is still a compatibility and
debugging aid rather than a healthy gameplay target.

Known constraints:

- `run-egoboo-windows.sh` still defaults compatibility environment variables for
  mipmaps and audio.
- Native Windows with an open-source toolchain is still a target, not a
  documented first-class path.
- Visual Studio-specific workflows are legacy/deprecated and should not be
  revived as the maintained path.

## Submodules

Initialize the top-level submodules only for normal superproject work:

```bash
git submodule update --init data external idlib idlib-game-engine
```

The nested `idlib-game-engine/idlib` checkout is not required for the
superproject build because the top-level `idlib/` path is passed in by CMake.

## Current Tooling Health

- Current ctest count and green baseline live in `CODEBASE-HEALTH-STATUS.md`.
- `egoboo-content-validator` is integrated into the build and has a known
  full-content baseline recorded in `06-validator-baseline.md`.
- `utilities/` contains legacy helper tools with mixed maintenance status; they
  are not default-build verification surfaces.

## Practical Refactor Guidance

1. Ignore `backup-copy/` and generated build directories when measuring source.
2. Use `CODEBASE-HEALTH-STATUS.md` for volatile counts.
3. Keep CMake source-list edits scoped and verify archive direction after moving
   files.
4. Prefer seam creation and file-local splits over broad rewrites.
5. Run the validator after changes to VFS, content loading, object profiles,
   scripts, or module loading.
