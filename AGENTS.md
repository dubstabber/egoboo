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

**Link layout**: egolib builds as an acyclic DAG of **nine** static archives, defined in `egolib/library/CMakeLists.txt` and nm-symbol-closure verified (see `refactoring-documents/71-completed-passes-log.md`): `egolib-foundation-base` (151 TUs, the dependency-closed bottom) ◄ `egolib-physics` (6, the collision nucleus + `physics.c` / `physics_intersect.c`) and ◄ `egolib-renderer` (28, SDL windowing + OpenGL backend; sibling of physics, zero cross-edges) ◄ `egolib-gui` (24, the generic GUI widget toolkit — Component/Container/widgets/UIManager + the abstract `GameState` base + the `ActiveGameEngine` ownership-move seam — above renderer, game-state-free in the runtime sense: the `GameState` base only needs `Ego::GUI::Container` and the `activeGameEngine()` seam, both intra-gui) ◄ `egolib-library` (65, the game-core remainder) ◄ `egolib-game-graphics` (19, the 3D scene-rendering layer — `Camera`/`CameraSystem`/`BillboardSystem`/`TextureAtlasManager` + the 11 concrete `RenderPasses` + the `GFX`/`GameAppImpl` construction in `graphic_init.cpp` + the upward-relocated `graphic_mad.c/.h` and `App.cpp/.hpp` pair, both pure-CMakeLists carves: `graphic_mad` 2026-06-12, `App` 2026-06-13) ◄ `egolib-hud-widgets` (6, the game-coupled in-game HUD widgets — CharacterStatus/CharacterWindow/InventorySlot/LevelUpWindow/MiniMap/ModuleSelector) ◄ **two sibling top layers** `egolib-scriptvm` (25, the EgoScript VM — `script.c` + `script_implementation`/`script_variables` + the 13 `script_functions_*.c` dispatch family + `ScriptSystemAdapter`) and `egolib-gamestates` (19, the concrete `GameState` screens — menus/options/select/loading/playing/map-editor/debug/victory). The **four ABOVE-`egolib-library` layers** form a linear stack with two top siblings: `egolib-game-graphics` is the LOWEST upper layer (directly above library); `egolib-hud-widgets` is a MIDDLE upper layer (above game-graphics — MiniMap projects through the Camera — BELOW both scriptvm and gamestates); `egolib-scriptvm` and `egolib-gamestates` are top-layer **siblings** (nm-verified zero edges in either direction). All reach *down* into game-core (game-graphics 61 forward edges into library; scriptvm 85, gamestates ~63, hud-widgets 43). `EntityList`/`TileList`/`ObjectGraphics`/`ParticleGraphics` (deep Entities/GameSession coupling) and the `GraphicsBootstrap` hook holder STAY in `egolib-library`; the `GameState` base + the `ActiveGameEngine` ownership-move seam live in `egolib-gui` (relocated 2026-06-12 — move-only, zero source-file edits; the abstract base only needs `Ego::GUI::Container` plus the intra-gui `activeGameEngine()` seam, which itself carries zero game-core deps beyond a forward decl of `GameEngine`); the lower-layer `Ego::Script::IScriptSystem` accessor stays in `egolib-foundation-base`. Consumers `egoboo` and the test executable link **both** `egolib-gamestates` and `egolib-scriptvm` (which provide hud-widgets + game-graphics + library transitively); `cartman` and the content-validator use no screens, VM, in-game HUD, or 3D scene render, so they link only `egolib-library` (and link clean — proof the library remainder is cluster-free). The DAG is **fully acyclic — zero known back-edges** (all 9 layers verify 0 forbidden edges). Move-only absorption into the *lower* layers is mostly exhausted but not strictly so: tightly-decoupled top-of-call-graph fragments can occasionally still cross a boundary cleanly without surgery (e.g. the `GameState` base + `ActiveGameEngine` seam moved into `egolib-gui` on 2026-06-12 via CMakeLists-only edits, since their U-syms closed inside gui — both depend only on `Ego::GUI::Container` and `<stdexcept>` plus a forward decl of `GameEngine`). The general rule still holds: growing the lower layers usually needs seam-cutting (e.g. the gui carve needed `activeRenderer`/`activeGraphicsSystem`/`activeUIManager` seams). The four *upward* splits were carved by seam-cutting the `egolib-library → upper` **reverse** edges (measure REVERSE edges, not forward, for an above-library carve): gamestates via the `IPlayingStateController` interface (dynamic_cast-to-interface) + a `GameEngine` main-menu factory injected from `egoboo/Main.cpp` + `Ego::activeTextureManager()`; scriptvm via relocating `ai_state_t`'s state methods down into `Entities/AiState.cpp` (7 of 10 reverse edges) + routing the 3 driver entries (`scr_run_chr_script`/`set_alerts`/`scripting_system_end`) through the `Ego::Script::IScriptSystem` interface installed from above library (the game's `Main.cpp` and the test harness's gtest global environment); hud-widgets via a single `IPlayingStateController::setMiniMapShowPlayerPosition` interface method cutting the lone `MiniMap::setShowPlayerPosition` reverse edge (1 reverse edge, render-driver-free, GL-safe); game-graphics (the 9th, ninth split) by cutting the 15 reverse edges (14 from `graphic.c` — the 11 RenderPass ctors + BillboardSystem ctor + `render_all` + TextureAtlasManager — and 1 from `GameEngine.cpp` — `CameraSystem::CameraSystem`) via a *construction* seam: relocate the order-sensitive `GFX`/`GameAppImpl` construction/teardown bodies down into `graphic_init.cpp` (above library) and trigger them from `GameEngine::initialize()`/teardown through the `Ego::Graphics::registerGraphicsBootstrap`/`runGraphicsBootstrap{Init,Teardown}` `std::function` hook (the holder stays in egolib-library), with `installDefaultGraphicsSystems()` injected from `egoboo/Main.cpp` (mirrors `installDefaultScriptSystem`) — ordering preserved byte-identically. When touching egolib CMake or moving sources, preserve the acyclicity — re-run the per-archive nm back-edge check (mangled symbols, set math, with a positive control; measure against the live `.a` **archives**, not the `CMakeFiles/*.dir` object dirs, which hold stale `.o` from before the carves); do not trust prior "acyclic" claims. The carve-layer C-as-C++ `set_source_files_properties` foreach matches `\.c$` only — a `.h` added to a layer source list would otherwise get `LANGUAGE CXX` and be force-compiled into a stray `<name>.h.o` archive member (the master `SOURCE_FILES` loop avoids this via a `HEADER_FILE_ONLY` follow-up; the carve-layer loop has none).

