# AGENTS.md

Project-level instructions for AI coding agents working in this repository. Keep this file focused on repository expectations and setup. If a subdirectory later needs stricter rules, add a nested `AGENTS.md` close to that work.

## Project

Egoboo is an open-source 3D dungeon crawler (C/C++, SDL2, OpenGL 2.1), licensed GPLv3, actively migrating from legacy C toward modern C++.

Current direction: modernize the mixed C/C++ runtime, make native Windows and Linux-hosted Windows cross-compilation first-class targets, reduce portability debt and warning noise, improve runtime stability and modularity.

## Repository Layout

| Directory | Purpose |
|-----------|---------|
| `egolib/` | Main runtime library (~680 source files, 25 subsystems) — where most code lives |
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

**Test runner is `-j`-safe:** each test process gets its own temp directory via `EGOBOO_USER_DIR` (per-PID isolation in `TestEnvironment.hpp`). Temp directories are cleaned up automatically via `atexit`. Use `ctest -j20` freely.

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

**Link layout**: egolib builds as an acyclic DAG of **six** static archives, defined in `egolib/library/CMakeLists.txt` and nm-symbol-closure verified (see `refactoring-documents/71-completed-passes-log.md`): `egolib-foundation-base` (145 TUs, the dependency-closed bottom) ◄ `egolib-physics` (5, the collision nucleus + `physics.c`) and ◄ `egolib-renderer` (28, SDL windowing + OpenGL backend; sibling of physics, zero cross-edges) ◄ `egolib-gui` (22, the generic GUI widget toolkit — Component/Container/widgets/UIManager — above renderer, game-state-free) ◄ `egolib-library` (98, the game-core remainder) ◄ `egolib-gamestates` (19, the concrete `GameState` screens — menus/options/select/loading/playing/map-editor/debug/victory). `egolib-gamestates` is the **first ABOVE-`egolib-library` layer** (the screens orchestrate the game, so they reach *down* into game-core); the `GameState` base itself stays in `egolib-library`. Consumers `egoboo` and the test executable link `egolib-gamestates` (which provides `egolib-library` transitively); the `cartman` tool links only `egolib-library` (it uses no screens). The DAG is **fully acyclic — zero known back-edges** (all 6 layers verify 0 forbidden edges). Move-only absorption into the *lower* layers is exhausted; growing them needs seam-cutting (e.g. the gui carve needed `activeRenderer`/`activeGraphicsSystem`/`activeUIManager` seams). The gamestates carve was the first *upward* split — it needed seam-cutting the `egolib-library → screen` reverse edges via the `IPlayingStateController` interface (dynamic_cast-to-interface, no concrete-`PlayingState` link edge), a `GameEngine` main-menu-state factory injected from `egoboo/Main.cpp`, and the `Ego::activeTextureManager()` seam. When touching egolib CMake or moving sources, preserve the acyclicity — re-run the per-archive nm back-edge check (mangled symbols, set math, with a positive control); do not trust prior "acyclic" claims.

### Global State (major coupling points)

The runtime was historically wired around three mutable globals, all now retired from active runtime code:

- `_gameEngine` — **0 references.** Engine access routes through `EngineContext::get().setEngine()` / `engine()`.
- `_currentModule` — **0 references.** Consumers go through `GameSessionContext` and `GameModule` accessor surfaces.
- `update_wld` — **0 active references.** Variable gone; a few stale string-literal/comment artifacts remain in `script.c`, `ObjectGraphics.hpp`, `Particle.hpp`. Functional replacement is `worldUpdateCount()` (via `GameSessionContext`), ~77 call sites across ~31 files.

The remaining coupling hotspot is singleton access: ~632 `::get()` call sites persist (down from ~863; the bulk are the intentional `EngineContext::get()` (451) and `GameSessionContext::get()` (129) seam calls). Actionable direct singletons: `video_buffer_manager::get()` (1), `InputSystem::get()` (8), `GraphicsSystemNew::get()` (6), `egoboo_config_t::get()` (6, already seamed), `TLT::get()` (5, const table). The `EngineContext` service-interface layer covers audio, perk, image, particle, profile, logging, config, font, input, graphics system, texture manager, texture atlas, GFX, billboard system, and camera system (15 service seams); broader DI does not yet exist. Avoid reintroducing hidden global dependencies. Be careful around code affecting VFS setup, module loading, object profile loading, or script compilation.

