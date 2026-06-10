# Codebase Health Status

Consolidated, current-state health snapshot of the Egoboo codebase. Supersedes and replaces the earlier point-in-time summaries:

- `17-codebase-health-assessment.md` (2026-04-13 quantitative baseline)
- `18-modularization-analysis.md` (2026-04-13 modularization view)
- `32-project-health-and-solid-assessment.md` (2026-04-16 SOLID/design assessment)
- `46-cross-platform-and-third-party-independence-status.md` (2026-04-17 portability snapshot)

Snapshot date: 2026-06-10 (updated from the 2026-06-09, 2026-06-08, 2026-06-06, and 2026-04-20 baselines). This document is intentionally standalone — it does not cross-reference numbered passes beyond what is necessary to locate canonical plans, so it survives as a single health reference even if the individual pass documents move.

**Latest (2026-06-10) — mesh-AI terrain seam:** `AStar` and `LineOfSight` now consume the lower-layer `Ego::Mesh::ITerrainQuery` terrain interface instead of `ego_mesh_t` / `game/mesh.h`; `GameModule` implements the interface by forwarding to the existing active-module mesh surface. `AI/AStar.cpp` and `AI/LineOfSight.cpp` moved into `egolib-foundation-base`, giving the current verified layout **`egolib-foundation-base` 117 / `egolib-physics` 6 / `egolib-renderer` 29 / `egolib-library` 140**. New `AITerrainQueries.cpp` coverage pins LOS and A* blocked-tile/fan-off behavior. Gates green: build, targeted AI tests 5/5, `ar t`, aggregate nm acyclicity with live positive controls, validator `test.mod` 0/0, ctest -j1 828/830 (only the two known `ScriptLoaderFixture` failures), menu smoke exit-124 clean.

**Previous (2026-06-10) — Core/System foundation seam:** the small bootstrap edge named by the frontier-absorption pass has landed. `Core/System.cpp` no longer includes `game/Core/EngineContext.hpp`; it installs/clears the existing lower-layer `Log::activeTarget` and `activeConfig` seams directly and downloads setup into `Ego::activeConfig()`. That TU moved into `egolib-foundation-base`, giving the verified layout **`egolib-foundation-base` 115 / `egolib-physics` 6 / `egolib-renderer` 29 / `egolib-library` 142**. Gates green: build, `ar t`, aggregate nm acyclicity with live positive controls, validator `test.mod` 0/0, ctest -j1 823/825 (only the two known `ScriptLoaderFixture` failures), menu smoke exit-124 clean.

**Earlier (2026-06-09) — foundation-base growth absorptions:** two nm-pre-verified pure-CMake source-list moves grew the lowest layer: **InputControl** (3 TUs) and **Image + `Graphics/PixelFormat.cpp`** (7 TUs, which must move as a unit — Image alone has 6 `pixel_descriptor` blockers all defined in `PixelFormat.cpp`). Cumulative membership then reached **`egolib-foundation-base` 83 / `egolib-physics` 4 / `egolib-library` 205** (was 73/4/215 at the physics carve). SDL2_image needed no CMake change (`IMG_*` resolves transitively via `idlib-game-engine-library`, which the base PUBLIC-links). Each acyclic (live positive controls fired), full gate green (build / `ar t` / nm / validator 0/0 / ctest -j1 823/825 / smoke exit-124). Branches `refactor/egolib-inputcontrol-foundation-absorb`, `refactor/egolib-image-foundation-absorb`.

**Change since the foundation carve (2026-06-09):** the link-split deepened to **three layers**. The carved foundation was re-split into `egolib-foundation-base` (73 TUs) + a thin `egolib-physics` middle layer (4 TUs: the collision nucleus `Collidable`/`ICollisionWorld`/`MeshLookupTables`/`PhysicalConstants`), giving the acyclic chain **`egolib-foundation-base ← egolib-physics ← egolib-library`** — landing the long-named `egolib-physics` link target. A pure CMake partition (zero source edits), nm-verified acyclic on fresh artifacts (physics needs only base symbols + 2 intra-nucleus, 0 into the upper library; 0 base→physics back-edges; positive control fired). All gates green (`ar t` 73/4/215, validator `test.mod` 0/0, ctest -j1 823/825, smoke clean). Branch `refactor/egolib-physics-middle-carve`. See `19-refactoring-roadmap.md` and `71-completed-passes-log.md`.

**The preceding foundation carve (2026-06-09):** the **first real link-split of `egolib-library`** — a dependency-closed `egolib-foundation-library` (77 TUs: Math/Log/Mesh/VFS/Time/FileFormats/Platform + the Physics nucleus + the Script DDL/PDL lexer + Logic/TreasureTables + toplevel math/IO) that `egolib-library` (the remaining 215 TUs) depends on one-way, verified acyclic by an nm symbol-closure proof (0 cycle edges). Eight verified passes (branch `refactor/egolib-physics-nucleus-carve`); the nm proof first showed the previously-documented *nucleus-only* carve was circular, and two small seams (Time→`SDL_GetTicks()`, `ego_texture_exists_vfs` fileutil→Image) grew the closed foundation from 47→77 TUs before the `add_library` carve. (This `egolib-foundation-library` was subsequently re-split into `egolib-foundation-base` + `egolib-physics` — see the change above.)

