# Codebase Health Status

Current-state health snapshot for the Egoboo workspace. This document is the
canonical place for volatile size, archive, and test-count numbers; other
Markdown files should link here instead of carrying duplicate copies.

Snapshot date: 2026-07-15. Measurements below were taken from the live tree and
the existing `build/products/x64/lib/libegolib-*.a` archives.

## Executive Summary

Egoboo remains a mixed C/C++ runtime in active modernization. The largest wins
since the April 2026 baseline are still intact:

- The former mutable runtime globals `_gameEngine`, `_currentModule`, and
  `update_wld` are retired from active code. Only four `update_wld` text/comment
  artifacts remain.
- `egolib` is split into nine static archives with an acyclic intended
  dependency direction.
- Former runtime monoliths have been split aggressively. There are now no
  production runtime files over 1,000 lines under `egolib/library/src` or
  `egoboo/src`.
- The test suite is substantially larger than the April baseline and currently
  configures 955 ctest cases.
- The content validator has a stable known legacy-content baseline: 42 modules,
  10 warnings, 245 errors.

The main remaining debt is not raw file size anymore. It is interface,
ownership, and dependency-visibility coupling: `Object` is still broad by
interface, `GameModule` still mixes world ownership with loading and update
logic, session and engine access still route through context singletons, script
dispatch remains procedural, and the Windows runtime path remains unstable under
Wine.

Architectural health is now medium and improving. The project is easier to
develop than the April baseline for localized changes, characterization tests,
and layer-preserving file splits. It is still hard to extend safely when a
change crosses object lifecycle, module loading, script semantics, VFS behavior,
or engine/session service ownership.

## Key Metrics

| Metric | Current value | Notes |
| --- | ---: | --- |
| `egolib` archives | 9 | `foundation-base`, `physics`, `renderer`, `gui`, `library`, `game-graphics`, `hud-widgets`, `scriptvm`, `gamestates` |
| Archive members | 168 / 6 / 28 / 24 / 83 / 21 / 6 / 33 / 19 | In the archive order above, measured with `ar t` |
| Runtime source files | 791 | `egolib/library/src` + `egoboo/src`; 103 `.c`, 286 `.cpp`, 73 `.h`, 329 `.hpp` |
| Runtime source lines | 129,833 | Same scope as above |
| Test files / lines | 51 / 25,258 | `egolib/tests`, source/header files only |
| ctest cases | 955 | `ctest --test-dir build -N` |
| ctest baseline | 955 / 955 | Last recorded green baseline in the pass log; use `ctest -j20 --output-on-failure` |
| `::get()` call sites | 467 | `rg "::get\\(" egolib/library/src`; includes intentional context seams |
| `EngineContext::get()` | 388 | Dominant intentional engine seam |
| `GameSessionContext::get()` | 23 | Dominant intentional session seam; Pass 309 moved the last gamestates read-only module accesses onto lower-layer seams |
| `TODO`/`FIXME`/`HACK` markers | 59 | `egolib/library/src` + `egoboo/src` |
| `throw` references | 662 | Broad grep count, not semantic classification |
| Interface headers | 67 | `I*.hpp`/`I*.h` headers under `egolib/library/src/egolib`, excluding `IDSZ.hpp` |
| Object role interfaces | 22 | 24 `Entities/I*.hpp` files total, including 2 service interfaces |
| `idlib::singleton` references | 19 | Intentional services plus legacy-singleton remnants |

## Link Layout

The live archive member counts are:

| Archive | Members | Role |
| --- | ---: | --- |
| `egolib-foundation-base` | 168 | Dependency-closed base: math, logging, VFS, file formats, profiles data/model loading, script compiler pieces, low-level services |
| `egolib-physics` | 6 | Collision nucleus and physics primitives |
| `egolib-renderer` | 28 | SDL windowing and OpenGL renderer backend |
| `egolib-gui` | 24 | Generic GUI toolkit and abstract `GameState` base |
| `egolib-library` | 83 | Core gameplay remainder: entities, session/module runtime, lower service holders, game physics, object graphics |
| `egolib-game-graphics` | 21 | 3D scene-rendering layer: camera, billboard, texture atlas, render passes, GFX bootstrap |
| `egolib-hud-widgets` | 6 | Game-coupled in-game HUD widgets |
| `egolib-scriptvm` | 33 | EgoScript VM and `script_functions_*` dispatch family |
| `egolib-gamestates` | 19 | Concrete game state screens |

