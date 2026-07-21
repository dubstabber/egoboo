# Completed Passes Log

Compact historical record for refactoring passes that have already landed.
Detailed per-pass narratives are recoverable from git history; this file keeps
only outcomes, durable constraints, reusable techniques, and current follow-up
signals.

Last compacted: 2026-07-21. Current metrics live in
`CODEBASE-HEALTH-STATUS.md`; do not duplicate volatile counts here.

## Maintenance Rule

Future refactoring passes should append a compact entry here: date, theme,
files or subsystem touched, behavior preserved or intentionally changed, and
the verification gate — 2 to 5 lines. Reserve a new numbered design document
only for an active multi-page architectural boundary.

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
  against `test.mod` on Linux. `run-cartman.sh` builds the gated target on
  demand and validates arguments to avoid the legacy no-argument shutdown
  crash. Open items: whether to flip the default after more module coverage,
  and fixing the pre-existing no-arg `atexit`/VFS cleanup crash.

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

Key completed fronts: `egolib-foundation-base` (dependency-closed math, file
formats, VFS, model loading, low-level services), `egolib-physics` (collision
nucleus), `egolib-renderer`/`egolib-gui` (SDL/OpenGL base and generic GUI
toolkit), then `egolib-gamestates`, `egolib-scriptvm`, `egolib-hud-widgets`,
and `egolib-game-graphics` carved above `library` with injection hooks or
interface seams so lower archives never name upper concrete types.

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
  `ModelAnimationMetadata`, then landed the glTF/GLB static-subset loader.
  Current loader behavior is documented in `03-data-and-content-audit.md`.
- The 2026-06 within-archive split campaign brought every production runtime
  file under 1,000 lines (health doc has the current largest-file list).

## ObjectRef Ownership Arc

- Passes 271-279 moved team, inventory, particle, enchantment, module spawn,
  particle attachment, shop, and combat attribution surfaces away from public
  `std::shared_ptr<Object>` APIs and toward `ObjectRef` or explicit attribution
  values.
- Passes 280-292 completed the ref-first cleanup across `GameModule` spawning,
  player binding, `ObjectHandler` query/enumeration APIs, passage/team/module
  loops, gameplay targeting, inventory, message/export helpers, shop/particle
  helpers, script spawn contexts, and enchantment owner/target/overlay identity.
- State after Pass 292: public object enumeration is ref-first, the legacy
  `ObjectHandler::operator[](ObjectRef)` and public handle iterator are
  retired, and remaining shared-handle lookups are intentional ownership or
  weak-storage paths such as player bootstrap and billboard attachment.

## Active-Seam Decoupling And Composition Roots (Passes 294–311)

Unless noted, every pass below was verified with the Linux build, a focused
ctest filter, full `ctest`, the `test.mod` validator smoke, and (where archive
boundaries were touched) the live-archive `nm` back-edge check; passes 305-308
also gated on the Linux-hosted Windows cross-build and the full validator
baseline. Full ctest grew 947 → 955 across the arc; behavior was preserved
throughout.

- Passes 294-296 (2026-06-30) widened the lower-layer `IObjectWorld` seam with
  live object-lookup and object-handler helpers, moved entity, graphics,
  audio, script, targeting, HUD, and UI callers onto it, then retired the
  `GameSessionContext` object-lookup/handler forwarding API entirely.
- Pass 297 (2026-06-30) added `IModuleEnvironment` and `ISessionState` seams
  for active environment and read-only session-state reads; ownership and
  mutation stayed on `GameModule`/`GameSessionContext`, with lifecycle-only
  pre-module fallbacks kept explicit.
- Pass 298 (2026-06-30) added the `GameModuleRuntime` provider surface so
  `GameModule` load/spawn/update/passage-music/weather/player-startup code
  receives services explicitly; normal teardown now calls
  `GameModule::shutdownRuntime()` while services are still installed.
- Pass 299 (2026-06-30) added the read-only `IModuleStatus` seam
  (export/respawn/beaten/passage-count/profile/import reads) and moved HUD,
  menu, victory, entity, player, loading, and script read-only callers onto it.