**Changes since the 2026-06-06 snapshot:** the uber-header teardown completed and **`egolib/egolib.h` was deleted** (T3.3); `cartman` was wired into the CMake graph behind `option(EGOBOO_BUILD_CARTMAN OFF)` and now compiles/links/runs (T3.5); `vfs.c`'s dead cstdio backend was eliminated (2,456 → 1,921 lines, T3.6); the `CameraSystem` EngineContext seam landed; six T3.4 characterization batches were added (physics collision-normal, bounding-box ops, map twist, particle recoil, damage/attribute enums, and the first live-Object combat-damage batch); and the T3.7 logging-seam include-decoupling front made the `Log` subsystem a clean downward leaf (17 leaf TUs moved off `game/Core/EngineContext.hpp`). The T3.7 **service-hub** continuation (2026-06-08) then cut the non-game leaf includers of `game/Core/EngineContext.hpp` from 33 to **8** via free-function `active*()` seams (sugar over the lower-layer singleton for `profileSystem`/`imageManager`; Log-style ownership-move keystones for `config`/`particleHandler`/`audioSystem`), which also reduced egolib `::get()` sites ~895→794 — the remaining 8 are bootstrap installers or are blocked on `perkHandler`/`billboardSystem`/`fontManager` seams and the deeper Entities↔game coupling (branch `refactor/egolib-service-hub-decoupling`, not yet merged).

### Key Metrics (canonical — other docs should defer here for these volatile numbers)

Verified against the live tree on 2026-06-10. These are the single source of truth; sections below and sibling docs (`01`, `02`, `AGENTS.md`) reference this table rather than re-stating the figures.

| Metric | Value | Note |
| ------ | ----: | ---- |
| Active source files (egolib+egoboo, excl. tests) | **654** | `.c` 61 · `.cpp` 233 · `.h` 60 · `.hpp` 300 (incl. the lower-layer `egolib/Mesh/ITerrainQuery.hpp`) |
| Active source lines (egolib+egoboo) | ~122,600 | — |
| Test lines / ratio | ~21,500 / **~17.5%** | 43 test `.cpp` files, **830** ctest cases (incl. `AITerrainQueries.cpp`, `CombatDamageIntegration.cpp`, and `CollisionPipeline.cpp`) |
| ctest result | **828 / 830** | the only 2 failures are the perennial `ScriptLoaderFixture` Missing/Invalid-PrimaryScript fallback cases |
| Singleton `::get()` call sites (egolib) | **~760** | the 2026-06-08 service-hub free-fn seams cut ~100; down from ~863 (2026-06-08 pre-front) / ~912 (2026-06-06) / ~1,150 (2026-04-19) / 1,239 (baseline) |
| `EngineContext` service seams | **15** install seams (~16 services) | incl. `CameraSystem` (2026-06-07) |
| `game/Core/EngineContext.hpp` includers | 92 total, **8** non-game leaf | down from 117 / 33 (2026-06-08 service-hub front) and 51 before T3.7 |
| `Object` role interfaces | **18** | `Entities/I*.hpp` (19 `I*.hpp` files incl. the `IParticleHandler` *service* interface) |
| Largest TU | `script_functions_systems.c` **3,206** | no other TU exceeds ~1,700 |
| `Object.hpp` | **1,616** lines | monolithic by interface |
| `vfs.c` | **1,921** lines | was 2,456 before T3.6 |
| `shared_ptr` occurrences | ~1,200 | `unique_ptr` ~52, `weak_ptr` ~26 |
| `throw` sites / `try`-`catch` files | ~570 / ~35 | — |
| `TODO`/`FIXME`/`HACK` markers | ~59 | — |
| `egolib/egolib.h` | **DELETED** (T3.3, 2026-06-07) | the uber-header is gone from the active tree |
| Overall maintainability | **3 / 5** ↗ | up from 2.5 (April 2026) |

---

## 1. Executive Summary

The codebase is in an **active, well-managed transitional state**. The original C dungeon crawler is being incrementally modernized to C++, and the completed refactoring passes (dozens of in-repo passes so far) have landed real structural wins:

- All three former mutable globals (`_currentModule`, `_gameEngine`, `update_wld`) are fully retired from active runtime code. `update_wld`'s functional replacement `worldUpdateCount()` routes through `GameSessionContext` (~77 call sites across ~31 files).
- Every historically oversized translation unit has been file-split. The largest TU is `script_functions_systems.c` at ~3,200 lines; no other exceeds ~1,700 (see the Key Metrics table).
- File splitting, context wrappers, and accessor encapsulation work have progressed through role extraction on the `Object` god class (18 role interfaces) and deep singleton-seam work on `EngineContext` (15 service interfaces).
- A native validator tool exists, content parser tests exist, and module load/spawn smoke tests exist.

Core design debt that remains:

- The `Object` class is still monolithic by interface — `Object.hpp` is now ~1,617 lines. Its implementation is split across seven files (`Object.cpp` plus six `Object_{appearance,attributes,combat,interaction,lifecycle,update}.cpp` TUs), and the role-extraction passes have peeled off 18 role interfaces: `IInventoryHolder`, `IRenderable`, `IScriptable`, `IDamageable`, `IPhysical`, `ITargetInfo`, `ICharacterState`, `ITeamMember`, `IWallet`, `IAnimationControl`, `IAppearanceProfile`, `IEnchantable`, `IMovementControl`, `IVisualControl`, `IItemInfo`, `ILifecycleControl`, `IMorphControl`, and `IProfiled`. Single-param role-narrowing has reached its ceiling; remaining coupling is intrinsic to multi-role functions.
- Singleton access is still pervasive (~863 `::get()` call sites, down from ~1,150 at 2026-04-19), and the `EngineContext` service-interface layer now covers audio, perk, image, particle, profile, logging, runtime `egoboo_config_t`, font, input, graphics system, texture manager, texture atlas, GFX, billboard system, and camera system; full DI still does not exist.
- Error handling still mixes C++ exceptions, `egolib_rv` return codes, and silent failure — but a written policy is now published at `doc/error-handling-policy.md` and new code is being held to it.
- The Linux-hosted Windows cross build is unstable at runtime (font atlas / audio crash under Wine); the native-Windows open-source path is undocumented.

