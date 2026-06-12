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

**Link layout**: egolib builds as an acyclic DAG of **nine** static archives, defined in `egolib/library/CMakeLists.txt` and nm-symbol-closure verified (see `refactoring-documents/71-completed-passes-log.md`): `egolib-foundation-base` (146 TUs, the dependency-closed bottom) ◄ `egolib-physics` (5, the collision nucleus + `physics.c`) and ◄ `egolib-renderer` (28, SDL windowing + OpenGL backend; sibling of physics, zero cross-edges) ◄ `egolib-gui` (22, the generic GUI widget toolkit — Component/Container/widgets/UIManager — above renderer, game-state-free) ◄ `egolib-library` (62, the game-core remainder) ◄ `egolib-game-graphics` (17, the 3D scene-rendering layer — `Camera`/`CameraSystem`/`BillboardSystem`/`TextureAtlasManager` + the 11 concrete `RenderPasses` + the `GFX`/`GameAppImpl` construction in `graphic_init.cpp`) ◄ `egolib-hud-widgets` (6, the game-coupled in-game HUD widgets — CharacterStatus/CharacterWindow/InventorySlot/LevelUpWindow/MiniMap/ModuleSelector) ◄ **two sibling top layers** `egolib-scriptvm` (17, the EgoScript VM — `script.c` + `script_implementation`/`script_variables` + the 13 `script_functions_*.c` dispatch family + `ScriptSystemAdapter`) and `egolib-gamestates` (19, the concrete `GameState` screens — menus/options/select/loading/playing/map-editor/debug/victory). The **four ABOVE-`egolib-library` layers** form a linear stack with two top siblings: `egolib-game-graphics` is the LOWEST upper layer (directly above library); `egolib-hud-widgets` is a MIDDLE upper layer (above game-graphics — MiniMap projects through the Camera — BELOW both scriptvm and gamestates); `egolib-scriptvm` and `egolib-gamestates` are top-layer **siblings** (nm-verified zero edges in either direction). All reach *down* into game-core (game-graphics 61 forward edges into library; scriptvm 85, gamestates ~63, hud-widgets 43). `EntityList`/`TileList`/`ObjectGraphics`/`ParticleGraphics` (deep Entities/GameSession coupling) and the `GraphicsBootstrap` hook holder STAY in `egolib-library`; the `GameState` base stays in `egolib-library`; the lower-layer `Ego::Script::IScriptSystem` accessor stays in `egolib-foundation-base`. Consumers `egoboo` and the test executable link **both** `egolib-gamestates` and `egolib-scriptvm` (which provide hud-widgets + game-graphics + library transitively); `cartman` and the content-validator use no screens, VM, in-game HUD, or 3D scene render, so they link only `egolib-library` (and link clean — proof the library remainder is cluster-free). The DAG is **fully acyclic — zero known back-edges** (all 9 layers verify 0 forbidden edges). Move-only absorption into the *lower* layers is exhausted; growing them needs seam-cutting (e.g. the gui carve needed `activeRenderer`/`activeGraphicsSystem`/`activeUIManager` seams). The four *upward* splits were carved by seam-cutting the `egolib-library → upper` **reverse** edges (measure REVERSE edges, not forward, for an above-library carve): gamestates via the `IPlayingStateController` interface (dynamic_cast-to-interface) + a `GameEngine` main-menu factory injected from `egoboo/Main.cpp` + `Ego::activeTextureManager()`; scriptvm via relocating `ai_state_t`'s state methods down into `Entities/AiState.cpp` (7 of 10 reverse edges) + routing the 3 driver entries (`scr_run_chr_script`/`set_alerts`/`scripting_system_end`) through the `Ego::Script::IScriptSystem` interface installed from above library (the game's `Main.cpp` and the test harness's gtest global environment); hud-widgets via a single `IPlayingStateController::setMiniMapShowPlayerPosition` interface method cutting the lone `MiniMap::setShowPlayerPosition` reverse edge (1 reverse edge, render-driver-free, GL-safe); game-graphics (the 9th, ninth split) by cutting the 15 reverse edges (14 from `graphic.c` — the 11 RenderPass ctors + BillboardSystem ctor + `render_all` + TextureAtlasManager — and 1 from `GameEngine.cpp` — `CameraSystem::CameraSystem`) via a *construction* seam: relocate the order-sensitive `GFX`/`GameAppImpl` construction/teardown bodies down into `graphic_init.cpp` (above library) and trigger them from `GameEngine::initialize()`/teardown through the `Ego::Graphics::registerGraphicsBootstrap`/`runGraphicsBootstrap{Init,Teardown}` `std::function` hook (the holder stays in egolib-library), with `installDefaultGraphicsSystems()` injected from `egoboo/Main.cpp` (mirrors `installDefaultScriptSystem`) — ordering preserved byte-identically. When touching egolib CMake or moving sources, preserve the acyclicity — re-run the per-archive nm back-edge check (mangled symbols, set math, with a positive control; measure against the live `.a` **archives**, not the `CMakeFiles/*.dir` object dirs, which hold stale `.o` from before the carves); do not trust prior "acyclic" claims.