### Global State (major coupling points)

The runtime was historically wired around three mutable globals, all now retired from active runtime code:

- `_gameEngine` — **0 references.** Engine access routes through `EngineContext::get().setEngine()` / `engine()`.
- `_currentModule` — **0 references.** Consumers go through `GameSessionContext` and `GameModule` accessor surfaces.
- `update_wld` — **0 active references.** Variable gone; a few stale string-literal/comment artifacts remain in `script.c`, `ObjectGraphics.hpp`, `Particle.hpp`. Functional replacement is `worldUpdateCount()` (via `GameSessionContext`), ~77 call sites across ~31 files.

The remaining coupling hotspot is singleton access: ~632 `::get()` call sites persist (down from ~863; the bulk are the intentional `EngineContext::get()` (451) and `GameSessionContext::get()` (129) seam calls). Actionable direct singletons: `video_buffer_manager::get()` (1), `InputSystem::get()` (8), `GraphicsSystemNew::get()` (6), `egoboo_config_t::get()` (6, already seamed), `TLT::get()` (5, const table). The `EngineContext` service-interface layer covers audio, perk, image, particle, profile, logging, config, font, input, graphics system, texture manager, texture atlas, GFX, billboard system, and camera system (15 service seams); broader DI does not yet exist. Avoid reintroducing hidden global dependencies. Be careful around code affecting VFS setup, module loading, object profile loading, or script compilation.