Overall maintainability: **3 / 5**, up from 2.5 at the April 2026 baseline. All three mutable globals retired, eighteen role interfaces extracted (single-param narrowing ceiling reached), fifteen service seams landed on `EngineContext`, singleton sites down ~30% (1,239→~863), test coverage up to ~17.5%, the uber-header `egolib.h` deleted, `cartman` wired into the build (gated off), orphaned third-party deps removed, MSVC CMake branches dropped. The trend is unambiguously positive; the remaining frontier is multi-role Object decoupling, broader DI, upper-archive splitting, and fixture isolation.

---

## 2. Size and Composition

### Source files (egolib + egoboo; cartman is present but not in the main build)

| Category                         | Count |
| -------------------------------- | ----: |
| C implementation files (`.c`)    |    61 |
| C++ implementation files (`.cpp`)|   233 |
| C headers (`.h`)                 |    60 |
| C++ headers (`.hpp`)             |   300 |
| **Total active source files**    |**654**|

The `.c` count drifted down from 70 (2026-04-19) as small C utilities were folded into their C++ neighbors; `.h` dropped similarly as legacy headers were deleted. `.cpp` and `.hpp` growth reflects the new role-interface headers and their implementation seams.

### Lines of code

| Area                                                          | Approx. Lines |
| ------------------------------------------------------------- | ------------: |
| `egolib` C sources (`.c`)                                     |        36,100 |
| `egolib` C++ sources (`.cpp`)                                 |        46,561 |
| `egolib` + `egoboo` headers (combined)                        |        39,977 |
| `egoboo/src/` (thin executable)                               |           ~90 |
| Active source + headers total (egolib + egoboo)               |      **122,638** |
| `egolib/tests/`                                               |       ~21,500 |

The C vs. C++ split by implementation-file line count is roughly 44% C / 56% C++, essentially flat against the prior snapshot. The aggregate source line count dropped slightly even while role interfaces grew, indicating that the role passes are net-simplifying at the callsite despite adding new headers.

### Test-to-code ratio

**~21,500** test lines against ~122,600 active source lines = **~17.5%** (43 test `.cpp` files, 830 ctest cases). This is a large jump from the ~3.6% figure at the April baseline and crosses the threshold at which tests start protecting behavior rather than only compilation. The jump came from bringing script-dispatch, gameplay, physics/collision math, combat-damage, collision-pipeline, and AI terrain-query surfaces under test rather than from raw parser coverage growth.

Current test files under `egolib/tests/egolib/tests/`:

- `Compilation.cpp`, `StringUtilities.cpp`, `QuadTree.cpp`, `MeshInfoIterator.cpp`, `AITerrainQueries.cpp` — utility and AI terrain-query coverage
- `ContentParsers.cpp`, `SpawnName.cpp` — parser coverage
- `ConfigReadMostly.cpp`, `ConfigMutations.cpp`, `GameText.cpp` — configuration / text subsystem coverage
- `ModuleLoadSmoke.cpp`, `ModuleSpawnPlanning.cpp`, `ModuleSpawnRealization.cpp`, `ModulePlayerStartup.cpp`, `ModuleUpdate.cpp` — module-loading and update smoke coverage
- `LoadPlayerElement.cpp`, `PlayerQuestLog.cpp` — player startup / quest hydration coverage
- `EngineContext.cpp` — engine-context seam coverage
- `ObjectAccessors.cpp`, `ObjectHandlerQueries.cpp` — accessor-encapsulation regression and object-handler query coverage
- `CameraTracking.cpp` — camera-tracking behavior coverage
- `ImportWorkflow.cpp` — character import/export workflow coverage
- `ScriptLoader.cpp`, `ScriptRuntime.cpp`, `ScriptActionFunctions.cpp`, `ScriptMovementFunctions.cpp`, `ScriptStateFunctions.cpp`, `ScriptSystemsFunctions.cpp`, `ScriptTargetFunctions.cpp`, `ScriptVariables.cpp`, `ScriptBitwiseFunctions.cpp` — script-loader / VM / dispatch behavior coverage (`ScriptLoader` holds the 2 perennial PrimaryScript-fallback failures)
- `GameplayAlertPublication.cpp`, `ShopInteractions.cpp` — gameplay-level behavior coverage
- `PhysicsIntersection.cpp`, `PhysicsCollisionNormal.cpp`, `BoundingBox.cpp`, `BoundingBoxOps.cpp`, `ParticleRecoil.cpp` — physics / collision / bounding-volume math characterization (T3.4)
- `MapTwist.cpp`, `LogicDamageAttribute.cpp` — map twist↔normal math and Damage/Attribute enum-mapping characterization (T3.4)
- `CombatDamageResolution.cpp` — combat damage-resolution math on a **live spawned `Object`** (resistance/reduction/invictus); the first live-Object-fixture characterization batch (T3.4, 2026-06-08)
- `CombatDamageIntegration.cpp` — the integrated `Object::damage(...)`/`kill(...)` side-effect chain on a **live module-spawned `Object`** (life subtraction, hurt/careful timers, attack alert, lethal kill, invictus/dead/zero guards); gates the `ICollidable` extraction (T3.4, 2026-06-08)
- `CollisionPipeline.cpp` — live-spawn collision-pipeline characterization for `do_chr_prt_collision` and `CollisionSystem::detectCollision` gates (T3.4, 2026-06-09)
- `math/` — math submodule tests