### Global State (major coupling points)

The runtime was historically wired around three mutable globals, all now retired from active runtime code:

- `_gameEngine` — **0 references.** Engine access routes through `EngineContext::get().setEngine()` / `engine()`.
- `_currentModule` — **0 references.** Consumers go through `GameSessionContext` and `GameModule` accessor surfaces.
- `update_wld` — **0 active references.** Variable gone; a few stale string-literal/comment artifacts remain in `script.c`, `ObjectGraphics.hpp`, `Particle.hpp`. Functional replacement is `worldUpdateCount()` (via `GameSessionContext`), ~77 call sites across ~31 files.

The remaining coupling hotspot is singleton access: ~632 `::get()` call sites persist (down from ~863; the bulk are the intentional `EngineContext::get()` (451) and `GameSessionContext::get()` (129) seam calls). Actionable direct singletons: `video_buffer_manager::get()` (1), `InputSystem::get()` (8), `GraphicsSystemNew::get()` (6), `egoboo_config_t::get()` (6, already seamed), `TLT::get()` (5, const table). The `EngineContext` service-interface layer covers audio, perk, image, particle, profile, logging, config, font, input, graphics system, texture manager, texture atlas, GFX, billboard system, and camera system (15 service seams); broader DI does not yet exist. Avoid reintroducing hidden global dependencies. Be careful around code affecting VFS setup, module loading, object profile loading, or script compilation.

### High-Risk Hotspots

Read relevant audit docs before modifying. Files over 1,000 lines (by size) — exactly eight as of 2026-06-12:
- `egolib/library/src/egolib/Entities/Object.hpp` (~1613 lines, monolithic interface — 18 role interfaces extracted but header still large; the single largest TU in the tree)
- `egolib/library/src/egolib/game/Physics/particle_collision_response.c` (~1308 lines, the chr-prt response pipeline; carved 2026-06-12 from the former 1528-line `particle_collision.c` — see `particle_collision_physics.c` (274) for the pure mass/recoil/platform-detection sibling)
- `egolib/library/src/egolib/vfs.c` (~1276 lines, the SearchContext slice carved 2026-06-12 to `vfs_search.c` (260); earlier SDL_RWops→`vfs_rwops.c` and mount mgmt→`vfs_mount.c` slices landed 2026-06-11)
- `egolib/library/src/egolib/Script/script.c` (~1156 lines, in `egolib-scriptvm`; `ai_state_t` state methods split out to `Entities/AiState.cpp`)
- `egolib/library/src/egolib/game/script_compile.c` (~1151 lines)
- `egolib/library/src/egolib/game/Physics/ObjectPhysics.cpp` (~1138 lines)
- `egolib/library/src/egolib/game/script_functions_action.c` (~1101 lines)
- `egolib/library/src/egolib/game/script_functions_target.c` (~1044 lines)

