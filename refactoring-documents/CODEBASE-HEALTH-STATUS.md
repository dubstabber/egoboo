# Codebase Health Status

Consolidated, current-state health snapshot of the Egoboo codebase. Supersedes and replaces the earlier point-in-time summaries:

- `17-codebase-health-assessment.md` (2026-04-13 quantitative baseline)
- `18-modularization-analysis.md` (2026-04-13 modularization view)
- `32-project-health-and-solid-assessment.md` (2026-04-16 SOLID/design assessment)
- `46-cross-platform-and-third-party-independence-status.md` (2026-04-17 portability snapshot)

Snapshot date: 2026-04-20. This document is intentionally standalone — it does not cross-reference numbered passes beyond what is necessary to locate canonical plans, so it survives as a single health reference even if the individual pass documents move.

---

## 1. Executive Summary

The codebase is in an **active, well-managed transitional state**. The original C dungeon crawler is being incrementally modernized to C++, and the completed refactoring passes (dozens of in-repo passes so far) have landed real structural wins:

- The two biggest global-state boundaries — `_currentModule` and `_gameEngine` — are fully retired from active runtime code. Remaining mentions are confined to legacy comments, debug labels, and a terminology-only `update_wld` residue in three files.
- Every historically oversized translation unit has been file-split. No file in the active tree exceeds 2,500 lines.
- File splitting, context wrappers, and accessor encapsulation work have now progressed into early role extraction on the `Object` god class rather than stopping at the runtime boundary.
- A native validator tool exists, content parser tests exist, and module load/spawn smoke tests exist.

Core design debt that remains:

- The `Object` class is still monolithic by interface — `Object.hpp` is now 1,636 lines, actually larger than in the April 19 snapshot because role extraction has added interface inheritance and query surfaces faster than legacy members have been retired. Its implementation is now split across seven files (`Object.cpp` plus six `Object_{appearance,attributes,combat,interaction,lifecycle,update}.cpp` TUs), and the role-extraction passes have peeled off `IInventoryHolder`, `IRenderable`, `IScriptable`, `IDamageable`, `IPhysical`, `ITargetInfo`, `ICharacterState`, `ITeamMember`, `IWallet`, `IAnimationControl`, `IAppearanceProfile`, `IEnchantable`, `IMovementControl`, `IVisualControl`, `IItemInfo`, `ILifecycleControl`, and `IMorphControl`.
- Singleton access is still pervasive (~946 `::get()` call sites, down from ~1,150), and the `EngineContext` service-interface layer now covers audio, perk, image, particle, profile, logging, and runtime `egoboo_config_t` access; full DI still does not exist.
- Error handling still mixes C++ exceptions, `egolib_rv` return codes, and silent failure — but a written policy is now published at `doc/error-handling-policy.md` and new code is being held to it.
- The Linux-hosted Windows cross build is unstable at runtime (font atlas / audio crash under Wine); the native-Windows open-source path is undocumented.

Overall maintainability: **2.5 / 5**, unchanged as a score but with meaningful forward motion inside the envelope (test coverage tripled, singleton call sites down ~18%, error-handling policy landed, eight additional role interfaces in place). The trend is unambiguously positive, but core decoupling work (DIP / ISP) remains the critical frontier.

---

## 2. Size and Composition

### Source files (egolib + egoboo; cartman is present but not in the main build)

| Category                         | Count |
| -------------------------------- | ----: |
| C implementation files (`.c`)    |    61 |
| C++ implementation files (`.cpp`)|   226 |
| C headers (`.h`)                 |    59 |
| C++ headers (`.hpp`)             |   286 |
| **Total active source files**    |**632**|

The `.c` count drifted down from 70 (2026-04-19) as small C utilities were folded into their C++ neighbors; `.h` dropped similarly as legacy headers were deleted. `.cpp` and `.hpp` growth reflects the new role-interface headers and their implementation seams.

### Lines of code

| Area                                                          | Approx. Lines |
| ------------------------------------------------------------- | ------------: |
| `egolib` C sources (`.c`)                                     |        35,547 |
| `egolib` C++ sources (`.cpp`)                                 |        44,856 |
| `egolib` + `egoboo` headers (combined)                        |       ~38,800 |
| `egoboo/src/` (thin executable)                               |           ~90 |
| Active source + headers total (egolib + egoboo)               |      **119,219** |
| `egolib/tests/`                                               |        13,094 |