### High-Risk Hotspots

Read relevant audit docs before modifying. Files over 1,000 lines (by size) — exactly two as of 2026-06-12 (after the script.c operand-evaluator carve + the Object.hpp Strategy 1+4 partial carve that shaved Object.hpp 1613→1530 without dropping it off the list):
- `egolib/library/src/egolib/Entities/Object.hpp` (~1530 lines after the 2026-06-12 Strategy 1+4 partial pass, down from 1613 — monolithic interface; 18 role interfaces extracted; the single largest TU in the tree). The 2026-06-12 partial carve added `Object::getGraphics()` / `getGraphics() const` accessors returning `Ego::Graphics::ObjectGraphics&`, deleted 26 non-override `inst.*` forwarding wrappers (interface-free ones), de-inlined ~198 inline method bodies into the new `Object_accessors.cpp` (550 lines), and migrated 13 caller files to the `obj.getGraphics().X()` pattern. The 20 *override* wrappers (interface contract for IRenderable / IAnimationControl / IVisualControl) had to stay — their bodies de-inlined but their signatures kept. Below 1000 would require PIMPL, factoring into sub-objects, or eliminating role interfaces — out of scope for the routine file-split cadence.
- `egolib/library/src/egolib/game/Physics/particle_collision_response.c` (~1308 lines, the chr-prt response pipeline; carved 2026-06-12 from the former 1528-line `particle_collision.c` — see `particle_collision_physics.c` (274) for the pure mass/recoil/platform-detection sibling; the largest `.c` TU). Scouted 2026-06-12 as splittable (3-way: residual + _damage + _effects) but recently carved (eba024d19); recommend letting settle before another split.

