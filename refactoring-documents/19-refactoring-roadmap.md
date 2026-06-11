# Refactoring Roadmap

Prioritized forward plan for ongoing Egoboo refactoring work. Snapshot date: 2026-06-10 (updated from 2026-06-08 / 2026-06-06 / 2026-04-19). Supersedes and replaces:

- `19-new-refactoring-plan.md` (the original phase A–G plan — build-hygiene and global-state phases are complete)
- `22-module-runtime-ownership-plan.md` (fully executed; all checkpoints landed)
- `25-entity-layer-decomposition-plan.md` (phases 1–3 complete; `Object.cpp` / `ObjectProfile.cpp` / `Particle.cpp` all split)
- `33-maintainability-improvement-plan.md` (Tier 1.1 context-wrapper migration complete; Tier 1.2 `Object` seam closure and role extraction in flight — see passes 72–81 in `71-completed-passes-log.md`)

For the current-state snapshot that underpins this plan, read `CODEBASE-HEALTH-STATUS.md`. For the completed work that got us here, read `71-completed-passes-log.md`.

## Principles (still in force)

1. **No flag-day rewrites.** Every change incremental and verifiable.
2. **Characterization tests before restructuring.** Especially on behavior-dense code.
3. **Decouple from globals before extracting subsystems.**
4. **Prioritize by coupling reduction, not cosmetic cleanup.**
5. **Keep the Windows toolchain fully open source.** No new Visual Studio-only requirements.
6. **Treat warnings as portability/maintainability debt, not background noise.**

---

## Tier 1 — In-flight (next 3–6 passes)

### T1.1 Finish `Object` mutable-seam closure

Passes 75 and 76 completed the remaining broad inventory/team seams, so T1.1 is effectively done. `Object.hpp` no longer exposes `getInventory()`, mutable `getTeam()`, a public `getObjectPhysics()` seam, or the old `aiStateForScript()` raw-state bridge. The remaining deliberate escape hatches are now small alias-style handle returns plus the Script-owned `Ego::Script::runtimeState(...)` helper used by the legacy script runtime.

- Keep any remaining closure work narrowly focused on small alias-style handle returns rather than reopening broad field-access passes.
- Preserve the accessor characterization coverage in `egolib/tests/egolib/tests/ObjectAccessors.cpp` as role extraction begins.

**Risk:** Low. Mechanical, bounded, covered by existing test harness.

### T1.2 Introduce `Object` role interfaces

Role extraction is now underway. The public `Object` surface is small enough to keep peeling off bounded interfaces without reopening the earlier field-access work.

Landed so far (18 role interfaces):

- `IInventoryHolder` — equipment, held, inventory slot access
- `IRenderable` — render-facing surface (matrix cache, tint, model descriptor)
- `IScriptable` — script-visible state and commands
- `IDamageable` — combat damage application surface
- `IPhysical` — collision volume, orientation, bumper state, position
- `ITargetInfo` — bounded target/self query surface for script helpers
- `ICharacterState` — bounded mutable ammo/mana/kurse/timer/perk/attribute state
- `ITeamMember` — team mutation, leadership, team-wide XP publication
- `IWallet` — bounded money query and mutation
- `IAnimationControl` — bounded script-facing action resolution and animation control
- `ILifecycleControl` — bounded respawn, detach, drop, crush/item, threshold, and stealth control
- `IAppearanceProfile` — appearance profile query
- `IEnchantable` — enchant list and enchant-related state
- `IMovementControl` — movement and latch control
- `IVisualControl` — visual state (light, alpha, shadow)
- `IItemInfo` — item-specific state (isItem, isMount, etc.)
- `IMorphControl` — morph/polymorph control
- `IProfiled` — `getProfile()` accessor (Pass 220; only one clean single-role caller exists)

Follow-on work inside this tier:

- Migrate more callers to the landed role surfaces instead of `Object`.
- Keep the remaining mixed-domain script-dispatch helpers (now spread across the `script_functions_*.c` family — `script_functions_systems.c` itself has been decomposed and deleted) focused on bounded caller migration; quest/profile/armor policy coupling still remains deferred.
- Keep the raw `ai_state_t` bridge confined to `Ego::Script::runtimeState(...)` until `Script/script.c` no longer consumes raw script-runtime state.

This remains the SRP/ISP keystone for `Object`.

**Risk:** Medium. The pattern is established now, but the remaining caller migration still needs careful, role-by-role passes.

### T1.3 Service-interface layer over singletons

~632 `::get()` call sites remain (down from ~863 at 2026-06-06, ~912 earlier, ~946 at 2026-04-20, ~1,150 at 2026-04-19). Keep taking the smallest-reach singleton and applying the same DIP seam pattern one service at a time:

- Landed so far: `IAudioSystem`, `IPerkHandler`, `IImageManager`, `IParticleHandler`, `IProfileSystem`, `IFontManager` (Pass 211), `IInputSystem` (Pass 212), `IGraphicsSystem` (Pass 213, with headless test mock), `ITextureManager` (Pass 214), `ITextureAtlasManager` (Pass 217), `IGFX` (Passes 218–219, two sub-passes), `ICameraSystem` (CameraSystem Passes 1–2, 2026-06-07 — interface widened + game-layer consumers migrated), plus `IBillboardSystem` caller rerouting (Pass 215), `Time` clock abstraction (Pass 216), engine-routed logging, and `egoboo_config_t`.
- Bootstrap ownership now publishes audio through `GameEngine`, and perk/image services through `ContentRuntimeBootstrap` or `App`/`GFX` as appropriate.
- `egoboo_config_t` is now published through `EngineContext` for system/bootstrap lifecycle, module-load sync, lightweight content-bootstrap paths, read-mostly runtime callers, and the former write-heavy audio/video options flow.
- Keep subsystem-local `::get()` as bootstrap/lifecycle seams where the singleton predates the EngineContext install. Follow-on work is limited to subsystem-local cleanup around bootstrap and lifecycle edges.
- **Renderer: DEFERRED** — already an abstract polymorphic facade; migratable surface is ~23 methods (nearly the whole interface), low value/high churn.
- **CameraSystem: DONE** (2026-06-07, two passes) — the seam was found mid-flight, not deferred (`ICameraSystem` + `EngineContext` install/clear + `GameEngine` lifecycle already existed). `ICameraSystem` was widened with the four methods consumers use (`getMainCamera`, `getCamera`, `getCameraOptions`, `renderAll`) and all clean game-layer `.cpp` consumers (`PlayingState`, `MapEditorState`, `Player`, `GameEngine` config sync) migrated onto `EngineContext::get().cameraSystem()`. Remaining `CameraSystem::get()` sites are principled exceptions: the install site, `Audio/AudioSystem.cpp` (×4, layer-inversion — lower-layer subsystem stays on the concrete singleton), `Entities/Object_appearance.cpp` (×2, behind a `CameraSystem::is_initialized()` existence-guard whose semantics do not match the seam in the test bootstrap), and 10 in deferred `.c` files.

**Risk:** Medium. Pattern is well-established; remaining passes mostly mechanical.

### T1.4 Document error-handling policy, retire `egolib_rv` — EFFECTIVELY DONE (2026-06-09)

The policy is published and the `egolib_rv` retirement is **effectively complete**. A live-tree grep (2026-06-09) found **only 3 `egolib_rv` occurrences in 2 files**: the enum definition in `egolib/typedef.h` and the `typedef egolib_rv gfx_rv;` alias in `game/egoboo.h` (graphics return codes). There are no remaining `egolib_rv` *uses* in the C++ code paths to retire. The "three strategies coexist / touches a lot of code" framing is stale.

- Landed: `doc/error-handling-policy.md` defines exceptions for exceptional paths, ordinary return values for expected boundary outcomes, and a no-new-silent-failure rule.
- The only residue is the `gfx_rv` alias (a graphics-layer tri-state); retiring it is a separate, low-value graphics-typedef cleanup, not the broad migration this item originally scoped.

**Risk:** Done. (Any future `gfx_rv` cleanup is Low.)

---

## Tier 2 — Build and cross-platform

### T2.1 Retire MSVC from CI — PARTIALLY DONE

**Done** (commits `ede1ed976`, `63530491d`):
- Dropped the MSVC-only CMake branches (root `CMakeLists.txt` CPACK block, `egoboo/CMakeLists.txt` `VS_DEBUGGER_WORKING_DIRECTORY`) and the `platform.h` `#if defined(_MSC_VER)` warning-pragma island.
- Removed `distribute.ps1`, `egoboo.gta.runsettings`, and quarantined the four legacy platform READMEs (`README.VisualStudio`, `README.Windows`, `README.MinGW`, `README.OSX`) to `doc/legacy/`.