The C vs. C++ split by implementation-file line count is roughly 44% C / 56% C++, essentially flat against the prior snapshot. The aggregate source line count dropped slightly even while role interfaces grew, indicating that the role passes are net-simplifying at the callsite despite adding new headers.

### Test-to-code ratio

**13,094** test lines against ~119,000 active source lines = **~11.0%**. This is a ~3× jump from the ~3.6% figure at the previous snapshot and crosses the threshold at which tests start protecting behavior rather than only compilation. The jump came from bringing script-dispatch and gameplay surfaces under test rather than from raw parser coverage growth.

Current test files under `egolib/tests/egolib/tests/`:

- `Compilation.cpp`, `StringUtilities.cpp`, `QuadTree.cpp`, `MeshInfoIterator.cpp` — utility coverage
- `ContentParsers.cpp`, `SpawnName.cpp` — parser coverage
- `ConfigReadMostly.cpp`, `ConfigMutations.cpp`, `GameText.cpp` — configuration / text subsystem coverage
- `ModuleLoadSmoke.cpp`, `ModuleSpawnPlanning.cpp`, `ModuleSpawnRealization.cpp`, `ModulePlayerStartup.cpp`, `ModuleUpdate.cpp` — module-loading and update smoke coverage
- `LoadPlayerElement.cpp`, `PlayerQuestLog.cpp` — player startup / quest hydration coverage
- `EngineContext.cpp` — engine-context seam coverage
- `ObjectAccessors.cpp` — accessor-encapsulation regression coverage
- `ScriptActionFunctions.cpp`, `ScriptMovementFunctions.cpp`, `ScriptStateFunctions.cpp`, `ScriptSystemsFunctions.cpp`, `ScriptTargetFunctions.cpp`, `ScriptVariables.cpp` — script-dispatch behavior coverage (new since last snapshot)
- `GameplayAlertPublication.cpp`, `ShopInteractions.cpp` — gameplay-level behavior coverage (new since last snapshot)
- `math/` — math submodule tests

Notably absent: physics/collision tests, rendering correctness tests, GUI tests. Script-VM and gameplay surfaces are now partially covered.

---

## 3. Hotspot Files

### Files over 1,000 lines (active tree)

Fourteen files remain over the 1k-line threshold. `script_functions.c` (8,183 lines) no longer exists, and `Object.cpp` (3,201 lines) has been split. `Object.hpp` is the surviving "large header" and still sits above the threshold because role extraction is extending its interface before trimming the legacy surface.

| File                                              |  Lines | Role                                       |
| ------------------------------------------------- | -----: | ------------------------------------------ |
| `egolib/vfs.c`                                    |  2,456 | Virtual file system (largest TU in tree)   |
| `game/script_functions_systems.c`                 |  2,412 | Script dispatch (systems subset)           |
| `game/script_functions_target.c`                  |  1,870 | Script dispatch (target subset)            |
| `Entities/Object.hpp`                             |  1,636 | Core entity — still monolithic by interface |
| `game/script_functions_state.c`                   |  1,600 | Script dispatch (state subset)             |
| `game/Physics/particle_collision.c`               |  1,520 | Particle collision                         |
| `game/Graphics/ObjectGraphics.cpp`                |  1,479 | Object rendering                           |
| `game/mesh.c`                                     |  1,370 | Mesh management                            |
| `game/script_functions_spawn.c`                   |  1,363 | Script dispatch (spawn subset)             |
| `fileutil.c`                                      |  1,327 | File utilities                             |
| `game/script_compile.c`                           |  1,148 | Script compiler                            |
| `game/Physics/ObjectPhysics.cpp`                  |  1,109 | Object physics                             |
| `Script/script.c`                                 |  1,075 | Script runtime                             |
| `game/script_functions_action.c`                  |  1,025 | Script dispatch (action subset)            |

The split script-dispatch TUs have continued to grow as new helpers have been moved in from `Object` and friends rather than authored from scratch — a consequence of role extraction, not a regression. No individual file exceeds 2,500 lines.

### Files that have been decomposed

