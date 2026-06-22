# AGENTS.md

Project-level instructions for AI coding agents working in this repository. Keep
this file focused on current repository expectations, setup, and safety rules.
For volatile metrics, prefer `refactoring-documents/CODEBASE-HEALTH-STATUS.md`.

## Project

Egoboo is an open-source 3D dungeon crawler (C/C++, SDL2, OpenGL 2.1), licensed
GPLv3, actively migrating from legacy C toward modern C++.

Current direction: modernize the mixed C/C++ runtime, make native Windows and
Linux-hosted Windows cross-compilation first-class targets, reduce portability
debt and warning noise, and improve runtime stability and modularity.

## Repository Layout

| Directory | Purpose |
| --- | --- |
| `egolib/` | Main runtime library and highest-risk code area |
| `egoboo/` | Thin executable wrapper |
| `tools/` | Active tools, including `egoboo-content-validator` |
| `cartman/` | Map editor, CMake-gated behind `EGOBOO_BUILD_CARTMAN=OFF` |
| `data/` | Game content submodule |
| `idlib/` | Foundation library submodule |
| `idlib-game-engine/` | Engine framework submodule |
| `external/` | Third-party dependency submodule |
| `doc/` | Canonical build and policy docs |
| `refactoring-documents/` | Architecture, roadmap, health, and pass-history docs |
| `backup-copy/` | Read-only reference snapshot; never modify, delete, rename, or clean up |
| `build/`, `build-windows/` | Generated output; never manually edit or treat as source |

The superproject passes the top-level `idlib/` into `idlib-game-engine` during
CMake, so `idlib-game-engine/idlib` does not need separate initialization for
normal work.

## Build Commands

Parallelism on this machine is safe at `-j20` (i7-13700HX, 24 threads, 31 GB
RAM). Prefer Ninja with ccache for incremental builds.

```bash
# Initial setup
git submodule update --init data external idlib idlib-game-engine

# Linux build
cmake -S . -B build -G Ninja \
  -DCMAKE_C_COMPILER_LAUNCHER=ccache \
  -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
cmake --build build -j20

# Tests
ctest --test-dir build -j20 --output-on-failure

# Windows cross-build from Linux
cmake -S . -B build-windows -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/toolchains/mingw-w64-x86_64.cmake"
cmake --build build-windows -j20
```

The test runner is parallel-safe. Each test process gets a per-PID
`EGOBOO_USER_DIR`, and temp directories are cleaned up automatically.

Binary output:

- Linux: `build/products/x64/bin/`
- Windows cross-build: `build-windows/products/x64/bin/`

Launch helpers:

```bash
./run-egoboo.sh
./run-egoboo-windows.sh
```

In sandboxed or read-only-home environments, redirect writable user-data paths:
`HOME=/tmp/egoboo-home XDG_DATA_HOME=/tmp/egoboo-xdg ...`

## Content Validation

Run the validator after changes to runtime code, content loading, module/object
data, VFS behavior, script compilation, model loading, or object profiles.

```bash
# Fast smoke check
./build/products/x64/bin/egoboo-content-validator --data-dir "$PWD/data" --module test.mod

# Full known-baseline validation
./build/products/x64/bin/egoboo-content-validator --data-dir "$PWD/data"
```

The legacy content set is not internally consistent. The current full baseline
is 42 modules, 10 warnings, 245 pre-existing errors. See
`refactoring-documents/06-validator-baseline.md` before treating validator
failures as new regressions.

## Architecture Snapshot

Boot path:

```text
Main.cpp
  -> Ego::Core::System::initialize()
  -> EngineContext::get().setEngine(make_unique<GameEngine>())
  -> install default graphics/script systems and main-menu factory
  -> engine().start()
  -> GameEngine::initialize()
  -> fixed update/render main loop
```

Main loop: fixed update at 50 UPS, fixed render at 60 FPS, with frame skipping.

State management: stack-based game states (`MainMenuState`, `PlayingState`,
selection/loading/options/debug/victory/menu states).

Content system: directory-based modules with convention-driven files
(`menu.txt`, `spawn.txt`, `data.txt`, `script.txt`) mounted through PhysFS
virtual paths (`mp_data`, `mp_modules`, `mp_objects`).

