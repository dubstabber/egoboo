# Completed Passes Log

Compact historical record for refactoring passes that have already landed.
Detailed per-pass narratives are recoverable from git history; this file keeps
only outcomes, durable constraints, reusable techniques, and current follow-up
signals.

Last compacted: 2026-06-30. Current metrics live in
`CODEBASE-HEALTH-STATUS.md`; do not duplicate volatile counts here.

## Maintenance Rule

Future refactoring passes should append a compact entry here: date, theme,
files or subsystem touched, behavior preserved or intentionally changed, and the
verification gate. Reserve a new numbered design document only for an active
multi-page architectural boundary.

## April Baseline And Validator Work

- Passes 10 and 24 reconciled `spawn.txt` behavior and established the
  validator as the structural content-health baseline. The live validator
  baseline is maintained in `06-validator-baseline.md`.
- Passes 11-23 replaced direct runtime-global reach with `EngineContext`,
  `GameSessionContext`, and module/session accessors. The former `_gameEngine`
  and `_currentModule` globals are retired from active runtime code.
- Passes 22 and 26-31 moved module runtime ownership into `GameModule` and split
  loading, spawn planning, and spawn realization into narrower units.
- Passes 34-51 moved player startup, local-player bookkeeping, perception,
  respawn cooldowns, and legacy local stats behind session/module surfaces.

## Object And Script Surface Cleanup

- Passes 52-69 encapsulated broad `Object` and `ObjectGraphics` fields,
  timers, flags, attachments, collision volumes, graphics state, and animation
  publication.
- Passes 72-112 introduced and widened role interfaces such as inventory,
  renderable, scriptable, damageable, physical, profile, character state,
  target info, and team-member surfaces. The main lesson: migrate callers only
  where the role captures the real dependency; avoid broad `Object&` in new
  code.
- Passes 113-202 pushed script helper callers toward role/context surfaces,
  retired many shared-pointer compatibility pockets, and replaced direct result
  mutation with explicit helper paths. Script opcode behavior stayed unchanged.

## Build, Service, And Header Fronts

- Passes 203-210 resumed role-interface cleanup, removed duplicate or typo
  compatibility APIs, and completed `override` coverage on role
  implementations.
- Passes 211-219 published input, graphics, font, texture, GFX, billboard,
  texture-atlas, and timing access through installed services instead of
  reaching directly for concrete singletons.
- Passes 220-226 eliminated the `egolib/egolib.h` uber-header. The reusable
  technique was to guard the aggregate include, then compile headers under the
  cut until each one was self-contained:

```cpp
#ifndef EGOBOO_NO_UBER_INCLUDE
#include "egolib/egolib.h"
#endif
```

- The June platform cleanup quarantined legacy platform READMEs under
  `doc/legacy/`, removed proprietary/Visual-Studio-only build hooks from the
  maintained path, and kept Linux plus Linux-hosted MinGW cross-builds as the
  active open-source build story.

## Cartman Integration

- On 2026-06-07, `cartman/` was wired into CMake behind
  `EGOBOO_BUILD_CARTMAN=OFF`, ported through API drift, and runtime-verified
  against `test.mod` on Linux with all four editor viewports rendered.
- `run-cartman.sh` builds the gated target on demand and validates arguments to
  avoid the legacy no-argument shutdown crash.
- Current status: useful but still not part of the default build. Open items are
  deciding whether to flip the default after more module coverage and fixing the
  pre-existing no-arg/bad-arg `atexit`/VFS cleanup crash in the editor itself.

## Include Decoupling And Characterization

- The service-hub front moved active log/config/audio/profile/image/particle
  access toward lower-layer `active*()` style accessors while preserving the
  existing `EngineContext` delegating API for callers and tests.
- The Entities/game include front removed conduit-only `game/` includes from
  propagating headers, moved pure primitives down, and used keep-going builds to
  identify free-riders that needed direct includes. Durable rule: classify
  header self-use separately from transitive include free-riding.
