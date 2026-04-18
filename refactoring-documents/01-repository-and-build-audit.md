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

## 3. Build documentation

Canonical build paths:

- `doc/build-linux.md` — Linux native build.
- `doc/build-windows.md` — Linux-hosted mingw-w64 cross-build.
- Top-level `README.md` describes the out-of-tree CMake build.
- `run-egoboo.sh` runs the local binary at `build/products/x64/bin/egoboo` with the environment variables Linux execution requires.

Legacy platform READMEs (`README.MinGW`, `README.OSX`, `README.Windows`, `README.VisualStudio`) still exist in the tree but are outside the maintained path; `README.Linux` is a short stub that redirects to `doc/build-linux.md`. Retirement of the legacy READMEs is tracked as roadmap item T2.6.

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

The checked-in `debug-output.txt` shows concrete evidence of that runtime state on the current Linux-hosted Windows path:

- font atlas creation escalates until a fatal failure in `egolib/Graphics/Font.cpp`
- the font manager falls back after failing to load `mp_data/IMMORTAL.ttf`
- execution then dies with an unhandled page fault under Wine while inside `Mix_LoadWAV_RW`

This is enough to treat the current Windows-on-Linux path as a debugging baseline, not as completed portability work.

## 6. Submodule state

`.gitmodules` declares `branch = master` for both `idlib` and `idlib-game-engine`. `git submodule status` confirms both submodules track `master`. Refactors across the engine boundary should not silently depend on local submodule state; if submodule branch policy changes, update this section.

## 7. Tooling health

### Tests

Automated test coverage at the Egoboo layer has grown but is still thin. Current `egolib/tests/egolib/tests/` sources:

- utility: `Compilation.cpp`, `StringUtilities.cpp`, `QuadTree.cpp`, `MeshInfoIterator.cpp`
- parsers: `ContentParsers.cpp`, `SpawnName.cpp`
- module load / spawn smoke: `ModuleLoadSmoke.cpp`, `ModuleSpawnPlanning.cpp`, `ModuleSpawnRealization.cpp`, `ModulePlayerStartup.cpp`
- player startup / quest hydration: `LoadPlayerElement.cpp`, `PlayerQuestLog.cpp`
- seam/accessor regression: `EngineContext.cpp`, `ObjectAccessors.cpp`
- math: `math/` submodule tests

`idlib` has broader utility tests. `idlib-game-engine` has a single compilation-level test source.

Test-to-code ratio is roughly 3.6% (≈4,340 test lines against ≈120,000 active source lines). Still absent: gameplay/combat logic, physics/collision, rendering correctness, script VM, and GUI tests.

### Utilities

`utilities/` contains potentially useful tools, but maintenance quality varies:

- `objectviewer/` and `modelverifier/` are niche legacy helpers
- `migrator/` contains real migration intent, but several tools are only scaffolds
- `migrator/README.md` is currently unrelated to the actual toolset, which is a sign of documentation rot

## 8. Large code hotspots

The file-split passes have eliminated every former oversized translation unit. No active file now exceeds 2,500 lines. The current largest TUs are a proxy for where the interface, not the line count, is still the refactoring frontier.

| File | Approx. lines |
| --- | ---: |
| `egolib/library/src/egolib/vfs.c` | 2,445 |
| `egolib/library/src/egolib/game/script_functions_systems.c` | 2,128 |
| `egolib/library/src/egolib/game/script_functions_target.c` | 1,776 |
| `egolib/library/src/egolib/game/script_functions_state.c` | 1,551 |
| `egolib/library/src/egolib/game/Physics/particle_collision.c` | 1,480 |
| `egolib/library/src/egolib/game/Graphics/ObjectGraphics.cpp` | 1,459 |
| `egolib/library/src/egolib/Entities/Object.hpp` | 1,381 |
| `egolib/library/src/egolib/game/mesh.c` | 1,370 |
| `egolib/library/src/egolib/fileutil.c` | 1,339 |
| `egolib/library/src/egolib/game/script_functions_spawn.c` | 1,181 |

`Object.cpp` itself is now 79 lines: the implementation was split across seven per-aspect TUs (`Object_{core,combat,interaction,appearance,update,attributes,lifecycle}.cpp`) while the interface surface in `Object.hpp` stayed fat. That is the ISP/SRP frontier for T1.2 role-interface extraction. Script dispatch is likewise split across seven `script_functions_*.c` files but still one logical subsystem — the extensibility fix is T3.2 (registry model). For the full current hotspot table and the pre-split sizes, see `CODEBASE-HEALTH-STATUS.md` §3.

## 9. Practical conclusions for the refactor

Before large-scale code motion:

1. Define the active source scope and stop reading `backup-copy/` and `build/` as if they were source.
2. Publish one canonical Linux build/run document that includes the Fedora-specific realities already captured in code and `run-egoboo.sh`.
3. Stop using recursive file globbing as the only expression of library structure once subsystem extraction begins.
4. Treat local portability patches as first-class tracked work items, not invisible environment glue.
5. Avoid starting the refactor in the largest files without first adding seams around them.
6. Treat Linux-native, native-Windows, and Linux-hosted Windows builds as one portability problem with shared architectural causes.
7. Use the current `debug-output.txt` failures as a baseline for Windows runtime stabilization work, not as acceptable known behavior.