Pure physics/collision math (intersection, swept bounds, collision normals, oct-box ops, recoil), map twist math, Damage/Attribute enum logic, combat damage-resolution/integration, live collision pipeline behavior, and the AI LOS/pathing terrain-query contract are covered by the characterization batches; script-VM, module-load, accessor, and gameplay-alert surfaces are partially covered. Notably still absent: rendering correctness tests, GUI tests, and broader AI behavior beyond LOS/pathing terrain queries.

---

## 3. Hotspot Files

### Files over 1,000 lines (active tree)

Fourteen files remain over the 1k-line threshold. `script_functions.c` (8,183 lines) and `Object.cpp` (3,201 lines) have been split. `Object.hpp` is the surviving "large header." `script_functions_systems.c` has grown to become the largest TU in the tree as role-extraction helpers have been moved in. `vfs.c` dropped from 2,456 to 1,922 lines (~22%) when its dead cstdio backend was eliminated (2026-06-07, three passes — see `71-completed-passes-log.md`).

| File                                              |  Lines | Role                                       |
| ------------------------------------------------- | -----: | ------------------------------------------ |
| `game/script_functions_systems.c`                 |  3,206 | Script dispatch — systems (largest TU)     |
| `egolib/vfs.c`                                    |  1,922 | Virtual file system (PHYSFS-only since the cstdio-backend elimination) |
| `game/script_functions_target.c`                  |  1,676 | Script dispatch — target                   |
| `Entities/Object.hpp`                             |  1,617 | Core entity — still monolithic by interface |
| `game/script_functions_spawn.c`                   |  1,576 | Script dispatch — spawn                    |
| `game/Physics/particle_collision.c`               |  1,525 | Particle collision                         |
| `game/Graphics/ObjectGraphics.cpp`                |  1,487 | Object rendering                           |
| `game/script_functions_state.c`                   |  1,478 | Script dispatch — state                    |
| `game/mesh.c`                                     |  1,370 | Mesh management                            |
| `Script/script.c`                                 |  1,371 | Script runtime                             |
| `fileutil.c`                                      |  1,327 | File utilities                             |
| `game/script_compile.c`                           |  1,148 | Script compiler                            |
| `game/script_functions_action.c`                  |  1,100 | Script dispatch — action                   |
| `game/Physics/ObjectPhysics.cpp`                  |  1,109 | Object physics                             |

The split script-dispatch TUs have continued to grow as helpers have been moved in from `Object` and friends rather than authored from scratch — a consequence of role extraction, not a regression. No individual file exceeds 3,300 lines.

### Files that have been decomposed

| Former monolith                    | Prior Size | Current Split                                                                                                        |
| ---------------------------------- | ---------: | ------------------------------------------------------------------------------------------------------------------ |
| `script_functions.c`               |      8,183 | Seven files: `script_functions_{action,bitwise,movement,spawn,state,systems,target}.c`                               |
| `Entities/Object.cpp`              |      3,201 | Seven files: `Object.cpp` (195) + `Object_{appearance,attributes,combat,interaction,lifecycle,update}.cpp`           |
| `game/game.c`                      |      2,456 | Six files: `game.c` (548) + `game_{combat,export,loop,targeting,wawalite}.c`                                         |
| `Profiles/ObjectProfile.cpp`       |      1,468 | Three files: `ObjectProfile_{core,load,export}.cpp`                                                                  |
| `Entities/Particle.cpp`            |      1,447 | Five files: `Particle_{core,combat,spawn,update}.cpp` + `ParticleHandler.cpp`                                        |
| `game/Module/Module.cpp`           |      1,225 | Seven files under `Module/`: `Module.cpp` (135) + `Module_{bootstrap,loading,spawn,spawn_plan,spawn_realization,update}.cpp` |

These splits made compilation and navigation tractable, but they did not finish the interface problem. `Object.hpp` has stabilized around ~1,617 lines (down from the 1,636 peak as some legacy surface has been pruned). Single-param role-narrowing has reached its ceiling (Pass 220); the remaining `Object` decoupling requires multi-role strategies.

### Deeply nested / switch-heavy regions

The former deep-nesting concentration in `script_functions.c` is now distributed across the seven split files. Switch-statement density remains high in script dispatch and game logic (on the order of 100+ `switch` statements in `egolib`). This has not meaningfully improved and will only fully resolve once the script system moves to a registry model (plan item T3.2).

---

## 4. Global State and Coupling

### Direct global runtime-state references

| Global           | 2026-04-12 baseline | Current |
| ---------------- | ------------------: | ------: |
| `_currentModule` |                 592 |       0 |
| `_gameEngine`    |                 266 |       0 |
| `update_wld`     |                  65 |       0 |

All three former mutable globals are fully retired. `_currentModule` and `_gameEngine` have no references anywhere. `update_wld` has a few stale string-literal / comment artifacts (a format string in `script.c`, Doxygen comments in `ObjectGraphics.hpp` and `Particle.hpp`) but zero active variable references. Its functional replacement `worldUpdateCount()` routes through `GameSessionContext` with ~77 call sites across ~31 files.

This is the largest single structural win since the baseline.

### Singleton and service-locator access

Raw `::get()` singleton calls now number approximately **863** across egolib, down from ~912 at 2026-06-06, ~946 at 2026-04-20, ~1,150 at 2026-04-19, and 1,239 at the April 13 baseline. The decline reflects genuine migration onto `EngineContext`-published services.

Engine-published service seams landed so far:

