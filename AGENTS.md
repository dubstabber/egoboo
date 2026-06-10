# AGENTS.md

Project-level instructions for AI coding agents working in this repository. Keep this file focused on repository expectations and setup. If a subdirectory later needs stricter rules, add a nested `AGENTS.md` close to that work.

## Project

Egoboo is an open-source 3D dungeon crawler (C/C++, SDL2, OpenGL 2.1), licensed GPLv3, actively migrating from legacy C toward modern C++.

Current direction: modernize the mixed C/C++ runtime, make native Windows and Linux-hosted Windows cross-compilation first-class targets, reduce portability debt and warning noise, improve runtime stability and modularity.

## Repository Layout

| Directory | Purpose |
|-----------|---------|
| `egolib/` | Main runtime library (~640 source files, 25 subsystems) — where most code lives |
| `egoboo/` | Minimal executable wrapper (`src/game/Main.cpp` creates `GameEngine` and enters main loop) |
| `idlib/` | Foundation library submodule (math, types, utilities) |
| `idlib-game-engine/` | Engine framework submodule (graphics, physics, file systems) |
| `data/` | Game content submodule (modules, objects, core data) |
| `external/` | Third-party dependencies submodule (SDL2, googletest) |
| `tools/` | Content validator tool |
| `refactoring-documents/` | Architecture audits, refactoring strategy, baseline docs |
| `doc/` | Canonical build guides (`build-linux.md`, `build-windows.md`) |
| `backup-copy/` | **Read-only** reference snapshot — never modify, delete, rename, or "clean up" |
| `build/` | Generated output — never manually edit, never treat as source |

The superproject passes the top-level `idlib/` into `idlib-game-engine` during CMake, so `idlib-game-engine/idlib` does not need separate initialization.

## Build Commands

**Parallelism (current machine: i7-13700HX, 24 threads, 31 GB RAM):** the build is parallel-safe — use `-j20` (leave a couple of threads free). The older `-j4` cap was a laptop stability limit and no longer applies here. Prefer the Ninja generator with ccache for fast incremental rebuilds: `-G Ninja -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache`.

**Test runner is NOT `-j`-safe:** several fixtures (import/export, quest-log hydration, active-module-menu) share writable user-data paths and race under high `ctest -j`, producing ~4 spurious failures. Run `ctest -j1` (or a low `-j`) for a trustworthy baseline until that fixture isolation is fixed. See the Testing section.

```bash
# Initial setup (clone + submodules)
git submodule update --init data external idlib idlib-game-engine

# Linux build (Ninja + ccache recommended)
cmake -S . -B build -G Ninja \
  -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
cmake --build build -j20

# Run tests
ctest --test-dir build --output-on-failure

# Windows cross-build (mingw-w64)
cmake -S . -B build-windows \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/toolchains/mingw-w64-x86_64.cmake"
cmake --build build-windows -j20

# Run the game (Linux) — or follow doc/build-linux.md
./run-egoboo.sh

# Run Windows build via Wine (temporary compatibility path)
./run-egoboo-windows.sh
```

Binary output: `build/products/x64/bin/` (Linux), `build-windows/products/x64/bin/` (Windows).

In sandboxed or read-only-home environments, redirect writable user-data paths: `HOME=/tmp/egoboo-home XDG_DATA_HOME=/tmp/egoboo-xdg ...`

## Content Validation

Run the validator after changes to runtime code, content loading, module/object data, VFS behavior, or scripts:

```bash
# Single-module smoke check (minimum after most changes)
./build/products/x64/bin/egoboo-content-validator --data-dir "$PWD/data" --module test.mod

# Full validation (for VFS, shared loading paths, format changes)
./build/products/x64/bin/egoboo-content-validator --data-dir "$PWD/data"
```

The legacy content set is **not** internally consistent. Many validator failures are pre-existing (missing spawn-referenced objects, not parser crashes). Check `refactoring-documents/06-validator-baseline.md` before treating failures as newly introduced regressions.

## Architecture