- Passes 300 and 303 (2026-06-30) extracted `module_loading::ModuleLoadPhase`
  as the named constructor-load orchestration boundary, then replaced its
  friend access with an explicit `ModuleLoadContext` carrying load state,
  runtime providers, and callbacks. Load order is unchanged.
- Pass 301 (2026-06-30) retired the `GameSessionContext` module-environment
  forwarding API; `activeModuleEnvironment()` is the supported read path.
- Pass 302 (2026-06-30) added the `ISessionStatePublisher` seam for live
  local-player/enemy-sense/respawn publication from the game loop, map editor,
  script presentation, and death/perk paths. Teardown-local reset stays on
  `GameSessionContext` because active seams clear before legacy player reset.
- Pass 304 (2026-06-30) added `ITerrainQuery` and `IModuleCommands` seams and
  migrated terrain line-of-sight/path callers plus bounded module
  command/mutation callers (spawning, team XP, passages, shops, pits, tiles,
  respawn/export/beaten flags) across scripts, entities, graphics, loading,
  and game-loop code.
- Passes 305-307 (2026-07-11) made `ScriptOperandContext` /
  `ResolvedSelfContext` role-only (no cached concrete `Object*`), made
  `IDamageable` the authoritative script-visible liveness role (removing the
  `ITargetInfo` duplicate), and added the narrow `IAttachmentControl` role plus
  `IPhysical` safe-position state for spawned-character handling.
- Pass 308 (2026-07-11) completed the strict EgoScript/concrete-object cut:
  the lower-layer `ObjectRoleAccess` adapter resolves live refs to roles;
  `IScriptSystem` and `scr_run_chr_script()` dispatch only `ObjectRef`; new
  `IScriptRuntimeState`/`IVisibilityObserver` roles cover the VM driver;
  interpreter `ObjectValue` stores `ObjectRef`. Strict script sources no
  longer name concrete `Object`/`ObjectHandler`.
- Pass 309 (2026-07-15) moved the last read-only `GameModule` accesses in the
  top-of-DAG gamestates screens (`PlayingState` debug watches/export
  check/cheat, `MapEditorState::update`) onto the installed status, commands,
  object-world, and environment seams. `GameSessionContext::get()` dropped
  30 → 23; debug-only stragglers with no matching seam deliberately stayed
  concrete rather than widening a seam.
- Passes 310-311 (2026-07-15) extracted the audio+particle and developer-
  console lifecycles out of `GameEngine::initialize()/uninitialize()` into
  RAII composition-root members `GameplaySubsystemsBootstrap` and
  `ConsoleBootstrap` (joining `ContentRuntimeBootstrap`), preserving exact
  install/teardown order. This dropped the heavy `Object.hpp` aggregate from
  both `GameEngine` TUs. Durable note: on the abnormal-exit path
  (`~GameEngine` without `uninitialize()`), the bootstraps now tear their
  subsystems down where they previously leaked; verified non-throwing and
  safely ordered by reverse member destruction.
- Pass 313 (2026-07-21) started the T1.2 render-chain narrowing: the nine
  `ObjectGraphicsRenderer`/`ParticleGraphicsRenderer` render functions
  (`render`, `render_ref/trans/solid`, `render_enviro/tex`,
  `render_one_prt_solid/trans/ref`) now take the `Ego::Renderer&` their three
  render-pass callers already cache instead of re-fetching it from
  `EngineContext`. `EngineContext::get()` dropped 388 → 380 with zero new
  sites; debug-only helpers and non-renderer fetches were left as is. A scout
  scoped the follow-on: the single-root closures under
  `gfx_system_render_world` (`graphic_scene.c`, ~20 reducible sites) and
  `draw_hud` (`graphic_hud.c`, ~11) are the next candidates; multi-root
  utilities (`draw_blip`, `gfx_do_clear_screen/flip_pages`, init/teardown
  paths) stay on the singleton.
