# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

Egoboo is an open-source 3D dungeon crawler (C/C++, SDL2, OpenGL 2.1). The codebase is actively being migrated from legacy C toward modern C++. Licensed under GPLv3.

Current direction: modernize the mixed C/C++ runtime, make native Windows and Linux-hosted Windows cross-compilation first-class targets, reduce portability debt and warning noise, improve runtime stability and modularity.

## Build Commands

```bash
# Initial setup (clone + submodules)
git submodule update --init data external idlib idlib-game-engine

# Linux build
cmake -S . -B build
cmake --build build -j4

# Run tests
ctest --test-dir build --output-on-failure

# Windows cross-build (mingw-w64)
cmake -S . -B build-windows \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/toolchains/mingw-w64-x86_64.cmake"
cmake --build build-windows -j4

# Run the game (Linux)
./run-egoboo.sh

# Run Windows build via Wine (temporary compatibility path)
./run-egoboo-windows.sh
```

**Never use more than 4 parallel jobs (`-j4` max)**; higher values destabilize this machine.

Binary output: `build/products/x64/bin/` (Linux), `build-windows/products/x64/bin/` (Windows).

## Content Validation

Run the validator after changes to runtime code, content loading, module/object data, VFS behavior, or scripts:

```bash
# Single-module smoke check (minimum after most changes)
./build/products/x64/bin/egoboo-content-validator --data-dir "$PWD/data" --module test.mod

# Full validation (for VFS, shared loading paths, format changes)
./build/products/x64/bin/egoboo-content-validator --data-dir "$PWD/data"
```

The legacy content set is not internally consistent. Many validator failures are pre-existing (missing spawn-referenced objects, not parser crashes). Check `refactoring-documents/06-validator-baseline.md` before treating failures as new regressions.

## Architecture

### Repository Layout

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
| `backup-copy/` | **Read-only** reference snapshot — never modify |
| `build/` | Generated output — never manually edit |

The superproject passes the top-level `idlib/` into `idlib-game-engine` during CMake, so `idlib-game-engine/idlib` does not need separate initialization.

### Runtime Architecture

**Boot path**: `Main.cpp` → `Ego::Core::System::initialize()` (VFS, logging, config, SDL timer/events/video/audio/input) → `EngineContext::get().setEngine(make_unique<GameEngine>())` → `engine().start()` → `GameEngine::initialize()` (GFX/OpenGL, CameraSystem, AudioSystem, UIManager, CollisionSystem, pushes `MainMenuState`) → main loop.

**Main loop**: Fixed update (50 UPS) / fixed render (60 FPS) with frame skipping (max 10 frame skip tolerance).

**State management**: Stack-based game states (`MainMenuState`, `PlayingState`, `SelectModuleState`, `SelectPlayersState`, `LoadingState`, `InGameMenuState`, `MapEditorState`, options, debug, victory states).

**Content system**: Directory-based module format with convention-driven files (`menu.txt`, `spawn.txt`, `data.txt`, `script.txt`). Virtual file system (PhysFS) with mount points (`mp_data`, `mp_modules`, `mp_objects`).

### Global State (major coupling points)

Historically the runtime was wired around three mutable globals. All three have been fully retired from active runtime code:

- `_gameEngine` — **0 references.** Fully eliminated. Engine access now routes through `EngineContext::get().setEngine()` / `engine()`.
- `_currentModule` — **0 references.** All consumers go through `GameSessionContext` and `GameModule` accessor surfaces.
- `update_wld` — **0 active references.** The global variable is gone; 3 stale string-literal / comment artifacts remain in `script.c`, `ObjectGraphics.hpp`, and `Particle.hpp`. The functional replacement is `worldUpdateCount()` (routing through `GameSessionContext`), which has ~50 call sites across ~20 files.

The remaining coupling hotspot is singleton access: ~912 `::get()` call sites persist. The `EngineContext` service-interface layer now covers audio, perk, image, particle, profile, logging, config, font, input, graphics system, texture manager, texture atlas, and GFX; broader DI does not yet exist.

Avoid reintroducing hidden global dependencies. Be careful around code that affects VFS setup, module loading, object profile loading, or script compilation.