**Boot path**: `Main.cpp` → `Ego::Core::System::initialize()` (VFS, logging, config, SDL timer/events/video/audio/input) → `EngineContext::get().setEngine(make_unique<GameEngine>())` → `engine().start()` → `GameEngine::initialize()` (GFX/OpenGL, CameraSystem, AudioSystem, UIManager, CollisionSystem, pushes `MainMenuState`) → main loop.

**Main loop**: Fixed update (50 UPS) / fixed render (60 FPS) with frame skipping (max 10 frame skip tolerance).

**State management**: Stack-based game states (`MainMenuState`, `PlayingState`, `SelectModuleState`, `SelectPlayersState`, `LoadingState`, `InGameMenuState`, `MapEditorState`, options, debug, victory states).

**Content system**: Directory-based module format with convention-driven files (`menu.txt`, `spawn.txt`, `data.txt`, `script.txt`). Virtual file system (PhysFS) with mount points (`mp_data`, `mp_modules`, `mp_objects`).

**Egolib subsystems**: AI, Audio, Configuration, Console, Core (quad-trees, thread pool), Entities (objects/particles), Extensions (OpenGL), FileFormats (MD2, maps, configs), Graphics (fonts, textures, framebuffer), Grid, Image, InputControl, Log, Logic, Math, Mesh, Platform, Profiles, Renderer (OpenGL), Script (bytecode VM, compiler), Time, VFS, game (core gameplay), integrations.

**Link layout**: egolib builds as an acyclic DAG of four static archives, defined in `egolib/library/CMakeLists.txt` and nm-symbol-closure verified (see `refactoring-documents/71-completed-passes-log.md`): `egolib-foundation-base` (115 TUs, the dependency-closed bottom) ◄ `egolib-physics` (6, the collision nucleus + `physics.c`) and ◄ `egolib-renderer` (29, SDL windowing + OpenGL backend; sibling of physics, zero cross-edges) ◄ `egolib-library` (142, the game-core remainder). Consumers link only `egolib-library`. Move-only absorption is exhausted — growing the lower layers now requires seam-cutting. When touching egolib CMake or moving sources, preserve the acyclicity (verify with the nm set-intersection method, with a positive control).

### Global State (major coupling points)

The runtime was historically wired around three mutable globals, all now retired from active runtime code:

- `_gameEngine` — **0 references.** Engine access routes through `EngineContext::get().setEngine()` / `engine()`.
- `_currentModule` — **0 references.** Consumers go through `GameSessionContext` and `GameModule` accessor surfaces.
- `update_wld` — **0 active references.** Variable gone; a few stale string-literal/comment artifacts remain in `script.c`, `ObjectGraphics.hpp`, `Particle.hpp`. Functional replacement is `worldUpdateCount()` (via `GameSessionContext`), ~77 call sites across ~31 files.

The remaining coupling hotspot is singleton access: ~863 `::get()` call sites persist. The `EngineContext` service-interface layer covers audio, perk, image, particle, profile, logging, config, font, input, graphics system, texture manager, texture atlas, GFX, billboard system, and camera system (15 service seams); broader DI does not yet exist. Avoid reintroducing hidden global dependencies. Be careful around code affecting VFS setup, module loading, object profile loading, or script compilation.

### High-Risk Hotspots

Read relevant audit docs before modifying. Files over 1,000 lines (by size):
- `egolib/library/src/egolib/game/script_functions_systems.c` (~3200 lines, largest TU)
- `egolib/library/src/egolib/vfs.c` (~1920 lines, down from 2,460 after the dead cstdio backend was removed)
- `egolib/library/src/egolib/game/script_functions_target.c` (~1680 lines)
- `egolib/library/src/egolib/Entities/Object.hpp` (~1620 lines, monolithic interface — 18 role interfaces extracted but header still large)
- `egolib/library/src/egolib/game/script_functions_spawn.c` (~1580 lines)
- `egolib/library/src/egolib/game/Physics/particle_collision.c` (~1530 lines)
- `egolib/library/src/egolib/game/Graphics/ObjectGraphics.cpp` (~1490 lines)
- `egolib/library/src/egolib/game/script_functions_state.c` (~1480 lines)
- `egolib/library/src/egolib/Script/script.c` (~1370 lines)
- `egolib/library/src/egolib/game/mesh.c` (~1370 lines)
- `egolib/library/src/egolib/fileutil.c` (~1330 lines)