### High-Risk Hotspots

Read relevant audit docs before modifying. Files over 1,000 lines (by size) — exactly ten:
- `egolib/library/src/egolib/Entities/Object.hpp` (~1613 lines, monolithic interface — 18 role interfaces extracted but header still large; now the single largest TU in the tree)
- `egolib/library/src/egolib/game/script_functions_spawn.c` (~1576 lines)
- `egolib/library/src/egolib/game/Physics/particle_collision.c` (~1528 lines)
- `egolib/library/src/egolib/vfs.c` (~1500 lines, split this session into vfs.c + vfs_rwops.c + vfs_mount.c)
- `egolib/library/src/egolib/game/Graphics/ObjectGraphics.cpp` (~1488 lines)
- `egolib/library/src/egolib/Script/script.c` (~1369 lines)
- `egolib/library/src/egolib/game/script_compile.c` (~1151 lines)
- `egolib/library/src/egolib/game/Physics/ObjectPhysics.cpp` (~1138 lines)
- `egolib/library/src/egolib/game/script_functions_action.c` (~1101 lines)
- `egolib/library/src/egolib/game/script_functions_target.c` (~1044 lines)

Note: `script_functions_systems.c` (formerly ~3,200 lines) has been fully decomposed and deleted, spread across 14 `script_functions_*.c` files (action, alerts, appearance, bitwise, combat, commerce, enchant, movement, quests, spawn, state, stat_gifts, target, target_select). The largest TU is now `Entities/Object.hpp`, not this deleted file.

Architecturally central but now small after split passes:
- `egolib/library/src/egolib/game/game.c` (~522 lines, split into `game_{combat,export,loop,targeting,wawalite}.c`)
- `egolib/library/src/egolib/Entities/Object.cpp` (~200 lines, split into six `Object_*.cpp` TUs)
- `egolib/library/src/egolib/game/Module/Module.cpp` (~277 lines, split into six `Module_*.cpp` siblings)

## Refactoring Guidelines

- Before large refactors, read `refactoring-documents/README.md`, `refactoring-documents/04-refactoring-strategy.md`, and `refactoring-documents/06-validator-baseline.md`.
- Prefer seam creation, file-splitting, and dependency reduction over speculative rewrites.
- Prefer small, verifiable changes over broad rewrites without checkpoints.
- Preserve observable behavior unless the task explicitly calls for behavior change.
- Preserve current Linux/Fedora portability behavior unless intentionally revisiting it — and document any such change.
- If an edit changes runtime ownership, loading flow, or subsystem boundaries, or if you analyze architecture / plan refactors / discover important structural issues, write or update markdown in `refactoring-documents/`.

## Testing

Google Test framework. Tests in `egolib/tests/` (44 test files, **875** ctest cases; full run is 875/875 PASS on this machine — the two historical `ScriptLoaderFixture` PrimaryScript-fallback cases now pass here). **Parallel-safe at `-j20`** — each test process gets per-PID isolation via `EGOBOO_USER_DIR` (`TestEnvironment.hpp`), with automatic `atexit` cleanup of temp directories. Coverage spans utilities (quad-tree, string utilities, mesh iterators), content parsers, module load/spawn, script dispatch/VM, gameplay alerts, shop interactions, physics/collision math, and — via a live spawned `Object` — combat damage-resolution math. Still uncovered: rendering, GUI, AI, and the full combat *integration* path.

## Environment Variables

| Variable | Purpose |
|----------|---------|
| `EGOBOO_DATA_DIR` | Override game data directory (Linux) |
| `EGOBOO_USER_DIR` | Override writable user-data directory (used by test harness for per-PID isolation) |
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