| Former monolith                    | Prior Size | Current Split                                                                                                        |
| ---------------------------------- | ---------: | ------------------------------------------------------------------------------------------------------------------ |
| `script_functions.c`               |      8,183 | Seven files: `script_functions_{action,bitwise,movement,spawn,state,systems,target}.c`                               |
| `Entities/Object.cpp`              |      3,201 | Seven files: `Object.cpp` (195) + `Object_{appearance,attributes,combat,interaction,lifecycle,update}.cpp`           |
| `game/game.c`                      |      2,456 | Six files: `game.c` (548) + `game_{combat,export,loop,targeting,wawalite}.c`                                         |
| `Profiles/ObjectProfile.cpp`       |      1,468 | Three files: `ObjectProfile_{core,load,export}.cpp`                                                                  |
| `Entities/Particle.cpp`            |      1,447 | Five files: `Particle_{core,combat,spawn,update}.cpp` + `ParticleHandler.cpp`                                        |
| `game/Module/Module.cpp`           |      1,225 | Seven files under `Module/`: `Module.cpp` (135) + `Module_{bootstrap,loading,spawn,spawn_plan,spawn_realization,update}.cpp` |

These splits made compilation and navigation tractable, but they did not finish the interface problem. `Object.hpp` has grown (1,527 → 1,636 lines) as new role interfaces are added to its inheritance list faster than the legacy surface is retired; the remaining work is to continue moving callers onto the role interfaces and then prune the redundant methods from `Object` itself.

### Deeply nested / switch-heavy regions

The former deep-nesting concentration in `script_functions.c` is now distributed across the seven split files. Switch-statement density remains high in script dispatch and game logic (on the order of 100+ `switch` statements in `egolib`). This has not meaningfully improved and will only fully resolve once the script system moves to a registry model (plan item T3.2).

---

## 4. Global State and Coupling

### Direct global runtime-state references

| Global           | 2026-04-12 baseline | Current |
| ---------------- | ------------------: | ------: |
| `_currentModule` |                 592 |       0 |
| `_gameEngine`    |                 266 |       0 |
| `update_wld`     |                  65 |       3 |

Both `_currentModule` and `_gameEngine` are fully retired from active runtime code. Remaining references are limited to commented-out documentation inside `egolib/AGENTS.md`. The `update_wld` residue is confined to `Script/script.c`, `game/Graphics/ObjectGraphics.hpp`, and `Entities/Particle.hpp` as a legacy debug label and commentary, not active global coupling.

This is the largest single structural win since the baseline.

### Singleton and service-locator access

Raw `::get()` singleton calls now number approximately **946** across the codebase, down from ~1,150 at the previous snapshot and from 1,239 at the April 13 baseline. The decline reflects genuine migration onto `EngineContext`-published services rather than just funnelling — whole caller clusters in physics, script dispatch, GUI, and state-management have moved off the concrete singleton lookups.

Engine-published service seams landed so far:

- `IAudioSystem`, `IPerkHandler`, `IImageManager`, `IParticleHandler`, and `IProfileSystem` are now published through `EngineContext`, with non-subsystem callers migrated off the concrete singleton lookup.
- Runtime logging routes through the installed `EngineContext` log target outside the `Log` subsystem's bootstrap/lifecycle code.
- `egoboo_config_t` is published through `EngineContext` for bootstrap/lifecycle paths, module-load sync, lightweight content bootstrap, and the cross-cutting runtime/UI caller set including the write-heavy audio/video options flow.

Dominant direct singletons still reachable outside the session/engine wrappers:

- `egoboo_config_t::get()` — confined to subsystem-local bootstrap/lifecycle or singleton-definition code (`AudioSystem`, `ImageManager`, `ParticleHandler`, `Core::System`, `ContentRuntimeBootstrap`, `egoboo_setup.c`).
- `AudioSystem::get()`, `PerkHandler::get()`, `ImageManager::get()`, and `ParticleHandler::get()` remain as subsystem-local bootstrap seams inside their own implementations.
- `ProfileSystem::get()` and `Log::get()` remain as subsystem-local lifecycle/bootstrap seams inside their own implementations.

### Smart pointer distribution

| Pattern         | Count (active code) | Note                                                              |
| --------------- | ------------------: | ----------------------------------------------------------------- |
| `shared_ptr`    |              ~1,250 | Still over-used; `Object` inherits from `enable_shared_from_this` |
| `unique_ptr`    |                 ~52 | Still under-used                                                  |
| `weak_ptr`      |                 ~26 | Appropriate use for back-references                               |
| Raw `new`/`delete` |              ~220 | Concentrated in C-era code and some legacy C++                  |