- `IAudioSystem`, `IPerkHandler`, `IImageManager`, `IParticleHandler`, `IProfileSystem`, `IFontManager`, `IInputSystem`, `IGraphicsSystem`, `ITextureManager`, `ITextureAtlasManager`, `IGFX`, `IBillboardSystem`, and `ICameraSystem` are now published through `EngineContext`, with non-subsystem callers migrated off the concrete singleton lookups.
- Runtime logging routes through the installed `EngineContext` log target outside the `Log` subsystem's bootstrap/lifecycle code.
- `egoboo_config_t` is published through `EngineContext` for bootstrap/lifecycle paths, module-load sync, lightweight content bootstrap, and the cross-cutting runtime/UI caller set.
- Clock callers decoupled from `Core::System` via the `Time` abstraction (Pass 216).

Dominant direct singletons still reachable outside the session/engine wrappers:

- `egoboo_config_t::get()` — confined to subsystem-local bootstrap/lifecycle or singleton-definition code.
- `AudioSystem::get()`, `PerkHandler::get()`, `ImageManager::get()`, `ParticleHandler::get()`, `ProfileSystem::get()`, and `Log::get()` remain as subsystem-local bootstrap/lifecycle seams inside their own implementations.
- `Renderer` — **deferred**: already an abstract polymorphic facade with low value/high churn for an EngineContext seam.
- `CameraSystem` — **DONE** (2026-06-07): `ICameraSystem` widened with `getMainCamera`/`getCamera`/`getCameraOptions`/`renderAll`; clean game-layer `.cpp` consumers migrated to `EngineContext::get().cameraSystem()`. Remaining `CameraSystem::get()` sites are principled exceptions (install, `AudioSystem` ×4 layer-inversion, `Object_appearance` ×2 `is_initialized()`-guard, 10 deferred `.c`).

### Smart pointer distribution

| Pattern         | Count (active code) | Note                                                              |
| --------------- | ------------------: | ----------------------------------------------------------------- |
| `shared_ptr`    |              ~1,250 | Still over-used; `Object` inherits from `enable_shared_from_this` |
| `unique_ptr`    |                 ~52 | Still under-used                                                  |
| `weak_ptr`      |                 ~26 | Appropriate use for back-references                               |
| Raw `new`/`delete` |              ~220 | Concentrated in C-era code and some legacy C++                  |

The raw occurrence count for `shared_ptr` in headers and source combined is higher than previously reported because the new role-interface headers forward handles through the codebase. The `shared_ptr<Object>` pattern remains the single most ownership-opaque idiom in the codebase, and would take a dedicated pass to address safely.

### Error handling

Two strategies still coexist (the `egolib_rv` return-code strand is effectively gone):

- C++ exceptions — ~240 `throw` sites, `try`/`catch` in ~54 files
- Silent failure via `nullptr`/`false` returns
- ~~C return codes — `egolib_rv`~~ **effectively retired (2026-06-09 verification):** only 3 occurrences remain in 2 files — the enum in `typedef.h` and the `gfx_rv` graphics-typedef alias in `game/egoboo.h`; no `egolib_rv` *uses* remain in the C++ code paths.

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
| `egolib-foundation-base`     | Static library       | Dependency-closed lowest layer (117 TUs): Math, Log, Mesh, VFS, Time, Core/System bootstrap, FileFormats, Platform, InputControl, Image, AI path/LOS terrain queries, Profiles data/writers, MD2 + texture/font clusters, Script DDL/PDL lexer, Logic/Perk/TreasureTables, toplevel math/IO — verified acyclic | ~31,000 |
| `egolib-physics`             | Static library       | Thin middle layer (6 TUs): collision nucleus plus `physics.c` and `Entities/Common.cpp` — depends one-way on `egolib-foundation-base` | ~1,000 |
| `egolib-renderer`            | Static library       | Renderer middle layer (29 TUs): SDL display/windowing and OpenGL backend — sibling of `egolib-physics`, depends one-way on `egolib-foundation-base` | ~10,000 |
| `egolib-library`             | Static library       | The rest of egolib (140 TUs): Entities/game core, the EgoScript VM, and remaining higher gameplay/UI code — depends one-way on `egolib-physics` and `egolib-renderer` | ~78,000  |
| `egoboo`                     | Executable           | Thin entry point                                                                                     |       90 |
| `egoboo-content-validator`   | Executable (tool)    | Content validation tool                                                                              |   ~1,200 |
| `cartman`                    | Gated off by default | Map editor — wired into CMake behind `option(EGOBOO_BUILD_CARTMAN OFF)`; compiles/links/runs when ON; not in the default build (T3.5) | ~9,300 |

### `egolib` internal structure — now made explicit in CMake

The historical `GLOB_RECURSE` in `egolib/library/CMakeLists.txt` has been replaced with explicit, per-subsystem source lists (one `set()` block per directory, grouped alphabetically). Ownership is visible in the build system, and as of 2026-06-10 the build enforces an acyclic four-archive DAG: `egolib-foundation-base` (117 TUs) ← sibling middle layers `egolib-physics` (6 TUs) and `egolib-renderer` (29 TUs) ← `egolib-library` (140 TUs). The dependency direction is nm symbol-closure verified with live positive controls.

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
3. **Profiles → Runtime services.** ~~`ObjectProfile_load.cpp` pulls `PerkHandler`, `ImageManager`, and `ProfileSystem` singletons during parsing.~~ **Largely addressed (2026-06-09 verification):** `ObjectProfile_load.cpp`/`ObjectProfile_export.cpp` now reach these through the lower-layer `active*()` seams (`Perks::activePerkHandler()`, `activeProfileSystem()`, `Log::activeTarget()`, `Ego::activeConfig()`, `tryActiveAudioSystem()`), not raw `::get()`. Parsing still *uses* runtime services, but the singleton coupling is seamed.
4. **FileFormats → Runtime services.** **Not actually a singleton violation (2026-06-09 verification):** `FileFormats/*.cpp` have zero `::get()`; the VFS dependency is a legitimate file-I/O abstraction, not runtime-service coupling.
5. **Entities ↔ Game Module.** The structural cycle at the runtime-ownership layer has been broken by the session/engine context. The `Collidable` base class (a major Entities→Module edge, inherited by both `Object` and `Particle`) is now fully decoupled and relocated to the lower layer (`egolib/Physics/`, behind the `ICollisionWorld` seam, 2026-06-08); `Object.hpp`/`Particle.hpp` game/ transitive closures are down to 4/3 (the by-value composition members). The four `game/Physics/` TUs are now also decoupled from `GameModule`'s object container: they reach it through the lower-layer `Ego::Entities::IObjectWorld` seam (2026-06-09), so `game/Module/Module.hpp` is gone from all four. The remaining Entity↔game coupling is the non-propagating internal headers + impl `.cpp`s, plus the residual `worldUpdateCount()` (`GameSessionContext`) and `game/physics.h` free-function edges on the physics TUs.

