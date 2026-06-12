# Codebase Health Status

Consolidated, current-state health snapshot of the Egoboo codebase. Supersedes and replaces the earlier point-in-time summaries:

- `17-codebase-health-assessment.md` (2026-04-13 quantitative baseline)
- `18-modularization-analysis.md` (2026-04-13 modularization view)
- `32-project-health-and-solid-assessment.md` (2026-04-16 SOLID/design assessment)
- `46-cross-platform-and-third-party-independence-status.md` (2026-04-17 portability snapshot)

Snapshot date: 2026-06-12 (updated from the 2026-06-11, 2026-06-10, 2026-06-09, 2026-06-08, 2026-06-06, and 2026-04-20 baselines). This document is intentionally standalone — it does not cross-reference numbered passes beyond what is necessary to locate canonical plans, so it survives as a single health reference even if the individual pass documents move.

**Latest (2026-06-12) — three within-layer file-splits land the scout's "go" trio:** an 11-agent ultracode scout-and-refute workflow ranked five candidates (10th-archive carve, Object.hpp split, script_functions_spawn 3-way, particle_collision split, vfs_search final slice); three came back as `go` and all three executed and merged the same day. None crossed an archive boundary — these were navigability splits, not link-graph restructures.

1. **`script_functions_spawn.c` 3-way split (merge `e6aaf1291`):** the 1576-line file (the largest .c TU after the prior `script_functions_systems.c` decomposition) split into `script_functions_spawn.c` (629, residual 24 lifecycle/drop/cleanup entries), `script_functions_spawn_character.c` (458, 8 character spawn/respawn), `script_functions_spawn_particle.c` (463, 12 particle spawn/poof) + private `script_functions_spawn_internal.h` (74, shared `SpawnSelfContext` / `makeSpawnSelfContext` / `gameSession()` / `isLiveSpawnObjectRef` via a `script_spawn_detail` namespace mirroring the existing `script_functions_internal.h` / `script_detail` pattern). All three TUs stay in `egolib-scriptvm`. No header changes (script_functions.h already declares all 44 dispatch entries).

2. **`vfs_search.c` extraction (merge `7d682043e`):** completes the 4-slice `vfs.c` carve (slices A `vfs_rwops.c` + B `vfs_mount.c` landed 2026-06-11 at merge `49a2c532a`). Moved `SearchContext` (the four ctors + dtor + the two `makePredicate` factories + `predicate` + `nextData` + `hasData` + `getData` + `enumerateFiles`) plus its only client `vfs_copyDirectory` from `vfs.c` (1500 → 1276) to the new sibling `vfs_search.c` (260). `vfs_internal.h` 48 → 58: added a `to_physfs_path` declaration (refuter-caught — the function already had external linkage at vfs.c L222 but no header decl) and relocated the `VFS_PATH` / `VFS_MAX_PATH` typedef so `vfs_copyDirectory`'s srcPath/destPath buffers see the same bound from both TUs. `vfs_removeDirectoryAndContents` stays in `vfs.c` (zero SearchContext dep). All four TUs stay in `egolib-foundation-base`.

3. **`particle_collision.c` 2-way split (merge `eba024d19`):** the 1528-line file split into `particle_collision_physics.c` (274, the game-state-light slice: `get_prt_mass` + `get_recoil_factors` + `do_prt_platform_detection` + static `attach_prt_to_platform`) and `particle_collision_response.c` (1308, the chr-prt response chain: `do_chr_prt_collision_{get_details, deflect, damage, bump, handle_bump, init, knockback}` + the `do_chr_prt_collision` orchestrator + `spawn_bump_particles`). Both stay in `egolib-library` (game/Physics/). `chr_prt_collision_data_t` lives entirely in `response.c` (no consumer outside the do_chr_prt_collision_* family); the two TUs communicate only through public `particle_collision.h` (response's knockback calls physics's `get_prt_mass`). The 3 shared anonymous-namespace seams (objectWorld / worldUpdateCount / physical) duplicate in each TU; the 7 response-only seams stay in response. **Key refuter catch:** `spawn_bump_particles` (the static-fwd-declared-at-L105 / defined-at-L1369 helper) had to move *entirely* to response.c — its only caller `do_chr_prt_collision_handle_bump` is there, and static linkage cannot cross TUs. The immovable-tent guards (`CHR_INFINITE_WEIGHT == phys.weight || bumpdampen == 0.0` in knockback; the particle-side guard in `get_prt_mass`) are behavior-preserved.