- The Collidable and collision-world work moved terrain/entity world queries
  behind lower-layer interfaces, then relocated collision pieces toward the
  physics archive without changing gameplay behavior.
- Characterization nets were added around pure physics helpers, combat damage,
  collision pipeline behavior, GUI component/container behavior, script runtime
  dispatch, and gameplay helper surfaces before risky dependency cuts.

## Archive Carves

The monolithic `egolib-library` was split into nine static archives while
preserving an acyclic intended direction:

```text
foundation-base <- {physics, renderer <- gui} <- library
library <- game-graphics <- hud-widgets <- {scriptvm, gamestates}
```

Key completed fronts:

- `egolib-foundation-base`: math, file formats, VFS, model loading,
  low-level services, and dependency-closed content/runtime helpers.
- `egolib-physics`: collision nucleus and physics primitives.
- `egolib-renderer` and `egolib-gui`: SDL/OpenGL rendering base and generic GUI
  toolkit.
- `egolib-gamestates`, `egolib-scriptvm`, `egolib-hud-widgets`, and
  `egolib-game-graphics`: upper archives carved out with injection hooks or
  interface seams so lower archives do not name upper concrete types.

When moving sources between archives, measure live `.a` archives with `nm` and
`ar`; do not trust stale `CMakeFiles/*.dir` object directories.

## File-Split And Loader Fronts

- Passes 240-250 split large runtime files such as script spawn helpers, VFS
  search/mount/RWops helpers, particle collision, script compiler helpers,
  `GameEngine`, and object profile loading.
- The `vfs.c` cleanup removed the dead cstdio backend, collapsed the single live
  PhysFS representation, and deduplicated fixed-width read/write helpers without
  changing the public `vfs_FILE*` API.
- The MD2-to-glTF preparation created a format-neutral `AnimatedModel` path,
  `ObjectModelAsset` search-order helpers, `ObjectModelLoader`, and
  `ModelAnimationMetadata`. Current loader behavior is documented in
  `03-data-and-content-audit.md`.

## ObjectRef Ownership Arc

- Passes 271-279 moved team, inventory, particle, enchantment, module spawn,
  particle attachment, shop, and combat attribution surfaces away from public
  `std::shared_ptr<Object>` APIs and toward `ObjectRef` or explicit attribution
  values. The validator baseline briefly fluctuated during audit work, then was
  rechecked back to the maintained `42 modules / 10 warnings / 245 errors`.
- Passes 280-292 completed the ref-first cleanup across `GameModule` spawning,
  player binding, `ObjectHandler` query/enumeration APIs, passage/team/module
  loops, gameplay targeting, inventory, message/export helpers, shop/particle
  helpers, script spawn contexts, and enchantment owner/target/overlay identity.
- Current state after Pass 292: public object enumeration is ref-first, the
  legacy `ObjectHandler::operator[](ObjectRef)` and public handle iterator are
  retired, and remaining shared-handle lookups are intentional ownership or
  weak-storage paths such as player bootstrap and billboard attachment.

## Documentation Passes

- Pass 274 on 2026-06-23 shortened the top-level docs, redirected volatile
  numbers to `CODEBASE-HEALTH-STATUS.md`, and refreshed the then-current test
  count and validator references.
- Pass 293 on 2026-06-30 compressed this history log, removed superseded
  standalone historical-front notes, folded live glTF and cartman guidance into
  canonical docs, and rechecked current archive, test, singleton, and validator
  metrics.
- Pass 294 on 2026-06-30 widened the lower-layer `IObjectWorld` seam with live
  object lookup helpers, redirected `GameSessionContext::tryObject()` through
  that seam, and moved a bounded batch of team/player/graphics/HUD callers off
  direct session object lookup. Ownership and lifetime behavior stayed with
  `GameModule`/`ObjectHandler`; coverage was added in `ObjectHandlerQueries`.
  Verified with the Linux build, focused object/session ctest filter, full
  `ctest`, and the `test.mod` validator smoke.