### "Gravity well" headers

| Header                          | Transitive reach |
| ------------------------------- | ---------------: |
| `game.h`                        | Most game code   |
| `game/Core/GameEngine.hpp`      | Entry + states   |
| `Entities/Object.hpp`           | Physics, script, graphics, module |

The former top "gravity well", the `egolib/egolib.h` uber-header (54–57 subsystem includes), was **deleted in the T3.3 uber-header teardown** (2026-06-07); its `game/egoboo.h → egolib.h` amplifier link was cut first, then the header itself removed. It has zero includers anywhere in the tree (the last dangling reference, in the unbuilt `utilities/migrator/src/Tool.hpp`, was replaced with precise standard-library includes on 2026-06-08). The remaining gravity wells are `game.h`, `GameEngine.hpp`, and `Object.hpp`.

### `idlib` as the target quality level

`idlib` itself is well-modularized into eleven sub-libraries (`idlib-math`, `idlib-filesystem`, `idlib-color`, `idlib-numeric`, `idlib-math-geometry`, `idlib-parsing-expression`, `idlib-hll`, `idlib-type`, `idlib-signal`, `idlib-document`, `idlib-chrono`). This remains the reference pattern for the eventual `egolib` decomposition.

---

## 6. SOLID Assessment (Current)

| Principle | Score | Trend | Why                                                                                                   |
| --------- | :---: | :---: | ----------------------------------------------------------------------------------------------------- |
| SRP       | 2 / 5 |   ↗   | `Object` header now ~1,617 lines; file splits landed but interface still owns too many responsibilities. |
| OCP       | 2.5/5 |   →   | `GameState` hierarchy is exemplary; script dispatch and damage systems still closed to extension.     |
| LSP       |  3/5  |   →   | Shallow entity hierarchies avoid substitution problems by avoiding specialization altogether.          |
| ISP       |  3/5  |   ↗   | Eighteen `Object` role interfaces extracted; single-param narrowing has reached its ceiling — remaining coupling is multi-role. |
| DIP       | 2.5/5 |   ↗   | Context wrappers adopted and fifteen service seams landed; broader singleton abstraction progressing (~863 `::get()` sites). |

### Patterns used well

- **State** — `GameState` hierarchy with a clean stack lifecycle.
- **Iterator** — `ObjectHandler::ObjectIterator` with RAII locking.
- **Composition over inheritance** — `ObjectPhysics`, `ObjectGraphics` composed inside `Object`.
- **Signal/Slot** — `idlib::signal` / `idlib::connection` for event subscription.
- **Non-copyable mixin** — consistent use of `idlib::non_copyable` on managers and handlers.

### Patterns misapplied

- **Singleton as service locator.** Still ~863 `::get()` calls (down from ~1,150), even though fifteen services now have `EngineContext` testing seams.
- **God object.** `Object` remains the primary SRP / ISP offender.
- **Anemic domain model.** `ObjectProfile` is still data + bolted-on parsing / export.

### Patterns that would add value

Factory (entity creation), Strategy (damage formulas / AI / render passes), Command (script dispatch), Service Registry / DI, Observer (game events), Builder (profile and particle init).

---

## 7. Code Cleanliness

### Naming

Three eras still coexist: legacy `snake_case` with prefixes (`chr_find_target`, `prt_find_target`), transitional `camelCase` methods layered over legacy state terminology, and modern `PascalCase` types with `camelCase` methods (`GameSessionContext`, `beginModule()`).

### Encapsulation

Passes 52–76 moved raw `Object` state behind accessor methods or narrow helpers, and passes 77 onward have expressed those narrowed surfaces as explicit role interfaces. Role interfaces now extracted: `IInventoryHolder`, `IRenderable`, `IScriptable`, `IDamageable`, `IPhysical`, `ITargetInfo`, `ICharacterState`, `ITeamMember`, `IWallet`, `IAnimationControl`, `IAppearanceProfile`, `IEnchantable`, `IMovementControl`, `IVisualControl`, `IItemInfo`, `ILifecycleControl`, `IMorphControl`, and `IProfiled` — eighteen seams that cover team, wallet, inventory, rendering, enchant/appearance, movement/animation/visuals, items, lifecycle/morph, targeting, character-state query, and profile access. Single-param role-narrowing has reached its ceiling (Pass 220 analysis); remaining coupling is intrinsic to multi-role functions. The raw `ai_state_t` compatibility bridge has been moved into the Script subsystem as `Ego::Script::runtimeState(...)`.

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
- `doc/ego2xml/` — an abandoned 2015 XML-content proposal; **archived to `doc/legacy/ego2xml/`** (with a `DEPRECATED.md`)
- `backup-copy/` — read-only snapshot; not part of the build