Cumulative effect on the over-1000-line .c list: down from nine to **seven** entries; the new largest .c TU is `particle_collision_response.c` (1308). `Object.hpp` (1613) is still the single largest TU overall. ctest grew 875 → **877** (the +2 are commit `7495f2955`'s `script.c` VM dispatch test and commit `ee7487deb`'s UIManager activeRenderer-seam test). The 10th-archive carve was parked (`audio` candidate had the best ratio at 1 TU / ~6 seam sites, and the refuter found its main isolation payoff — "cartman/validator stop linking SDL_mixer" — is wrong because `idlib-game-engine-library` links SDL_mixer unconditionally). The Object.hpp split was parked (the refuter cut the scout's claimed payoff from 415 → ~123 lines saved, and the header is documentation-dominated rather than code-dominated).

**Previous (2026-06-11) — three more archive carves landed (the 7th, 8th, and 9th link-splits, all merged to master):**

- **`egolib-scriptvm` carve (7th split, merge `313a5d5b0`):** extracted the EgoScript VM as the second above-library archive (sibling of `egolib-gamestates`) — 17 TUs comprising `script.c` + `script_implementation.c` + `script_variables.c` + the 13 `script_functions_*.c` dispatch family (the 14th, `script_functions_bitwise.c`, is in foundation-base) + the `ScriptSystemAdapter`. The 10 measured reverse edges were cut in three gated passes: P1 relocated `ai_state_t`'s state methods (ctor / dtor / reset / add_order / set_changed / set_bumplast / spawn) from `script.c` to a new `Entities/AiState.cpp` in `egolib-library` (7 reverse edges cut, no new edge to the cluster); P2 routed the three genuine driver entries (`scr_run_chr_script` / `set_alerts` / `scripting_system_end`) through a new `Ego::Script::IScriptSystem` interface (accessor in foundation-base beside `IObjectWorld`; the gtest `ScriptSystemEnvironment` global env was required since the tests drive `quitModule→endScriptingSystem`/`kill→runCharacterScript` and would throw on an uninstalled `activeScriptSystem()`); P3 did the pure-CMake `add_library(egolib-scriptvm)` carve.

- **`egolib-hud-widgets` carve (8th split, merge `896209438`):** the six game-coupled in-game HUD widgets that stayed in library after the egolib-gui carve — CharacterStatus / CharacterWindow / InventorySlot / LevelUpWindow / MiniMap / ModuleSelector. The easiest split yet: 1 reverse edge (`MiniMap::setShowPlayerPosition` from `Object_update`/`game_loop` AI minimap-reveal) cut via a single `IPlayingStateController::setMiniMapShowPlayerPosition` method routed through the existing interface vtable. Sits above library, below scriptvm + gamestates.

- **`egolib-game-graphics` carve (9th split, merge `cbd19f710`):** the 3D scene render layer — `Camera` / `CameraSystem` / `BillboardSystem` / `TextureAtlasManager` + the 11 concrete `RenderPasses` + the `GFX` / `GameAppImpl` construction in a new `graphic_init.cpp` (17 TUs). `EntityList` / `TileList` / `ObjectGraphics` / `ParticleGraphics` (deep Entities/GameSession coupling) excluded — those stay in `egolib-library`. The 15 reverse edges (14 from `graphic.c`: the 11 RenderPass ctors via `GFX::GFX` + BillboardSystem ctor + `render_all` via `renderBillboards` + TextureAtlasManager; 1 from `GameEngine.cpp`: `CameraSystem::CameraSystem` via the singleton initialize/get) were cut via a **construction-injection seam** — a new pattern. Runtime access already went through `IGFX` / `ICameraSystem` / `IBillboardSystem`; the only concrete refs were singleton *construction*, triggered order-sensitively mid-`GameEngine::initialize()` (so it couldn't move to Main.cpp pre-start like the gamestates main-menu factory did). The fix: a `std::function`-based `GraphicsBootstrap` register/run holder (`registerGraphicsBootstrap` / `runGraphicsBootstrap{Init,Teardown}`, null-safe) stays in `egolib-library`; a new `graphic_init.cpp` (in the new archive) holds the relocated `GFX` / `GameAppImpl` ctor/dtor/accessor bodies plus an `installDefaultGraphicsSystems()` function; `GameEngine.cpp` calls `runGraphicsBootstrap*` at the identical original call sites (ordering byte-identical); `egoboo/Main.cpp` calls `installDefaultGraphicsSystems()` before `start()` (mirroring `installDefaultScriptSystem`). Tests never run `GameEngine::initialize` so they don't register (hook is null-safe). This superseded the parked `graphics-narrow` attempt.

**Live link layout (2026-06-12):** `egolib` is now a **nine-archive acyclic DAG** (the previous "six-archive" claim later in this document predates the 7th/8th/9th carves):

```
base 146 ◄ {physics 5, renderer 28 ◄ gui 22} ◄ library 62 ◄ game-graphics 17 ◄ hud-widgets 6 ◄ {scriptvm 17, gamestates 19}
```

The **four above-library layers** form a linear stack with two top siblings: `egolib-game-graphics` is the LOWEST upper layer (directly above library); `egolib-hud-widgets` is a MIDDLE upper layer (above game-graphics — MiniMap projects through the Camera — BELOW both scriptvm and gamestates); `egolib-scriptvm` and `egolib-gamestates` are top-layer **siblings** (nm-verified zero edges in either direction). Consumers `egoboo` and the test executable link **both** scriptvm and gamestates (which provide hud-widgets + game-graphics + library transitively); `cartman` and the content-validator use no screens, VM, in-game HUD, or 3D scene render, so they link only `egolib-library` (and link clean — proof the library remainder is cluster-free). All nine layers verify 0 forbidden back-edges. **Move-only absorption into the *lower* layers is exhausted** (the 10th-archive scout confirmed this); growing them needs seam-cutting, as the four *upward* splits did.

**Older entries (2026-06-11 and earlier) — egolib-gamestates carve: the SIXTH link-split, the first ABOVE egolib-library (de-risk → carve GameStates, step 3 + carve):** extracted the 19 concrete `GameState` screen TUs into a new **`egolib-gamestates`** STATIC archive that sits **above** `egolib-library` — the first *upward* layer (screens orchestrate the game, reaching down into game-core; the `GameState` base stays in library). The reframe that unblocked it: GameStates is topologically at the *top*, so the blocker is the small set of `library → screen` **reverse** edges (nm-measured: 7 staying TUs / 8 symbols — `PlayingState` HUD/status reach-ins + `VictoryScreen`/`MainMenuState` construction + a `typeid`), **not** the 63 forward edges a prior scout measured (which assumed a wrong-direction below-library carve). Three seam-cuts removed every reverse edge: a lower-layer **`IPlayingStateController`** interface that `PlayingState` implements (accessors `dynamic_cast` to the *interface*, whose typeinfo is lower-layer → no concrete-`PlayingState` link edge); a `GameEngine` **main-menu-state factory** injected from `egoboo/Main.cpp` (GameEngine no longer references `MainMenuState`); and routing `pushModuleEndVictoryScreen()` through `IPlayingStateController::endModuleInVictory()`. Also landed **Pass 1** (the de-risk step 3): the 10 menu screens moved off `EngineContext::get().{config,graphicsSystem,profileSystem,textureManager}()` onto `active*()` seams, adding the `Ego::activeTextureManager()` ownership-move seam. Layout: **base 145 ◄ {physics 5, renderer 28 ◄ gui 22} ◄ library 98 ◄ gamestates 19**, all six archives nm-acyclic (`library → gamestates` = 0; positive control = 67). Gates: in-place + **from-scratch** builds, exact `ar t`, nm-acyclic, ctest **875/875**, validator `test.mod` 0/0; a 4-lens adversarial review returned zero confirmed high/critical (medium/low findings — factory landmine, a gratuitous cartman link bump, a guard/comment — all addressed). Next: re-measure ScriptVM / game-graphics-render *reverse* edges for further upward carves; or the `Object` god-class multi-role decoupling.

**Previous (2026-06-11) — GameState base seam-cut (de-risk → carve GameStates, step 2):** freed the `GameState` base class of its only game-core coupling. `GameState.hpp` was already lower-layer-clean; the sole edge was `GameState.cpp`'s `engine()` calling `EngineContext::get().engine()`. Introduced the global-namespace ownership-move seam `activeGameEngine()` (new `game/Core/ActiveGameEngine.{hpp,cpp}`, mirroring `Ego::activeRenderer`), installed/cleared from `EngineContext::setEngine`/`clearEngine` (the path the test fixtures actually exercise — not `GameEngine::initialize`), and rewired `GameState::engine()` to call it. `GameState.cpp.o` now has **zero** `EngineContext` undefined symbols (nm-proven) — the base is relocatable. The seam stays in egolib-library this pass (topology-neutral); the actual `egolib-gamestates` carve is deferred (only 4 leaf screens become nm-clean, and GameStates is topologically the wrong shape for a below-remainder layer). Plan was adversarially verified across 9 break-vectors pre-implementation. Gates: build 0, ctest -j20 **875/875** (+1 seam-lifecycle test), validator `test.mod` 0/0, DAG still acyclic. Next: seam the LIGHT 2–4-`EngineContext` menu screens to enlarge the clean cohort.

**Previous (2026-06-11) — GUI Component/Container characterization net (de-risk → carve GameStates, step 1):** added `egolib/tests/egolib/tests/GuiComponentBehavior.cpp` — **44 tests, test-only, zero production/CMake edits** — pinning the egolib-gui base classes (`Component`/`Container`/`LayoutColumns`/`LayoutRows`) that every widget and every `GameState` (`GameState : Container`) inherits. Covers geometry, the `isEnabled()` visibility gate, closed-interval `contains()`, derived-position chaining, `destroy()` dominance, container membership + the `clearComponents()` parent-dangling asymmetry, z-order, and especially **input propagation** (reverse-order/first-consumer-wins, disabled-skip, mouse position translated by each container's **own** position composing per nesting level, keyboard/wheel **not** translated, the not-forwarded `Released`/`Typed`/`Clicked` trio, no hit-testing). Engine-free (never calls `draw()`/`activeUIManager()`). Adversarially reviewed (3 lenses): ship-as-is, zero must-fix. This is the safety net for the next link-split (carving the GameStates/menu screens — Front B, currently blocked at 63 back-edges and needing seam-cutting first). Gates: build 0, ctest -j20 **874/874**, validator `test.mod` 0/0. Next: relocate the `GameState` base to break menu-screen vtable/typeinfo back-edges.

**Previous (2026-06-11) — MeshLookupTables → foundation-base (DAG now fully acyclic):** resolved the last back-edge (`game/mesh_geometry.c` in base → `g_meshLookupTables` in physics). Root cause: the `MeshLookupTables` twist tables' only physics dependency was the gravity-derived `twist_vel` table — which is **dead** (written in the ctor, never read anywhere). Dropped `twist_vel` (and its `g_environment`/`PhysicalConstants` dependency), leaving pure base-layer geometry (`twist_nrm`/`twist_facing_x/y`/`twist_flat` via `twist_to_normal`/`vec_to_facing`), and relocated `MeshLookupTables.{hpp,cpp}` from `egolib-physics` to `egolib-foundation-base`. Layout: **base 142 / physics 5 / renderer 28 / gui 22 / library 116**. All five archives now verify **0 forbidden back-edges (ACYCLIC ✓)**. Gates green: build, `ar t`, per-archive nm acyclicity with positive controls, ctest -j20 830/830, validator `test.mod` 0/0.

**Previous (2026-06-11) — egolib-gui carve (the fifth link-split) + DAG acyclicity repair:** extracted the generic GUI widget toolkit as a new **`egolib-gui`** STATIC archive (22 TUs: Component/Container/Layout/Material/InputListener/DrawingContext + the leaf widgets + UIManager + ScreenMessage), a cohesive middle layer ABOVE `egolib-renderer` and `egolib-foundation-base`, below `egolib-library`. Layout: **base 141 ◄ {physics 6, renderer 28 ◄ gui 22} ◄ library 116** (was 143/6/29/132). This is the first *cohesive* (non-grab-bag) carved layer — it carries no game-session/object-state dependency, reaching engine services through lower-layer `active*()` seams. Required three new seams: `Ego::activeRenderer()` + `Ego::activeGraphicsSystem()` (ownership-move keystones) and `Ego::GUI::activeUIManager()` + a `postScreenMessage` sink. **Also repaired the DAG:** fresh nm analysis found the documented "acyclic" invariant was *already violated* in the live tree (recent absorptions had not nm-verified) — base/renderer→library back-edges via `EngineContext::renderer()` in Font/TextureManager/Material/Component + 3 renderer TUs; the `activeRenderer` seam + the gui carve eliminated all of them. egolib-gui/renderer/physics now verify 0 forbidden back-edges. One pre-existing back-edge remains and is documented (`mesh_geometry.c`→`g_meshLookupTables`, base→physics). Gates green: build, exact `ar t`, per-archive nm acyclicity with positive controls, ctest -j20 830/830, validator `test.mod` 0/0. (Menu smoke unusable in this sandbox — fonts fail via SDL_ttf on pristine master too.)

**Previous (2026-06-10) — IGraphicsSystem widening:** widened `IGraphicsSystem` with 3 methods (`setCursorVisibility`, `update`, `getDisplays`), implemented in `GraphicsSystem` by delegating to `GraphicsSystemNew::get()`. Migrated all 6 game-layer `GraphicsSystemNew::get()` callers to `EngineContext::get().graphicsSystem()`. All 5 migrated files dropped their `GraphicsSystemNew.hpp` include. `GraphicsSystemNew::get()` now confined to `GraphicsSystem.cpp` (bootstrap/delegation). Updated `MockGraphicsSystem` with no-op stubs. Gates green: build, ctest -j20 830/830, validator `test.mod` 0/0, menu smoke clean exit.

**Previous (2026-06-10) — broader `getMeshPointer()` cleanup:** migrated 17 call sites off direct `ego_mesh_t` access onto `ICollisionWorld` / `ITerrainQuery` seam interfaces (impl-file sites 32→15). Widened `ICollisionWorld` with 4 map-dimension accessors (`getEdgeX/Y`, `getTileCountX/Y`). Removed 1 dead-code site (broken `DEBUG_WAYPOINTS` block). Remaining 15 sites intentionally deferred (wall collision blocked by `mesh_wall_data_t`, mutating tile ops = game-layer-only, infrastructure forwarder).

**Previous (2026-06-10) — QuestLog foundation absorb:** seam-cut `QuestLog.cpp` (replaced the single `EngineContext::get().logTarget()` call in `exportToFile()` with `Log::activeTarget()`) and moved `QuestLog` + `PlayerQuestLog` (2 TUs) into `egolib-foundation-base`. Current verified layout **`egolib-foundation-base` 119 / `egolib-physics` 6 / `egolib-renderer` 29 / `egolib-library` 138**. Gates green: build, `ar t`, aggregate nm acyclicity with live positive controls, validator `test.mod` 0/0, ctest -j1 828/830, menu smoke exit-124 clean.

**Previous (2026-06-10) — mesh-AI terrain seam:** `AStar` and `LineOfSight` now consume the lower-layer `Ego::Mesh::ITerrainQuery` terrain interface instead of `ego_mesh_t` / `game/mesh.h`; `GameModule` implements the interface by forwarding to the existing active-module mesh surface. `AI/AStar.cpp` and `AI/LineOfSight.cpp` moved into `egolib-foundation-base`. New `AITerrainQueries.cpp` coverage pins LOS and A* blocked-tile/fan-off behavior.

**Previous (2026-06-10) — Core/System foundation seam:** the small bootstrap edge named by the frontier-absorption pass has landed. `Core/System.cpp` no longer includes `game/Core/EngineContext.hpp`; it installs/clears the existing lower-layer `Log::activeTarget` and `activeConfig` seams directly and downloads setup into `Ego::activeConfig()`. That TU moved into `egolib-foundation-base`, giving the verified layout **`egolib-foundation-base` 115 / `egolib-physics` 6 / `egolib-renderer` 29 / `egolib-library` 142**. Gates green: build, `ar t`, aggregate nm acyclicity with live positive controls, validator `test.mod` 0/0, ctest -j1 823/825 (only the two known `ScriptLoaderFixture` failures), menu smoke exit-124 clean.

**Earlier (2026-06-09) — foundation-base growth absorptions:** two nm-pre-verified pure-CMake source-list moves grew the lowest layer: **InputControl** (3 TUs) and **Image + `Graphics/PixelFormat.cpp`** (7 TUs, which must move as a unit — Image alone has 6 `pixel_descriptor` blockers all defined in `PixelFormat.cpp`). Cumulative membership then reached **`egolib-foundation-base` 83 / `egolib-physics` 4 / `egolib-library` 205** (was 73/4/215 at the physics carve). SDL2_image needed no CMake change (`IMG_*` resolves transitively via `idlib-game-engine-library`, which the base PUBLIC-links). Each acyclic (live positive controls fired), full gate green (build / `ar t` / nm / validator 0/0 / ctest -j1 823/825 / smoke exit-124). Branches `refactor/egolib-inputcontrol-foundation-absorb`, `refactor/egolib-image-foundation-absorb`.

**Change since the foundation carve (2026-06-09):** the link-split deepened to **three layers**. The carved foundation was re-split into `egolib-foundation-base` (73 TUs) + a thin `egolib-physics` middle layer (4 TUs: the collision nucleus `Collidable`/`ICollisionWorld`/`MeshLookupTables`/`PhysicalConstants`), giving the acyclic chain **`egolib-foundation-base ← egolib-physics ← egolib-library`** — landing the long-named `egolib-physics` link target. A pure CMake partition (zero source edits), nm-verified acyclic on fresh artifacts (physics needs only base symbols + 2 intra-nucleus, 0 into the upper library; 0 base→physics back-edges; positive control fired). All gates green (`ar t` 73/4/215, validator `test.mod` 0/0, ctest -j1 823/825, smoke clean). Branch `refactor/egolib-physics-middle-carve`. See `19-refactoring-roadmap.md` and `71-completed-passes-log.md`.

**The preceding foundation carve (2026-06-09):** the **first real link-split of `egolib-library`** — a dependency-closed `egolib-foundation-library` (77 TUs: Math/Log/Mesh/VFS/Time/FileFormats/Platform + the Physics nucleus + the Script DDL/PDL lexer + Logic/TreasureTables + toplevel math/IO) that `egolib-library` (the remaining 215 TUs) depends on one-way, verified acyclic by an nm symbol-closure proof (0 cycle edges). Eight verified passes (branch `refactor/egolib-physics-nucleus-carve`); the nm proof first showed the previously-documented *nucleus-only* carve was circular, and two small seams (Time→`SDL_GetTicks()`, `ego_texture_exists_vfs` fileutil→Image) grew the closed foundation from 47→77 TUs before the `add_library` carve. (This `egolib-foundation-library` was subsequently re-split into `egolib-foundation-base` + `egolib-physics` — see the change above.)

**Changes since the 2026-06-06 snapshot:** the uber-header teardown completed and **`egolib/egolib.h` was deleted** (T3.3); `cartman` was wired into the CMake graph behind `option(EGOBOO_BUILD_CARTMAN OFF)` and now compiles/links/runs (T3.5); `vfs.c`'s dead cstdio backend was eliminated (2,456 → 1,921 lines, T3.6); the `CameraSystem` EngineContext seam landed; six T3.4 characterization batches were added (physics collision-normal, bounding-box ops, map twist, particle recoil, damage/attribute enums, and the first live-Object combat-damage batch); and the T3.7 logging-seam include-decoupling front made the `Log` subsystem a clean downward leaf (17 leaf TUs moved off `game/Core/EngineContext.hpp`). The T3.7 **service-hub** continuation (2026-06-08) then cut the non-game leaf includers of `game/Core/EngineContext.hpp` from 33 to **8** via free-function `active*()` seams (sugar over the lower-layer singleton for `profileSystem`/`imageManager`; Log-style ownership-move keystones for `config`/`particleHandler`/`audioSystem`), which also reduced egolib `::get()` sites ~895→794 — the remaining 8 are bootstrap installers or are blocked on `perkHandler`/`billboardSystem`/`fontManager` seams and the deeper Entities↔game coupling (branch `refactor/egolib-service-hub-decoupling`, not yet merged).

### Key Metrics (canonical — other docs should defer here for these volatile numbers)

Verified against the live tree on 2026-06-12. These are the single source of truth; sections below and sibling docs (`01`, `02`, `AGENTS.md`) reference this table rather than re-stating the figures.

| Metric | Value | Note |
| ------ | ----: | ---- |
| egolib link archives | **9 (acyclic DAG)** | `egolib-foundation-base` 146 ◄ `{egolib-physics` 5 / `egolib-renderer` 28 ◄ `egolib-gui` 22`}` ◄ `egolib-library` 62 ◄ `egolib-game-graphics` 17 ◄ `egolib-hud-widgets` 6 ◄ `{egolib-scriptvm` 17 / `egolib-gamestates` 19`}`. Top siblings nm-verified zero edges either way. Move-only absorption into lower layers exhausted (10th-carve scout confirmed). |
| Active source files (egolib+egoboo, excl. tests) | **701** | `.c` 79 · `.cpp` 251 · `.h` 63 · `.hpp` 308. Drift from the 654/684 prior baselines is driven by the within-layer file-splitting work — the 14 `script_functions_*.c` siblings, the 4 `vfs.c` slices, the 3 spawn siblings (2026-06-12), the 2 particle_collision siblings (2026-06-12) — plus role-interface headers. |
| Active source lines (egolib+egoboo) | ~122,700 | net ~+100 from the three within-layer splits (per-file headers + duplicated seam definitions; bodies move 1:1) |
| Test lines / ratio | ~22,000 / **~17.7%** | 44 test `.cpp` files, **877** ctest cases (incl. `GuiComponentBehavior.cpp` (44), `AITerrainQueries.cpp`, `CombatDamageIntegration.cpp`, `CollisionPipeline.cpp`, plus the `script.c` `runCharacterScript` VM-dispatch test added 2026-06-11 and the UIManager `activeRenderer` seam test added in the lowrisk-seam batch) |
| ctest result | **877 / 877** | clean on this machine; the two historical `ScriptLoaderFixture` PrimaryScript-fallback cases now pass here |
| Singleton `::get()` call sites (egolib) | ~632 | of which `EngineContext::get()` ~451 + `GameSessionContext::get()` ~129 are the intentional seam calls; actionable direct singletons are now ≤8 each (`InputSystem` 8, `GraphicsSystemNew` 6, `egoboo_config_t` 6 — already seamed, `Console` 4, `TLT` 5 (const table), `video_buffer_manager` 1, `CollisionSystem` 1). Down from ~760 / ~863 / ~912 / ~1,150 / 1,239 (baseline) |
| `EngineContext` service seams | **15** install seams (16 services incl. renderer) | incl. `CameraSystem` (2026-06-07); `IGraphicsSystem` widened (2026-06-10) |
| `game/Core/EngineContext.hpp` includers | 92 total, **8** non-game leaf | down from 117 / 33 (2026-06-08 service-hub front) and 51 before T3.7 |
| `Object` role interfaces | **18** | `Entities/I*.hpp` (20 `I*.hpp` files incl. the `IParticleHandler` *and* `IObjectWorld` *service* interfaces) |
| Largest TU | `Entities/Object.hpp` **1,613** | the single largest TU overall; the largest .c TU is now `particle_collision_response.c` 1,308 (was `particle_collision.c` 1,528; the 1576-line `script_functions_spawn.c` is gone, split 3-way) |
| `Object.hpp` | **1,613** lines | monolithic by interface — refuter-cut mechanical-de-inline payoff = ~123 lines; documentation-dominated, not code-dominated |
| `vfs.c` | **1,276** lines | was 2,456 before T3.6; 1,920 before the 06-11 RWops/mount split; 1,500 before the 06-12 SearchContext split (slice C to `vfs_search.c` 260) |
| `particle_collision_response.c` | **1,308** lines | the chr-prt response chain carved from the former 1,528-line `particle_collision.c` (2026-06-12); sibling `particle_collision_physics.c` is 274 lines |
| `script_functions_spawn.c` | **629** lines residual | carved 2026-06-12 from the former 1,576-line file; siblings `script_functions_spawn_{character,particle}.c` are 458 + 463 lines; private `script_functions_spawn_internal.h` is 74 lines |
| `shared_ptr` occurrences | ~1,200 | `unique_ptr` ~52, `weak_ptr` ~26 |
| `throw` sites / `try`-`catch` files | ~570 / ~35 | — |
| `TODO`/`FIXME`/`HACK` markers | ~59 | — |
| `egolib/egolib.h` | **DELETED** (T3.3, 2026-06-07) | the uber-header is gone from the active tree |
| Overall maintainability | **3 / 5** ↗ | up from 2.5 (April 2026) |

---

## 1. Executive Summary

The codebase is in an **active, well-managed transitional state**. The original C dungeon crawler is being incrementally modernized to C++, and the completed refactoring passes (dozens of in-repo passes so far) have landed real structural wins:

- All three former mutable globals (`_currentModule`, `_gameEngine`, `update_wld`) are fully retired from active runtime code. `update_wld`'s functional replacement `worldUpdateCount()` routes through `GameSessionContext` (~77 call sites across ~31 files).
- Every historically oversized translation unit has been file-split. `script_functions_systems.c` has been deleted/decomposed; the largest TU is now `Entities/Object.hpp` at ~1,613 lines, and no other exceeds ~1,600 (see the Key Metrics table).
- File splitting, context wrappers, and accessor encapsulation work have progressed through role extraction on the `Object` god class (18 role interfaces) and deep singleton-seam work on `EngineContext` (15 service interfaces).
- A native validator tool exists, content parser tests exist, and module load/spawn smoke tests exist.

Core design debt that remains:

- The `Object` class is still monolithic by interface — `Object.hpp` is now ~1,613 lines. Its implementation is split across seven files (`Object.cpp` plus six `Object_{appearance,attributes,combat,interaction,lifecycle,update}.cpp` TUs), and the role-extraction passes have peeled off 18 role interfaces: `IInventoryHolder`, `IRenderable`, `IScriptable`, `IDamageable`, `IPhysical`, `ITargetInfo`, `ICharacterState`, `ITeamMember`, `IWallet`, `IAnimationControl`, `IAppearanceProfile`, `IEnchantable`, `IMovementControl`, `IVisualControl`, `IItemInfo`, `ILifecycleControl`, `IMorphControl`, and `IProfiled`. Single-param role-narrowing has reached its ceiling; remaining coupling is intrinsic to multi-role functions.
- Singleton access is still pervasive (~632 `::get()` call sites, down from ~863/~1,150 at the 2026-04-19 baseline — the bulk are the intentional `EngineContext::get()` (~451) and `GameSessionContext::get()` (~129) seam calls), and the `EngineContext` service-interface layer now covers audio, perk, image, particle, profile, logging, runtime `egoboo_config_t`, font, input, graphics system, texture manager, texture atlas, GFX, billboard system, and camera system; full DI still does not exist.
- Error handling still mixes C++ exceptions, `egolib_rv` return codes, and silent failure — but a written policy is now published at `doc/error-handling-policy.md` and new code is being held to it.
- The Linux-hosted Windows cross build is unstable at runtime (font atlas / audio crash under Wine); the native-Windows open-source path is undocumented.

Overall maintainability: **3 / 5**, up from 2.5 at the April 2026 baseline. All three mutable globals retired, eighteen role interfaces extracted (single-param narrowing ceiling reached), fifteen service seams landed on `EngineContext`, singleton sites down ~30% (1,239→~863), test coverage up to ~17.5%, the uber-header `egolib.h` deleted, `cartman` wired into the build (gated off), orphaned third-party deps removed, MSVC CMake branches dropped. The trend is unambiguously positive; the remaining frontier is multi-role Object decoupling, broader DI, upper-archive splitting, and fixture isolation.

---

## 2. Size and Composition

### Source files (egolib + egoboo; cartman is present but not in the main build)

| Category                         | Count |
| -------------------------------- | ----: |
| C implementation files (`.c`)    |    79 |
| C++ implementation files (`.cpp`)|   251 |
| C headers (`.h`)                 |    63 |
| C++ headers (`.hpp`)             |   308 |
| **Total active source files**    |**701**|

The `.c` count has drifted up from the 61 / 70 prior baselines because of the within-layer file-splitting work (the 14 `script_functions_*.c` siblings carved from `script_functions_systems.c`; the 4 `vfs.c` slices; the 3 spawn siblings; the 2 particle_collision siblings). `.cpp` and `.hpp` growth reflects the new role-interface headers and their implementation seams.

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

**~22,000** test lines against ~122,700 active source lines = **~17.7%** (44 test `.cpp` files, **877** ctest cases as of 2026-06-12). This is a large jump from the ~3.6% figure at the April baseline and crosses the threshold at which tests start protecting behavior rather than only compilation. The jump came from bringing script-dispatch, gameplay, physics/collision math, combat-damage, collision-pipeline, AI terrain-query, GUI Component/Container behavior, and (most recently) the `script.c` VM dispatch loop + UIManager activeRenderer seam under test rather than from raw parser coverage growth.

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

**Eight** files remain over the 1k-line threshold (down from ten on 2026-06-11). Today's (2026-06-12) three within-layer splits removed two entries from the table: `script_functions_spawn.c` (1576) was decomposed into three sub-1000 TUs, and `particle_collision.c` (1528) was replaced by a 274-line physics sibling + a still-large 1308-line response sibling (still over 1000, but smaller and well-bounded). `script_functions_systems.c`, formerly the 3,206-line largest TU, was previously decomposed across 14 `script_functions_*.c` files. `ObjectGraphics.cpp` (was 1,488) is no longer over the 1k threshold either — split into `ObjectGraphics.cpp` (~741) + `ObjectGraphics_animation.cpp` (~587) in the lowrisk-seam batch (commit `6bc27941b`). `Object.hpp` is the single largest TU (the surviving "large header"); refuter analysis (2026-06-12) confirmed that mechanical de-inlining buys only ~123 lines because the header is documentation-dominated (979 of 1613 lines are doxygen + blanks), not code-dominated.

| File                                              |  Lines | Role                                       |
| ------------------------------------------------- | -----: | ------------------------------------------ |
| `Entities/Object.hpp`                             |  1,613 | Core entity — single largest TU; monolithic by interface |
| `game/Physics/particle_collision_response.c`      |  1,308 | Chr-particle collision response pipeline (largest .c TU; carved 2026-06-12 from particle_collision.c 1,528) |
| `egolib/vfs.c`                                    |  1,276 | Virtual file system (PHYSFS-only; SDL_RWops→`vfs_rwops.c`, mount→`vfs_mount.c`, SearchContext→`vfs_search.c`) |
| `Script/script.c`                                 |  1,156 | EgoScript runtime (in `egolib-scriptvm`; `ai_state_t` state methods split out to `Entities/AiState.cpp`) |
| `game/script_compile.c`                           |  1,151 | Script compiler                            |
| `game/Physics/ObjectPhysics.cpp`                  |  1,138 | Object physics                             |
| `game/script_functions_action.c`                  |  1,101 | Script dispatch — action                   |
| `game/script_functions_target.c`                  |  1,044 | Script dispatch — target                   |

The split script-dispatch TUs have continued to grow as helpers have been moved in from `Object` and friends rather than authored from scratch — a consequence of role extraction, not a regression. No individual file exceeds 1,700 lines.

### Files that have been decomposed

| Former monolith                    | Prior Size | Current Split                                                                                                        |
| ---------------------------------- | ---------: | ------------------------------------------------------------------------------------------------------------------ |
| `script_functions.c`               |      8,183 | Seven files: `script_functions_{action,bitwise,movement,spawn,state,systems,target}.c`                               |
| `script_functions_systems.c`       |      3,206 | Fully decomposed across 14 files: `script_functions_{action,alerts,appearance,bitwise,combat,commerce,enchant,movement,quests,spawn,state,stat_gifts,target,target_select}.c` |
| `Entities/Object.cpp`              |      3,201 | Seven files: `Object.cpp` (195) + `Object_{appearance,attributes,combat,interaction,lifecycle,update}.cpp`           |
| `game/game.c`                      |      2,456 | Six files: `game.c` (522) + `game_{combat,export,loop,targeting,wawalite}.c`                                         |
| `egolib/vfs.c`                     |      1,920 | Four files: `vfs.c` (1,276) + `vfs_rwops.c` + `vfs_mount.c` + `vfs_search.c` (260; 2026-06-12); shared `vfs_internal.h` |
| `game/script_functions_spawn.c`    |      1,576 | Three files: `script_functions_spawn.c` (629, residual) + `script_functions_spawn_character.c` (458) + `script_functions_spawn_particle.c` (463); private `script_functions_spawn_internal.h` (74; 2026-06-12) |
| `game/Physics/particle_collision.c`|      1,528 | Two files: `particle_collision_physics.c` (274) + `particle_collision_response.c` (1,308; 2026-06-12); public `particle_collision.h` unchanged |
| `game/Graphics/ObjectGraphics.cpp` |      1,488 | Two files: `ObjectGraphics.cpp` (~741) + `ObjectGraphics_animation.cpp` (~587, the animation state machine); shared `ObjectGraphics_internal.hpp` (commit `6bc27941b`) |
| `Profiles/ObjectProfile.cpp`       |      1,468 | Three files: `ObjectProfile_{core,load,export}.cpp`                                                                  |
| `Entities/Particle.cpp`            |      1,447 | Five files: `Particle_{core,combat,spawn,update}.cpp` + `ParticleHandler.cpp`                                        |
| `game/Module/Module.cpp`           |      1,225 | Seven files under `Module/`: `Module.cpp` (277) + `Module_{bootstrap,loading,spawn,spawn_plan,spawn_realization,update}.cpp` |

These splits made compilation and navigation tractable, but they did not finish the interface problem. `Object.hpp` has stabilized around ~1,613 lines (down from the 1,636 peak as some legacy surface has been pruned). Single-param role-narrowing has reached its ceiling (Pass 220); the remaining `Object` decoupling requires multi-role strategies.

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
| `egolib-foundation-base`     | Static library       | Dependency-closed lowest layer (146 TUs): Math, Log, Mesh, VFS (now 4-slice: vfs.c + vfs_rwops + vfs_mount + vfs_search), Time, Core/System bootstrap, FileFormats, Platform, InputControl, Image, AI path/LOS terrain queries, QuestLog, Profiles data/writers, MD2 + texture/font clusters (incl. the `activeTextureManager` + `activeScriptSystem` seams), Script DDL/PDL lexer + `IScriptSystem` accessor, Logic/Perk/TreasureTables, toplevel math/IO — verified acyclic | ~31,300 |
| `egolib-physics`             | Static library       | Thin middle layer (5 TUs): collision nucleus (`Collidable`/`ICollisionWorld`/`PhysicalConstants`) plus `physics.c` and `Entities/Common.cpp` — depends one-way on `egolib-foundation-base` | ~900 |
| `egolib-renderer`            | Static library       | Renderer middle layer (28 TUs): SDL display/windowing and OpenGL backend — sibling of `egolib-physics`, depends one-way on `egolib-foundation-base` | ~10,000 |
| `egolib-gui`                 | Static library       | Generic GUI widget toolkit (22 TUs): Component/Container/Layout/widgets + UIManager + DrawingContext/Material — a cohesive middle layer above `egolib-renderer`; game-state-free (reaches services via `active*()` seams). Game-coupled widgets stay in `egolib-hud-widgets`. | ~6,000 |
| `egolib-library`             | Static library       | Game-core remainder (62 TUs): Entities/game core (incl. `Object.hpp` and the `EntityList`/`TileList`/`ObjectGraphics`/`ParticleGraphics` deep entity-coupling), `GameSession`/`Module`, the `GameState` *base*, the lower-layer service holders, `GraphicsBootstrap` hook holder, particle_collision (now 2-slice: physics + response), ObjectPhysics. Cluster-free remainder — `cartman` and `egoboo-content-validator` link only this archive and link clean. | ~38,000 |
| `egolib-game-graphics`       | Static library       | The 3D scene render layer (17 TUs, 2026-06-11, 9th carve): `Camera` / `CameraSystem` / `BillboardSystem` / `TextureAtlasManager` + the 11 concrete `RenderPasses` + `GFX`/`GameAppImpl` construction (in `graphic_init.cpp`) + `installDefaultGraphicsSystems()`. Lowest above-library upper layer. | ~9,200 |
| `egolib-hud-widgets`         | Static library       | The 6 game-coupled in-game HUD widgets (2026-06-11, 8th carve): `CharacterStatus` / `CharacterWindow` / `InventorySlot` / `LevelUpWindow` / `MiniMap` / `ModuleSelector`. Middle above-library upper layer; MiniMap projects through the Camera. | ~3,000 |
| `egolib-scriptvm`            | Static library       | The EgoScript VM (17 TUs, 2026-06-11, 7th carve): `script.c` + `script_implementation.c` + `script_variables.c` + the 13 `script_functions_*.c` dispatch family (including the 2026-06-12 3-way spawn split: `script_functions_spawn.c` + `_character.c` + `_particle.c` + private `_spawn_internal.h`) + `ScriptSystemAdapter.cpp`. Top-layer sibling of `egolib-gamestates`. | ~16,800 |
| `egolib-gamestates`          | Static library       | The 19 concrete `GameState` screens (2026-06-11, 6th carve): menus, options, select, loading, playing, map-editor, debug, victory. The first ABOVE-library layer; sibling of `egolib-scriptvm` (nm-verified zero edges either way). Reaches the active in-game state via the `IPlayingStateController` seam; the initial `MainMenuState` is injected from `egoboo/Main.cpp`. | ~6,000 |
| `egoboo`                     | Executable           | Thin entry point (registers `installDefaultGraphicsSystems` + `installDefaultScriptSystem` + the main-menu-state factory before `start()`). Links both `egolib-scriptvm` and `egolib-gamestates` (which provide the lower 7 archives transitively). | ~90 |
| `egoboo-content-validator`   | Executable (tool)    | Content validation tool. Links only `egolib-library` (no VM, no screens, no in-game HUD, no 3D scene render) — proof that the library remainder is cluster-free. | ~1,200 |
| `cartman`                    | Gated off by default | Map editor — wired into CMake behind `option(EGOBOO_BUILD_CARTMAN OFF)`; compiles/links/runs when ON; not in the default build (T3.5). Links only `egolib-library`. | ~9,300 |

### `egolib` internal structure — now made explicit in CMake

The historical `GLOB_RECURSE` in `egolib/library/CMakeLists.txt` has been replaced with explicit, per-subsystem source lists (one `set()` block per directory, grouped alphabetically). Ownership is visible in the build system, and as of 2026-06-12 the build enforces an acyclic **nine-archive** DAG: `egolib-foundation-base` (146 TUs) ← sibling middle layers `egolib-physics` (5 TUs) and `egolib-renderer` (28 TUs) ← `egolib-gui` (22 TUs, above renderer) ← `egolib-library` (62 TUs) ← `egolib-game-graphics` (17 TUs, the 3D scene render layer) ← `egolib-hud-widgets` (6 TUs, the in-game HUD) ← top siblings `egolib-scriptvm` (17 TUs, the EgoScript VM) and `egolib-gamestates` (19 TUs, the concrete screens). The dependency direction is nm symbol-closure verified with live positive controls (per-archive nm back-edge check against the live `.a` archives, *not* the stale `CMakeFiles/*.dir` object directories), and the DAG is **fully acyclic — zero known back-edges** across all nine archives (`egolib-scriptvm ↔ egolib-gamestates` = 0 either way; `egolib-library → upper` = 0 across all four upper layers).

The four upward splits were each carved by cutting the small set of `egolib-library → upper` **reverse** edges (not the much-larger forward edges a prior scout had measured for below-library carves). The seam toolkit, in order of use: interface dispatch via dynamic_cast (gamestates' `IPlayingStateController`); ownership-move `active*()` accessors (gui's `activeRenderer`/`activeGraphicsSystem`/`activeUIManager`); relocate-down (scriptvm's `ai_state_t` state methods → `Entities/AiState.cpp`); single-method interface extensions (hud-widgets' `IPlayingStateController::setMiniMapShowPlayerPosition`); **construction-injection via `std::function` bootstrap hooks** for order-sensitive constructors that can't move to Main.cpp pre-start (game-graphics' `GraphicsBootstrap` register/run holder + relocate-down of the `GFX`/`GameAppImpl` ctor bodies to `graphic_init.cpp`). Move-only absorption into the *lower* layers is now exhausted (the 2026-06-12 10th-archive scout confirmed no clean candidate remains: every coherent cluster has been extracted, and the residual `egolib-library` 62 TUs are the intentional Entities↔Module↔Physics↔GameSession↔Graphics mutual-dependency game-core "glue").

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
| SRP       | 2 / 5 |   ↗   | `Object` header now ~1,613 lines; file splits landed but interface still owns too many responsibilities. |
| OCP       | 2.5/5 |   →   | `GameState` hierarchy is exemplary; script dispatch and damage systems still closed to extension.     |
| LSP       |  3/5  |   →   | Shallow entity hierarchies avoid substitution problems by avoiding specialization altogether.          |
| ISP       |  3/5  |   ↗   | Eighteen `Object` role interfaces extracted; single-param narrowing has reached its ceiling — remaining coupling is multi-role. |
| DIP       | 2.5/5 |   ↗   | Context wrappers adopted and fifteen service seams landed; broader singleton abstraction progressing (~632 `::get()` sites). |

### Patterns used well

- **State** — `GameState` hierarchy with a clean stack lifecycle.
- **Iterator** — `ObjectHandler::ObjectIterator` with RAII locking.
- **Composition over inheritance** — `ObjectPhysics`, `ObjectGraphics` composed inside `Object`.
- **Signal/Slot** — `idlib::signal` / `idlib::connection` for event subscription.
- **Non-copyable mixin** — consistent use of `idlib::non_copyable` on managers and handlers.

### Patterns misapplied

- **Singleton as service locator.** Still ~632 `::get()` calls (down from ~1,150), though the bulk are the intentional `EngineContext::get()` (~451) and `GameSessionContext::get()` (~129) seam calls; actionable direct singletons are now ≤8 each. Fifteen services have `EngineContext` testing seams.
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

- **`egolib` is now nine static archives, not one** — `egolib-foundation-base` (146 TUs) with sibling middle layers `egolib-physics` (5 TUs) and `egolib-renderer` (28 TUs), then `egolib-gui` (22 TUs) above renderer, feeding `egolib-library` (62 TUs), with four ABOVE-library layers stacked linearly: `egolib-game-graphics` (17 TUs, the 3D scene render layer, lowest upper) ← `egolib-hud-widgets` (6 TUs, in-game HUD, middle upper) ← top siblings `egolib-scriptvm` (17 TUs, the EgoScript VM) and `egolib-gamestates` (19 TUs, the concrete screens) — fully acyclic. The four upward splits (gamestates, scriptvm, hud-widgets, game-graphics) landed between 2026-06-11 and 2026-06-12 by cutting `library → upper` *reverse* edges via interface seams, ownership-move accessors, relocate-down, and (for the order-sensitive `GFX`/`GameAppImpl` ctors) a `std::function`-based construction-injection bootstrap hook. Move-only absorption into the lower layers is exhausted; further sub-libraries would need new seam-cutting on the residual game-core glue.
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
| DIP adherence              | 2.5/5   |   ↗   | Context wrappers adopted; fifteen service seams landed; ~632 `::get()` sites remain |
| Design pattern quality     | 2.5/5   |   →   | State/Iterator well done; Factory/Strategy/Observer missing         |
| Naming consistency         | 2.5/5   |   →   | Three naming eras coexist                                            |
| Encapsulation              |  3/5    |   ↗   | Accessor closure plus broadening role extraction on `Object`        |
| Error handling             | 2.5/5   |   ↗   | Policy now written (`doc/error-handling-policy.md`); migration pending |
| Smart pointer discipline   | 2.5/5   |   →   | `shared_ptr` over-used, `unique_ptr` under-used                     |
| Test coverage              |  3/5    |   ↑   | From ~3.6% → ~17.5%; script dispatch, gameplay, physics/collision math, live-Object combat damage, collision pipeline, and AI terrain queries now covered |
| Build system               | 3.5/5   |   ↗   | Explicit source lists, validator integrated, **nine-archive** acyclic DAG (`egolib-foundation-base` 146 ← `{egolib-physics 5, egolib-renderer 28 ← egolib-gui 22}` ← `egolib-library` 62 ← `egolib-game-graphics` 17 ← `egolib-hud-widgets` 6 ← `{egolib-scriptvm 17, egolib-gamestates 19}`) |
| Global state discipline    | 3.5/5   |   ↗   | All three mutable globals retired; singletons down to ~863          |
| File size discipline       | 3.5/5   |   →   | Largest TU is ~3,200 lines; script-dispatch TUs growing within budget |
| Module boundaries          | 3 / 5   |   ↗   | Nine-archive DAG landed (`egolib-foundation-base` 146 ← `{egolib-physics 5, egolib-renderer 28 ← egolib-gui 22}` ← `egolib-library` 62 ← `egolib-game-graphics` 17 ← `egolib-hud-widgets` 6 ← `{egolib-scriptvm 17, egolib-gamestates 19}`, acyclic). The four upward splits (gamestates, scriptvm, hud-widgets, game-graphics) all required reverse-edge seam-cutting, not just source-list moves. |
| Language consistency       | 2.5/5   |   →   | C/C++ split roughly 44/56; no net C→C++ migration since last snapshot |
| Dead code hygiene          | 3.5/5   |   ↗   | Lua/Network removed; legacy READMEs + ego2xml quarantined to `doc/legacy/`; orphaned SDL2/physfs deleted; `utilities/migrator` marked deprecated |
| Documentation              | 3.5/5   |   ↑   | Error-handling policy landed; refactoring-documents tree authoritative |
| Cross-platform parity      |  2/5    |   →   | Linux native OK; Wine cross is unstable; no native-Win open-source path |
| Third-party independence   | 3.5/5   |   ↗   | Network fetch eliminated; orphaned SDL2/PhysFS removed; single SDL2 story per platform |
| **Overall maintainability**| **3/5** | ↗ | Meaningful forward motion: globals retired, 18 role interfaces, 15 service seams, T2 build cleanup |

---

## 10. Key Strengths

1. **Global-state boundary eliminated.** `_currentModule` and `_gameEngine` are no longer direct dependencies from any runtime code.
2. **File splitting is working.** Every former oversized TU has been decomposed; `script_functions_systems.c` has been deleted/decomposed and the largest is now `Entities/Object.hpp` (~1,613 lines), with no other exceeding ~1,600.
3. **Encapsulation discipline is sustained.** The numbered passes show an incremental, verified path from raw field access toward explicit `Object` role seams.
4. **Game state machine is clean.** The `GameState` hierarchy remains the model of how the rest of the codebase should eventually look.
5. **Entity container is well-designed.** `ObjectHandler` with RAII iterator locking and quad-tree spatial queries is solid.
6. **Build system makes structure visible — and now enforces a nine-archive DAG.** Explicit per-subsystem source lists plus the carved `egolib-foundation-base` ← `{egolib-physics, egolib-renderer ← egolib-gui}` ← `egolib-library` ← `egolib-game-graphics` ← `egolib-hud-widgets` ← `{egolib-scriptvm, egolib-gamestates}` DAG (dependency-closed and verified fully acyclic, with four upward layers cut by reverse-edge seam work) mean dependency direction is enforced at link time. Move-only growth of lower archives is exhausted; further sub-libraries need new seams.
7. **Validator exists and is integrated.** `egoboo-content-validator` provides a non-UI verification surface for content loading.
8. **`idlib` is the target pattern.** Eleven well-scoped sub-libraries demonstrate what `egolib` should eventually look like.

## 11. Key Weaknesses

1. **`Object` is still a god class by interface.** ~1,613-line header despite eighteen role interfaces already extracted; single-param narrowing has reached its ceiling — remaining coupling is multi-role. Further decoupling requires interface pollution or multi-param strategies.
2. **Singleton proliferation persists.** ~632 `::get()` call sites remain. Fifteen services now have `EngineContext` abstraction boundaries, but subsystem-local bootstrap seams and the deferred Renderer keep the count high.
3. **No dependency injection.** Every subsystem reaches directly for concrete service classes or `EngineContext::get()`.
4. **`shared_ptr<Object>` is pervasive.** Entity ownership is shared-by-default; `enable_shared_from_this<Object>` locks this in.
5. **Error handling is inconsistent.** Exceptions, `egolib_rv`, and silent failure coexist; `doc/error-handling-policy.md` is now the written target but the migration of existing callers has not yet begun.
6. **Script system is monolithic.** ~400 script functions in procedural dispatch split across seven files with no extensibility seam.
7. **Cross-platform parity is weak at runtime.** Wine cross build is unstable; native-Windows open-source path is undocumented.
8. **Test coverage is still thin in key areas.** Script dispatch, module load, gameplay alerts, accessor regressions, physics/collision math, live-Object combat damage, collision pipeline behavior, and AI LOS/pathing terrain queries are covered; rendering, GUI, and broader AI behavior are not.
9. **`egolib` is now nine static archives (was one).** The full DAG: lower layers `egolib-foundation-base` 146 + `egolib-physics` 5 + `egolib-renderer` 28 + `egolib-gui` 22 feed `egolib-library` 62; the four ABOVE-library layers stack `egolib-game-graphics` 17 ← `egolib-hud-widgets` 6 ← top siblings `egolib-scriptvm` 17 and `egolib-gamestates` 19. Move-only absorption into the lower layers is exhausted (10th-archive scout 2026-06-12 confirmed no clean candidate); growing further requires new seam-cutting on the cluster-free game-core glue that remains in `egolib-library`.
10. **Stale CI.** `appveyor-windows.yml` still generates a Visual Studio 2017 solution.

---

## 12. Immediate Next-Phase Priorities

These items compound the refactoring progress most efficiently given the current state. Each is scoped small enough to become its own numbered pass.

### Runtime and structure

1. **Object role-decoupling: next frontier** — single-param narrowing is exhausted (Pass 220 analysis). Next value is either: (a) interface pollution — co-locating `getProfile()` onto existing narrow interfaces to unlock multi-role callers, or (b) multi-param strategies, or (c) the deferred `IMatrixCacheControl` interface for the matrix-cache render-path surface.
2. **Continue the service-interface layer over singletons** — fifteen services are seamed; next candidates are subsystem-local cleanup and the deferred Renderer (if interface split becomes worthwhile). ~632 `::get()` sites remain.
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