Intended direction:

```text
foundation-base <- {physics, renderer <- gui} <- library
library <- game-graphics <- hud-widgets <- {scriptvm, gamestates}
```

When touching `egolib/library/CMakeLists.txt` or moving sources between these
archives, re-run the live-archive `nm` back-edge check. Measure the `.a` files,
not `CMakeFiles/*.dir`, because object directories can retain stale `.o` files
from earlier carves.

## File Size Status

Production runtime source no longer has a >1,000-line file. Current largest
runtime files:

| File | Lines |
| --- | ---: |
| `egolib/library/src/egolib/Entities/Object.hpp` | 998 |
| `egolib/library/src/egolib/Profiles/ObjectProfile.hpp` | 808 |
| `egolib/library/src/egolib/FileFormats/wawalite_file.h` | 736 |
| `egolib/library/src/egolib/game/game_combat.c` | 721 |
| `egolib/library/src/egolib/Script/script.h` | 684 |
| `egolib/library/src/egolib/Audio/AudioSystem.cpp` | 675 |
| `egolib/library/src/egolib/map_functions.c` | 668 |
| `egolib/library/src/egolib/game/Physics/CollisionSystem.cpp` | 660 |
| `egolib/library/src/egolib/bbox.c` | 660 |
| `egolib/library/src/egolib/vfs.c` | 658 |

Large non-runtime files still exist and are intentional test/tool hotspots:

| File | Lines |
| --- | ---: |
| `egolib/tests/egolib/tests/ScriptSystemsFunctions.cpp` | 3,243 |
| `egolib/tests/egolib/tests/ObjectAccessors.cpp` | 3,054 |
| `egolib/tests/egolib/tests/ScriptStateFunctions.cpp` | 1,928 |
| `tools/egoboo-content-validator.cpp` | 1,550 |
| `egolib/tests/egolib/tests/ScriptTargetFunctions.cpp` | 1,360 |
| `egolib/tests/egolib/tests/EngineContext.cpp` | 1,236 |
| `egolib/tests/egolib/tests/ScriptActionFunctions.cpp` | 1,235 |
| `egolib/tests/egolib/tests/ContentParsers.cpp` | 1,230 |

Current file-size priority: do not chase production file size mechanically.
Further value is in reducing interface breadth and cross-archive coupling.

## Global State And Singleton Coupling

Former globals:

| Global | Current state |
| --- | --- |
| `_gameEngine` | 0 active references; engine access routes through `EngineContext` |
| `_currentModule` | 0 active references; module access routes through `GameSessionContext` and `GameModule` surfaces |
| `update_wld` | variable removed; four text/comment/debug-label artifacts remain |

The remaining coupling hotspot is service-locator access. `EngineContext::get()`
and `GameSessionContext::get()` are intentional seams for now, but they still
flatten dependency visibility. Broader constructor injection does not exist.

Actionable direct singleton calls should remain small and justified. When adding
or moving code, prefer existing service interfaces and `active*()` seams over
new hidden global access.

Directly owned `EngineContext` services now delegate through active service
registries for input, image, font, texture-atlas, and GFX access. Active
object-handler and object lookup access routes through the lower-layer
`IObjectWorld` seam for the migrated gameplay, graphics, script, audio, and
entity callers; `GameSessionContext` no longer exposes object lookup forwarding
methods. Active module environment, read-only module status, and read-only
session state access now route through the narrower `IModuleEnvironment`,
`IModuleStatus`, and `ISessionState` seams for migrated rendering, camera, HUD,
menu, script, entity, and spawn callers. Terrain queries now use the active
`ITerrainQuery` seam, and active module command/mutation paths such as spawning,
team experience, passages, shops, pit controls, respawn/export/beaten flags, and
tile changes route through `IModuleCommands` for migrated script, entity,
graphics, shop, loading, game-loop, and now top-of-DAG gamestates-screen callers
(`PlayingState` debug watches/export check/cheat and `MapEditorState::update`
were the last read-only stragglers, moved onto the status/commands/object-world/
environment seams in Pass 309). `GameModule` loading, spawning,
update, passage music, weather, and player-startup code now receive session and
engine services through an explicit `GameModuleRuntime` provider surface instead
of directly reaching into `EngineContext` or `GameSessionContext`.
`GameSessionContext` and `GameModule` still own lifetime, lifecycle orchestration,
and concrete pre-module fallback state. This keeps context APIs narrower while
moving ownership seams toward lower archives.