### Egolib Subsystems

AI, Audio, Configuration, Console, Core (quad-trees, thread pool), Entities (objects/particles), Extensions (OpenGL), FileFormats (MD2, maps, configs), Graphics (fonts, textures, framebuffer), Grid, Image, InputControl, Log, Logic, Math, Mesh, Platform, Profiles, Renderer (OpenGL), Script (bytecode VM, compiler), Time, VFS, game (core gameplay), integrations.

### High-Risk Hotspots

Read relevant audit docs before modifying. Files over 1,000 lines (by size):
- `egolib/library/src/egolib/game/script_functions_systems.c` (~3200 lines, largest TU)
- `egolib/library/src/egolib/vfs.c` (~2460 lines)
- `egolib/library/src/egolib/game/script_functions_target.c` (~1680 lines)
- `egolib/library/src/egolib/Entities/Object.hpp` (~1620 lines, monolithic interface — 18 role interfaces extracted but header still large)
- `egolib/library/src/egolib/game/script_functions_state.c` (~1480 lines)
- `egolib/library/src/egolib/game/Physics/particle_collision.c` (~1530 lines)
- `egolib/library/src/egolib/game/Graphics/ObjectGraphics.cpp` (~1490 lines)
- `egolib/library/src/egolib/game/mesh.c` (~1370 lines)
- `egolib/library/src/egolib/game/script_functions_spawn.c` (~1580 lines)
- `egolib/library/src/egolib/fileutil.c` (~1330 lines)
- `egolib/library/src/egolib/Script/script.c` (~1370 lines)

Architecturally central but now small after split passes:
- `egolib/library/src/egolib/game/game.c` (~550 lines, split into `game_{combat,export,loop,targeting,wawalite}.c`)
- `egolib/library/src/egolib/Entities/Object.cpp` (~200 lines, split into six `Object_*.cpp` TUs)
- `egolib/library/src/egolib/game/Module/Module.cpp` (~200 lines, split into six `Module_*.cpp` siblings)

## Refactoring Guidelines

- Read `refactoring-documents/README.md` and `refactoring-documents/04-refactoring-strategy.md` before large refactors.
- Prefer seam creation, file-splitting, and dependency reduction over speculative rewrites.
- Preserve observable behavior unless the task explicitly calls for behavior change.
- Prefer small, verifiable changes over broad rewrites.
- If an edit changes runtime ownership, loading flow, or subsystem boundaries, update `refactoring-documents/`.
- Preserve current Linux/Fedora portability behavior unless intentionally revisiting it.

## Testing

Google Test framework. Tests in `egolib/tests/`. Coverage is limited (compilation checks, quad-tree, string utilities, mesh iterators, content parsers, module loading smoke tests). No gameplay logic tests exist.

## Environment Variables

| Variable | Purpose |
|----------|---------|
| `EGOBOO_DATA_DIR` | Override game data directory (Linux) |
| `EGOBOO_DISABLE_MIPMAPS` | Wine compatibility |
| `EGOBOO_DISABLE_AUDIO` | Wine compatibility |
| `SDL_VIDEODRIVER=x11` | Useful on Wayland systems |
| `DRI_PRIME=1` | Use discrete GPU |

For sandboxed environments: `HOME=/tmp/egoboo-home XDG_DATA_HOME=/tmp/egoboo-xdg`

## Sub-Agents

Project-scoped sub-agents live in `.claude/agents/`. Delegate to them to keep the main conversation context clean.

| Agent | Model | Purpose | Tools |
|-------|-------|---------|-------|
| `repo-architect` | sonnet | Architecture exploration, coupling analysis, dependency tracing | Read-only |
| `content-auditor` | sonnet | Content format analysis, data integrity, module structure inspection | Read-only |
| `validator-runner` | haiku | Build the project and run the content validator, report results | Read-only + Bash |
| `refactor-worker` | sonnet | Execute bounded refactoring tasks (runs in isolated worktree) | Full edit access |
| `linux-portability` | sonnet | Diagnose platform issues across Linux, Windows, and cross-build targets | Read-only |

`repo-architect`, `content-auditor`, and `refactor-worker` have project-scoped persistent memory to accumulate findings across sessions.
