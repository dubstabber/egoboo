# Repository And Build Audit

## 1. Active versus inactive repository areas

The repository root is not a clean picture of the product. It contains live code, vendored dependencies, generated output, and at least one large local archive copy.

### Active code and content

| Path | Role | Notes |
| --- | --- | --- |
| `egoboo/` | Thin executable target | Only `egoboo/src/game/Main.cpp` is built into the main executable |
| `egolib/` | Main runtime library | Contains engine services, gameplay, rendering, GUI, file formats, scripting, and legacy C |
| `idlib/` | Base support library | Submodule |
| `idlib-game-engine/` | Additional engine support | Submodule |
| `data/` | Game content | Modules, global objects, particle data, menus, docs |
| `utilities/` | Auxiliary tools | Mixed quality and maintenance level |

### Repository noise that should not drive refactoring decisions

| Path | Current state | Why it matters |
| --- | --- | --- |
| `backup-copy/` | Untracked, about 2.2G | Local archive copy; doubles visual complexity of the repo |
| `build/` | Untracked, about 1.7G | Generated build output; should not be read as source |
| `external/` | Vendored, about 266M | Important for portability, but not part of Egoboo's architectural core |
| `cartman/` | Source present, but not wired into root build | Editor code exists, but is currently outside the main product build graph |

## 2. Actual build graph

The root `CMakeLists.txt` is the real build entry point.

It does the following:

1. Adds `idlib/`
2. Adds `idlib-game-engine/`
3. Adds `egolib/`
4. Adds `egoboo/`

Important details:

- Tests are force-enabled for both `idlib` and `idlib-game-engine`.
- `egolib` is built as one static library via recursive globbing of all `*.c`, `*.cpp`, `*.h`, and `*.hpp` files.
- `egoboo` is essentially a thin wrapper executable linked against `egolib-library`.

### Why this build shape is risky

- Recursive globbing hides subsystem ownership.
- There is no explicit module graph inside `egolib`.
- Legacy C and newer C++ are compiled into the same library without a clear boundary.
- The build graph does not communicate intended architecture, only "compile everything".

## 3. Build documentation drift

The repo already contains contradictory build stories.

### Current situation

- `README.md` describes an out-of-tree CMake build.
- `README.Linux` still describes `make all` and `make install`.
- `run-egoboo.sh` assumes a local binary in `build/products/x64/bin/egoboo` and injects environment variables for Linux execution.

### What this implies

- The canonical build instructions are unclear.
- Fresh contributors are likely to read stale instructions first.
- Linux support currently depends on local knowledge that is not represented in one authoritative document.
- Windows support is also split between future intent and current reality: a Linux-to-Windows cross-build exists, but the runtime and dependency story are still fragile.

## 4. Local portability patches already in the workspace

The current workspace contains uncommitted edits that change how the game builds or runs on Linux/Fedora.

### Observed local edits

- `egolib/library/src/egolib/Platform/file_linux.c`
  - Adds `EGOBOO_DATA_DIR` support before falling back to `SDL_GetBasePath()`
- `egolib/library/src/egolib/vfs.c`
  - Enables `PHYSFS_permitSymbolicLinks(1)`
- `egolib/library/src/egolib/Graphics/SDL/GraphicsWindow.cpp`
  - Forces an OpenGL 2.1 compatibility profile
- `egolib/library/src/egolib/Graphics/SDL/GraphicsContext.cpp`
  - Enables `glewExperimental`, improves GLEW error handling, logs GL context details
- `egolib/library/src/egolib/Graphics/SDL/Utilities.cpp`
  - Changes fullscreen requirement relaxation behavior
- `egolib/library/src/egolib/Float.hpp`
  - Adds missing `<cstdint>` include

### Interpretation

These are not random edits. They are evidence that:

- The project has real portability debt on modern Linux systems.
- The runtime still assumes filesystem and GL behavior that is no longer portable enough.
- "It compiles on my machine" currently depends on local source drift.
- The same class of portability debt is likely blocking cleaner Windows support and Linux-hosted Windows cross-build parity.