Script operand and resolved-self contexts now carry object role views rather
than cached concrete `Object*` pointers. Script-visible liveness is derived from
`IDamageable`, not duplicated on `ITargetInfo`, and spawned-character handling
uses `IAttachmentControl`, `IPhysical`, and the existing lifecycle, movement,
script, and character-state roles. These seams reduce concrete `Object`
dependencies inside `egolib-scriptvm` while preserving the existing script ABI
and behavior.

The VM driver and legacy script-operation family now resolve active entity roles
through the lower-layer `ObjectRoleAccess` adapter. `IScriptSystem` and
`scr_run_chr_script()` dispatch only `ObjectRef`; VM-owned runtime state and
visibility use `IScriptRuntimeState` and `IVisibilityObserver`; movement reset
is part of `IMovementControl`; and interpreter `ObjectValue` stores `ObjectRef`
instead of a raw pointer. The strict script sources no longer include the
aggregate entity header or name concrete `Object`/`ObjectHandler`, while the
`IObjectWorld` virtual interface remains unchanged.

## Design-Pattern Usage

Egoboo now uses several recognizable patterns intentionally, but not yet
uniformly:

| Pattern | Current usage | Health |
| --- | --- | --- |
| State | `GameState` plus concrete menu/loading/playing/debug states on the engine state stack | Healthy for screen flow; preserve this shape for new screens |
| Factory / composition root | `Main.cpp` injects the main-menu-state factory and installs script/graphics bootstraps from higher archives; `GameEngine` now delegates the content (`ContentRuntimeBootstrap`) and audio+particle (`GameplaySubsystemsBootstrap`) subsystem lifecycles to RAII bootstrap objects | Good archive-boundary tool; `GameEngine::initialize()` still directly orchestrates the remaining concrete systems (gfx hook, console, collision), but the ordered subsystem lifecycles are increasingly encapsulated |
| Service locator / singleton | `EngineContext`, `GameSessionContext`, `idlib::singleton`, and active-service registries | Useful migration path away from raw globals, but still hides dependencies from call signatures and tests |
| Facade / context object | `GameSessionContext` centralizes active module, import list, local-player state, clocks, and session-owned environment state | Better than exported globals; risk is continued API growth |
| Interface / adapter | `IAudioSystem`, `IProfileSystem`, `IScriptSystem`, graphics/input/image interfaces, `ScriptSystemAdapter`, `ObjectRoleAccess`, and object roles such as `IAttachmentControl` and `IScriptRuntimeState` | Improving DIP/ISP story; several interfaces still expose broad subsystem surfaces |
| Function table / registry | EgoScript dispatch maps script function values to native function pointers through `Ego::Script::Runtime` | More extensible than a monolithic switch; still requires edits to central function/variable lists and focused regression tests |
| RAII ownership | `unique_ptr`/`shared_ptr` are used for engine, module, UI, player/object/profile ownership | Generally improving; still mixed with C globals, raw pointers, and legacy lifecycle calls |

Pattern usage is pragmatic rather than framework-heavy. The most successful
modernization pattern is "extract an interface or context boundary, preserve
behavior, then move callers in small batches." Continue that approach.

## SOLID Assessment

| Principle | Status | Evidence and implication |
| --- | --- | --- |
| Single Responsibility | Partial | File size is controlled, but `Object`, `GameModule`, `GameEngine::initialize()`, `ProfileSystem`, VFS, and script dispatch still combine multiple reasons to change. Future work should extract behavior families, not just split files. |
| Open/Closed | Partial | New game states and some services can be added behind interfaces/factories. New script functions, profile fields, and legacy content conventions still require central list/parser/dispatch edits. |
| Liskov Substitution | Mostly acceptable | Service and role interfaces are thin enough to substitute in tests or adapters. The risk area is `Object` implementing many roles at once; callers can accidentally depend on the concrete object even when a role interface would suffice. |
| Interface Segregation | Improving, uneven | Object role interfaces and service interfaces reduce the need for full concrete types. `EngineContext`, `GameSessionContext`, `IProfileSystem`, and `GameModule` remain broad interfaces that can pull unrelated dependencies into small changes. |
| Dependency Inversion | Partial | Higher-level code increasingly talks to interfaces and bootstrapped services, but many dependencies are still resolved by `::get()` service locators instead of constructor/function parameters. This limits isolated tests and makes dependency changes harder to reason about. |