- Pass 295 on 2026-06-30 widened the same seam with active object-handler
  helpers, redirected the session object-handler compatibility methods through
  them, and moved entity object/particle code plus graphics, audio, script,
  targeting, matrix, inventory, shop, HUD, and UI object-container callers off
  direct `GameSessionContext` / `GameModule` object-handler access. Module-owned
  spawn/export/passage/editor behavior stayed on `GameModule`; coverage was
  extended in `ObjectHandlerQueries`.
- Pass 296 on 2026-06-30 retired the remaining `GameSessionContext` object
  lookup compatibility API after moving the last script/link callers and test
  fixtures to the active `IObjectWorld` helpers. `GameSessionContext` still
  installs and clears the active object world during module lifetime, but no
  longer exposes object-handler or object-lookup forwarding methods. Verified
  with the Linux build, focused object/module/script gtest filter, full
  `ctest`, and the `test.mod` validator smoke.
- Pass 297 on 2026-06-30 added lower-layer `IModuleEnvironment` and
  `ISessionState` seams for active module environment and read-only session
  state access. `GameSessionContext` installs and clears the active session
  state during module lifetime, while `GameModule` implements the environment
  view; ownership and mutation remain with the concrete runtime owners. A
  bounded batch of rendering, camera, HUD, script, spawn, and texture-atlas
  callers moved off direct `GameSessionContext` / `GameModule` access, with
  lifecycle-only pre-module fallbacks kept in object/particle/vertex-cache
  construction paths, render-state accessors, and engine frame-rate/update-frame
  reporting before module load. Coverage was extended in `ObjectHandlerQueries`
  to assert empty, mirrored, and cleared active seams. Verified with the Linux
  build, focused object/module/script/camera ctest filter, full `ctest`, and the
  `test.mod` validator smoke.
- Pass 298 on 2026-06-30 added the explicit `GameModuleRuntime` provider
  surface and moved `GameModule` bootstrap/loading/spawn/update paths, passage
  music checks, weather updates, and player-startup bookkeeping off direct
  `EngineContext::get()` / `GameSessionContext::get()` access. Normal module
  teardown now calls `GameModule::shutdownRuntime()` while services are still
  installed, with the destructor kept as an abnormal-teardown fallback.
  `GameSessionContext` remains the composition point for module services and
  session-owned counters/import state. Verified with the Linux build, focused
  module/player/object/shop ctest filter, full `ctest`, and the `test.mod`
  validator smoke.
- Pass 299 on 2026-06-30 added the lower-layer `IModuleStatus` seam for active
  module export, respawn, beaten, passage-count, module-profile, and import-list
  reads. `GameSessionContext` now installs and clears this status surface with
  the active module, and `GameModule` implements the read-only view while
  retaining concrete ownership and mutation. A bounded batch of HUD, menu,
  victory, GUI status, entity/particle environment, player, loading, and script
  callers moved off direct `GameSessionContext` / `GameModule` access where they
  only needed active status, environment, or session-state reads. Module
  mutation, passage mutation, lifecycle orchestration, and pre-module bootstrap
  fallbacks stay on the concrete owners. Coverage was extended in
  `ObjectHandlerQueries` to assert empty, mirrored, and cleared active module
  status. Verified with the Linux build, focused fallback and
  object/module/script/camera ctest filters, full `ctest`, the `test.mod`
  validator smoke, and the live-archive `nm` back-edge check.
