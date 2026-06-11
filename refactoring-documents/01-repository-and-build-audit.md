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
| `cartman/` | Wired into the build, gated off by default | Map editor; `add_subdirectory(cartman)` behind `option(EGOBOO_BUILD_CARTMAN OFF)` (T3.5) — compiles/links/runs when enabled, excluded from the default build |
| `tools/` | Active tool target | The `egoboo-content-validator` (`add_subdirectory(tools)`) |

## 2. Actual build graph

The root `CMakeLists.txt` is the real build entry point.

It does the following:

1. Adds `idlib/`
2. Adds `idlib-game-engine/`
3. Adds `egolib/`
4. Adds `egoboo/`
5. Adds `cartman/` — behind `option(EGOBOO_BUILD_CARTMAN OFF)`, so excluded from the default build (CMakeLists.txt:51)
6. Adds `tools/` — the `egoboo-content-validator` (CMakeLists.txt:54)

Important details:

- Tests are force-enabled for both `idlib` and `idlib-game-engine`.
- `egolib` now builds as a fully-acyclic five-archive DAG (egolib-foundation-base, egolib-physics, egolib-renderer, egolib-gui, egolib-library), defined in `egolib/library/CMakeLists.txt`. The former recursive `GLOB_RECURSE` has been replaced with explicit, per-subsystem `set()` source lists, so subsystem ownership is now visible in the build files — but everything still links into a single static library.
- `egoboo` is essentially a thin wrapper executable linked against `egolib-library`.

### Why this build shape is still imperfect

- `egolib` is now split into five static archives forming an acyclic DAG (nm-symbol-closure verified), so dependency direction IS enforced at link time; consumers link only `egolib-library`.
- There is no explicit module/link graph inside `egolib` (the `idlib` 11-sub-library shape is the target).
- Legacy C and newer C++ are compiled into the same library without a clear boundary.

## 3. Build documentation

Canonical build paths:

- `doc/build-linux.md` — Linux native build.
- `doc/build-windows.md` — Linux-hosted mingw-w64 cross-build.
- Top-level `README.md` describes the out-of-tree CMake build.
- `run-egoboo.sh` runs the local binary at `build/products/x64/bin/egoboo` with the environment variables Linux execution requires.

Legacy platform READMEs (`README.MinGW`, `README.OSX`, `README.Windows`, `README.VisualStudio`) were quarantined out of the repo root into `doc/legacy/` (T2.6, done); they remain there as deprecated references. The canonical build docs are `doc/build-linux.md` and `doc/build-windows.md`.

## 4. Fedora portability behavior (preserved)

Egoboo runs on modern Fedora thanks to specific portability fixes that are now part of the committed history (see commit `b97717e48` and related). These should be preserved — they represent real portability decisions, not accidental workspace drift:

- `egolib/library/src/egolib/Platform/file_linux.c` — `EGOBOO_DATA_DIR` env override before falling back to `SDL_GetBasePath()`.
- `egolib/library/src/egolib/vfs.c` — `PHYSFS_permitSymbolicLinks(1)` enabled.
- `egolib/library/src/egolib/Graphics/SDL/GraphicsWindow.cpp` — forces an OpenGL 2.1 compatibility profile.
- `egolib/library/src/egolib/Graphics/SDL/GraphicsContext.cpp` — `glewExperimental` enabled, GLEW error handling improved, GL context details logged.
- `egolib/library/src/egolib/Graphics/SDL/Utilities.cpp` — relaxed fullscreen requirement behavior.
- `egolib/library/src/egolib/Float.hpp` — explicit `<cstdint>` include.

Broader portability debt still exists. Treat further Linux/Wine runtime surprises as opportunities to make more portability behavior explicit rather than layer more local patches on top of them.

## 5. Current Windows cross-build reality

The repository now has a documented `mingw-w64` path for building Windows binaries from Linux, but that should not be mistaken for healthy Windows support yet.

Current problems:

- old dependency handling still leaks into platform-specific setup
- the monolithic `egolib` build shape makes platform assumptions hard to isolate
- Linux-native, native-Windows, and Linux-hosted Windows builds do not yet behave like variations of one coherent build flow
- runtime failures under Wine mean the current Windows artifact is not yet a usable gameplay target