Overall SOLID health: better than legacy C-style global access, but not yet a
clean dependency-injected architecture. The code is safest to extend where a
small role interface, active service, or state/factory boundary already exists.
It is riskiest where a change needs concrete `Object`, concrete `GameModule`, or
direct `EngineContext::get()` / `GameSessionContext::get()` access from deep
runtime code.

## Development And Extension Health

Low-risk development areas:

- Adding focused tests or validator checks around existing parser/runtime
  behavior.
- Adding a new game state that follows the existing `GameState` stack pattern.
- Splitting files inside an archive without changing public ownership or archive
  direction.
- Extending a subsystem through an existing `I*` service or role interface.

Medium-risk areas:

- Adding a new script function or variable, because the runtime table,
  function lists, and behavior tests must stay synchronized.
- Adding a loader or content rule, because legacy content conventions and the
  validator baseline are tightly coupled.
- Moving sources between archives, because the nine-archive DAG must remain
  acyclic and the bootstrap injection points must stay above `egolib-library`.

High-risk areas:

- Changing object lifecycle, attachment, combat, inventory, or script-visible
  state.
- Changing module loading, spawn planning/realization, VFS mounts, object
  profiles, or model loading.
- Broadening `EngineContext` / `GameSessionContext` or adding new hidden global
  access.
- Treating the Wine runtime path as representative of native Windows support.

Practical extension guidance: prefer new role/service interfaces only when they
remove concrete `Object`/`GameModule` dependencies or expose a real substitution
point. For small behavior moves, keep the current pattern of characterization
tests first, behavior-preserving extraction second, and only then interface
narrowing.

## Testing And Validation

Recommended default verification:

```bash
cmake --build build -j20
ctest --test-dir build -j20 --output-on-failure
./build/products/x64/bin/egoboo-content-validator --data-dir "$PWD/data" --module test.mod
```

The test runner is parallel-safe in the current harness. Each test process gets
its own `EGOBOO_USER_DIR` through per-PID isolation.

Full validator baseline, last rechecked 2026-07-11:

| Metric | Value |
| --- | ---: |
| Modules validated | 42 |
| Warnings | 10 |
| Errors | 245 |

The full validator exits nonzero because the shipped legacy content has
pre-existing integrity errors. Treat parser crashes, new error categories, or
baseline count changes as suspicious; do not treat the current 245 legacy
content errors as a new regression by themselves.

## Build And Platform Status

| Target | Build status | Runtime status | Canonical doc |
| --- | --- | --- | --- |
| Linux native | Works | Primary development path | `doc/build-linux.md` |
| Linux-hosted Windows cross-build | Works | Wine path remains a debugging/compatibility path | `doc/build-windows.md` |
| Native Windows open-source toolchain | Future target | Not first-class yet | not documented |
| MSVC / Visual Studio | Legacy only | Deprecated | `doc/legacy/` |
| macOS | Not maintained | Deprecated | `doc/legacy/` |

The Wine helper still uses compatibility defaults for mipmaps and audio. Do not
treat the Wine path as a completed Windows runtime port.

## Current Priorities

1. Keep the nine-archive DAG acyclic when moving sources.
2. Reduce interface and ownership coupling, especially around `Object`,
   `GameModule`, `GameSessionContext`, and `EngineContext`.
3. Narrow service-locator use by passing role/service interfaces into new code
   where the call path already has those dependencies available.
4. Keep script dispatch splits and parser/model-loader work behavior-preserving
   and covered by focused tests.
5. Stabilize the open-source Windows path without reintroducing Visual
   Studio-only requirements.
6. Maintain docs by updating this file first for volatile numbers, then linking
   to it from roadmap/audit documents.