### Include hygiene

- The `egolib.h` uber-header is **deleted** (T3.3); includes are now precise. `game/egoboo.h` survives as a thin header (no longer pulls in `egolib.h`)
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

- **`egolib` is now four static archives, not one** — the dependency-closed lower layer is carved into `egolib-foundation-base` (117 TUs) with sibling middle layers `egolib-physics` (6 TUs) and `egolib-renderer` (29 TUs), all feeding `egolib-library` (140 TUs), verified acyclic. The *higher* subsystems (Entities, game core, the EgoScript VM, and remaining gameplay/UI code) still share the single upper archive and do not yet enforce dependency direction among themselves at link time — further sub-libraries remain ahead.
- **Cartman is gated off by default** (`option(EGOBOO_BUILD_CARTMAN OFF)`) — now in the CMake graph and building/running when enabled, but excluded from the default build and CI, so still at some bit-rot risk.
- **No native-Windows open-source build docs or toolchain file.** Only Linux-hosted cross exists.
- **Wine runtime instability** — font atlas init failure and audio loading crash. `run-egoboo-windows.sh` gates with `EGOBOO_DISABLE_MIPMAPS=1` and `EGOBOO_DISABLE_AUDIO=1` as a workaround.
- **Stale CI** — `appveyor-windows.yml` still generates a Visual Studio 2017 solution, contradicting the documented mingw-w64 cross path. `external/install-vsix-appveyor.ps1` and `external/external.sln` remain in the `external` submodule.

### Proprietary-toolchain artifacts still checked in

`appveyor-windows.yml`, `external/install-vsix-appveyor.ps1`, `external/external.sln`, `osx/Egoboo.xcodeproj`.

Previously checked in but now removed or quarantined: `egoboo.gta.runsettings`, `distribute.ps1` (removed by T2.6/T2.1); `external/SDL2-2.0.3/`, `external/physfs-2.1.1` (removed by T2.4); `README.VisualStudio`, `README.Windows`, `README.MinGW`, `README.OSX` (quarantined to `doc/legacy/` by T2.6); MSVC-only CMake branches and `platform.h` pragma island (removed by T2.1).

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
| ISP adherence              |  3/5    |   ↗   | Eighteen role interfaces extracted on `Object`; single-param narrowing ceiling reached |
| DIP adherence              | 2.5/5   |   ↗   | Context wrappers adopted; fifteen service seams landed; ~863 `::get()` sites remain |
| Design pattern quality     | 2.5/5   |   →   | State/Iterator well done; Factory/Strategy/Observer missing         |
| Naming consistency         | 2.5/5   |   →   | Three naming eras coexist                                            |
| Encapsulation              |  3/5    |   ↗   | Accessor closure plus broadening role extraction on `Object`        |
| Error handling             | 2.5/5   |   ↗   | Policy now written (`doc/error-handling-policy.md`); migration pending |
| Smart pointer discipline   | 2.5/5   |   →   | `shared_ptr` over-used, `unique_ptr` under-used                     |
| Test coverage              |  3/5    |   ↑   | From ~3.6% → ~17.5%; script dispatch, gameplay, physics/collision math, live-Object combat damage, collision pipeline, and AI terrain queries now covered |
| Build system               | 3.5/5   |   ↗   | Explicit source lists, validator integrated, four-archive DAG (`egolib-foundation-base` ← `{egolib-physics, egolib-renderer}` ← `egolib-library`, acyclic) |
| Global state discipline    | 3.5/5   |   ↗   | All three mutable globals retired; singletons down to ~863          |
| File size discipline       | 3.5/5   |   →   | Largest TU is ~3,200 lines; script-dispatch TUs growing within budget |
| Module boundaries          | 2.5/5   |   ↗   | Four-archive DAG landed (`egolib-foundation-base` 117 ← `{egolib-physics 6, egolib-renderer 29}` ← `egolib-library` 140, acyclic); higher gameplay subsystems still share one upper archive |
| Language consistency       | 2.5/5   |   →   | C/C++ split roughly 44/56; no net C→C++ migration since last snapshot |
| Dead code hygiene          | 3.5/5   |   ↗   | Lua/Network removed; legacy READMEs + ego2xml quarantined to `doc/legacy/`; orphaned SDL2/physfs deleted; `utilities/migrator` marked deprecated |
| Documentation              | 3.5/5   |   ↑   | Error-handling policy landed; refactoring-documents tree authoritative |
| Cross-platform parity      |  2/5    |   →   | Linux native OK; Wine cross is unstable; no native-Win open-source path |
| Third-party independence   | 3.5/5   |   ↗   | Network fetch eliminated; orphaned SDL2/PhysFS removed; single SDL2 story per platform |
| **Overall maintainability**| **3/5** | ↗ | Meaningful forward motion: globals retired, 18 role interfaces, 14 service seams, T2 build cleanup |

---

## 10. Key Strengths

1. **Global-state boundary eliminated.** `_currentModule` and `_gameEngine` are no longer direct dependencies from any runtime code.
2. **File splitting is working.** Every former oversized TU has been decomposed; the largest is now `script_functions_systems.c` (~3,200 lines) and no other exceeds ~1,700.
3. **Encapsulation discipline is sustained.** The numbered passes show an incremental, verified path from raw field access toward explicit `Object` role seams.
4. **Game state machine is clean.** The `GameState` hierarchy remains the model of how the rest of the codebase should eventually look.
5. **Entity container is well-designed.** `ObjectHandler` with RAII iterator locking and quad-tree spatial queries is solid.
6. **Build system makes structure visible — and now enforces a four-archive DAG.** Explicit per-subsystem source lists plus the carved `egolib-foundation-base` ← `{egolib-physics, egolib-renderer}` ← `egolib-library` DAG (dependency-closed and verified acyclic) mean dependency direction is enforced at link time for the lower layers; further sub-libraries are still ahead.
7. **Validator exists and is integrated.** `egoboo-content-validator` provides a non-UI verification surface for content loading.
8. **`idlib` is the target pattern.** Eleven well-scoped sub-libraries demonstrate what `egolib` should eventually look like.

