# Refactoring Roadmap

Prioritized forward plan for ongoing Egoboo refactoring work. Snapshot date: 2026-04-19. Supersedes and replaces:

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

Landed so far:

- `IInventoryHolder` — equipment, held, inventory slot access
- `IRenderable` — render-facing surface (matrix cache, tint, model descriptor)
- `IScriptable` — script-visible state and commands
- `IDamageable` — combat damage application surface
- `IPhysical` — collision volume, orientation, bumper state
- `ITargetInfo` — bounded target/self query surface for script helpers
- `ICharacterState` — bounded mutable ammo/mana/kurse/timer/perk/attribute state
- `ITeamMember` — team mutation, leadership, team-wide XP publication
- `IWallet` — bounded money query and mutation

Follow-on work inside this tier:

- Migrate more callers to the landed role surfaces instead of `Object`.
- Keep the remaining mixed-domain `script_functions_systems.c` helpers focused on bounded caller migration; quest/profile/armor policy coupling still remains deferred.
- Keep the raw `ai_state_t` bridge confined to `Ego::Script::runtimeState(...)` until `Script/script.c` no longer consumes raw script-runtime state.

This remains the SRP/ISP keystone for `Object`.

**Risk:** Medium. The pattern is established now, but the remaining caller migration still needs careful, role-by-role passes.

### T1.3 Service-interface layer over singletons

~1,150 `::get()` call sites remain (see `CODEBASE-HEALTH-STATUS.md` §4). Keep taking the smallest-reach singleton and applying the same DIP seam pattern one service at a time:

- Landed so far: `IAudioSystem`, `IPerkHandler`, `IImageManager`, `IParticleHandler`, `IProfileSystem`, plus engine-routed logging through the installed `EngineContext` log target.
- Bootstrap ownership now publishes audio through `GameEngine`, and perk/image services through `ContentRuntimeBootstrap` or `App`/`GFX` as appropriate.
- `ParticleHandler` was taken as the next safe small-reach seam ahead of the broader profile/log/config passes; runtime ownership now publishes it through `GameEngine`, while `ParticleHandler::get()` stays as a subsystem-local bootstrap seam.
- `ProfileSystem` is now published through `EngineContext`, with validator and profile-loading callers migrated onto the installed `IProfileSystem` seam while `ProfileSystem::get()` remains a subsystem-local lifecycle seam inside `Profiles/`.
- `egoboo_config_t` is now published through `EngineContext` for system/bootstrap lifecycle, module-load sync, lightweight content-bootstrap paths, read-mostly runtime callers, and the former write-heavy audio/video options flow.
- Keep `AudioSystem::get()`, `PerkHandler::get()`, `ImageManager::get()`, `ParticleHandler::get()`, `ProfileSystem::get()`, `Log::get()`, and `egoboo_config_t::get()` as subsystem-local bootstrap or lifecycle seams until a DI container is defined or each caller cluster has been migrated.
- Follow-on config work is now limited to subsystem-local cleanup around audio/image/particle bootstrap and lifecycle edges rather than broad caller migration.

**Risk:** Medium. First pass defines the pattern; later passes mostly mechanical.

### T1.4 Document error-handling policy, retire `egolib_rv`

Three strategies still coexist: C++ exceptions (~290 throw sites, ~76 try/catch), `egolib_rv` return codes, silent failure. Deliverables:

- Landed: `doc/error-handling-policy.md` now defines exceptions for exceptional paths, ordinary return values for expected boundary outcomes, and a no-new-silent-failure rule.
- Started C++ `egolib_rv` retirement with the smallest public seam: `CameraSystem::renderAll()` now treats a missing render callback as invalid input rather than returning a legacy status code.
- Continue retiring `egolib_rv` from C++ code paths one subsystem at a time, preserving meaningful tri-state semantics where they still exist.

**Risk:** Low for policy doc; medium for `egolib_rv` retirement (touches a lot of code).

---

## Tier 2 — Build and cross-platform

### T2.1 Retire MSVC from CI

`appveyor-windows.yml` still generates a Visual Studio 2017 solution. The maintained Windows path is mingw-w64 cross.