- Pass 314 (2026-07-21) resolved the `graphic_scene.c` half of that scope
  without a new services struct: `gfx()` was the single dominant service
  (17 of the file's 24 sites), so `gfx_system_render_world` now fetches
  `IGFX&` once and threads it through the anonymous-namespace
  `render_scene`/`render_scene_init` chain as a plain trailing reference
  parameter — the same idiom as Pass 313. `graphic_scene.c` dropped 24 → 8
  sites and `EngineContext::get()` 380 → 364, zero external signature
  changes. The `draw_hud` half was deliberately left: its 11 sites split
  across three services (input, camera-system, particle-handler), so the
  churn-to-visibility trade is worse and it needs its own call.
- Pass 315 (2026-07-21) executed the `draw_hud` half after that call was
  made: `draw_hud()` now fetches `Ego::Input::IInputSystem&`,
  `ICameraSystem&`, and `IParticleHandler&` once and passes them to the
  file-local `draw_help`/`draw_debug` helpers as reference parameters
  (`draw_debug` takes all three; the other helpers were untouched — they
  had no sites). `graphic_hud.c` dropped 16 → 8 sites and
  `EngineContext::get()` 364 → 356; zero external signature changes
  (`draw_hud()` keeps its `graphic.h` declaration). The root fetch is now
  unconditional where the debug branches previously fetched lazily; safe
  because `draw_hud` only runs from the `PlayingState` render path, where
  all three services are installed and already used unconditionally each
  frame. Still on the singleton by design: the local `config()` wrapper,
  multi-caller `draw_blip`, and the separate `draw_mouse_cursor` root.
- Pass 316 (2026-07-21) consolidated the `graphic.c` per-function fetch
  clusters the render-chain passes had scoped around: `reinitClocks()` now
  fetches `IGFX&` once (12 sites → 1; local named `igfx` because the
  file-scope `gfx_config_t` global is already named `gfx`),
  `gfx_system_load_assets()` fetches `Ego::Renderer&` once (4 → 1), and
  `TileRenderer::get_texture`/`TileRenderer::bind` fetch the atlas manager
  and renderer once per call (2 → 1 and 3 → 1; `bind` is a per-frame hot
  path). Pure within-function consolidation — zero signature changes and no
  parameter threading. `graphic.c` dropped 28 → 11 sites and
  `EngineContext::get()` 356 → 339 (total `::get()` 435 → 418). Still on
  the singleton by design: the init/teardown singles
  (`gfx_system_release_all_graphics`, `gfx_system_reload_all_textures`,
  `gfx_do_flip_pages`) and the single-site `GFX::update_particle_instances`.
- Pass 317 (2026-07-21) extended the Pass 313 particle render chain to its
  second service: `render_one_prt_solid/trans/ref` now take
  `IParticleHandler&` as a trailing parameter after the `Ego::Renderer&`
  they already receive, replacing the nine per-particle
  `EngineContext::get().particleHandler()` fetches (particle lookup plus
  transparent/light texture getters) inside the three functions. The three
  render-pass callers (`OpaqueEntities`, `NonOpaqueEntities`,
  `EntityReflections`) fetch the handler once at their `doRun` root next to
  `objectHandler` — Opaque and Reflections previously re-fetched it inside
  their entity loops, so this also hoists a per-entity singleton lookup out
  of two hot loops; NonOpaque gains the one new root fetch. Same-file
  cleanup: `render_prt_bbox` fetches the renderer once (2 → 1).
  `graphic_prt.c` dropped 20 → 10 sites and `EngineContext::get()`
  339 → 330 (total `::get()` 418 → 409). Still on the singleton: the local
  `config()` wrapper, the `logTarget()` error-path fetches in the three
  render functions, the `render_all_prt_attachment/bbox` iterator roots,
  and the debug-only `inputSystem` F7 check.
- Pass 318 (2026-07-21) constructor-injected `egoboo_config_t&` and
  `Log::Target&` into `AudioSystem` — the first engine service moved from
  per-call service-locator fetches to constructor injection. A teardown
  characterization established safety first: both referents outlive the
  audio system on every path (`SystemService` installs log/config before
  any engine exists and tears them down after `clearEngine()`;
  `egoboo_config_t::_singleton` is process-lifetime; `~AudioSystem` is
  Mix-only and never logs), no code swaps the active log target
  mid-lifetime, and all 22 test fixtures uninitialize audio strictly
  before their `ContentRuntimeBootstrap`. `AudioSystemCreateFunctor` takes
  the two services via `idlib::singleton::initialize` argument forwarding;
  `GameplaySubsystemsBootstrap` resolves them once at the composition
  root. Bonus fix: `AudioSystem::download(cfg)` previously ignored its
  parameter and re-fetched the global config — it now uses `cfg` (same
  object in production, passed by `config_synch`). The anonymous-namespace
  `config()` wrapper and the `EngineContext.hpp` include are gone from
  `AudioSystem.cpp` entirely (13 → 0 sites, file no longer names
  `EngineContext`); accepted semantic delta: services are captured at
  `initialize()` instead of resolved per call, observable only if a seam
  were re-pointed mid-lifetime, which nothing does. `EngineContext::get()`
  330 → 316 (total `::get()` 409 → 397). 22 test files updated to pass the
  installed services at their existing `AudioSystem::initialize()` sites.
- Pass 319 (2026-07-21) consolidated EngineContext fetch clusters across the
  six gamestate screens (`LoadingState`, `DebugModuleLoadingState`,
  `DebugObjectLoadingState`, `MapEditorState`, `PlayingState`,
  `DebugParticlesScreen`) — the Pass 316 within-function root idiom applied
  to the gamestates archive. Design call recorded: this family cannot hold
  service references as members (services are cleared before the state
  stack is destroyed — the teardown ordering documented in
  `PlayingState::~PlayingState`) and its virtual signatures come from the
  gui-archive `GameState` base, so function-root locals are the endpoint
  idiom here; deferred lambdas (the `LoadingState` start-button callback)
  keep fetching at invoke time by design. The dominant clusters were the
  loading screens' repeated `logTarget()` fetches spanning try/catch
  handlers (7/5/4 → 1 each, hoisted above the `try`), `profileSystem` in
  `DebugObjectLoadingState::loadObjectData` (3 → 1) and the
  `DebugModuleLoadingState` ctor (2 → 1), `particleHandler` plus
  `profileSystem` in the `DebugParticlesScreen` ctor, `cameraSystem` in
  `MapEditorState::drawContainer`/`loadModuleData`, and the
  `graphicsSystem`+`config` pairs in the `beginState` methods. Bonus:
  removed `PlayingState`'s dead anonymous-namespace `audioSystem()` helper
  (unused since the destructor moved to `tryAudioSystem`; eliminates a
  pre-existing `-Wunused-function` warning). Zero signature changes;
  behavior-identical singleton references. Family 65 → 44 lines;
  `EngineContext::get()` 316 → 294 (total `::get()` 397 → 375). Remaining
  sites are intentional residual: one-fetch-per-function singles, the
  teardown-guarded destructor accesses, and the anonymous-namespace
  `inputSystem()`/`audioSystem()` consolidators.
- Pass 320 (2026-07-21) constructor-injected `Log::Target&` into
  `ProfileSystem` — the second engine service moved to constructor
  injection, reusing the Pass 318 pattern (characterize teardown → inject
  via singleton create-functor args → composition root resolves).
  Characterization: the referent outlives `ProfileSystem` on every path —
  the game installs the active log target in `Ego::Core::System`
  initialization before any engine exists and tears it down after
  `clearEngine()`, while the validator and the 26 logging-enabled test
  fixtures nest `ProfileSystem` strictly inside the log target within
  `ContentRuntimeBootstrap` itself (install line precedes
  `ProfileSystem::initialize`, clear follows `uninitialize`); the one
  `initializeLogging = false` fixture (`PlayerQuestLog`) also disables the
  profile system entirely; no code swaps the active target while a
  `ProfileSystem` lives (installers are the two `System.cpp` boot
  constructors plus CRB-with-logging only); `~ProfileSystem` is log-free
  (`parser_state_t::uninitialize` only); all seven fetch sites are
  `ProfileSystem` member functions (`loadOneProfile` ×5,
  `loadModuleProfiles`, `loadAllSavedCharacters`). Implementation mirrors
  `AudioSystem`: `ProfileSystemCreateFunctor`/`DestroyFunctor`,
  three-parameter `idlib::singleton` base, protected
  `ProfileSystem(Log::Target&)`, reference member `_logTarget`,
  `namespace Log { struct Target; }` forward declaration in the header.
  Unlike Pass 318's 22 fixture updates, `ProfileSystem::initialize` has
  exactly one call site — `ContentRuntimeBootstrap`, which now resolves
  `EngineContext::get().logTarget()` at the composition root (provably
  populated there on every reachable path, so capture-at-initialize
  cannot introduce a new throw). `ProfileSystem.cpp` 7 → 0 sites and no
  longer names `EngineContext` (the `game.h` include comment dropped its
  "EngineContext conduit" note); CRB 12 → 13 (intentional).
  `EngineContext::get()` 294 → 288 (total `::get()` 375 → 369). Accepted
  semantic delta: capture-at-initialize instead of per-call resolution,
  observable only if a log-target seam were re-pointed mid-lifetime,
  which nothing does.
- Pass 321 (2026-07-21) threaded `Ego::Renderer&` and
  `Ego::IGraphicsSystem&` through the `CameraSystem` render chain — the
  Pass 313-315 trailing-parameter idiom, chosen after the characterization
  ruled out constructor injection for this service. Two hard blockers:
  the `ScriptSystemsFunctions` harness initializes the real
  `CameraSystem` singleton in a headless ctest fixture that installs a
  mock graphics system but no renderer (injection would force
  `EngineContext::get().renderer()` where no renderer can exist), and the
  runtime never calls `CameraSystem::uninitialize()` — the graphics
  bootstrap teardown hook only clears the seam — so the singleton object
  outlives `~AppImpl`, where the renderer and graphics system die,
  leaving captured references dangling for the process tail.
  `renderAll` now resolves both services once at its per-frame root next
  to the existing `renderedFrameCount` fetch and passes them as trailing
  reference parameters into the private `beginCameraMode`/`endCameraMode`
  (no external callers, so zero public signature changes); this also
  hoists five per-camera singleton re-fetches out of the per-frame
  camera loop. `autoFormatTargets` keeps its single load-time fetch
  (one-per-function residual). `CameraSystem.cpp` 7 → 4 sites;
  `EngineContext::get()` 288 → 285 (total `::get()` 369 → 366).

## Documentation Passes

- The 2026-04-18 consolidation collapsed the directory from 65 files to 14:
  four overlapping strategic plans merged into the roadmap, ~50 per-pass docs
  merged into this log (full detail remains in git history), inline
  status-update blocks stripped from the foundational docs, and the README
  rewritten as a Reference/Roadmap/History index.
- Pass 274 (2026-06-23) shortened the top-level docs and redirected volatile
  numbers to `CODEBASE-HEALTH-STATUS.md`. Pass 293 (2026-06-30) compressed
  this log, folded live glTF and cartman guidance into canonical docs, and
  rechecked metrics.
- Pass 312 (2026-07-21) — second major consolidation: merged
  `04-refactoring-strategy.md` into `19-refactoring-roadmap.md` (now the
  single strategy + what-is-left document with phase status), deleted
  `70-documentation-consolidation.md` into this section, compressed docs
  02/03/05/06/07 and the Pass 294-311 entries above, and added the measured
  pre-refactoring comparison against `backup-copy/` to
  `CODEBASE-HEALTH-STATUS.md`. Directory now 12 documents; no runtime,
  build, or test changes.