The historically-observed Wine failure mode (a font-atlas escalation/fatal failure in `egolib/Graphics/Font.cpp` and an unhandled page fault inside `Mix_LoadWAV_RW` during audio load) is why `run-egoboo-windows.sh` gates the cross build with `EGOBOO_DISABLE_MIPMAPS=1 EGOBOO_DISABLE_AUDIO=1`; diagnosing these is roadmap item T2.5. (The earlier checked-in `debug-output.txt` capture is no longer in the tree.) Note that the now-integrated `cartman` editor *does* boot an OpenGL 4.6 context under Wine, so the cross-build GL path is not uniformly broken.

This is enough to treat the current Windows-on-Linux path as a debugging baseline, not as completed portability work.

## 6. Submodule state

`.gitmodules` declares `branch = master` for both `idlib` and `idlib-game-engine`, but the actual checkouts differ — `idlib` is on `develop` and `idlib-game-engine` on `master` (all four submodules — `idlib`, `idlib-game-engine`, `data`, `external` — are the maintainer's own forks under `github.com/dubstabber/...`, checked out detached). Refactors across the engine boundary should not silently depend on local submodule state; if submodule branch policy changes, update this section.

## 7. Tooling health

### Tests

Automated test coverage at the Egoboo layer has grown substantially. `egolib/tests/egolib/tests/` now holds **40 test `.cpp` files / 811 ctest cases** (for the full enumerated list see `CODEBASE-HEALTH-STATUS.md` §2). Coverage now spans utilities, content parsers, module load/spawn, player/quest startup, seam/accessor regression, script loader/VM/dispatch, gameplay alerts and shop interactions, physics/collision and bounding-volume math, map twist and damage/attribute enums, and — via a live spawned `Object` — combat damage-resolution math.

`idlib` has broader utility tests. `idlib-game-engine` has a single compilation-level test source.

Test-to-code ratio is now roughly **16.9%** (≈20,400 test lines against ≈121,000 active source lines), up from ~3.6% at the April baseline. Still absent: rendering correctness, GUI state transitions, AI, and the full combat *integration* path (`Object::damage(...)` side effects, `do_chr_prt_collision` pipelines).

### Utilities

`utilities/` contains potentially useful tools, but maintenance quality varies:

- `objectviewer/` and `modelverifier/` are niche legacy helpers
- `migrator/` contains real migration intent, but several tools are only scaffolds
- `migrator/README.md` is currently unrelated to the actual toolset, which is a sign of documentation rot

## 8. Large code hotspots

The file-split passes have decomposed every former oversized translation unit. `script_functions_systems.c` has been fully decomposed and deleted. The single largest TU is now `Object.hpp` (~1,613 lines); the next tier is `script_functions_spawn.c` ~1,576, `particle_collision.c` ~1,528, `vfs.c` ~1,500 (split this session into vfs.c + vfs_rwops.c + vfs_mount.c), `ObjectGraphics.cpp` ~1,488, `script.c` ~1,369, `script_compile.c` ~1,151 sits well below that. **The authoritative, live hotspot table lives in `CODEBASE-HEALTH-STATUS.md` §3** (Key Metrics) — this doc defers to it rather than maintaining a parallel copy that drifts.

`Object.cpp` itself is now ~200 lines: the implementation was split across six per-aspect TUs (`Object_{appearance,attributes,combat,interaction,lifecycle,update}.cpp`, plus the separate `ObjectHandler.cpp`) while the interface surface in `Object.hpp` stayed fat. That is the ISP/SRP frontier for T1.2 role-interface extraction. Script dispatch is likewise split across seven `script_functions_*.c` files but still one logical subsystem — the extensibility fix was scoped under T3.2 (the dispatch is in fact already an X-macro registry; see the roadmap note).

## 9. Practical conclusions for the refactor

Before large-scale code motion:

1. Define the active source scope and stop reading `backup-copy/` and `build/` as if they were source.
2. Publish one canonical Linux build/run document that includes the Fedora-specific realities already captured in code and `run-egoboo.sh`.
3. Stop using recursive file globbing as the only expression of library structure once subsystem extraction begins.
4. Treat local portability patches as first-class tracked work items, not invisible environment glue.
5. Avoid starting the refactor in the largest files without first adding seams around them.
6. Treat Linux-native, native-Windows, and Linux-hosted Windows builds as one portability problem with shared architectural causes.
7. Treat the known Wine runtime failures (font-atlas init, `Mix_LoadWAV_RW` audio crash; T2.5) as a stabilization baseline, not as acceptable known behavior.