Architecturally central but now small after split passes:
- `egolib/library/src/egolib/game/game.c` (~550 lines, split into `game_{combat,export,loop,targeting,wawalite}.c`)
- `egolib/library/src/egolib/Entities/Object.cpp` (~200 lines, split into six `Object_*.cpp` TUs)
- `egolib/library/src/egolib/game/Module/Module.cpp` (~200 lines, split into six `Module_*.cpp` siblings)

## Refactoring Guidelines

- Before large refactors, read `refactoring-documents/README.md`, `refactoring-documents/04-refactoring-strategy.md`, and `refactoring-documents/06-validator-baseline.md`.
- Prefer seam creation, file-splitting, and dependency reduction over speculative rewrites.
- Prefer small, verifiable changes over broad rewrites without checkpoints.
- Preserve observable behavior unless the task explicitly calls for behavior change.
- Preserve current Linux/Fedora portability behavior unless intentionally revisiting it — and document any such change.
- If an edit changes runtime ownership, loading flow, or subsystem boundaries, or if you analyze architecture / plan refactors / discover important structural issues, write or update markdown in `refactoring-documents/`.

## Testing

Google Test framework. Tests in `egolib/tests/` (42 test files, **825** ctest cases; the only 2 expected failures are the perennial `ScriptLoaderFixture` PrimaryScript-fallback cases). **Run serially (`ctest -j1`) for an accurate baseline** — at high `-j` ~4 additional fixtures fail spuriously due to shared writable-path races (`ImportWorkflowFixture` copy/export, `ModulePlayerStartupFixture` quest-log hydration, `ScriptSystemsFunctionsFixture` AddIDSZ module-menu), all of which pass in isolation. Coverage spans utilities (quad-tree, string utilities, mesh iterators), content parsers, module load/spawn, script dispatch/VM, gameplay alerts, shop interactions, physics/collision math, and — via a live spawned `Object` — combat damage-resolution math. Still uncovered: rendering, GUI, AI, and the full combat *integration* path.

## Environment Variables

| Variable | Purpose |
|----------|---------|
| `EGOBOO_DATA_DIR` | Override game data directory (Linux) |
| `EGOBOO_DISABLE_MIPMAPS` | Wine compatibility |
| `EGOBOO_DISABLE_AUDIO` | Wine compatibility |
| `SDL_VIDEODRIVER=x11` | Useful on Wayland systems |
| `DRI_PRIME=1` | Use discrete GPU |

For sandboxed environments: `HOME=/tmp/egoboo-home XDG_DATA_HOME=/tmp/egoboo-xdg`

## Documentation Lookup

When the user asks about a library, framework, SDK, API, CLI tool, or cloud service, fetch current documentation via `ctx7`:

1. `npx ctx7@latest library <name> "<full user question>"`
2. Pick the best `/org/project` match.
3. `npx ctx7@latest docs <libraryId> "<full user question>"`

Use it for API syntax, configuration, version migration, setup, CLI usage, and library-specific debugging. Do **not** use it for refactoring plans, project-specific business logic, code review, or general programming concepts.

## Sub-Agents

Project-scoped agents live in `.claude/agents/`. Delegate narrow, parallelizable tasks to keep the main conversation context clean; keep ownership explicit when delegating implementation work.

| Agent | Model | Purpose | Tools |
|-------|-------|---------|-------|
| `repo-architect` | sonnet | Architecture exploration, coupling analysis, dependency tracing | Read-only |
| `content-auditor` | sonnet | Content format analysis, data integrity, module structure inspection | Read-only |
| `validator-runner` | haiku | Build the project and run the content validator, report results | Read-only + Bash |
| `refactor-worker` | sonnet | Execute bounded refactoring tasks (runs in isolated worktree) | Full edit access |
| `linux-portability` | sonnet | Diagnose platform issues across Linux, Windows, and cross-build targets | Read-only |

`repo-architect`, `content-auditor`, and `refactor-worker` have project-scoped persistent memory to accumulate findings across sessions.