The raw occurrence count for `shared_ptr` in headers and source combined is higher than previously reported because the new role-interface headers forward handles through the codebase. The `shared_ptr<Object>` pattern remains the single most ownership-opaque idiom in the codebase, and would take a dedicated pass to address safely.

### Error handling

Three competing strategies still coexist:

- C++ exceptions — ~240 `throw` sites, `try`/`catch` in ~54 files
- C return codes — `egolib_rv`
- Silent failure via `nullptr`/`false` returns

A written policy now exists at `doc/error-handling-policy.md`: exceptions for exceptional failures and invariant violations, expected-failure return types at subsystem boundaries, no silent failure. The migration of existing callers has not yet run — most of the mixed-style code predates the policy — but new code is now held to it.

### TODO / FIXME / HACK density

60 markers across active `egolib` source — essentially flat against the previous snapshot's 61 and still indicative of carried debt.

---

## 5. Modularization State

### Build targets

| Target                       | Type                 | Role                                                                                                | Lines    |
| ---------------------------- | -------------------- | --------------------------------------------------------------------------------------------------- | -------: |
| `idlib`                      | Submodule (11 libs)  | Foundation utilities (math, color, filesystem, parsing, signals, types, chrono, document, hll)      | ~33,000  |
| `idlib-game-engine`          | Submodule            | OpenGL (GLEW), PhysFS, googletest integration                                                        | ~5,000   |
| `egolib-library`             | Static library       | All runtime code                                                                                     |~120,000  |
| `egoboo`                     | Executable           | Thin entry point                                                                                     |       90 |
| `egoboo-content-validator`   | Executable (tool)    | Content validation tool                                                                              |   ~1,200 |
| `cartman`                    | Not built            | Map editor (disconnected from main CMake graph)                                                      |   ~6,000 |

### `egolib` internal structure — now made explicit in CMake

The historical `GLOB_RECURSE` in `egolib/library/CMakeLists.txt` has been replaced with explicit, per-subsystem source lists (one `set()` block per directory, grouped alphabetically). Ownership is visible in the build system even though all objects still link into a single static library.

Directory-level subsystem map (by line count, large to small):

| Subsystem            | Location                                      | Approx. Lines |
| -------------------- | --------------------------------------------- | ------------: |
| Game core (C)        | `game/` top-level `.c`/`.h`                   |       ~25,000 |
| Game Graphics        | `game/Graphics/`                              |        ~7,200 |
| Game GUI             | `game/GUI/`                                   |        ~5,600 |
| Game States          | `game/GameStates/`                            |        ~5,200 |
| Game Physics         | `game/Physics/`                               |        ~5,100 |
| Entities             | `Entities/`                                   |        ~8,200 |
| Graphics (engine)    | `Graphics/`                                   |        ~5,800 |
| Script               | `Script/`                                     |        ~5,700 |
| Profiles             | `Profiles/`                                   |        ~5,500 |
| FileFormats          | `FileFormats/`                                |        ~5,100 |
| Renderer             | `Renderer/`                                   |        ~4,100 |
| Game Module          | `game/Module/`                                |        ~2,600 |
| Image                | `Image/`                                      |        ~2,000 |
| Logic                | `Logic/`                                      |        ~1,600 |
| Game Core (engine)   | `game/Core/`                                  |        ~1,300 |
| Log                  | `Log/`                                        |        ~1,100 |
| Math                 | `Math/`                                       |        ~1,100 |
| Time                 | `Time/`                                       |        ~1,000 |
| Core                 | `Core/`                                       |          ~940 |
| Audio                | `Audio/`                                      |          ~890 |
| AI                   | `AI/`                                         |          ~880 |

### Dependency-flow violations that still exist

1. **Script → Everything.** Script dispatch (split into seven `.c` files but still one logical subsystem) includes headers spanning Entities, Profiles, Physics, Graphics, GUI, Module, and game state.
2. **GUI → Game internals.** GUI screens directly read player state, inventory, and session state through session-context accessors.
3. **Profiles → Runtime singletons.** `ObjectProfile_load.cpp` pulls `PerkHandler`, `ImageManager`, and `ProfileSystem` singletons during parsing.
4. **FileFormats → Runtime services.** Content parsing remains entangled with VFS mount state.
5. **Entities ↔ Game Module.** The structural cycle at the runtime-ownership layer has been broken by the session/engine context, but the Entity and Module headers still cross-include via forward-declared surfaces.

### "Gravity well" headers