- Pass 300 on 2026-06-30 extracted `module_loading::ModuleLoadPhase` as the
  named constructor-load orchestration boundary for `GameModule`. The new phase
  owns the existing VFS/RNG, team/texture, shared asset, environment, content,
  and final clock/debug setup order while `GameModule` still owns loaded state
  and `GameSessionContext` still controls module lifetime and active seam
  publication. No spawn realization, import/export, profile-slot, parser, or
  legacy content-format behavior changed. Coverage was extended in
  `ObjectHandlerQueries` for loaded constructor state and installed active
  seams. Verified with the Linux build, focused module/loading ctest filter,
  full `ctest`, the `test.mod` validator smoke, a no-stray-header archive check,
  and the live-archive `nm` back-edge check.
- Pass 301 on 2026-06-30 retired the remaining `GameSessionContext` module
  environment forwarding API for mesh, tile/water textures, water, weather, fog,
  and animated-tile reads. The active `IModuleEnvironment` seam remains the
  supported read path, while `GameSessionContext` keeps module lifecycle,
  mutation, and concrete active-module ownership. The last test-only fog callers
  moved to `activeModuleEnvironment()`. Verified with the Linux build, focused
  `ObjectHandlerQueries|ScriptSystemsFunctions|ScriptStateFunctions` ctest
  filter, full `ctest`, and the `test.mod` validator smoke.
- Pass 302 on 2026-06-30 added the active `ISessionStatePublisher` seam for
  live local-player, enemy-sense, and respawn publication. `GameSessionContext`
  still owns session state and module lifecycle, but module-time publication in
  the game loop, map editor, script presentation functions, and object death and
  perk update paths now routes through the active publisher instead of concrete
  session access. Teardown-local reset behavior remains on `GameSessionContext`
  because active seams are cleared before legacy player reset. Coverage was
  extended in `ObjectHandlerQueries` to assert empty, mirrored, and cleared
  active publisher state. Verified with the Linux build, focused
  `ObjectHandlerQueries|GameplayAlertPublication|ModuleUpdate|ScriptSystemsFunctions|ScriptStateFunctions`
  ctest filter, full `ctest`, and the `test.mod` validator smoke.
- Pass 303 on 2026-06-30 replaced `ModuleLoadPhase`'s direct `GameModule`
  friend access with an explicit `ModuleLoadContext` carrying only constructor
  load state, runtime providers, and callbacks for the existing private load
  helpers. `GameModule` still owns loaded state and module lifetime, while the
  named phase preserves the existing VFS/RNG, team/texture, shared asset,
  environment, content, and final clock/debug setup order. No spawn
  realization, import/export, profile-slot, parser, legacy content-format,
  active seam publication, or archive membership behavior changed. Verified
  with the Linux build, focused
  `ModuleLoadSmoke|ObjectHandlerQueries|ModuleUpdate` ctest filter, full
  `ctest`, and the `test.mod` validator smoke.
- Pass 304 on 2026-06-30 added active `ITerrainQuery` and `IModuleCommands`
  seams, installed and cleared by `GameSessionContext` for the active module
  lifetime. Migrated terrain line-of-sight/path callers and bounded module
  command/mutation callers in scripts, entities, shop logic, graphics, loading,
  and game-loop code off direct `GameSessionContext` / concrete `GameModule`
  access where they only needed terrain, spawn, passage, tile, pit, shop, team,
  player, import, respawn, export, beaten, path, or module-update operations.
  `activeSessionState()` remains strict, with an explicit pre-module fallback
  accessor for legacy object/particle construction paths that can still run
  before an active module publishes the seam. Coverage was extended in
  `ObjectHandlerQueries` to assert empty, mirrored, and cleared module command
  and terrain-query seams. Verified with the Linux build, targeted fallback
  ctest rerun, full `ctest`, the `test.mod` validator smoke, the full validator
  baseline (`42 modules / 10 warnings / 245 errors`, expected nonzero exit),
  no-stray-header archive check, and the live-archive `nm` back-edge check.