- Replace the Visual Studio generator in `appveyor-windows.yml` with mingw-w64 cross (matching `cmake/toolchains/mingw-w64-x86_64.cmake`).
- Drop the two MSVC-only CMake branches (`CMakeLists.txt:51-69`, `egoboo/CMakeLists.txt:41-46`) and the `platform.h:125-136` MSVC pragma island.
- Remove `distribute.ps1`, `external/install-vsix-appveyor.ps1`, `external/external.sln`, `external/SDL2-*/VisualC/`, `egoboo.gta.runsettings`.

**Risk:** Low. Deletes unsupported config; repo audit in `CODEBASE-HEALTH-STATUS.md` §8 confirms no active consumer.

### T2.2 Native-Windows open-source build

Add `doc/build-windows-native.md` + `cmake/toolchains/msys2-ucrt64.cmake` for building on a Windows host with MSYS2 UCRT64. This completes the supported matrix alongside Linux native and Linux-hosted cross.

### T2.3 Eliminate configure-time network fetch

`idlib/CMakeLists.txt` fetches googletest 1.16.0 from GitHub by default. This breaks offline and sandboxed builds.

- Default `idlib-with-fetch-googletest=OFF`.
- Use the vendored `external/googletest` tree.

### T2.4 Collapse third-party dependency divergence

- Decide on one SDL2 story (system pkg-config on Linux, MinGW bundle on Windows cross); delete the orphaned `external/SDL2-2.0.3/` tree.
- Remove `external/physfs-2.1.1` (the real PhysFS is in `idlib-game-engine/library/physfs-3.0.0`).

### T2.5 Fix Wine font-atlas / audio crash

`debug-output.txt` shows font atlas init failure in `egolib/Graphics/Font.cpp` and a Wine page-fault inside `Mix_LoadWAV_RW` during audio load. Without a fix, the cross build is not a credible verification substitute — `run-egoboo-windows.sh` currently gates it with `EGOBOO_DISABLE_MIPMAPS=1 EGOBOO_DISABLE_AUDIO=1` as a workaround.

### T2.6 Quarantine legacy platform READMEs

Move `README.VisualStudio`, `README.Windows`, `README.MinGW`, `README.OSX` to `doc/legacy/` or delete. The canonical docs are `doc/build-linux.md` and `doc/build-windows.md`.

---

## Tier 3 — Deeper structural work

These are unblocked only after Tier 1 lands. They represent the next frontier of maintainability work once the Object/singleton debt is paid down.

### T3.1 Shrink `shared_ptr<Object>` usage

~955 `shared_ptr` call sites, many of them `shared_ptr<Object>`, with `Object` inheriting from `enable_shared_from_this`. Entity ownership is shared-by-default. A dedicated pass should:

- Identify which `shared_ptr<Object>` uses are true shared ownership vs. observer patterns.
- Move observer-style uses to `weak_ptr<Object>` or raw non-owning references.
- Consider whether `ObjectHandler` can become the sole owning reference, with all call-site refs becoming non-owning.

### T3.2 Script dispatch → registry model

`script_functions_{action,bitwise,movement,spawn,state,systems,target}.c` still implements ~400 script functions as one procedural dispatch split across seven files. Moving to a registry-based model would:

- Enable Command pattern on script ops.
- Remove the `switch` density density (~100+ switches in `egolib`).
- Allow extension without touching the dispatch layer.

### T3.3 Reduce `egolib.h` transitive reach

`egolib.h` still pulls in 57 subsystems. Only a handful of `.c` files still include it directly. Finish removing direct includes and physically delete the uber-header.

### T3.4 Behavioral test coverage

Current test-to-code ratio is ~3.6% and covers parsers / module smoke / accessor regressions. Gaps:

- Gameplay combat logic
- Physics / collision behavior
- Rendering correctness (golden-image or matrix-cache comparisons)
- Script VM behavior
- GUI state transitions

Add characterization coverage before the next restructuring wave in each area.

### T3.5 Native-Cartman build integration

`cartman/` exists in-tree but is disconnected from the main CMake graph. Gate it with a CMake option and add it to the build matrix. Prevents further bit-rot.

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