| Header                          | Transitive reach |
| ------------------------------- | ---------------: |
| `egolib.h` (uber-header)        | 57 subsystems    |
| `game.h`                        | Most game code   |
| `game/Core/GameEngine.hpp`      | Entry + states   |
| `Entities/Object.hpp`           | Physics, script, graphics, module |

`egolib.h` is still physically present and still pulls in most subsystems. Only a handful of `.c` files still include it directly.

### `idlib` as the target quality level

`idlib` itself is well-modularized into eleven sub-libraries (`idlib-math`, `idlib-filesystem`, `idlib-color`, `idlib-numeric`, `idlib-math-geometry`, `idlib-parsing-expression`, `idlib-hll`, `idlib-type`, `idlib-signal`, `idlib-document`, `idlib-chrono`). This remains the reference pattern for the eventual `egolib` decomposition.

---

## 6. SOLID Assessment (Current)

| Principle | Score | Trend | Why                                                                                                   |
| --------- | :---: | :---: | ----------------------------------------------------------------------------------------------------- |
| SRP       | 2 / 5 |   ↗   | `Object` header now 1,636 lines; file splits landed but interface still owns too many responsibilities. |
| OCP       | 2.5/5 |   →   | `GameState` hierarchy is exemplary; script dispatch and damage systems still closed to extension.     |
| LSP       |  3/5  |   →   | Shallow entity hierarchies avoid substitution problems by avoiding specialization altogether.          |
| ISP       | 2.5/5 |   ↗   | Seventeen `Object` role interfaces are now extracted; caller migration is steadily draining the god surface. |
| DIP       | 2 / 5 |   ↗   | Context wrappers are adopted and seven service seams are landed; broader singleton abstraction remains incomplete. |

### Patterns used well

- **State** — `GameState` hierarchy with a clean stack lifecycle.
- **Iterator** — `ObjectHandler::ObjectIterator` with RAII locking.
- **Composition over inheritance** — `ObjectPhysics`, `ObjectGraphics` composed inside `Object`.
- **Signal/Slot** — `idlib::signal` / `idlib::connection` for event subscription.
- **Non-copyable mixin** — consistent use of `idlib::non_copyable` on managers and handlers.

### Patterns misapplied

- **Singleton as service locator.** Still ~946 `::get()` calls (down from ~1,150), even though audio/perk/image/particle/profile now have `EngineContext` testing seams.
- **God object.** `Object` remains the primary SRP / ISP offender.
- **Anemic domain model.** `ObjectProfile` is still data + bolted-on parsing / export.

### Patterns that would add value

Factory (entity creation), Strategy (damage formulas / AI / render passes), Command (script dispatch), Service Registry / DI, Observer (game events), Builder (profile and particle init).

---

## 7. Code Cleanliness

### Naming

Three eras still coexist: legacy `snake_case` with prefixes (`chr_find_target`, `prt_find_target`), transitional `camelCase` methods layered over legacy state terminology, and modern `PascalCase` types with `camelCase` methods (`GameSessionContext`, `beginModule()`).

### Encapsulation

Passes 52–76 moved raw `Object` state behind accessor methods or narrow helpers, and passes 77 onward have expressed those narrowed surfaces as explicit role interfaces. Role interfaces now extracted: `IInventoryHolder`, `IRenderable`, `IScriptable`, `IDamageable`, `IPhysical`, `ITargetInfo`, `ICharacterState`, `ITeamMember`, `IWallet`, `IAnimationControl`, `IAppearanceProfile`, `IEnchantable`, `IMovementControl`, `IVisualControl`, `IItemInfo`, `ILifecycleControl`, and `IMorphControl` — seventeen seams that cover team, wallet, inventory, rendering, enchant/appearance, movement/animation/visuals, items, lifecycle/morph, targeting, and character-state query. The remaining `Object` surface is mostly alias-style handle returns plus mixed-domain helpers; the raw `ai_state_t` compatibility bridge has been moved into the Script subsystem as `Ego::Script::runtimeState(...)`.

### Const correctness

Still incomplete. Several logically-const accessors are declared non-const; function-parameter `const` propagation is sparse.

### Type safety

- ~200+ C-style casts remain
- Entity reference wrappers (`ObjectRef`, `PIP_REF`, `ENC_REF`) are uniformly used — strong
- `BIT_FIELD` is still a raw integer bitfield
- ~60 unscoped `enum` types survive in C++ headers

### Dead / commented-out code