- Pass 305 on 2026-07-11 made `ScriptOperandContext` and
  `ResolvedSelfContext` role-only boundaries. Script operand population now
  resolves the existing physical, character-state, target, wallet, inventory,
  and profile roles directly instead of retaining concrete `Object*` pointers,
  and `hasSelf()` requires a valid reference plus the complete self-role bundle.
  Existing invalid-reference sentinels, leader fallback, conversions, and
  profile metadata mutation behavior remain unchanged. Coverage now asserts the
  complete-context invariant. Verified with a clean Linux build, the focused
  217-case script/object ctest filter, full `ctest` (`947 / 947`), both content
  validator modes, and the Linux-hosted Windows cross-build.
- Pass 306 on 2026-07-11 removed duplicated liveness from `ITargetInfo` and made
  `IDamageable` the authoritative script-visible alive/dead role. Shared living
  damageable and character-state adapters now gate target predicates, actions,
  combat, enchantment, stat-gift, and spawn-cleanup paths consistently, while
  target relationship data stays on `ITargetInfo`. Coverage now asserts that a
  killed target fails living team and owner predicates. Verified with the same
  Linux build, focused and full ctest gates, validator baselines, and Windows
  cross-build as Pass 305.
- Pass 307 on 2026-07-11 added the narrow `IAttachmentControl` object role and
  exposed safe-position state through `IPhysical`. Spawned-character script
  handling now retains only an `ObjectRef` and resolved attachment, physical,
  script, character-state, lifecycle, and movement roles; attachment, holder,
  safe-position, and termination operations no longer require a concrete
  `Object*`. Coverage now checks unresolved child profiles and verifies that the
  temporary holder reference is cleared after held-object script execution.
  Verified with the same Linux build, focused and full ctest gates, validator
  baselines, and Windows cross-build, plus no-stray-header and live-archive `nm`
  boundary checks.
- Pass 308 on 2026-07-11 completed the strict EgoScript/concrete-object cut.
  The new lower-layer `ObjectRoleAccess` adapter resolves live `ObjectRef`
  handles to existing roles without widening `IObjectWorld`; new
  `IScriptRuntimeState` and `IVisibilityObserver` roles expose the two remaining
  VM driver needs, while input reset moved onto `IMovementControl` and bounded
  enchant operations stayed behind `IEnchantable`. `IScriptSystem` and
  `scr_run_chr_script()` are now reference-only, the VM driver and legacy script
  operations no longer include the aggregate entity header or name concrete
  `Object`/`ObjectHandler`, script constants use the neutral
  `ObjectConstants` surface, and interpreter `ObjectValue` stores `ObjectRef`
  with corrected cross-tag copy assignment. Coverage asserts live/invalid/stale
  role resolution, AI-versus-player movement reset, stale dispatch no-ops,
  final-death ref dispatch, and tagged-value construction/copy/conversion/
  streaming. Verified with the Linux and MinGW builds, focused **8 / 8** tests,
  full ctest **955 / 955**, `test.mod` validation **0 / 0**, the JSON full
  baseline (**42 modules / 10 warnings / 245 errors**, expected nonzero), strict
  concrete-object scans, no stray header archive members, and the live-archive
  `nm` DAG check. The console validator also reproduced its previously observed
  alternate **25-warning / 230-error** fallback classification; no validator,
  content, or compiler behavior changed in this pass.