Notes:
- `script_functions_systems.c` (formerly ~3,200 lines) has been fully decomposed and deleted, spread across 14 `script_functions_*.c` files (action, alerts, appearance, bitwise, combat, commerce, enchant, movement, quests, spawn, state, stat_gifts, target, target_select). The largest TU is now `Entities/Object.hpp`, not this deleted file.
- `script_functions_spawn.c` (formerly ~1576 lines, the largest .c TU) was split 2026-06-12 into 3 within-`egolib-scriptvm` siblings: `script_functions_spawn.c` (629, residual: 24 lifecycle/drop/cleanup/identify/state-mutation entries), `script_functions_spawn_character.c` (458, 8 character spawn/respawn entries), `script_functions_spawn_particle.c` (463, 12 particle spawn/poof entries), plus a private `script_functions_spawn_internal.h` (74, shared `SpawnSelfContext` / `makeSpawnSelfContext` / `gameSession()` / `isLiveSpawnObjectRef`).
- `particle_collision.c` (formerly ~1528 lines, second-largest .c TU) was split 2026-06-12 into 2 within-`egolib-library` siblings: `particle_collision_physics.c` (274, `get_prt_mass` / `get_recoil_factors` / `do_prt_platform_detection` / `attach_prt_to_platform`) and `particle_collision_response.c` (1308, the chr-prt response chain + `do_chr_prt_collision` orchestrator + `spawn_bump_particles`). Public `particle_collision.h` unchanged; the immovable-tent guards (CHR_INFINITE_WEIGHT / bumpdampen==0) live intact in both TUs.
- `script_compile.c` (formerly ~1151 lines) was split 2026-06-12 (merge 37b47657e) by extracting `line_scanner_state_t`'s method bodies into a sibling `script_compile_lexer.c` (~353 lines); residual `script_compile.c` is now ~798 lines (the parser/codegen body).
- `script_functions_action.c` (formerly ~1101 lines) was split 2026-06-12 (merge 9a2c7199f) into 3 within-`egolib-scriptvm` siblings: `script_functions_action.c` (545 residual, 25 misc entries), `script_functions_action_audio.c` (211, 10 sound/music entries), `script_functions_action_visual.c` (297, 11 animation/visual entries), plus a private `script_functions_action_internal.h` (108, shared `SelfActionContext` in `namespace script_action_detail`).
- `script_functions_target.c` (formerly ~1044 lines) was split 2026-06-12 (merges 96fddcafe + d4feb6919) into 3 within-`egolib-scriptvm` siblings: `script_functions_target.c` (589 residual, 30 state-predicate `Is*`/`Facing`/`Distance` entries), `script_functions_target_identity.c` (213, 9 IDSZ `Has*` queries), `script_functions_target_orders.c` (267, 13 order/getter/mutator ops); shared `script_functions_target_impl.h` updated in the follow-on cleanup to use `namespace script_target_detail` + `inline` (replacing the anon-namespace pattern) to silence the 42 `-Wunused-function` warnings the wider includer set introduced.
- `ObjectPhysics.cpp` (formerly ~1138 lines) was split 2026-06-12 into 3 within-`egolib-library` siblings: `ObjectPhysics.cpp` (503 residual, the movement-integration pipeline — updatePhysics dispatcher, voluntary movement, hill/Z-velocity, facing, max-speed, trivial accessors), `ObjectPhysics_terrain.cpp` (234, 4 methods: detachFromPlatform, attachToPlatform, updatePlatformPhysics, updateMeshCollision), `ObjectPhysics_attachment.cpp` (432, 3 methods: grabStuff, attachToObject, updateCollisionSize), plus a private `ObjectPhysics_internal.h` (74, the 5 former anon-namespace helpers — objectWorld, collisionWorld, worldUpdateCount, scriptable, objectByRef — now inline in `namespace object_physics_detail`, with the `using namespace object_physics_detail;` directive placed INSIDE the `Ego::Physics` block of the header so the 3 sibling TUs see the helpers unqualified). No public API changes, no static-to-extern promotions; nm 0 back-edges from any of the 3 .o files into game-graphics/scriptvm/gamestates/hud-widgets/gui. Drops the >1000-line list 5→4.
- `CollisionSystem.cpp` (formerly ~989 lines, the largest actionable <1000 TU) was split 2026-06-13 into 2 within-`egolib-library` siblings: `CollisionSystem.cpp` (648 residual, the `CollisionSystem::` class methods — update/integration, updateObjectCollisions, updateParticleCollisions, the two detectCollision overloads, handleCollision dispatcher, handleMountingCollision, handlePlatformCollision; the anon-namespace `objectWorld()`/`worldUpdateCount()` seam helpers stay here) and `CollisionSystem_chr_chr.cpp` (373, the `do_chr_chr_collision` chr-chr response free function + its private static helper `get_recoil_factors`), plus a private `CollisionSystem_internal.h` (44) declaring `do_chr_chr_collision` with external linkage. ONE static-to-extern promotion: `do_chr_chr_collision` (the lone cross-boundary symbol — called from the residual's handleCollision) goes from file-scope static to a non-static decl in the `_internal.h`, in `namespace Ego::Physics` (no detail-namespace / call-site rename needed). `get_recoil_factors` stays file-scope static and travels with its only caller (adversarial verify corrected the scout's "promote BOTH" plan — only one symbol crosses). Note: this `Ego::Physics::get_recoil_factors` is a *different, independent* function from the global `get_recoil_factors` in `particle_collision_physics.c` — they coexist by namespace scoping and must NOT be unified. Byte-identical extraction; nm 0 back-edges from the new .o into game-graphics/hud-widgets/scriptvm/gamestates; `do_chr_chr_collision` is defined once (T) in egolib-library and referenced only intra-archive. ctest 877/877. CollisionSystem was never on the >1000 list, so that list (Object.hpp 1530, particle_collision_response.c 1308) is unchanged.
- `physics.c` (formerly ~964 lines) was split 2026-06-13 into 2 within-`egolib-physics` siblings (the archive grows 5→6 TUs): `physics.c` (499 residual — Cluster 1 the contact/pressure-normal estimators `phys_get_collision_depth`/`phys_get_pressure_depth`/`phys_warp_normal`/`phys_estimate_depth`/`phys_estimate_collision_normal`/`phys_estimate_pressure_normal`, plus Cluster 3 the `phys_expand_*`/`phys_data_t`/`apos_t` ops + `orientation_t::MAP_TURN_OFFSET`) and `physics_intersect.c` (495, Cluster 2 the swept-AABB pipeline — `phys_intersect_oct_bb_index` static, `phys_intersect_oct_bb` public, `phys_intersect_oct_bb_close_index` static). **ZERO static promotions** — the 2 statics are Cluster-2-local (the 2 fwd-decls at the top travel with them; both stay file-scope static in the new TU). The one cross-cluster call (`phys_intersect_oct_bb`→`phys_expand_oct_bb`) resolves through the public `physics.h` decl, no surgery. New TU needs only `physics.h` + `Float.hpp` (NOT `_Include.hpp` — that heavy include serves only Cluster 3's entity-deref'ing `phys_expand_chr_bb`/`_prt_bb`). `.c` file → 2-section CMake (EGOLIB_TOPLEVEL_SOURCES master + EGOLIB_PHYSICS_SOURCES, the latter already in both the REMOVE_ITEM and the C-as-C++ foreach). Byte-identical extraction; `physics_intersect.c.o` lands in egolib-physics (not library); `phys_intersect_oct_bb` defined once in egolib-physics; nm 0 back-edges from the new TU into library/gui/game-graphics/hud-widgets/scriptvm/gamestates/renderer. ctest 877/877. physics.c was never on the >1000 list.
- `ModelDescriptor.cpp` (formerly ~789 lines) was split 2026-06-13 into 2 within-`egolib-foundation-base` siblings (the cleanest member-function split — ZERO file-scope statics, ZERO anon namespaces, ZERO promotions): `ModelDescriptor.cpp` (319 residual — the action-name half: the `STRING_SWITCH` constexpr + `stringToAction` 90-case switch + the ctor + `getName`/`isActionValid`/`getAction`/`getMadFX`/`randomizeAction`) and `ModelDescriptor_frames.cpp` (481, the frame-descriptor half: `ripActions`, `parseFrameDescriptors`, `initializeWalkFrame`, `makeEquallyLit`, `initializeFrameLip`, `healActions`, `charToAction`, `actionCopyCorrect`, `getMD2`, `isFrameValid`, `getFrameLipToWalkFrame`). All are non-static member functions of one class, so the linker resolves the cross-file calls (the ctor in the residual calls `ripActions`/`initializeFrameLip`/`initializeWalkFrame` in the frames TU; the frames TU calls `getAction`/`isActionValid`/`stringToAction` back in the residual) — no internal header needed. `STRING_SWITCH` (file-scope `constexpr`, implicitly inline) is used only by `stringToAction` so it stays in the residual. The identical 7-line include block is kept in both files (slight over-inclusion, guaranteed correct); both reopen `namespace Ego`. Pre-existing quirk preserved: the original lacked a trailing-newline at EOF — both new files get a proper one (well-formedness only, no behavior change). `.cpp` files registered at the 2 ModelDescriptor.cpp sites (master SOURCE_FILES + EGOLIB_FOUNDATION_BASE_SOURCES). Byte-identical extraction; both `.o` land in egolib-foundation-base (not library); nm 0 back-edges from the new TU into any of the 8 upper archives. ctest 877/877. (The egolib-foundation-base inline DAG count was corrected 146→151 in the 2026-06-13 doc-count cleanup pass.)
- `script_functions_state.c` (formerly ~885 lines) was split 2026-06-13 into 2 within-`egolib-scriptvm` siblings + a shared private header: `script_functions_state.c` (361 residual — the flow/state/compare half: `scr_IfTimeOut`/`SetContent`/`SetTime`/`GetContent`/`Else`/`SetState`/`GetState`/the `IfStateIs*` family/comparisons/`IfInvisible`/`IfArmorIs`/`IfUnarmed`/`SetWeatherTime`/`IfContentIs`/`IfStateIsNot`/`DebugMessage`) and `script_functions_state_inventory.c` (493, the inventory/equipment/knowledge/world-query half: `scr_IfNameIsKnown`/`IfUsageIsKnown`/the `IfHolding*` family/`IfKursed`/`IfOverWater`/`IfAmmoOut`/`IfEquipped`/`FlashVariable*`/`IfOperatorIs*`/`IfModuleHasIDSZ`/`IfStealthed`/the high `IfStateIs8-15`). TWO anon-namespace helpers crossed the boundary — `SelfStateContext` (struct) + `makeSelfStateContext` (used by both halves) — promoted into a new private `script_functions_state_internal.h` (`namespace script_state_detail`, `inline`, `using namespace` at file scope, mirroring `script_functions_spawn_internal.h`). `makeSelfStateContext` becomes `inline` (only change to its body; weak/COMDAT, verified). `isLiveStateObjectRef` is first-half-only so it stays in the residual's anon namespace; the five second-half-only helpers (`heldItemRef`/`isUsableRangedWeapon`/`isMeleeWeapon`/`isShield`/`activeModuleHasIdszWithValidMessage`) move to the inventory TU's anon namespace. The shared `.h` goes in the master SOURCE_FILES list only (like `script_functions_spawn_internal.h`); the new `.c` goes in both the master list and EGOLIB_SCRIPTVM_LAYER_SOURCES (the C-as-C++ foreach). All `scr_*` and helper bodies byte-identical; both `.o` land in egolib-scriptvm (not library); nm 0 back-edges into the `gamestates` sibling; no duplicate `scr_*` definitions. ctest 877/877. (The stray empty `script_internal.h.o` this surfaced — plus the equivalent `graphic_mad.h.o` — and the drifted inline scriptvm count were all fixed in the 2026-06-13 doc-count + stray-.h.o cleanup pass: the carve-layer C-as-C++ foreach now matches `\\.c$` only, and the scriptvm count was corrected 17→25.)
- `graphic_lighting.c` (formerly ~885 lines) was split 2026-06-13 into 2 within-`egolib-library` siblings: `graphic_lighting.c` (541 residual — the GridIllumination tile-corner fan-lighting methods: grid_lighting_test, light_corners, grid_lighting_interpolate, test_one_corner, test_corners, light_one_corner, light_corner, light_fans_throttle_update, light_fans_update_lcache, grid_get_mix, ego_mesh_interpolate_vertex, light_fans_update_clst, light_fans) and `graphic_lighting_dynalist.c` (384, the ambient/global-light + dynalist accumulation: `get_ambient_level`, the static `sum_global_lighting`, `dynalist_t` ctor + `dynalist_t::init`, and the 273-line `GridIllumination::do_grid_lighting` orchestrator). ZERO promotions: `sum_global_lighting` is the only file-scope static and it's called solely from `do_grid_lighting`, so it stays static and travels with it; `get_ambient_level` is already declared in graphic.h (called from ObjectGraphics.cpp in library and WaterTilesRenderPass.cpp in game-graphics) and its definition stays in egolib-library — same archive, so the existing game-graphics→library forward edge is unchanged. Clean bidirectional cut: `do_grid_lighting` calls none of the 13 residual methods, and the residual references none of the moved symbols. The full include/`using namespace` block is copied to both files (harmless over-include; the residual no longer needs Camera/GameSession/EngineContext/GameEngine but they're left in place). `.c` in egolib-library → single CMake edit (EGOLIB_GAME_TOPLEVEL_SOURCES; the master C-as-C++ foreach handles the language, no layer REMOVE_ITEM/foreach involved). Byte-identical extraction; both `.o` in egolib-library; `get_ambient_level` + `GridIllumination::do_grid_lighting` each defined once; nm 0 back-edges from the new TU into game-graphics/hud-widgets/scriptvm/gamestates. ctest 877/877. This completes the 789–989-line within-archive split front.
- `vfs.c` (formerly ~1276 lines) was split 2026-06-12 (the 4th vfs slice carve in the cadence) into 3 within-`egolib-foundation-base` siblings: `vfs.c` (658 residual, init/exit/open/path-resolution + handle ops + filesystem helpers), `vfs_io.c` (494, typed-binary I/O + text/char I/O + error translation — `_vfs_translate_error`, `vfs_finish_io`, bulk `vfs_read`/`vfs_write`, 9 typed readers, 9 typed write specializations, `fake_physfs_vprintf`, `vfs_printf/getc/putc/ungetc/puts/rewind`, temp-dir helpers, `vfs_getError`), `vfs_bulk.c` (169, `vfs_set_base_search_paths` + `vfs_readEntireFile` overloads + `vfs_writeEntireFile`). The `vsf_file` struct + `vfs_file_flags` enum move from `vfs.c`'s preamble into the existing private `vfs_internal.h` (gains `#include <physfs.h>` + `#include <egolib/typedef.h>` for `BIT_FIELD`). Public `vfs.h` still sees `vfs_FILE` as an opaque forward-decl typedef. `vfs_bulk.c` also pulls in `vfs_internal.h` for `BAIL_IF_NOT_INIT` (the scout's plan called it a "pure public-API consumer", but `vfs_set_base_search_paths` uses the init-flag guard). nm 0 back-edges to any upper layer. Drops the >1000-line list 4→3. This completes the vfs.c slice cadence: `vfs_rwops.c` + `vfs_mount.c` (2026-06-11), `vfs_search.c` (2026-06-12, merge 7d682043e), now `vfs_io.c` + `vfs_bulk.c`.
- `script.c` (formerly ~1156 lines) was split 2026-06-12 into 2 within-`egolib-scriptvm` siblings: `script.c` (784 residual, the VM execution driver — Runtime singleton, execution-loop anon-ns helpers, `scripting_system_begin`/`end`, `scr_run_chr_script`, `runCharacterScript`, `run_function_call`/`run_operation`/`run_function`, `set_alerts`, `ai_state_t::get_wp`/`ensure_wp`, `issue_order`/`issue_special_order`), `script_operand.c` (415, the operand evaluator — `makeOperandContext` + 4 `populate*OperandContext` + leader resolution, `getVariableName`, `loadVariable` 80-case switch, `storeVariable`, `onVariableNotDefinedError`, `run_operand` arithmetic dispatcher, `script_info_t` position methods). Two file-scope statics (`script_error_model`, `script_error_classname`) promoted to extern via a new private `Script/script_internal.h` (37 lines). Set by `updateScriptErrorContext` (residual); read in `script_operand.c`'s divide/modulo error paths. Build-time pitfall caught: `script_operand.c` also needed `script_compile.h` for `debug_scripts`/`debug_script_file` globals (not in original plan; fixed during build triage). Residual stays at 784 — above the 800 target but below 1000; script.c is no longer the largest scriptvm TU. Drops the >1000-line list 3→2.

Architecturally central but now small after split passes:
- `egolib/library/src/egolib/game/game.c` (~522 lines, split into `game_{combat,export,loop,targeting,wawalite}.c`)
- `egolib/library/src/egolib/Entities/Object.cpp` (~200 lines, split into six `Object_*.cpp` TUs; a seventh sibling `Object_accessors.cpp` (550 lines) added 2026-06-12 holds the de-inlined override-wrapper bodies + trivial accessor bodies — see Object.hpp note above)
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