- `egolib/library/src/egolib/game/Lua/` — **removed**
- `egolib/library/src/egolib/Network/` — **removed**
- `utilities/migrator/` — still present, documented as stale
- `doc/ego2xml/` — still present, documented as stale
- `backup-copy/` — read-only snapshot; not part of the build

### Include hygiene

- `egolib.h` (57 includes) still present but rarely referenced by new code
- `#pragma once` is universal (all ~340 headers)
- `GAME_ENTITIES_PRIVATE` boundary guard is consistently used

---

## 8. Build System and Cross-Platform

### Supported matrix

| Target                             | Builds | Runs        | Open-source toolchain | Documented              |
| ---------------------------------- | :----: | :---------: | :-------------------: | ----------------------- |
| Linux native (x86_64)              |  Yes   |    Yes      |         Yes           | `doc/build-linux.md`    |
| Linux-hosted Windows cross (x64)   |  Yes   | Unstable    |         Yes           | `doc/build-windows.md`  |
| Native Windows (MSYS2 / UCRT64)    |  ???   |    ???      |   Would be yes        | **Missing**             |
| Native Windows (MSVC)              | Legacy |    ???      |          No           | Deprecated              |
| macOS                              |   No   |      —      |          No           | Deprecated              |

### Strengths

- CMake-based build works on Linux and MinGW-cross
- Clean separation of `idlib`, `idlib-game-engine`, `egolib`, and `egoboo`
- Content validator integrated into the build graph
- Explicit per-subsystem source lists in `egolib/library/CMakeLists.txt`
- One working open-source toolchain file: `cmake/toolchains/mingw-w64-x86_64.cmake`

### Weaknesses

- **`egolib` is still one monolithic static library** — the explicit source lists make ownership visible but do not enforce dependency direction at link time.
- **Cartman is not in the build graph** — risks further bit-rot.
- **No native-Windows open-source build docs or toolchain file.** Only Linux-hosted cross exists.
- **Wine runtime instability** — font atlas init failure and audio loading crash (see `debug-output.txt`). `run-egoboo-windows.sh` gates with `EGOBOO_DISABLE_MIPMAPS=1` and `EGOBOO_DISABLE_AUDIO=1` as a workaround.
- **Configure-time network fetch** — `idlib/CMakeLists.txt` fetches googletest 1.16.0 from GitHub by default, breaking offline builds.
- **`FetchContent` vs. vendored googletest divergence** — `external/googletest` is present but unused by default.
- **Dual-track SDL2 dependency story** — Linux uses system packages via `pkg-config`; Windows cross uses the prebuilt `external/mingw/` bundle. The vendored `external/SDL2-*` tree (from 2014) is orphaned.
- **PhysFS version split** — `external/physfs-2.1.1` is dead weight; `idlib-game-engine/library/physfs-3.0.0` is the real one.
- **Stale CI** — `appveyor-windows.yml` still generates a Visual Studio 2017 solution, contradicting the documented mingw-w64 cross path.
- **MSVC-only CMake branches** persist (`CMakeLists.txt:51-69`, `egoboo/CMakeLists.txt:41-46`, `platform.h:125-136` pragma island).

### Proprietary-toolchain artifacts still checked in

`appveyor-windows.yml`, `egoboo.gta.runsettings`, `distribute.ps1`, `external/install-vsix-appveyor.ps1`, `external/external.sln`, `external/SDL2-2.0.3/VisualC/` (+ nested SDL libs), `README.VisualStudio`, `README.Windows`, `README.MinGW`, `README.OSX`, `osx/Egoboo.xcodeproj`.

Removing Visual Studio as a supported target would be low-risk from a source-code standpoint: the remaining MSVC-specific code is one pragma island plus two small CMake branches.

### Platform-dependent code hotspots (small, well-isolated)

- Platform detection: `idlib/library/src/idlib/platform/platform.hpp`
- File/path abstraction: `egolib/library/src/egolib/Platform/file_{linux,win,mac}.{c,mm}`
- Direct Windows API usage: only `Platform/file_win.c` and `Log/ConsoleColor.cpp` (`#ifdef _WIN32` guarded)
- Wine gates: env-var driven (`EGOBOO_DISABLE_AUDIO`, `EGOBOO_DISABLE_MIPMAPS`)
- No inline assembly, no endianness hacks, no `#pragma comment(lib, …)` in own source

---

## 9. Consolidated Scorecard