These patches should be documented and then turned into explicit, reviewable portability decisions rather than staying as anonymous workspace edits.

## 5. Current Windows cross-build reality

The repository now has a documented `mingw-w64` path for building Windows binaries from Linux, but that should not be mistaken for healthy Windows support yet.

Current problems:

- old dependency handling still leaks into platform-specific setup
- the monolithic `egolib` build shape makes platform assumptions hard to isolate
- Linux-native, native-Windows, and Linux-hosted Windows builds do not yet behave like variations of one coherent build flow
- runtime failures under Wine mean the current Windows artifact is not yet a usable gameplay target

The checked-in `debug-output.txt` shows concrete evidence of that runtime state on the current Linux-hosted Windows path:

- font atlas creation escalates until a fatal failure in `egolib/Graphics/Font.cpp`
- the font manager falls back after failing to load `mp_data/IMMORTAL.ttf`
- execution then dies with an unhandled page fault under Wine while inside `Mix_LoadWAV_RW`

This is enough to treat the current Windows-on-Linux path as a debugging baseline, not as completed portability work.

## 6. Submodule state and branch drift

`.gitmodules` says `idlib` and `idlib-game-engine` are intended to follow `develop`, but the checked submodule statuses currently point at `master` lineage.

This matters because:

- Refactors across the engine boundary can silently depend on local submodule state.
- Reproducibility is weaker when the intended branch policy and the actual workspace state differ.

## 7. Tooling health

### Tests

Automated test coverage is shallow at the Egoboo layer.

- `egolib/tests` has only 4 test sources:
  - compilation
  - mesh iterator
  - quad tree
  - string utilities
- `idlib` has broader utility tests.
- `idlib-game-engine` has a single compilation-level test source.

What is not protected:

- module loading
- object profile loading
- scripting
- save/import/export
- mesh/content compatibility
- rendering behavior
- gameplay systems

### Utilities

`utilities/` contains potentially useful tools, but maintenance quality varies:

- `objectviewer/` and `modelverifier/` are niche legacy helpers
- `migrator/` contains real migration intent, but several tools are only scaffolds
- `migrator/README.md` is currently unrelated to the actual toolset, which is a sign of documentation rot

## 8. Large code hotspots

The largest active translation units are a good proxy for risk concentration.

| File | Approx. lines |
| --- | ---: |
| `egolib/library/src/egolib/game/script_functions.c` | 8153 |
| `egolib/library/src/egolib/Entities/Object.cpp` | 3155 |
| `egolib/library/src/egolib/game/game.c` | 2443 |
| `egolib/library/src/egolib/vfs.c` | 2435 |
| `egolib/library/src/egolib/game/graphic.c` | 2222 |
| `egolib/library/src/egolib/Profiles/ObjectProfile.cpp` | 1468 |
| `egolib/library/src/egolib/game/Physics/particle_collision.c` | 1465 |
| `egolib/library/src/egolib/game/Module/Module.cpp` | 1225 |

These are strong candidates for:

- characterization tests
- extraction of interfaces
- file-splitting before behavior changes

## 9. Practical conclusions for the refactor

Before large-scale code motion:

1. Define the active source scope and stop reading `backup-copy/` and `build/` as if they were source.
2. Publish one canonical Linux build/run document that includes the Fedora-specific realities already captured in code and `run-egoboo.sh`.
3. Stop using recursive file globbing as the only expression of library structure once subsystem extraction begins.
4. Treat local portability patches as first-class tracked work items, not invisible environment glue.
5. Avoid starting the refactor in the largest files without first adding seams around them.
6. Treat Linux-native, native-Windows, and Linux-hosted Windows builds as one portability problem with shared architectural causes.
7. Use the current `debug-output.txt` failures as a baseline for Windows runtime stabilization work, not as acceptable known behavior.