**Remaining:**
- Replace the Visual Studio 2017 generator in `appveyor-windows.yml` with mingw-w64 cross.
- Remove `external/install-vsix-appveyor.ps1` and `external/external.sln` (in the `external` submodule).

**Risk:** Low.

### T2.2 Native-Windows open-source build

Add `doc/build-windows-native.md` + `cmake/toolchains/msys2-ucrt64.cmake` for building on a Windows host with MSYS2 UCRT64. This completes the supported matrix alongside Linux native and Linux-hosted cross.

### T2.3 Eliminate configure-time network fetch — DONE

**Done** (commit `12bd9463e`): Vendored googletest updated to v1.16.0 in the `external` submodule. The default (`idlib-with-fetch-googletest=OFF`) now builds and tests offline with no network. The `-Didlib-with-fetch-googletest=ON` flag is obsolete.

### T2.4 Collapse third-party dependency divergence — DONE

**Done** (commit `cb836a2f5`): Deleted orphaned `external/SDL2-2.0.3` and `external/physfs-2.1.1` (1450 files / 428k lines) from the `external` submodule. Linux uses system pkg-config SDL2; Windows cross uses the prebuilt `external/mingw/` bundle; the real PhysFS is `idlib-game-engine/library/physfs-3.0.0`.

### T2.5 Fix Wine font-atlas / audio crash

The historically-observed Wine failure mode is a font atlas init failure in `egolib/Graphics/Font.cpp` and a Wine page-fault inside `Mix_LoadWAV_RW` during audio load. Without a fix, the cross build is not a credible verification substitute — `run-egoboo-windows.sh` currently gates it with `EGOBOO_DISABLE_MIPMAPS=1 EGOBOO_DISABLE_AUDIO=1` as a workaround. (Note: the now-integrated `cartman` editor does boot an OpenGL 4.6 context under Wine, so the GL path is not uniformly broken.)

**Fixed (2026-06-08): the `EGOBOO_DISABLE_MIPMAPS=1` workaround rendered all text/atlas textures as white boxes.** `Renderer/OpenGL/Texture.cpp::load` set `GL_TEXTURE_MIN_FILTER` from the requested sampler's mip filter (a mipmap mode) via `setSampler`, but when `EGOBOO_DISABLE_MIPMAPS=1` it uploaded only the base level — a mipmap min-filter with no mipmap levels is *mipmap-incomplete* and samples white (the white-box font/atlas symptom, confirmed by before/after screenshots on the native Linux menu). Fix: compute `useMipmaps` once and force the effective sampler's `mip_filter_method` to `none` when mipmaps won't be generated, so the min-filter (`GL_LINEAR`) matches the no-mipmap upload. The normal (mipmaps-enabled) path is byte-identical. This makes the `EGOBOO_DISABLE_MIPMAPS` workaround actually usable, which matters for the Wine path (`run-egoboo-windows.sh` sets it). The audio-load page-fault and the underlying Wine mipmap-generation crash that motivates the workaround remain open.

### T2.6 Quarantine legacy platform READMEs — DONE

**Done** (commit `63530491d`): Moved `README.VisualStudio`, `README.Windows`, `README.MinGW`, `README.OSX` to `doc/legacy/`. The canonical docs are `doc/build-linux.md` and `doc/build-windows.md`.

---

## Tier 3 — Deeper structural work

These are unblocked only after Tier 1 lands. They represent the next frontier of maintainability work once the Object/singleton debt is paid down.

### T3.1 Shrink `shared_ptr<Object>` usage — ownership model already sound (low priority)

**Scouting correction (2026-06-06):** the scary premise does not hold against live code. `ObjectHandler`
is already the sole owner (0 co-owning member fields elsewhere); every real observer relationship already
uses `weak_ptr`; the QuadTrees observe via `weak_ptr`; an `ObjectRef`-based non-owning iteration seam
exists and is adopted at ~13 sites (2 passes already landed). Of ~329 live `shared_ptr<Object>` hits, ~220
are zero-cost `const&` borrows. So this has degraded from an ownership redesign to incremental
refcount-churn cleanup (~21 observer loops) — good opportunistic/filler work, not a heavy thrust.

Remaining incremental opportunities:


- Identify which `shared_ptr<Object>` uses are true shared ownership vs. observer patterns.
- Move observer-style uses to `weak_ptr<Object>` or raw non-owning references.
- Consider whether `ObjectHandler` can become the sole owning reference, with all call-site refs becoming non-owning.

### T3.2 Script dispatch → registry model — PREMISE STALE (do not pursue as written)

**Scouting correction (2026-06-06):** dispatch is *already* a registry. `run_function`
(`Script/script.c:680-701`) does an `std::unordered_map<uint32_t, Function*>` lookup
(`script.h:660`) populated from a single `Functions.in` X-macro source of truth; there is no
function-dispatch switch (only 2 incidental switches inside the 7 files). "Extension without touching
the dispatch layer" is already true via `Functions.in`. The one real sliver of value is a
*characterization test* asserting registry completeness/alias-routing (the per-function tests call
`scr_*` directly and bypass the map) — that belongs under T3.4, not here. A self-registration/Command
rewrite would violate Rule 1 for little gain. The 404 functions remain split across seven files for
size reasons only.

### T3.3 Uber-header teardown — DONE (2026-06-07)

**Original framing was stale.** `egolib/egolib.h` (54 subsystem includes) has only ~24 direct includers,
several redundant. The real amplifier is the *thin* `game/egoboo.h`, whose only harm is its
`#include "egolib/egolib.h"` line — and ~55 of its ~60 consumers are **headers** (`Object.hpp`, the 19 role
interfaces, `mesh.h`, `graphic.h`, …), so the 54 subsystems propagate into essentially the whole game
library transitively.

Reframed goal: make every consumer self-sufficient in its egolib includes, then cut the
`egoboo.h → egolib.h` link (`egoboo.h` survives as a thin header). Deleting `egolib.h` outright is a later
optional stretch. Full plan, probe data (185/286 TUs transitively coupled; 37 non-self-contained headers),
the symbol→header dictionary, the `EGOBOO_NO_UBER_INCLUDE` guard, and the per-pass work-list live in
`72-uber-header-teardown.md`.

- **Passes 221–225 (done) — the egoboo.h→egolib.h link is CUT.** `game/egoboo.h` is now a thin game
  header (gfx_rv + gameplay constants + HUD timers + config_synch) and no longer includes `egolib.h`, so
  the game library's headers/TUs no longer transitively inherit the 54-subsystem uber-header. Sequence:
  Pass 221 role interfaces; Pass 222 mesh/graphic/lighting/script leaf headers; Pass 223
  camera/module/physics/inventory headers; Pass 224 the last 10 consumer headers (Object/Particle/
  Billboard/UI/CharacterMatrix); Pass 225 cut the link + fixed the ~20 source/header sites that had
  leeched egolib types through it (incl. a 4-agent parallel workflow for 16 source TUs). All green
  (build/validator/ctest 736/738) at every pass.
- **Pass 226 (done) — `egolib.h` DELETED.** The optional stretch was executed: the remaining 18 direct
  `egolib.h` includers were narrowed to precise includes, the keep-going build's transitive-leech tail (139
  errors across 30 TUs) was fixed with precise includes via a 28-agent parallel workflow, and `egolib.h` +
  its `CMakeLists.txt` entry were physically removed. Build/validator/ctest 736/738/smoke-run all green. **The
  uber-header pattern is now fully gone from the live codebase.** Only the disconnected, unbuildable `cartman`
  (4 files) + `utilities/migrator` (1 file) retain dangling `egolib.h` includes — out of scope (no CMake, not
  built; T3.5), to be fixed if/when those tools are rewired. Full detail: `72-uber-header-teardown.md`.

### T3.4 Behavioral test coverage

Current test-to-code ratio is ~17.5% and covers parsers, module smoke, accessor regressions, script dispatch, gameplay surfaces, physics/collision math, live-Object combat-damage math, collision-pipeline behavior, and the AI LOS/pathing terrain-query contract. Gaps:

- Full combat *integration* (`Object::damage(...)` side effects: life/mana, alerts, particle spawn) — the integrated damage/`kill` path is now pinned by `CombatDamageIntegration.cpp` (6 cases on a live module-spawned object: life subtraction, hurt timer, attack alert + its `careful_timer` gate, lethal `kill`, invictus/dead/zero guards); the `DAMAGEMANA`/`DAMAGECHARGE`/`DAMAGEINVERT` modifier+`heal` branches remain the one uncovered sliver
- ~~Collision *pipeline* behavior (`do_chr_prt_collision` / `particle_collision.c`) — the pure swept-bounds/normal math is covered, the pipeline is not~~ **— now COVERED (2026-06-09, `CollisionPipeline.cpp`, 8 live-spawn cases): the chr-prt damage path (exact life subtraction), re-hit/hasCollided gate, invincible-deflect, friend-foe gate, geometry gate, and chr-chr `CollisionSystem::detectCollision`/mount/platform behavior. This is the characterization net that gates the `ICollisionWorld` mesh-seam extension.**
- Rendering correctness (golden-image or matrix-cache comparisons)
- ~~GUI state transitions~~ — **GUI base classes now COVERED (2026-06-11, `GuiComponentBehavior.cpp`, 44 cases):** `Component`/`Container`/`LayoutColumns`/`LayoutRows` behavior — geometry, the `isEnabled()` visibility gate, closed-interval `contains()`, derived-position chaining, `destroy()` dominance, membership + the `clearComponents()` parent-dangling asymmetry, z-order, and input propagation (reverse-order/first-consumer-wins, disabled-skip, per-nesting-level own-position translation for mouse, no-translation for keyboard/wheel, the not-forwarded `Released`/`Typed`/`Clicked` trio, no hit-testing). This is the **step-1 safety net of the maintainer-chosen "de-risk → carve GameStates" program**: it gates the next link-split (carving the GameStates/menu screens, which derive from `Container`). Per-screen `GameState` *transition* behavior (push/pop lifecycle) remains uncovered.
- Broader AI behavior beyond LOS/pathing terrain queries

Add characterization coverage before the next restructuring wave in each area. (Already landed: physics/collision math, bounding-volume ops, map twist, particle recoil, damage/attribute enums, script loader/VM/dispatch, live-Object combat-damage math, live-Object combat-damage *integration* (`CombatDamageIntegration.cpp`), collision-pipeline behavior (`CollisionPipeline.cpp`), and AI terrain queries (`AITerrainQueries.cpp`) — see `71-completed-passes-log.md`.)

### T3.5 Native-Cartman build integration — DONE (2026-06-07)