## 11. Key Weaknesses

1. **`Object` is still a god class by interface.** ~1,617-line header despite eighteen role interfaces already extracted; single-param narrowing has reached its ceiling — remaining coupling is multi-role. Further decoupling requires interface pollution or multi-param strategies.
2. **Singleton proliferation persists.** ~863 `::get()` call sites remain. Fifteen services now have `EngineContext` abstraction boundaries, but subsystem-local bootstrap seams and the deferred Renderer keep the count high.
3. **No dependency injection.** Every subsystem reaches directly for concrete service classes or `EngineContext::get()`.
4. **`shared_ptr<Object>` is pervasive.** Entity ownership is shared-by-default; `enable_shared_from_this<Object>` locks this in.
5. **Error handling is inconsistent.** Exceptions, `egolib_rv`, and silent failure coexist; `doc/error-handling-policy.md` is now the written target but the migration of existing callers has not yet begun.
6. **Script system is monolithic.** ~400 script functions in procedural dispatch split across seven files with no extensibility seam.
7. **Cross-platform parity is weak at runtime.** Wine cross build is unstable; native-Windows open-source path is undocumented.
8. **Test coverage is still thin in key areas.** Script dispatch, module load, gameplay alerts, accessor regressions, physics/collision math, live-Object combat damage, collision pipeline behavior, and AI LOS/pathing terrain queries are covered; rendering, GUI, and broader AI behavior are not.
9. **`egolib` is now four static archives (was one).** The dependency-closed lower layers are real link targets (`egolib-foundation-base` with `egolib-physics` and `egolib-renderer` above it); the higher gameplay subsystems still share `egolib-library`, so much of the modular decomposition is still expressed only in directories and source-list blocks.
10. **Stale CI.** `appveyor-windows.yml` still generates a Visual Studio 2017 solution.

---

## 12. Immediate Next-Phase Priorities

These items compound the refactoring progress most efficiently given the current state. Each is scoped small enough to become its own numbered pass.

### Runtime and structure

1. **Object role-decoupling: next frontier** — single-param narrowing is exhausted (Pass 220 analysis). Next value is either: (a) interface pollution — co-locating `getProfile()` onto existing narrow interfaces to unlock multi-role callers, or (b) multi-param strategies, or (c) the deferred `IMatrixCacheControl` interface for the matrix-cache render-path surface.
2. **Continue the service-interface layer over singletons** — fifteen services are seamed; next candidates are subsystem-local cleanup and the deferred Renderer (if interface split becomes worthwhile). ~863 `::get()` sites remain.
3. **Begin enforcing the error-handling policy** — `doc/error-handling-policy.md` is landed; the next step is a bounded subsystem-by-subsystem migration that retires `egolib_rv` from the C++ code paths.

### Build and cross-platform

4. **Retire the MSVC path from CI** — replace `appveyor-windows.yml`'s Visual Studio generator with mingw-w64 cross; remove `external/install-vsix-appveyor.ps1` and `external/external.sln`.
5. **Add a native-Windows open-source build** — `doc/build-windows-native.md` plus an MSYS2 / UCRT64 toolchain file.
6. **Diagnose Wine runtime blockers** — font atlas init and audio load crash — so the cross build becomes a credible verification substitute.

~~7. Eliminate configure-time network fetch~~ — **DONE** (T2.3, commit `12bd9463e`).
~~8. Collapse third-party dependency divergence~~ — **DONE** (T2.4, commit `cb836a2f5`).
~~9. Quarantine legacy platform READMEs~~ — **DONE** (T2.6, commit `63530491d`).

---

## 13. Relationship to Other Refactoring Documents

- Prioritized forward plan: `19-refactoring-roadmap.md` (supersedes the earlier `19-new-refactoring-plan.md` + `22-module-runtime-ownership-plan.md` + `25-entity-layer-decomposition-plan.md` + `33-maintainability-improvement-plan.md`)
- Strategy and non-negotiable rules for refactors: `04-refactoring-strategy.md`
- Build/run baseline (Linux): `doc/build-linux.md`
- Build/run baseline (Windows cross): `doc/build-windows.md`
- Error-handling policy (new since last snapshot): `doc/error-handling-policy.md`
- Content-validator baseline (pre-existing failures): `06-validator-baseline.md`
- Spawn / data format contracts: `08-spawn-format-spec.md`, `09-data-format-spec.md`
- Chronological record of all completed passes (runtime context, module ownership, player startup, local-stats retirement, `Object`/`ObjectGraphics` encapsulation, role-interface extraction, the uber-header teardown through `egolib.h` deletion, the vfs cstdio elimination, cartman build integration, the T3.4 characterization batches, and the T3.7 logging-seam decoupling): `71-completed-passes-log.md`
- Meta-record of the 2026-04-18 documentation consolidation that collapsed ~50 per-pass docs and four overlapping plans: `70-documentation-consolidation.md`

The quantitative snapshots in the retired documents (17, 18, 32, 46) are preserved in git history and in `01-repository-and-build-audit.md` / the README refresh timestamps, so they remain available for trend analysis without being the active health reference.