## Link Layout

`egolib` builds as nine static archives. Live archive member counts from the
current build:

| Archive | Members | Role |
| --- | ---: | --- |
| `egolib-foundation-base` | 162 | Dependency-closed base |
| `egolib-physics` | 6 | Collision/physics nucleus |
| `egolib-renderer` | 28 | SDL windowing and OpenGL backend |
| `egolib-gui` | 24 | Generic GUI toolkit and abstract `GameState` base |
| `egolib-library` | 76 | Core gameplay remainder |
| `egolib-game-graphics` | 21 | 3D scene rendering layer |
| `egolib-hud-widgets` | 6 | Game-coupled HUD widgets |
| `egolib-scriptvm` | 33 | EgoScript VM and dispatch family |
| `egolib-gamestates` | 19 | Concrete game states |

Intended dependency direction:

```text
foundation-base <- {physics, renderer <- gui} <- library
library <- game-graphics <- hud-widgets <- {scriptvm, gamestates}
```

When touching `egolib/library/CMakeLists.txt` or moving sources, preserve the
DAG and re-run the live-archive `nm` back-edge check. Measure the `.a` archives,
not `CMakeFiles/*.dir`, which can contain stale object files. The carve-layer
C-as-C++ source-property loop must only match `.c` files; do not add private
headers to a layer source list where they can become stray `.h.o` members.

## Global State

The former mutable globals are retired from active runtime code:

- `_gameEngine`: 0 active references; engine access routes through
  `EngineContext`.
- `_currentModule`: 0 active references; module access routes through
  `GameSessionContext` and `GameModule` accessors.
- `update_wld`: variable removed; four text/comment/debug-label artifacts remain.

Current coupling hotspot: about 623 `::get()` call sites in `egolib/library/src`,
mostly intentional context seams (`EngineContext::get()` and
`GameSessionContext::get()`). Avoid adding new hidden global dependencies.

## Hotspots

Production runtime files are currently below 1,000 lines. The largest runtime
file is `egolib/library/src/egolib/Entities/Object.hpp` at 971 lines. Do not
chase file size mechanically; the remaining risk is interface breadth and
cross-subsystem coupling.

Large non-runtime files still exist in tests and tools, including
`ScriptSystemsFunctions.cpp`, `ObjectAccessors.cpp`, `ScriptStateFunctions.cpp`,
and `tools/egoboo-content-validator.cpp`. Treat those as focused verification
surfaces unless the task is explicitly to split test/tool code.

Before large refactors, read:

- `refactoring-documents/CODEBASE-HEALTH-STATUS.md`
- `refactoring-documents/04-refactoring-strategy.md`
- `refactoring-documents/06-validator-baseline.md`
- relevant completed-pass notes in `refactoring-documents/71-completed-passes-log.md`

## Refactoring Guidelines

- Prefer seam creation, file-splitting, and dependency reduction over speculative
  rewrites.
- Preserve observable behavior unless explicitly asked to change behavior.
- Preserve current Linux/Fedora portability behavior unless intentionally
  revisiting it, and document any new operational assumptions.
- If an edit changes runtime ownership, loading flow, archive boundaries, or
  subsystem boundaries, update `refactoring-documents/`.
- Be careful around VFS setup, module loading, object profile loading, model
  loading, and script compilation.

## Environment Variables

| Variable | Purpose |
| --- | --- |
| `EGOBOO_DATA_DIR` | Override game data directory on Linux |
| `EGOBOO_USER_DIR` | Override writable user-data directory; tests set this per process |
| `EGOBOO_DISABLE_MIPMAPS` | Wine compatibility path |
| `EGOBOO_DISABLE_AUDIO` | Wine compatibility path |
| `SDL_VIDEODRIVER=x11` | Useful on Wayland systems |
| `DRI_PRIME=1` | Use discrete GPU where applicable |

## Safety

- Never modify `backup-copy/`.
- Never manually edit `build/` or `build-windows/`.
- Do not revert unrelated user changes.
- Do not reintroduce Visual Studio-only requirements into the maintained build
  path.
