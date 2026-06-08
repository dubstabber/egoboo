# Refactoring Roadmap

Prioritized forward plan for ongoing Egoboo refactoring work. Snapshot date: 2026-06-08 (updated from 2026-06-06 / 2026-04-19). Supersedes and replaces:

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
- Keep the remaining mixed-domain `script_functions_systems.c` helpers focused on bounded caller migration; quest/profile/armor policy coupling still remains deferred.
- Keep the raw `ai_state_t` bridge confined to `Ego::Script::runtimeState(...)` until `Script/script.c` no longer consumes raw script-runtime state.

This remains the SRP/ISP keystone for `Object`.

**Risk:** Medium. The pattern is established now, but the remaining caller migration still needs careful, role-by-role passes.

### T1.3 Service-interface layer over singletons

~863 `::get()` call sites remain (down from ~912 at 2026-06-06, ~946 at 2026-04-20, ~1,150 at 2026-04-19). Keep taking the smallest-reach singleton and applying the same DIP seam pattern one service at a time:

- Landed so far: `IAudioSystem`, `IPerkHandler`, `IImageManager`, `IParticleHandler`, `IProfileSystem`, `IFontManager` (Pass 211), `IInputSystem` (Pass 212), `IGraphicsSystem` (Pass 213, with headless test mock), `ITextureManager` (Pass 214), `ITextureAtlasManager` (Pass 217), `IGFX` (Passes 218–219, two sub-passes), `ICameraSystem` (CameraSystem Passes 1–2, 2026-06-07 — interface widened + game-layer consumers migrated), plus `IBillboardSystem` caller rerouting (Pass 215), `Time` clock abstraction (Pass 216), engine-routed logging, and `egoboo_config_t`.
- Bootstrap ownership now publishes audio through `GameEngine`, and perk/image services through `ContentRuntimeBootstrap` or `App`/`GFX` as appropriate.
- `egoboo_config_t` is now published through `EngineContext` for system/bootstrap lifecycle, module-load sync, lightweight content-bootstrap paths, read-mostly runtime callers, and the former write-heavy audio/video options flow.
- Keep subsystem-local `::get()` as bootstrap/lifecycle seams where the singleton predates the EngineContext install. Follow-on work is limited to subsystem-local cleanup around bootstrap and lifecycle edges.
- **Renderer: DEFERRED** — already an abstract polymorphic facade; migratable surface is ~23 methods (nearly the whole interface), low value/high churn.
- **CameraSystem: DONE** (2026-06-07, two passes) — the seam was found mid-flight, not deferred (`ICameraSystem` + `EngineContext` install/clear + `GameEngine` lifecycle already existed). `ICameraSystem` was widened with the four methods consumers use (`getMainCamera`, `getCamera`, `getCameraOptions`, `renderAll`) and all clean game-layer `.cpp` consumers (`PlayingState`, `MapEditorState`, `Player`, `GameEngine` config sync) migrated onto `EngineContext::get().cameraSystem()`. Remaining `CameraSystem::get()` sites are principled exceptions: the install site, `Audio/AudioSystem.cpp` (×4, layer-inversion — lower-layer subsystem stays on the concrete singleton), `Entities/Object_appearance.cpp` (×2, behind a `CameraSystem::is_initialized()` existence-guard whose semantics do not match the seam in the test bootstrap), and 10 in deferred `.c` files.

**Risk:** Medium. Pattern is well-established; remaining passes mostly mechanical.

### T1.4 Document error-handling policy, retire `egolib_rv`

Three strategies still coexist: C++ exceptions (~570 throw sites across egolib, `try`/`catch` in ~35 files), `egolib_rv` return codes, silent failure. Deliverables:

- Landed: `doc/error-handling-policy.md` now defines exceptions for exceptional paths, ordinary return values for expected boundary outcomes, and a no-new-silent-failure rule.
- Started C++ `egolib_rv` retirement with the smallest public seam: `CameraSystem::renderAll()` now treats a missing render callback as invalid input rather than returning a legacy status code.
- Continue retiring `egolib_rv` from C++ code paths one subsystem at a time, preserving meaningful tri-state semantics where they still exist.

**Risk:** Low for policy doc; medium for `egolib_rv` retirement (touches a lot of code).

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

Current test-to-code ratio is ~16.9% and covers parsers, module smoke, accessor regressions, script dispatch, gameplay surfaces, physics/collision math, and live-Object combat-damage math. Gaps:

- Full combat *integration* (`Object::damage(...)` side effects: life/mana, alerts, particle spawn) — the pure resistance/reduction/invictus math is now pinned by `CombatDamageResolution.cpp`, but the integrated damage path is not
- Collision *pipeline* behavior (`do_chr_prt_collision` / `particle_collision.c`) — the pure swept-bounds/normal math is covered, the pipeline is not
- Rendering correctness (golden-image or matrix-cache comparisons)
- GUI state transitions
- AI

Add characterization coverage before the next restructuring wave in each area. (Already landed: physics/collision math, bounding-volume ops, map twist, particle recoil, damage/attribute enums, script loader/VM/dispatch, and live-Object combat-damage math — see `71-completed-passes-log.md`.)

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

**Remaining (separate, harder front):** the 33 leaf TUs still including `EngineContext.hpp` are genuine **service**-hub users (`config`/`audioSystem`/`particleHandler`/`profileSystem`/`imageManager`). These are not logging cuts — resolving them needs lower-layer service seams (the deeper DIP/DI work, related to T1.3), not the mechanical logging swap. Migrating the ~115 *game-layer* `logTarget()` callers is intra-layer consistency churn (game → game/Core is not a directional violation) and was deliberately **not** pursued. The optional endpoint — migrate those callers too and delete `EngineContext`'s log API outright (now unblocked by `tryInstalledTarget()`) — is low structural value and deferred.

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