`cartman/` (the ~9.3k-LOC SDL map editor) was disconnected from the main CMake graph (no build files at all,
last meaningful change 2017-11-29). **It is now wired in behind `option(EGOBOO_BUILD_CARTMAN OFF)`
(`add_subdirectory(cartman)` at root `CMakeLists.txt:51`) and fully ported to compile + link + run** against
current egolib (60→0 compile, 0 link, ~89 MB exe; ~9,291 LOC / 35 files; the default build is untouched).
Phase 3 runtime-verified: the GUI launch boots an OpenGL 4.6 context and renders all four viewports + the HUD.
The default-flip to ON is deferred (maintainer's call) and a pre-existing no-arg `atexit`/VFS crash remains open.
Full record: `73-cartman-build-integration-scouting.md`.

**Original scouting verdict (historical; full compile-probe data in `73-...`): feasible, MEDIUM effort, low
architectural risk — a port, not a rewrite.** Against current egolib it had produced
719 compile errors across 11/16 TUs, but that is mostly *cascade*: the genuine root surface is ~30 errors in
just 5 headers (14/19 headers + the entire core data/math model already compile clean), dominated by ~4
**mechanical systematic renames** (`id::`→`idlib::`, bare `singleton<>`→`idlib::singleton<>`, bare math types →
`Ego::`-qualified, `Ego::Math::Colour4f`→`Ego::Colour4f`) plus a residual of genuine egolib-API-drift fixes
(GraphicsWindow/window-size, ImageManager, gfx/mesh accessors) concentrated in `cartman_gfx.c`/`cartman_gui.c`/
`cartman.c`. The CMake target is trivial (links only `egolib-library`; gate `option(EGOBOO_BUILD_CARTMAN OFF)`).
Main risk: **no automated runtime verification** (GUI editor needs a display + a module). Doing this also
resolves the 4 dangling `egolib.h` includes left in cartman by Pass 226.

### T3.6 `vfs.c` dead-backend elimination — DONE (2026-06-07)

`vfs.c` (the largest non-Object TU, 2,456 lines) carried a fully-dead `cstdio` backend: `vsf_file` was a
discriminated union over `VFS_FILE_TYPE_CSTDIO` (a libc `FILE *`) vs `VFS_FILE_TYPE_PHYSFS`
(`PHYSFS_File *`), but `VFS_FILE_TYPE_CSTDIO` is **never assigned anywhere in the tree** — only PHYSFS is.
Surfaced by the 2026-06-07 next-heavy-front scouting workflow as the highest-value remaining structural
thrust (heavy, fully headless-verifiable, low-risk) after the Object/singleton, uber-header, and CameraSystem
fronts were exhausted. Eliminated in **three verified passes** (commits `3e336393b` / `043da643f` /
`0381d371e`; full detail in `71-completed-passes-log.md`):

- **Pass 1** — deleted the 33 dead `if (VFS_FILE_TYPE_CSTDIO == ...)` branches across ~22 functions plus
  the CSTDIO enum value and the union's `FILE *c` member (−224 lines).
- **Pass 2** — collapsed `vsf_file` from a discriminated union to a plain `{ BIT_FIELD flags; PHYSFS_File *p; }`:
  dropped the now-single-valued `type` field, the `vfs_file_type` enum, the union, the 32 always-true PHYSFS
  guards, and the unreachable corrupted-`else` (−55 lines).
- **Pass 3** — deduped the 18 fixed-width `vfs_read_*`/`vfs_write<T>` helpers behind one `vfs_finish_io`
  error-handling tail (PHYSFS calls kept explicit per width), and fixed the latent `sizeof(int8_t)` in
  `vfs_read_Uint8` (−256 lines).

Net: **2,456 → 1,922 lines (−534, ~22%)**, zero caller churn (the `vsf_file` struct is opaque — `vfs.h:187`),
behavior byte-identical. Each pass green on build + validator (`test.mod` 0/0, full errors=245 baseline) +
ctest 748/750; the cumulative change confirmed by a clean menu smoke-run (exit 124). **Optional deferred
follow-on:** a Pass 4 RAII wrapper for the residual `vsf_file` (ctor opens / dtor `PHYSFS_close`, eliminating
the manual `delete file`) — touches lifetime/ownership, so it would need an `AGENTS.md`-mandated note + smoke-run.

### T3.7 egolib include-level decoupling (logging seam) — DONE for the logging slice (2026-06-08)

The blocker to ever splitting the monolithic `egolib-library` into idlib-shaped sub-libraries is **include coupling, not CMake**. The dominant directional violation is the app-layer service hub `game/Core/EngineContext.hpp`, into which **51 non-game leaf TUs reached UP** (measured), a large share for nothing but `EngineContext::get().logTarget()`. A fresh `scout-next-heavy-front` workflow picked this as the only remaining candidate that is heavy AND fully headless-verifiable AND low-risk AND not stale.

Resolved the **logging slice** in three verified passes (commits on branch `refactor/egolib-include-decoupling`; full detail in `71-completed-passes-log.md`):

- **Passes 1–2** retargeted the 17 leaf TUs whose *only* EngineContext use was logging onto the existing lower-layer `Log::activeTarget()` seam (`egolib/Log/_Include.hpp`) and dropped the `EngineContext.hpp` include. Behavior-identical (the seam resolves through the same installed pointer, adding only a default-target fallback in the uninstalled edge). **51 → 34 leaf→hub upward includes (−17, −33%).**
- **Pass 3 (keystone)** moved the engine-installed `activeLogTarget` ownership out of `EngineContext.cpp` and into `Log/_Include.cpp` (`g_activeTarget` + `Log::installActiveTarget`/`clearActiveTarget`/`tryInstalledTarget`). `Log/_Include.cpp` no longer includes `EngineContext.hpp` — **the Log subsystem no longer reaches into `game/` anywhere and is now a clean downward leaf** (link-cleavable); this also cut the 18th leaf→hub edge (its own), bringing the count to **34 → 33**. `EngineContext`'s log methods became thin downward delegators (declarations unchanged → all ~120 `EngineContext::get().logTarget()` callers keep working; every throw/nullptr semantic preserved, incl. the `vfs.c` bootstrap guard via the new raw `tryInstalledTarget()`).

All passes green: build + validator `test.mod` 0/0 + ctest 798/800; Pass 3 also smoke-run-verified (clean OpenGL/image/font/audio boot + shutdown).

**Service-hub slice — DONE (2026-06-08, branch `refactor/egolib-service-hub-decoupling`, not yet merged).** The continuation onto the *service* hub. The 33 remaining leaf includers were genuine service-hub users (`profileSystem`/`config`/`particleHandler`/`imageManager`/`audioSystem`); eleven verified passes cut them to **8 (−25, −76%)**. The maintainer chose a **free-function `active*()` accessor** seam (uniform with `Log::activeTarget()`), which also *reduced* the tracked `::get()` count (egolib **895 → 794**) rather than trading it off. Two seam patterns: *sugar over an existing lower-layer singleton* for `profileSystem`/`imageManager` (no ownership to move), and the *Log-style ownership-move keystone* for `config`/`particleHandler`/`audioSystem` (installed pointer relocated into the subsystem; `EngineContext` install/clear/try become thin delegators, `.hpp` unchanged). The keystone is **mandatory for particle and audio**: their tests install a recording stub through `EngineContext` and assert the subsystem routes through it, so a `Singleton::get()` swap would bypass the stub (the particle/audio sentinels — 8 `ModuleUpdate` + 27 `ScriptActionFunctions`/`ConfigMutations`/`ContentParsers`/`GameplayAlert` tests — gate this). Config, particle, and audio keystones were each smoke-run verified. Full per-pass detail in `71-completed-passes-log.md`.

**Remaining 8 — genuinely blocked or out of scope** (not easy follow-ons): `App.cpp` + `Core/System.cpp` are the **bootstrap installers** publishing services *into* the hub (legitimate downward direction — exclude); `Object_attributes`/`Object_combat` need a `billboardSystem` seam (not an `idlib::singleton`, owned in `game/` — requires relocating ownership out of `game/`); `ObjectProfile_export`/`ObjectProfile_load`/`Object_attributes` need a `perkHandler` keystone (stub-hazard service, low ROI — fully drops only `ObjectProfile_export`); `Console.cpp` needs `fontManager`/`graphicsSystem`/`inputSystem` keystones (graphics/input carry stub hazards); and **`Object_internal.h` is not a clean leaf** — it pulls ~12 `game/` headers (`GameEngine.hpp`, `PlayingState.hpp`, `CameraSystem.hpp`, `Billboard.hpp`, …) of which `EngineContext.hpp` is one, so the Object_* implementation cluster is genuinely game-coupled and belongs to a **separate, larger Entities↔game decoupling front** — now the next lib-split blocker ahead of the EngineContext fan-in. The deferred logging endpoint (migrate the ~115 intra-layer game `logTarget()` callers and delete `EngineContext`'s log API) remains low value.

**Billboard + perk fan-in keystones — DONE (2026-06-09, branch `refactor/billboard-enginecontext-seam`).** Knocked the "genuinely blocked" set down by building the two named keystones. The `billboardSystem` seam was built (relocate `IBillboardSystem.hpp` → `egolib/Graphics/` + ownership-move accessor `Ego::Graphics::activeBillboardSystem` + `EngineContext` delegation) and the `perkHandler` keystone (ownership-move accessor `Ego::Perks::activePerkHandler`; `IPerkHandler.hpp` was already lower-layer). Both are **mandatory ownership-moves** (their tests install stubs through `EngineContext` and assert routing — a `Singleton::get()` swap would bypass the stub). Then migrated the upward leaf callers: **`Object_combat`, `Object_attributes`, `ObjectProfile_export`, `ObjectProfile_load` are now all `EngineContext.hpp`-free** (`Object_attributes`/`load` also used the already-existing `Log::activeTarget()`/`activeConfig()`/`activeProfileSystem()`/`tryActiveAudioSystem()` seams). **Non-game leaf includers 8 → 4**, the floor: `App`/`System` (bootstrap installers), `Console` (3-keystone, low ROI), `Object_update` (genuine `tryActivePlayingState` game-state dep). Five verified passes; ctest 815/817, smoke clean. The Entities↔game `.cpp` internal-header slice the old note named as "next" also landed the same day (see the dedicated row above + doc 74). Full detail in `71-completed-passes-log.md`.

**Entities ↔ game propagating-header slice — DONE (2026-06-08, branch `refactor/entities-game-decoupling`, not yet merged).** The next lib-split blocker the service-hub slice named. Attacked the four *propagating* Entities headers (`Common.hpp`, `IRenderable.hpp`, `Object.hpp`, `Particle.hpp`) whose `game/` includes flow into every consumer. Six verified passes cut their `game/` edges **15 → 7 (−53%)**: `Common.hpp` and `IRenderable.hpp` are now **fully game-free**, and the 7 survivors are all genuine by-value compositions / base classes. The work also relocated three mislocated lower-layer artifacts (`ONESECOND` → `egolib_config.h`; `GLvertex` → `egolib/Graphics/Vertex.hpp`; the physics primitives `orientation_t`/`apos_t`/`phys_data_t` → new `egolib/PhysicsData.h`) and made ~11 free-rider TUs include-what-they-use. **Key technique:** removing a dead-in-header include exposes downstream TUs that free-ride on the transitive conduit — enumerate them with a *keep-going* build (`-- -k`) and IWYU-fix each. Full detail in `74-entities-game-decoupling.md` + `71-completed-passes-log.md`. **Deferred (out of incremental scope):** the by-value composition core (relocating game service classes down / inverting ownership = flag-day scale); ~~the `Collidable` base → `Module.hpp` edge~~ **— CUT 2026-06-08**: a cheap conduit swap (`Collidable.hpp` `Module.hpp`→`mesh.h`, full def confined to `Collidable.cpp`) removed it without an `ICollidable` extraction, exposing a 32-TU free-rider fan-out that was IWYU-fixed. Then ~~the residual `mesh.h` edge~~ **— also CUT 2026-06-08**: `Collidable.hpp` is now **fully game-include-free** (lower-layer `Mesh/Info.hpp`/`integrations/math.hpp`/`typedef.h` + a forward-declared `mesh_wall_data_t`; 3 free-riders IWYU-fixed). `Object.hpp`/`Particle.hpp` game/ closures dropped **14→5 / 13→4** across those two passes (the whole `game/Module/` subtree + `mesh.h`/`lighting.h`). Then ~~the `activeModule()` position-validation dependency~~ **— also DECOUPLED 2026-06-08**: extracted a lower-layer `Ego::Physics::ICollisionWorld` DIP seam (`isInside`+`getTileIndex`, installed `active*()` accessor) implemented by `GameModule` and installed across the `_activeModule` lifetime; `Collidable.{hpp,cpp}` then `git mv`'d to `egolib/Physics/` (with `ICollisionWorld`) — `Collidable` is now a **fully lower-layer, game-symbol-free component**. **Across the whole Collidable front: `Object.hpp` game/ closure 14→4, `Particle.hpp` 13→3** (remaining edges are the by-value composition members + `CharacterMatrix.h`/`egoboo.h`). Behavior-identical (ctest 815/817, menu smoke-run clean). **Next toward an `egolib-physics` link target:** the sibling physics TUs (`CollisionSystem`/`ObjectPhysics`/`ParticlePhysics`/`particle_collision`) are still game-coupled; an actual library split also remains gated on the deeper Entities↔game `.cpp` front (the non-propagating internal headers + impl `.cpp`s).

**egolib-physics decoupling front — bounded wins DONE (2026-06-09, branch `refactor/egolib-physics-decoupling`).** Five verified passes paid down the `game/Physics/` → `game/` coupling and grew the lower-layer nucleus. Pass 1 relocated the pure-data `PhysicalConstants.hpp` down to `egolib/Physics/` (self-contained; **fixed 5 Entities/Profiles → game/Physics upward violations**) and Pass 3 moved its `g_environment` storage into a new `egolib/Physics/PhysicalConstants.cpp` — the **nucleus is now Collidable + ICollisionWorld + PhysicalConstants, fully lower-layer**. Passes 2/4/5 migrated **all four impl TUs off EngineContext** (and `ObjectPhysics` off `GameEngine` too) onto the `active*()` seams (`activeParticleHandler`/`activeBillboardSystem`/`activeProfileSystem`/`activeAudioSystem`; `worldUpdateCount()`; `ONESECOND`). Physics-TU `game/` includes **26 → 19**; egolib `::get()` **752 → 725**; behavior-identical (ctest 815/817, smoke clean). **The remaining hard core is the real link-split blocker:** the `GameModule` mesh queries (`getMeshPointer()`→`get_twist`/`getElevation`/`test_fx`) + `GameSessionContext::activeModule()` + `game.h`/`graphic.h`/`physics.h`/`Shop`/`CharacterMatrix`. Decoupling the mesh queries means **extending the `ICollisionWorld` DIP seam** — now **unblocked**: the gating collision-*pipeline* characterization net landed the same day (`CollisionPipeline.cpp`, 8 live-spawn cases pinning `do_chr_prt_collision` damage/gates + `CollisionSystem` detect/mount/platform). Full detail in `71-completed-passes-log.md`.

**Entities ↔ game `.cpp` / internal-header slice — DONE (2026-06-09, branch `refactor/entities-game-cpp-decoupling`).** The deeper `.cpp` front the propagating-header + Collidable work named as next. Attacked the **shared internal infrastructure headers** `Object_internal.h` (included by the 7 split `Object_*.cpp`) and `Particle_internal.h` (included by the 4 `Particle_*.cpp`) — *semi-propagating* (a `game/` include flows to every sibling impl TU, though not tree-wide). Six verified passes (A/E/F/G/H + Pass C/D analysis): classify each header `game/` include as **header-used** (the inline `object_detail`/`particle_detail` helpers genuinely need it) vs. **conduit-only** (present only for consumer free-riding), then push the conduit-only ones **down to the precise consumer** via IWYU, enumerated with a keep-going build. Cut `Object_internal.h` **12 → 2** and `Particle_internal.h` **5 → 2** game/ includes — both now pull from `game/` only the two headers their helpers use (`GameSessionContext.hpp` + `Module.hpp`); all thirteen conduit-only edges (incl. the `game.h` gravity-well, the minimap reveal chain `EngineContext`/`PlayingState`/`MiniMap`, and a now-dead `GameEngine.hpp`) left the propagating surface. Pass A also routed the last four `GameEngine::GAME_TARGET_UPS` Entities sites onto the low-layer `ONESECOND`. Byte-identical; ctest 815/817, menu smoke-run clean. **Pass D analysis:** the `IBillboardSystem` seam is ready (interface already lower-layer-portable + `EngineContext`-published) via the proven Log/`ICollisionWorld` ownership-move keystone — but it belongs to the **EngineContext fan-in front**, where (with `config`/`logTarget` already seamed) `Object_combat.cpp` is one billboard keystone + one `config()`→`activeConfig()` away from `EngineContext.hpp`-free. **Next:** the billboard/perk EngineContext-fan-in keystones; then the deferred hard core (by-value `ObjectPhysics`/`ObjectGraphics`/`ParticleGraphics` members + `activeModule()` object-lifetime/spawn = ownership-inversion scale). Full detail in `74-entities-game-decoupling.md` + `71-completed-passes-log.md`.

**collisionworld-mesh-seam front — DONE (2026-06-09, branch `refactor/collisionworld-mesh-seam`).** The egolib-physics front's named "next heavy thrust" — extend the `ICollisionWorld` DIP seam to cover the `GameModule` mesh queries — now executed (unblocked by the `CollisionPipeline.cpp` net, whose own header comment was written to gate it). Four verified passes: **(1)** relocated the pure-data `MeshLookupTables`/`g_meshLookupTables` twist tables down from `game/mesh.h`/`mesh.c` to a new lower-layer `egolib/Physics/MeshLookupTables.{hpp,cpp}` (its own comment already said *"should be in map, not in mesh"*; `game/mesh.h` re-includes so non-physics users are untouched); **(2)** widened `ICollisionWorld` from `isInside`/`getTileIndex` to the full terrain surface (`gridIsValid`/`getTwist`/`getFanTwist`/`testFX`/`getElevation`×2/`isWater`), `GameModule` implementing each as a thin mesh/water forwarder; **(3–4)** migrated all 8 `ObjectPhysics.cpp` + 9 `ParticlePhysics.cpp` `getMeshPointer()->…`/`getWater()._is_water` sites onto `activeCollisionWorld()`. Behavior-identical (the installed collision world IS the active `GameModule`); ctest -j1 **823/825**, validator `test.mod` 0/0 (`errors=245` baseline), menu smoke clean. The lower-layer physics nucleus is now **Collidable + ICollisionWorld (terrain-complete) + PhysicalConstants + MeshLookupTables**, and neither physics TU calls a mesh method directly. This is **API/data decoupling, not include reduction**: both TUs still need `Module.hpp` for `getObjectHandler()`/`getTeamList()`, so the `game/` include count is unchanged — that drop is gated on the **deferred entity-world strand** (seaming `ObjectHandler`/object-lifetime/spawn = ownership-inversion scale), which is now the remaining hard core for the `egolib-physics` link split. Full detail in `71-completed-passes-log.md`.

**game.h directional-decoupling front — DONE (2026-06-09, branch `refactor/game-h-decoupling`).** A four-angle scout workflow over the next-front candidates found `game.h` is the **single heaviest directional include violation in egolib** — a 272-line grab-bag whose include block drags a heavy conduit (`EngineContext.hpp`/`mesh.h`/`Inventory.hpp`/`Shop.hpp`/`AnimatedTiles`/`Water`/`Weather`/`Fog`) into 11 lower-layer consumers; and its dominant consumers are *genuinely-lower-layer* `Entities/` TUs reaching UP (a true directional violation, unlike the physics-TU files which already live inside `game/Physics/`). Three verified passes via the thin-header/IWYU technique: **(1)** dropped the one genuinely-stale `game.h` include (`Script/script.c`); **(2, keystone)** extracted the game-free `egolib/game/CharacterParticleOps.h` (the char/particle-op entry points the Entities layer calls back into — all lower-layer-typed signatures), moved those decls out of `game.h` (which re-includes it), and routed the **8 `Entities/` TUs** onto it (keep-going build found zero free-riders; verified via ninja deps DB that `game.h`/`EngineContext.hpp`/`Shop.hpp` left their transitive sets); **(3)** relocated the self-contained `namespace Zeitgeist` (special-time checks) down to lower-layer `egolib/Zeitgeist.{hpp,cpp}` and took `AudioSystem.cpp` off `game.h` (adding the one `EngineContext.hpp` it genuinely used); **(4, review follow-up)** an adversarial review caught that `particle_collision.c` also used only thin-header symbols, so routed it too. **`game.h`'s includers outside `egolib/game/` dropped 11 → 1** (only `ProfileSystem.cpp`, genuine `MAX_IMPORT` macros). Build 0 / ctest -j1 **823/825** / validator `test.mod` 0/0 / menu smoke clean; the 4-dimension review was clean/high-confidence on behavior, layering, and breakage. *Two verification lessons recorded:* (a) the scout's "stale include" claim for `ProfileSystem.cpp` was wrong — it uses `game.h`'s `MAX_IMPORT_*` **macros** (the per-function-symbol grep missed them); always check the macro surface; (b) the front's own "particle_collision.c genuinely needs game.h" claim was wrong — the review's exhaustive symbol grep caught it. **Deferred:** the `Entities/` `GameSessionContext.hpp`+`Module.hpp` edges (entity-world ownership inversion); the `Billboard::Flags` enum edge (future thin-enum header); relocating `MAX_IMPORT_*` to free the last consumer. Full detail in `71-completed-passes-log.md`.

**IObjectWorld entity-world seam — DONE (2026-06-09, branch `refactor/iobjectworld-seam`).** An 8-way scout-next-heavy-front workflow picked this as the heaviest non-stale continuation toward an `egolib-physics` link target: the four `game/Physics/` TUs held the heaviest remaining Entities↔game *runtime* coupling, reaching entity state through `GameSessionContext::activeModule().getObjectHandler()`/`.getTeamList()`. Five verified passes mirror the proven `ICollisionWorld` ownership-move keystone: **Pass 0** added the lower-layer `Ego::Entities::IObjectWorld` (`egolib/Entities/IObjectWorld.{hpp,cpp}` — a 2-method interface returning the already-lower-layer `ObjectHandler&`/`std::vector<Team>&`, so it drags in **no `game/` header**), `GameModule` now also implements it (its existing accessors became `override`s), and `GameSessionContext::{beginModule,quitModule}` install/clear it across the `_activeModule` lifetime alongside the collision world. **Passes 1–4** migrated `ParticlePhysics`/`ObjectPhysics`/`CollisionSystem`/`particle_collision` (47 `getObjectHandler()` + 1 `getTeamList()` sites total) onto a file-local `objectWorld()` → `activeObjectWorld()` helper. **`game/Module/Module.hpp` is now gone from all four physics TUs** (`ParticlePhysics` fully shed `GameSessionContext.hpp` too; the other three keep it only for the unseamed `worldUpdateCount()` strand). Keep-going builds IWYU-fixed the mesh-constant free-riders (`TWIST_FLAT`/`MAPFX_SLIPPY` → `map_fx.hpp`, `Info<float>::Grid::Size` → `map_file.h`). Behavior-identical (the installed object world *is* the active `GameModule`), gated by the existing `CollisionPipeline.cpp` + `CombatDamageIntegration.cpp` nets (no new test needed). Build 0 / ctest -j1 **823/825** / validator `test.mod` 0/0 / menu smoke clean at every pass. **Scout correction recorded:** the documented "quick `egolib-physics` CMake carve" is **not** clean — the lower-layer nucleus (`Collidable`/`ICollisionWorld`/`MeshLookupTables`/`PhysicalConstants`) is include-clean against `game/` but **not link-symbol closed** (`MeshLookupTables.cpp` → `twist_to_normal` in `map_functions.c` → `Log`/`FileFormats`/`Mesh`, + egolib `Math` symbols), so a naive `add_library(egolib-physics-library)` is circular. **Next front:** a dependency-closed *foundation* carve (the lower edges) is the remaining gate on an actual `egolib-physics` link split; the `worldUpdateCount()` strand (an `activeWorldUpdateCount()` seam) and the `game/physics.h` free-function edge are smaller follow-ons. Full detail in `71-completed-passes-log.md`.

**egolib-foundation carve — DONE: the first real link-split (2026-06-09, branch `refactor/egolib-physics-nucleus-carve`).** The continuation the IObjectWorld entry named ("a dependency-closed *foundation* carve is the remaining gate on an actual `egolib-physics` link split"). Eight verified passes. **`egolib-library` is no longer one monolithic STATIC archive — it is split into `egolib-foundation-library` (77 TUs) + `egolib-library` (215 TUs), one-way dependent and verified acyclic (0 cycle edges by nm proof).** This is the first genuine link-level modularization of egolib (idlib's eleven sub-libraries are the target). Passes 1–4 closed the named follow-ons (the `activeWorldUpdateCount()` seam in Pass 1 — done as a **dedicated free function**, not an `IObjectWorld` method, to avoid shadowing the `module_detail::worldUpdateCount()` helper; the `game/physics.h` relocation in Pass 2) **and** produced the **nm symbol-closure proof** that the documented nucleus-only carve is *circular* (its foundation/Entities symbols live in the monolith, which depends back on `Collidable`) — so the real first acyclic split is a foundation library. An nm-fixpoint over the 294 `.o` found the maximal dependency-closed set (47 TUs); two code seams (**5a** Time→`SDL_GetTicks()` off `Core/System`; **5b** `ego_texture_exists_vfs` `fileutil`→`Image`, un-cascading the FileFormats parsers and pulling in the clean Script DDL/PDL lexer) grew it to **77 TUs**; **5c** carved it via `list(REMOVE_ITEM)` + two `add_library()`, consumers unchanged (INTERFACE). Verified in-place + from-scratch clean builds (77+215 objects, egoboo built), ctest -j1 **823/825**, validator `test.mod` 0/0, smoke clean. **Foundation set:** whole Math/Log/Mesh/VFS/Time/FileFormats/Platform + the Physics nucleus + the Script DDL/PDL lexer (minus `script.c`) + `Logic/TreasureTables` + toplevel math/IO; genuinely-higher TUs (Image impl→`Graphics/PixelFormat`, `font_bmp`→`Renderer/Texture`, `physics.c`→Entities, `script.c`) correctly stay in `egolib-library`. The follow-on `Core/System` bootstrap edge is now resolved by the 2026-06-10 seam-swap; the larger ownership-inversion fronts remain deferred. Full detail in `71-completed-passes-log.md`.

**egolib-physics middle-layer carve — the second link-split (2026-06-09, branch `refactor/egolib-physics-middle-carve`).** The foundation carve's named continuation, picked + nm-verified by a scout workflow (both adversarial physics skeptics returned `refuted:false`; synthesis reproduced the closure on live artifacts). **`egolib-foundation-library` (77 TUs) is re-split into `egolib-foundation-base` (73) + `egolib-physics` (4: `Collidable`/`ICollisionWorld`/`MeshLookupTables`/`PhysicalConstants`)**, giving the acyclic three-layer chain **`egolib-foundation-base ← egolib-physics ← egolib-library`** — landing the `egolib-physics` link target named across ~6 prior fronts. A **pure CMake partition, ZERO source edits.** nm acyclicity proof (Python set math on mangled symbols, fresh artifacts): physics references only base symbols (`vec_to_facing`/`twist_to_normal`) + 2 intra-nucleus, **0** into the upper library; **0** base→physics back-edges; base needs **0** physics/library symbols; a positive control confirmed the 0-results are real (not a false-empty closure). `game/physics.c` correctly stays upper (needs `Ego::Particle::isTerminated`). Target rename safe (the name was referenced only in this CMakeLists); consumers untouched (INTERFACE + transitive link). All gates green: from-scratch build, `ar t` 73/4/215, nm acyclic, validator `test.mod` 0/0, ctest -j1 **823/825**, menu smoke clean (exit 124). Full detail in `71-completed-passes-log.md`.

**egolib-foundation-base growth absorptions — InputControl + Image (2026-06-09, branches `refactor/egolib-inputcontrol-foundation-absorb`, `refactor/egolib-image-foundation-absorb`).** The two nm-pre-verified foundation-growth fronts the physics carve queued, each a pure CMake source-list move (identical object code, just a different archive) verified by the same set-intersection method (a TU moves down safely iff it references **0** library-only symbols, excluding symbols defined within the moving group). **(1) InputControl** — moved the 3 `InputControl/*.cpp` (0 blockers; the `game → InputControl` edge becomes a clean downward edge). **(2) Image + `Graphics/PixelFormat.cpp`** — moved the 6 Image TUs **together with** `PixelFormat.cpp` (Image alone has 6 `pixel_descriptor` blockers all defined in `PixelFormat.cpp`; as a unit, 0). SDL2_image needs no CMake change — `IMG_*` already resolves via `idlib-game-engine-library`, which `egolib-foundation-base` PUBLIC-links. Cumulative membership **base 73 → 83, library 215 → 205** (physics 4 unchanged); each landed acyclic (live positive controls fired) with the full gate (build / `ar t` / nm / validator 0/0 / ctest -j1 823/825 / smoke exit-124). Follow-on fronts at that point were the `game/mesh.c` `ego_mesh_t` chokepoint, the `Core/System` bootstrap edge (now resolved below), and the `physics.c`/Entities ownership-inversion (flag-day scale). Full detail in `71-completed-passes-log.md`.

**egolib frontier absorption — the third link-split: egolib-renderer (2026-06-09/10, branch `refactor/egolib-frontier-absorb`).** A 5-scout workflow (fresh nm fixpoint over all 205 library TUs + per-front deep scouts + a generalist sweep) found the absorbable frontier was far larger than the queued quick carves: **60 TUs with 0 cumulative blockers**, verified by six adversarial skeptics (all `refuted:false` at high confidence, each with an independent from-scratch nm parser reproducing the numbers on live artifacts). Seven passes: **P1–P4** moved 31 TUs into `egolib-foundation-base` (Profiles data/writers, the IObjectWorld/IBillboardSystem/IPerkHandler seam interfaces, the MD2 model cluster, the texture/font cluster — retiring the documented `font_bmp` blocker by co-moving `Renderer/Texture.cpp` — and the Perk/profile core; 83→114). **P5–P6** carved the **third link-split**: a new `egolib-renderer` STATIC archive (SDL display/windowing + the OpenGL backend, 29 TUs), a verified-acyclic **sibling** of `egolib-physics` (zero symbol edges between them in either direction; consumers unchanged — `egolib-library` PUBLIC-links the new target). **P7** retired the documented `physics.c` blocker by inlining `Particle::isTerminated()` into `Particle.hpp` (the front's only source edit, semantics-identical) and moved `physics.c`+`Entities/Common.cpp` into `egolib-physics` (4→6). Layout after this front: base 114 ◄ {physics 6, renderer 29} ◄ library 143. Full gate green at every pass (build / `ar t` exact / nm acyclic + live positive controls / validator `test.mod` 0/0 / ctest -j1 823/825 / menu smoke exit-124) plus a 4-dimension adversarial review (CMake/layering, behavior preservation, from-scratch rebuild, Windows cross-build) before merge. **Terminal finding at that point:** move-only absorption was exhausted except for seam-cut satellites; the largest next fronts were the mesh-AI chain, `Core/System.cpp` seam-swap, and the questlog chain, while the Entities ownership-inversion re-measured at **123 blockers — flag-day, do not attempt incrementally**. Side-find: a **4th racing test fixture** (shared `.egoboo-runtime/user`, no env override at `file_linux.c:190-200`) — fixing fixture isolation would make `ctest -j20` trustworthy and cut ~70 s per gate cycle. Full detail in `71-completed-passes-log.md`.

**Core/System bootstrap seam — DONE (2026-06-10).** The small seam-swap named above has now landed. `Core/System.cpp` routes directly through `Log::installActiveTarget`/`clearActiveTarget` and `Ego::installActiveConfig`/`clearActiveConfig`/`activeConfig`, drops `game/Core/EngineContext.hpp`, and moves into `egolib-foundation-base`. Archive layout after this pass: **base 115 ◄ {physics 6, renderer 29} ◄ library 142**. Gates green: build, exact `ar t` counts, aggregate nm acyclic with live positive controls, validator `test.mod` 0/0, ctest -j1 823/825, menu smoke exit-124 clean.

**mesh-AI terrain seam — DONE (2026-06-10).** The mesh-AI chain's first seam-cut landed by introducing lower-layer `Ego::Mesh::ITerrainQuery` (`getTileIndex(Index2D)`, `testFX`, `tileHasBits`, `isFanOff`) and making `GameModule` implement it. `AStar`/`LineOfSight` now depend on that interface instead of `ego_mesh_t` / `game/mesh.h`, and their runtime call sites pass the active module as a terrain view. `AI/AStar.cpp` and `AI/LineOfSight.cpp` moved into `egolib-foundation-base`. Added `AITerrainQueries.cpp` (5 cases) for LOS and A* terrain behavior.

**QuestLog foundation absorb — DONE (2026-06-10).** Seam-cut `QuestLog.cpp` (single blocker: `EngineContext::get().logTarget()` → `Log::activeTarget()`) and moved `QuestLog` + `PlayerQuestLog` (2 TUs) into `egolib-foundation-base`. Current archive layout: **base 119 ◄ {physics 6, renderer 29} ◄ library 138**. Gates green: build, exact `ar t` 119/6/29/138, aggregate nm acyclic (mangled, all 7 back-edges = 0, positive controls fired), validator `test.mod` 0/0, ctest -j1 828/830, menu smoke exit-124 clean.

**Broader `getMeshPointer()` cleanup — DONE (2026-06-10).** Migrated 17 `getMeshPointer()` call sites off direct `ego_mesh_t` access onto `ICollisionWorld` / `ITerrainQuery` seam interfaces, reducing impl-file call sites from 32 to 15. Widened `ICollisionWorld` with 4 map-dimension accessors (`getEdgeX`, `getEdgeY`, `getTileCountX`, `getTileCountY`). Removed 1 dead-code site (`DEBUG_WAYPOINTS` block in `script_implementation.c`). Remaining 15 sites are intentionally deferred: wall collision (6, blocked by `mesh_wall_data_t`), mutating tile operations (8, game-layer-only, no layering benefit), and the `mesh()` forwarder (1, infrastructure). Gates green: build / ctest -j20 830/830 / validator `test.mod` 0/0 / menu smoke clean exit. Full detail in `71-completed-passes-log.md`.

**IGraphicsSystem widening — DONE (2026-06-10).** Widened `IGraphicsSystem` with `setCursorVisibility`, `update`, `getDisplays`. Implemented in `GraphicsSystem` by delegating to `GraphicsSystemNew::get()`. All 6 game-layer `GraphicsSystemNew::get()` callers migrated to `EngineContext::get().graphicsSystem()`. `GraphicsSystemNew::get()` now confined to `GraphicsSystem.cpp` (bootstrap/delegation only). Five files dropped their `GraphicsSystemNew.hpp` include. Gates green: build / ctest -j20 830/830 / validator `test.mod` 0/0 / menu smoke clean exit.

Remaining ranked fronts: `video_buffer_manager` (12 sites, idlib external singleton, all rendering — low ROI), `Console` (4 game-layer sites — low ROI), `TLT` (4 header-inline sites, const lookup table — zero ROI), and the Entities ownership-inversion (flag-day scale, 123 blockers).

**GameStates EngineContext seam (Pass 1) + egolib-gamestates carve — the SIXTH link-split, ABOVE library — DONE (2026-06-11).** Executed the "de-risk → carve GameStates" program to completion, and in doing so **refuted the strategic note's own "Front B blocked at flag-day scale" verdict** (see the correction below). **Pass 1:** migrated the 10 menu screens off `EngineContext::get().{config,graphicsSystem,profileSystem,textureManager}()` onto the lower-layer `active*()` seams, adding the missing `Ego::activeTextureManager()` ownership-move seam (mirrors `IGraphicsSystem.cpp`; the texture-manager service interface gains its free-function accessor, `EngineContext` delegates). **The carve:** extracted **`egolib-gamestates`** (19 concrete screen TUs) as a STATIC archive **above** `egolib-library` — the first upward split. The key reframe: GameStates is topologically at the *top*, so it belongs ABOVE library (screens reach DOWN into game-core), not below; the blocker is then the small set of `library → screen` *reverse* edges (nm-measured: just **7 staying TUs / 8 symbols**, all `PlayingState` HUD/status reach-ins + `VictoryScreen`/`MainMenuState` construction + a `typeid PlayingState`), **not** the 63 forward edges the strategic note measured. Three seam-cuts removed all reverse edges: (1) a lower-layer **`IPlayingStateController`** interface (`getMiniMap`/`getMessageLog`/`getStatusCharacterRef`/`addStatusMonitor`/`displayCharacterWindow`/`endModuleInVictory`) that `PlayingState` implements; `GameEngine::getActivePlayingState()` + `EngineContext::{try,}activePlayingState()` now `dynamic_pointer_cast<IPlayingStateController>` (source/target typeinfos both lower-layer → no concrete-`PlayingState` link edge); all 7 consumers (`Object_update`, `Player`, `game_loop.c`, `script_functions_{appearance,quests,commerce}.c`, the `game_internal.h`/`script_functions_internal.h` wrappers) route through it; (2) a `GameEngine` **main-menu-state factory** (`std::function<std::shared_ptr<GameState>()>`) injected from `egoboo/Main.cpp`, so `GameEngine` no longer `#include`s/constructs `MainMenuState` (guarded with a clear `std::logic_error` if uninstalled); (3) `script_functions_commerce.c`'s `pushModuleEndVictoryScreen()` routes through `IPlayingStateController::endModuleInVictory()` instead of constructing `VictoryScreen`. The `GameState` base stays in `egolib-library`. Layout: **base 145 ◄ {physics 5, renderer 28 ◄ gui 22} ◄ library 98 ◄ gamestates 19.** Gates green at every step: in-place + from-scratch builds, exact `ar t` counts, nm-acyclic (`egolib-library → egolib-gamestates` = **0**; positive control `gamestates → library` = 67; all four lower archives → gamestates = 0), validator `test.mod` 0/0, ctest **875/875**. A 4-lens adversarial review (behavior/layering/cmake/completeness) returned **zero confirmed high/critical**; its medium/low findings (the factory landmine → now guarded; a gratuitous cartman link-altitude bump → reverted to `egolib-library`; the VictoryScreen guard + a stale doc comment → documented) were all addressed. Full detail in `71-completed-passes-log.md`.

**egolib-scriptvm carve — the SEVENTH link-split, a SIBLING of gamestates ABOVE library — DONE (2026-06-11).** Acted directly on the strategic note's own prediction that ScriptVM "may be similarly tractable as an upward layer; re-measure reverse edges." A 9-agent verify-then-plan workflow (5 deep-feasibility probes + 3 independent adversarial refuters, all returning `refuted:false`) measured the ScriptVM cluster's **reverse** edges at just **10** (the 85 forward edges are fine above library), and the synthesized plan executed in three gated passes. **The reframe held again: ScriptVM is a top-of-call-graph cluster, so it carves ABOVE library; the blocker was the 10 reverse edges, not the 91 forward "back-edges" the original note measured.** Unlike gamestates (whose screens genuinely reach down), scriptvm is consumed by *library itself* (the `game_loop` AI tick) — making it a SIBLING of gamestates, not a layer above/below it (nm-verified zero edges in either direction). **P1 (relocate `ai_state_t` down):** `ai_state_t` is embedded by-value in `Object` but its 7 referenced lifecycle/state methods (ctor/dtor/reset/add_order/set_changed/set_bumplast/spawn) are pure data ops with zero interpreter coupling — moved (with their private spawn/bump helpers) from `script.c` into a new `egolib/Entities/AiState.cpp` that stays in `egolib-library` beside `Object`. Reverse edges **10 → 3**, zero new edges (nm-verified `AiState.cpp.o → cluster` = 0). **P2 (`IScriptSystem` driver seam):** the 3 genuine VM-driver entries (`scr_run_chr_script`/`set_alerts`/`scripting_system_end`) routed through a lower-layer `Ego::Script::IScriptSystem` interface (accessor in `egolib-foundation-base`, mirroring `IObjectWorld`), implemented by a `ScriptSystemAdapter` and installed from **above** library — `egoboo/Main.cpp` AND a gtest global environment `ScriptSystemEnvironment.cpp` (an adversarial refuter caught that the test executable drives `quitModule→endScriptingSystem` / `kill→runCharacterScript` and would throw without a harness install — the required gap-fix). Reverse edges **3 → 0** (the 3 now referenced only by the adapter, which joins the cluster in P3). **P3 (the carve):** `EGOLIB_SCRIPTVM_LAYER_SOURCES` (16 VM TUs + the adapter) REMOVE_ITEM'd into a new `add_library(egolib-scriptvm)` that PUBLIC-links `egolib-library`; `egoboo` + the test executable link it alongside `egolib-gamestates`, while `cartman`/the content-validator (no VM path) stay on `egolib-library`. Layout: **base 146 ◄ {physics 5, renderer 28 ◄ gui 22} ◄ library 83 ◄ {scriptvm 17, gamestates 19}.** Full gate green at every pass: in-place + from-scratch clean builds, exact `ar t` counts, nm-acyclic (**0 forbidden back-edges across all 7 archives**; positive controls real: scriptvm→library 85, library→scriptvm 0, gamestates↔scriptvm 0/0), validator `test.mod` 0/0, ctest **875/875** (the harness install verified). **Lesson reaffirmed:** the original "91 back-edges, blocked" verdict was the FORWARD count; measuring REVERSE edges is the right test for an upward carve, and a relocate-down pre-pass (mislocated definitions) plus an interface seam (genuine upward calls) is the standard two-tool kit. Full detail in `71-completed-passes-log.md`.

**egolib-hud-widgets carve — the EIGHTH link-split, a MIDDLE upper layer — DONE (2026-06-11, branch `refactor/egolib-hud-widgets-carve`).** Pivoted here after the graphics-narrow carve's hard core proved GL-gated (its render-driver seam is unverifiable in this environment — that front is started on branch `refactor/egolib-graphics-narrow-carve`, step 1 landed, steps 4–6 deferred). The in-game HUD widgets are a render-driver-FREE upward carve that completed cleanly. **The easiest split yet: ONE reverse edge.** The 6 game-coupled widgets (`CharacterStatus`/`CharacterWindow`/`InventorySlot`/`LevelUpWindow`/`MiniMap`/`ModuleSelector`) that stayed in `egolib-library` after the egolib-gui carve have, post-scriptvm, exactly **1** `library → HUD` reverse edge: `MiniMap::setShowPlayerPosition(bool)` (from the AI minimap-reveal in `Object_update`/`game_loop`; `addBlip` left the count when `script_functions_quests` went up to scriptvm). Topology nm-measured: hud sits **above library, BELOW both scriptvm (script_functions_quests→MiniMap, 2 edges) and gamestates (PlayingState owns the HUD, 8 edges)** — a MIDDLE upper layer that both top siblings link. **P1 seam (commit `69ab6b5aa`):** a single new `IPlayingStateController::setMiniMapShowPlayerPosition(bool)` method (the call sites already used `tryActivePlayingState()`), routing the edge through the lower-layer interface vtable (no concrete-`MiniMap` link edge); `getMiniMap()->setVisible()` stays (it's `Component`'s, in gui). 1→0. **P2 carve (commit `6c6a711e3`):** `EGOLIB_HUD_WIDGETS_LAYER_SOURCES` REMOVE_ITEM'd into `add_library(egolib-hud-widgets)`; scriptvm + gamestates link it; cartman/validator stay on library. Layout: **base 146 ◄ {physics 5, renderer 28 ◄ gui 22} ◄ library 77 ◄ hud-widgets 6 ◄ {scriptvm 17, gamestates 19}.** Full gate green: in-place + from-scratch builds, exact `ar t`, nm-acyclic (0 forbidden back-edges across all 8; positive controls hud→library 43 / scriptvm→hud 2 / gamestates→hud 8; invariants all 0), validator `test.mod` 0/0, ctest **875/875**. No GL gate — only archive membership + one flag-setter interface method changed; render code byte-identical. Full detail in `71-completed-passes-log.md`.

**Strategic note (2026-06-11 scout-next-heavy-front workflow, 8 fronts adversarially verified; CORRECTED 2026-06-11 by the carve above):** the safe **move-only modularization vein into the lower layers is exhausted**. The original note claimed a 6th link-split out of `egolib-library` (Front B) was **refuted as a direct move** — every candidate cluster circular by nm (ScriptVM **91** back-edges, game-graphics-render **74**, GameStates **63**). **That verdict was wrong for GameStates: it measured the FORWARD direction (gamestates→library, which is fine for an ABOVE-library layer) and assumed a below-library carve like the other five splits.** A layer *above* library is the right shape for a top-of-call-graph cluster; its blocker is the *reverse* edge count (`egolib-gamestates` had only 7/8), which was paid down with bounded seam-cutting (not flag-day). ScriptVM and game-graphics-render remain un-measured in the reverse direction — they may be similarly tractable as upward layers; re-measure reverse edges before declaring them blocked. **(ScriptVM UPDATE 2026-06-11: re-measured and CARVED — see the egolib-scriptvm entry above. Its reverse-edge count was 10, not the 91-forward "back-edges" the original note measured; it is now the seventh archive, a sibling of gamestates.)** Other fronts unchanged: mechanical C++ modernization is **do-not-attempt** (no `-Wold-style-cast`/clang-tidy ratchet); portability T2.2/T2.5 are un-gateable in the headless sandbox. **Program "de-risk → carve GameStates" — COMPLETE:** (step 1, DONE) GUI characterization net (`GuiComponentBehavior.cpp`, 44 tests); (step 2, DONE) `GameState` base freed via `activeGameEngine()`; (step 3, DONE) Pass-1 EngineContext seam + the `egolib-gamestates` carve (above). **Next candidates (ranked by the 2026-06-11 reverse-edge ground-truth measurement):** (1) **game-graphics-render NARROW** — an 8th upward carve; measured at **16** reverse edges (the pure draw pipeline: RenderPasses + `graphic*.c`, excluding the entity-coupled `ObjectGraphics`/`ParticleGraphics`/`Camera`), ~half resolvable by pure relocation-down (the `gfx_config_t` POD, the HUD icon helpers `draw_game_icon`/`draw_blip`, `grid_lighting_interpolate`) and the rest via the already-published `IGFX`/`EngineContext` seam — feasible-with-seams, a known multi-pass gfx-god-object cleanup. (2) **game-HUD widgets — DONE 2026-06-11 (the eighth split, see entry above):** carved as a MIDDLE upper layer below scriptvm + gamestates; the post-scriptvm reverse-edge count was **1** (just `MiniMap::setShowPlayerPosition`, seam-cut via one `IPlayingStateController` method). (3) graphics-render FULL is NOT carve-able as a whole (45 reverse edges — `ObjectGraphics` is entity-coupled, called from `Object_*`). (4) the deferred Entities ownership-inversion (flag-day, 123 blockers); (5) the `Object` god-class multi-role decoupling (T1.2).

---

## Items intentionally deferred

- Renderer modernization (still out of scope per `04-refactoring-strategy.md` §5).
- Save/import/export format replacement.
- Network/multiplayer ambitions.
- EgoScript → Lua migration (blocked on T3.2 script registry and a stable scripting API boundary — see `04-refactoring-strategy.md` §3 Phase 6).
- Gameplay rebalance and asset visual upgrades.

---

## Definition of success (unchanged from `04-refactoring-strategy.md` §7)

The refactor is succeeding when:

- A new contributor can build and launch reliably from one document.
- Gameplay code can be read without chasing globals through unrelated systems. *(Mostly achieved — see `CODEBASE-HEALTH-STATUS.md` §4.)*
- Module and object content can be validated without starting the full game. *(Achieved — `egoboo-content-validator`.)*
- Content semantics live in schemas and code, not scattered folklore.
- Scripting has a stable API boundary. *(Blocked on T3.2.)*
- Module loading and gameplay regressions are caught by repeatable tests.
- Supported Linux and Windows build paths are close enough that portability fixes are shared work.
- The Windows build is usable both natively and when cross-built from Linux. *(Blocked on T2.2 + T2.5.)*
- Supported C++ build configurations are free of routine warning noise.