- Pass 309 on 2026-07-15 finished migrating the last read-only `GameModule`
  accesses in the top-of-DAG `egolib-gamestates` screens onto the already
  installed lower-layer seams. In `PlayingState.cpp` the developer-mode debug
  watches for passages/export-valid/module-beaten now read `tryActiveModuleStatus()`
  (`IModuleStatus`), the path watch reads `tryActiveModuleCommands()->getPath()`
  (`IModuleCommands`), the destructor export check reads `tryActiveModuleStatus()`,
  and the `SDLK_F9` win-module cheat resolves the object container through
  `Ego::Entities::activeObjectHandler()` (`IObjectWorld`). In `MapEditorState::update()`
  the concrete `GameSessionContext`/`GameModule` locals were dropped: the quad-tree
  rebuild uses `Ego::Entities::activeObjectHandler()` and the water animation uses
  `activeModuleEnvironment().water()` (`IModuleEnvironment`). The three debug
  watches with no matching seam (players/imports/name), the player-list ctor loop,
  and the per-frame `activeModule().update()` / map-editor `beginModule()`
  lifecycle calls were deliberately left on the concrete owner rather than widen a
  seam for debug-only display. No new types, no archive moves; both files stay in
  `egolib-gamestates` and only add downward calls. `GameSessionContext::get()`
  dropped 30 -> 23. Seam-method equivalence, null/throw timing (the status seam is
  installed and cleared atomically with `_activeModule`, and `~PlayingState` runs
  before `quitModule()` in `GameEngine::uninitialize`), and throw equivalence for
  the cheat/editor paths were adversarially verified. Verified with the Linux
  build (no new warnings), full ctest **955 / 955**, `test.mod` validation
  **0 / 0**, the full validator baseline (**42 modules / 10 warnings / 245 errors**,
  expected nonzero), and the live-archive `nm` back-edge check (all four seam
  symbols resolve from `foundation-base`/`library`, referenced-only in
  `egolib-gamestates`).
- Pass 310 on 2026-07-15 extracted the inline audio + particle subsystem
  lifecycle out of `GameEngine::initialize()` / `uninitialize()` into a new
  same-archive RAII composition-root object,
  `GameplaySubsystemsBootstrap` (egolib-library, `EGOLIB_GAME_CORE_SOURCES`).
  Its constructor installs the audio system then the particle handler (the exact
  original order); its destructor clears the particle handler then the audio
  system (the exact original reverse order). `GameEngine` holds it as a
  `std::unique_ptr` member constructed at the original install point in
  `initialize()` and `reset()` at the original teardown point in
  `uninitialize()` (after `Console::uninitialize()`, before `unsubscribe()`),
  mirroring the existing `ContentRuntimeBootstrap` member exactly. This removes
  the last direct `AudioSystem`/`ParticleHandler` uses from the two `GameEngine`
  translation units, letting them drop the `Entities/_Include.hpp` (heavy
  `Object.hpp` aggregate) and `AudioSystem.hpp` includes; `CollisionSystem.hpp`
  stays in the lifecycle TU (still used) and was removed from `GameEngine.cpp`
  where it was already dead. Measured header impact (real compile flags):
  `Object.hpp` dropped from BOTH `GameEngine.cpp` (closure 1511 -> 1465) and
  `GameEngine_lifecycle.cpp` (1511 -> 1466) and is now confined to the single
  ~48-line bootstrap TU. Normal startup/shutdown is byte-for-byte equivalent.
  Adversarially verified; the one flagged divergence is error-path only: if an
  unhandled exception skips `uninitialize()`, `Main.cpp`'s `clearEngine()` ->
  `~GameEngine` now runs the bootstrap's audio+particle teardown (previously the
  singletons leaked on that crash-exit path). This is (a) architecturally
  identical to the already-shipped `ContentRuntimeBootstrap`, which likewise
  tears down the profile system via `~GameEngine` on that path; (b) non-throwing
  (the `clearActive*` seams are pure `nullptr` assignments, audio teardown is
  self-contained SDL_mixer, particle teardown is container destruction); and
  (c) safely ordered, since reverse member-destruction runs the audio+particle
  teardown before `ContentRuntimeBootstrap` (profile), keeping any
  particle->profile dependency valid. `egolib-library` member count 82 -> 83; no
  archive-boundary or DAG change (`GameplaySubsystemsBootstrap` symbols resolve
  within `egolib-library`). Verified with the Linux build (no new warnings),
  full ctest **955 / 955**, `test.mod` validation **0 / 0**, the full validator
  baseline (**42 modules / 10 warnings / 245 errors**, expected nonzero), the
  header-closure measurement above, and the archive-member/`nm` check.