Notes:
- `script_functions_systems.c` (formerly ~3,200 lines) has been fully decomposed and deleted, spread across 14 `script_functions_*.c` files (action, alerts, appearance, bitwise, combat, commerce, enchant, movement, quests, spawn, state, stat_gifts, target, target_select). The largest TU is now `Entities/Object.hpp`, not this deleted file.
- `script_functions_spawn.c` (formerly ~1576 lines, the largest .c TU) was split 2026-06-12 into 3 within-`egolib-scriptvm` siblings: `script_functions_spawn.c` (629, residual: 24 lifecycle/drop/cleanup/identify/state-mutation entries), `script_functions_spawn_character.c` (458, 8 character spawn/respawn entries), `script_functions_spawn_particle.c` (463, 12 particle spawn/poof entries), plus a private `script_functions_spawn_internal.h` (74, shared `SpawnSelfContext` / `makeSpawnSelfContext` / `gameSession()` / `isLiveSpawnObjectRef`).
- `particle_collision.c` (formerly ~1528 lines, second-largest .c TU) was split 2026-06-12 into 2 within-`egolib-library` siblings: `particle_collision_physics.c` (274, `get_prt_mass` / `get_recoil_factors` / `do_prt_platform_detection` / `attach_prt_to_platform`) and `particle_collision_response.c` (1308, the chr-prt response chain + `do_chr_prt_collision` orchestrator + `spawn_bump_particles`). Public `particle_collision.h` unchanged; the immovable-tent guards (CHR_INFINITE_WEIGHT / bumpdampen==0) live intact in both TUs.

Architecturally central but now small after split passes:
- `egolib/library/src/egolib/game/game.c` (~522 lines, split into `game_{combat,export,loop,targeting,wawalite}.c`)
- `egolib/library/src/egolib/Entities/Object.cpp` (~200 lines, split into six `Object_*.cpp` TUs)
- `egolib/library/src/egolib/game/Module/Module.cpp` (~277 lines, split into six `Module_*.cpp` siblings)
- `egolib/library/src/egolib/game/Graphics/ObjectGraphics.cpp` (~741 lines, split off `ObjectGraphics_animation.cpp` (~587, the animation state machine) + shared `ObjectGraphics_internal.hpp`; both TUs stay in egolib-library)

## Refactoring Guidelines

- Before large refactors, read `refactoring-documents/README.md`, `refactoring-documents/04-refactoring-strategy.md`, and `refactoring-documents/06-validator-baseline.md`.
- Prefer seam creation, file-splitting, and dependency reduction over speculative rewrites.
- Prefer small, verifiable changes over broad rewrites without checkpoints.
- Preserve observable behavior unless the task explicitly calls for behavior change.
- Preserve current Linux/Fedora portability behavior unless intentionally revisiting it — and document any such change.
- If an edit changes runtime ownership, loading flow, or subsystem boundaries, or if you analyze architecture / plan refactors / discover important structural issues, write or update markdown in `refactoring-documents/`.

## Testing

Google Test framework. Tests in `egolib/tests/` (44 test files, **877** ctest cases; full run is 877/877 PASS on this machine — the two historical `ScriptLoaderFixture` PrimaryScript-fallback cases now pass here; +2 since the 875 baseline are the `script.c` runCharacterScript VM dispatch test (commit 7495f2955) + the UIManager Renderer-seam activeRenderer test (commit ee7487deb)). **Parallel-safe at `-j20`** — each test process gets per-PID isolation via `EGOBOO_USER_DIR` (`TestEnvironment.hpp`), with automatic `atexit` cleanup of temp directories. Coverage spans utilities (quad-tree, string utilities, mesh iterators), content parsers, module load/spawn, script dispatch/VM, gameplay alerts, shop interactions, physics/collision math, and — via a live spawned `Object` — combat damage-resolution math. Still uncovered: rendering, GUI, AI, and the full combat *integration* path.

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