| Dimension                  | Score   | Trend | Notes                                                               |
| -------------------------- | :-----: | :---: | ------------------------------------------------------------------- |
| SRP adherence              | 2 / 5   |   ↗   | File splits landed; interface decomposition progressing via role interfaces |
| OCP adherence              | 2.5/5   |   →   | State machine is good; script/damage still closed to extension      |
| LSP adherence              |  3/5    |   →   | Shallow hierarchies, no real specialization                         |
| ISP adherence              | 2.5/5   |   ↗   | Seventeen role interfaces extracted on `Object`; migrations ongoing  |
| DIP adherence              | 2 / 5   |   ↗   | Context wrappers adopted; singleton abstraction not yet             |
| Design pattern quality     | 2.5/5   |   →   | State/Iterator well done; Factory/Strategy/Observer missing         |
| Naming consistency         | 2.5/5   |   →   | Three naming eras coexist                                            |
| Encapsulation              |  3/5    |   ↗   | Accessor closure plus broadening role extraction on `Object`        |
| Error handling             | 2.5/5   |   ↗   | Policy now written (`doc/error-handling-policy.md`); migration pending |
| Smart pointer discipline   | 2.5/5   |   →   | `shared_ptr` over-used, `unique_ptr` under-used                     |
| Test coverage              | 2.5/5   |   ↑   | From ~3.6% → ~11%; script dispatch and gameplay surfaces now covered |
| Build system               | 3.5/5   |   →   | Explicit source lists, validator integrated                         |
| Global state discipline    |  3/5    |   →   | `_currentModule`/`_gameEngine` retired; singletons declining but dense |
| File size discipline       | 3.5/5   |   →   | No file exceeds 2,500 lines; script-dispatch TUs growing within budget |
| Module boundaries          |  2/5    |   →   | One monolithic static library; directory shape is still indicative  |
| Language consistency       | 2.5/5   |   →   | C/C++ split roughly 44/56; no net C→C++ migration since last snapshot |
| Dead code hygiene          |  3/5    |   →   | Lua/Network removed; migrator/ego2xml/legacy READMEs still present  |
| Documentation              | 3.5/5   |   ↑   | Error-handling policy landed; refactoring-documents tree authoritative |
| Cross-platform parity      |  2/5    |   →   | Linux native OK; Wine cross is unstable; no native-Win open-source path |
| Third-party independence   | 2.5/5   |   →   | Network fetch at configure, dual-track SDL2, vendored PhysFS dup    |
| **Overall maintainability**| **2.5/5** | → | Unchanged score; clear interior movement since 2026-04-19           |

---

## 10. Key Strengths

1. **Global-state boundary eliminated.** `_currentModule` and `_gameEngine` are no longer direct dependencies from any runtime code.
2. **File splitting is working.** Every former oversized TU has been decomposed; no active file exceeds 2,500 lines.
3. **Encapsulation discipline is sustained.** The numbered passes show an incremental, verified path from raw field access toward explicit `Object` role seams.
4. **Game state machine is clean.** The `GameState` hierarchy remains the model of how the rest of the codebase should eventually look.
5. **Entity container is well-designed.** `ObjectHandler` with RAII iterator locking and quad-tree spatial queries is solid.
6. **Build system makes structure visible.** Explicit per-subsystem source lists are in place even though link-level modularization is still ahead.
7. **Validator exists and is integrated.** `egoboo-content-validator` provides a non-UI verification surface for content loading.
8. **`idlib` is the target pattern.** Eleven well-scoped sub-libraries demonstrate what `egolib` should eventually look like.

## 11. Key Weaknesses

1. **`Object` is still a god class by interface.** 1,636-line header and dozens of public methods despite seventeen role interfaces already extracted; caller migration is the gating work, not more interface authoring.
2. **Singleton proliferation persists.** ~946 `::get()` call sites remain. Audio/perk/image/particle/profile now have abstraction boundaries, but the broader codebase still leans on singleton lookup.
3. **No dependency injection.** Every subsystem reaches directly for concrete service classes or `EngineContext::get()`.
4. **`shared_ptr<Object>` is pervasive.** Entity ownership is shared-by-default; `enable_shared_from_this<Object>` locks this in.
5. **Error handling is inconsistent.** Exceptions, `egolib_rv`, and silent failure coexist; `doc/error-handling-policy.md` is now the written target but the migration of existing callers has not yet begun.
6. **Script system is monolithic.** ~400 script functions in procedural dispatch split across seven files with no extensibility seam.
7. **Cross-platform parity is weak at runtime.** Wine cross build is unstable; native-Windows open-source path is undocumented.
8. **Third-party dependency story is split.** Configure-time network fetch, dual-track SDL2 resolution, vendored PhysFS duplication.
9. **Test coverage is still thin in key areas.** Script dispatch, module load, gameplay alerts, and accessor regressions are covered; physics, rendering, GUI, and AI are not.
10. **`egolib` is a single static library.** Modular decomposition is expressed in directories and source-list blocks, not in link targets.

---

## 12. Immediate Next-Phase Priorities

These items compound the refactoring progress most efficiently given the current state. Each is scoped small enough to become its own numbered pass.

### Runtime and structure

1. **Continue caller migration onto the landed `Object` role seams** — expand use of `IInventoryHolder`, `IRenderable`, `IScriptable`, `IDamageable`, `IPhysical`, `ITargetInfo`, `ICharacterState`, `ITeamMember`, `IWallet`, `IAnimationControl`, `IAppearanceProfile`, `IEnchantable`, `IMovementControl`, `IVisualControl`, `IItemInfo`, `ILifecycleControl`, and `IMorphControl` instead of `Object` where only bounded role behavior is needed, then prune the redundant `Object` methods those interfaces now cover.
2. **Close the remaining alias-style handle seams on `Object`** — keep the Script-owned `runtimeState(...)` helper confined to the legacy script runtime while role extraction proceeds.
3. **Continue the service-interface layer over singletons** — `AudioSystem`, `PerkHandler`, `ImageManager`, `ParticleHandler`, `ProfileSystem`, logging, and the runtime-facing `egoboo_config_t` seam are landed; next is subsystem-local cleanup plus widening the abstractions to the remaining ~946 direct-singleton callers.
4. **Begin enforcing the error-handling policy** — `doc/error-handling-policy.md` is landed; the next step is a bounded subsystem-by-subsystem migration that retires `egolib_rv` from the C++ code paths.

### Build and cross-platform

5. **Retire the MSVC path from CI** — replace `appveyor-windows.yml`'s Visual Studio generator with mingw-w64 cross.
6. **Add a native-Windows open-source build** — `doc/build-windows-native.md` plus an MSYS2 / UCRT64 toolchain file.
7. **Eliminate configure-time network fetch** — default `idlib-with-fetch-googletest=OFF` and use the vendored `external/googletest`.
8. **Collapse third-party dependency divergence** — decide on one SDL2 story (system / MinGW bundle only), delete the orphaned `external/SDL2-*` tree, remove `external/physfs-2.1.1`.
9. **Diagnose Wine runtime blockers** — font atlas init and audio load crash — so the cross build becomes a credible verification substitute.
10. **Quarantine legacy platform READMEs** — `README.VisualStudio`, `README.Windows`, `README.MinGW`, `README.OSX` belong in `doc/legacy/` or deleted.

---

## 13. Relationship to Other Refactoring Documents

- Prioritized forward plan: `19-refactoring-roadmap.md` (supersedes the earlier `19-new-refactoring-plan.md` + `22-module-runtime-ownership-plan.md` + `25-entity-layer-decomposition-plan.md` + `33-maintainability-improvement-plan.md`)
- Strategy and non-negotiable rules for refactors: `04-refactoring-strategy.md`
- Build/run baseline (Linux): `doc/build-linux.md`
- Build/run baseline (Windows cross): `doc/build-windows.md`
- Error-handling policy (new since last snapshot): `doc/error-handling-policy.md`
- Content-validator baseline (pre-existing failures): `06-validator-baseline.md`
- Spawn / data format contracts: `08-spawn-format-spec.md`, `09-data-format-spec.md`
- Chronological record of completed passes 10–136 (runtime context, module ownership, player startup, local-stats retirement, `Object`/`ObjectGraphics` encapsulation, and the expanding `Object` role-interface extraction): `71-completed-passes-log.md`
- Meta-record of the 2026-04-18 documentation consolidation that collapsed ~50 per-pass docs and four overlapping plans: `70-documentation-consolidation.md`

The quantitative snapshots in the retired documents (17, 18, 32, 46) are preserved in git history and in `01-repository-and-build-audit.md` / the README refresh timestamps, so they remain available for trend analysis without being the active health reference.
