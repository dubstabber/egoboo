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
- Pass 322 (2026-07-22) constructor-injected `Ego::Input::IInputSystem&`,
  `Ego::Renderer&`, `Ego::IGraphicsSystem&`, and `Ego::IFontManager&` into
  the developer `Console` — the third injected engine service. Both
  Pass 321 blocker checks pass here: no test initializes the `Console`
  singleton (zero fixture churn), and its lifetime is strictly bounded —
  `ConsoleBootstrap` constructs it after the input-system install and
  graphics bootstrap and `~ConsoleBootstrap` uninitializes it before the
  graphics teardown and input-system clear in `GameEngine::uninitialize`,
  so the injected references provably outlive the console. The font
  manager is used only to load the console font in the constructor and is
  not retained; the other three become reference members. The
  `ConsoleCreateFunctor` forwards the wider signature through
  `idlib::singleton::initialize`. `Console.cpp` 6 → 0 sites and no longer
  includes `game/Core/EngineContext.hpp` (its only remaining includes of
  concrete service headers were replaced with the `I*` seam headers);
  `ConsoleBootstrap` resolves the services once at the composition root,
  2 → 1 sites. Cartman's gated call site was updated to pass its concrete
  singletons and now initializes `Ego::Input::InputSystem` around the
  console lifetime — previously its console threw on every keydown
  because cartman never installs the `EngineContext` input service
  (cartman modifiers still read as none since nothing pumps
  `InputSystem::update()` there). Note: the gated cartman link was found
  already broken at baseline — `Ego::AppImpl` moved to
  `egolib-game-graphics` in the June App.cpp relocate but
  `cartman/CMakeLists.txt` still links only `egolib-library` — this pass
  neither caused nor fixed that. `EngineContext::get()` 285 → 278 (total
  `::get()` 366 → 359).
- Pass 323 (2026-07-22) repointed cartman's link from `egolib-library` to
  `egolib-game-graphics`, restoring the gated cartman build: the
  `Ego::App`/`AppImpl` application base that `Cartman::GFX` derives from
  was relocated to `egolib-game-graphics` in the June App.cpp pass, which
  silently broke the (default-OFF) cartman link with undefined `AppImpl`
  references. One-line `target_link_libraries` change; the default build
  is unaffected (the cartman directory returns early when gated off).
  Verified with a `-DEGOBOO_BUILD_CARTMAN=ON` scratch build: cartman
  compiles and links; the default build no-ops.
- Pass 324 (2026-07-22) constructor-injected `Ego::Renderer&` into
  `BillboardSystem` — the fourth injected engine service. Both blocker
  checks pass: every test uses an `IBillboardSystem` stub (the real class
  is never constructed in tests, so the signature change causes zero
  fixture churn), and C++ base-before-member ordering bounds the
  lifetime — the `App` base (`AppImpl`) installs the renderer before the
  `GameApp` member (`GameAppImpl`) constructs the billboard system, and
  destruction exactly reverses that, so the injected reference provably
  outlives the system (contrast `CameraSystem`, whose singleton is never
  uninitialized). The renderer is resolved once in the `GameAppImpl`
  composition constructor. The fourth site —
  `EngineContext::get().uiManager()` for the floating-text font — was
  moved onto the GUI-layer `Ego::GUI::activeUIManager()` seam instead of
  being injected, because the `UIManager` lifetime is *not* nested in the
  billboard system's (created after the graphics bootstrap, destroyed
  before its teardown); the seam header documents both paths resolve the
  same instance in the running engine. `BillboardSystem.cpp` 4 → 0 sites
  and no longer includes `game/Core/EngineContext.hpp`;
  `graphic_init.cpp` 10 → 11 (intentional composition-root resolve).
  `EngineContext::get()` 278 → 275 (total `::get()` 359 → 356).
- Pass 325 (2026-07-22) threaded `IParticleHandler&` and `Log::Target&`
  through the `ParticleGraphics::update` chain — the Pass 313-315
  trailing-parameter idiom, chosen because `ParticleGraphics` is a
  per-particle value type embedded in every `Particle` (`pprt->inst`),
  not a service: constructor injection would relocate reference members
  into thousands of instances. The chain is perfectly closed:
  `update` has exactly one caller — the per-frame root
  `GFX::update_particle_instances` in `graphic.c`, which already fetched
  the particle handler for its iteration loop — and the protected
  `update_vertices`/`update_lighting` are called only from `update`, so
  the widened signatures have no other callers (none in tests).
  The root resolves both services once and threads them down.
  `ParticleGraphics.cpp` 4 → 0 sites and no longer includes
  `game/Core/EngineContext.hpp` (gains explicit
  `Entities/IParticleHandler.hpp` + `Log/_Include.hpp` includes);
  `graphic.c` 11 → 12 (the added one-line log-target root fetch).
  `EngineContext::get()` 275 → 272 (total `::get()` 356 → 353).
- Pass 326 (2026-07-22) — T1.3: `ModuleLoadContext` now carries only
  explicit inputs. The four `this`-capturing `std::function` callbacks
  (`loadProfiles`, `loadAllPassages`, `loadTeamAlliances`,
  `logSlotUsage`) were converted to `module_loading` free functions with
  explicit parameters, and the four private `GameModule` member
  functions were deleted (they had zero callers outside the constructor
  lambdas). Real dependencies turned out narrower than the class:
  `loadTeamAlliances(std::vector<Team>&)`,
  `logSlotUsage(IProfileSystem&, savename)`,
  `loadProfiles(GameModuleRuntime&, const ModuleProfile&)` (its
  `isImportValid()` was just `getImportAmount() > 0`; the `import_data`
  global is `pro_import_t`, ProfileSystem-owned, distinct from the
  session `importList` — left as-is). Only
  `loadPassages(GameModule&, const ego_mesh_t&, passages&)` still needs
  the module, because `Passage`'s constructor stores a reference to its
  owning module — that coupling is now a visible, documented context
  field (`module`) instead of a hidden lambda capture, and is the next
  narrowing target. `ModuleLoadPhase::finalizeInitialization` resolves
  the profile system through the runtime provider at the call site.
  No metric changes (structural pass). Gate: build green, ctest
  955/955, validator full 42/10/245 + test.mod 0/0, nm clean — the
  module-construction fixtures (ModuleUpdate, ShopInteractions,
  ImportWorkflow, ScriptRuntime, …) exercise the real load path.
- Pass 327 (2026-07-22) — T1.3 follow-on: narrowed `Passage`'s stored
  dependency from `GameModule&` to `ego_mesh_t&` + `ObjectHandler&`.
  All seven `_module` uses mapped to exactly those two members: mesh
  (tile-index build in the constructor, `clear_fx`/`add_fx` in
  open/close, `getTileInfo` in flashColor) and object handler
  (close-blocker scan, `whoIsBlockingPassage`, `makeShop`). The
  `ModuleLoadContext::module` field became `objectHandler`
  (`GameModule::_gameObjects`), so no load step references the module
  object anymore; `loadPassages` takes `(ego_mesh_t&, ObjectHandler&,
  passages&)`. Lifetime envelope unchanged: both referents are
  `GameModule` members that the passage list never outlives (and a
  script-held `shared_ptr<Passage>` past module teardown was equally
  dangling via the old `_module` reference). Six direct `Passage`
  constructions in four test files updated to
  `(*module.getMeshPointer(), module.getObjectHandler(), …)`.
  **Scout lesson: construction-site sweeps must include `egolib/tests`
  — the initial library-only grep missed all six test call sites.**
  Gate: build green, ctest 955/955, validator full 42/10/245 +
  test.mod 0/0, nm clean.
- Pass 328 (2026-07-22) — T1.3 update half: moved the per-update world
  steps off `GameModule`, mirroring the Pass 326 load-side pattern. The
  four private helpers became explicit-input code: `checkPassageMusic`,
  `updateAllObjects`, and `updateDamageTiles` are now `module_update`
  free functions (declared in new `Module_update.hpp` with the
  `DAMAGETILETIME` constant), and the pit logic plus its four loose
  fields (`_pitsClock`/`_pitsKill`/`_pitsTeleport`/`_pitsTeleportPos`)
  were aggregated into a `PitsState` value struct (new `Pits.hpp`,
  bodies in `Module_update.cpp`) with `enableKill`/`enableTeleport`/
  `update(ObjectHandler&, IParticleHandler&, IAudioSystem&, const
  damagetile_instance_t&)` — the same env-state idiom as
  `WeatherState`/`AnimatedTilesState`. `GameModule` keeps only the
  `update()` orchestrators (which resolve services through
  `GameModuleRuntime` once per step and pass them down) and the
  `IModuleCommands` pit overrides delegating to `_pits`; the
  `PITDEPTH`/`PIT_CLOCK_RATE` constants moved to
  `PitsState::DEPTH`/`CLOCK_RATE`. Real dependencies again proved
  narrower than the class: no update step needs the module object —
  only the handler, mesh, damage-tile config, and two services. Test
  sweep (per the Pass 327 lesson) updated three files: `ModuleUpdate`
  calls the free functions/`_pits.update` with explicit services,
  `ScriptActionFunctions` passes its `StubAudioSystem` directly to
  `checkPassageMusic`, `ScriptSystemsFunctions` reads `_pits.*`.
  Both new headers registered in the master `EGOLIB_GAME_MODULE_SOURCES`
  list (header-only: archive membership unchanged, 0 stray `.h.o`).
  Gate: build green, ctest 955/955, validator full 42/10/245 +
  test.mod 0/0, all nine archive counts unchanged.
- Pass 329 (2026-07-22) — T3.5 opened: `GameEngine` state-stack transition
  characterization. Scout first closed T1.3 (the spawn family is already at
  its endpoint: `realizeSpawnEntry` is extracted pure logic with dedicated
  tests, its `std::function` ops are a deliberate stub seam, and
  `GameModuleRuntime`'s providers must stay call-time-resolving because
  tests swap active services mid-module). A fresh five-axis re-scout then
  ranked next fronts; #1 was the state-stack flow inlined in
  `GameEngine::updateOneFrame` — no existing test exercised
  push/pop/clear/fallback semantics. Implementation: pure code motion of
  the stack-advance block into a new private
  `GameEngine::advanceGameStateStack()` (updateOneFrame now calls it
  first), plus new `GameStateStackTransitions.cpp` with 8 headless tests
  (GameEngine's default ctor touches no services; a `StubGameState` must
  override `draw` and `drawContainer` — `Component::draw` is also pure —
  and never draws). Pinned semantics: push begins the state immediately;
  fallthrough re-runs `beginState()` on each re-entered state (including
  already-ended ones — legacy quirk, they are re-begun then popped again in
  the same advance); `setGameState` clears lazily on the next advance and
  the surviving state is NOT re-begun; the clear applies BEFORE ended-state
  fallthrough; a drained stack calls the main-menu factory, or throws
  `std::logic_error` if none is installed. Ranked runner-ups recorded in the
  roadmap: scr_* dispatch coverage gap measured at 109/404 untested
  (T3.3), hud-widgets 4/6 untested (T3.5 follow-on); T1.4 gfx_rv migration
  rejected (GL-dependent, no harness), T3.6 rejected (LoadServices seam
  already exists). Gate: build green, ctest 963/963 (955 + 8 new),
  validator full 42/10/245 + test.mod 0/0, archive counts unchanged (no
  CMake change; tests glob via CONFIGURE_DEPENDS).
- Pass 330 (2026-07-22) — T3.3 dispatch-coverage slice: the entire
  32-function alert-check family (`script_functions_alerts.c`) went from
  zero test references to full characterization in new
  `ScriptAlertFunctions.cpp` (5 tests, standard ContentRuntimeBootstrap +
  test.mod fixture). Data-driven over three shapes: (1) the 27 one-bit
  `ALERTIF_*` checks pinned as exact one-bit tests (false on empty alert,
  true on own bit, false on all-other-bits; `ALERTIF_TAKENOUT` is
  `1 << 31`, a negative int — needs a cast into a `uint32_t` table field);
  (2) the four `IfHitFrom*` checks pinned as raw-value windows
  `[ATK_* - 8192, ATK_* + 8192)` over `Facing`'s unnormalized `int32_t` —
  including two legacy quirks: raw `-8192` is inside the front window
  while the canonically equal `57344` is not, and canonical facings in
  `[57344, 65535]` are a dead zone no quadrant accepts (Facing comparisons
  are raw-int comparisons; `Facing(int32_t)` deliberately does not
  normalize); (3) `IfSomeoneIsStealing` requires the
  `order_value == SHOP_STOLEN && order_counter == SHOP_THEFT` pair.
  A resolved-self guard test sweeps all 32 with every predicate satisfied
  and an invalid self ref. Untested `scr_*` count: ~110 → 78.
  Test-only pass (no production sources touched), so the validator
  baseline is unaffected by construction. Gate: build green, ctest
  968/968 (963 + 5).
- Pass 331 (2026-07-22) — T3.3 dispatch-coverage slice: the locomotion
  family, in new `ScriptLocomotionFunctions.cpp` (4 tests, standard
  fixture). The four maxSpeed setters
  (`script_functions_movement_locomotion.c` — the TU's other 8 functions
  were already covered) are pinned as exact fractions written to the
  script runtime's `ai_state_t::maxSpeed`: Run 1.0, Walk 0.66, Sneak
  0.33, Stop 0.0 — and as no-ops (false return, maxSpeed untouched) for
  an unresolved self. `scr_KeepAction`/`scr_UnkeepAction`
  (`script_functions_action.c`) are pinned against the object's
  animation freeze flag (`ObjectGraphics::_freezeAtLastFrame`, read via
  the existing `ObjectGraphicsTestAccess` helper): keep sets it, unkeep
  clears it, and neither touches any object when self is unresolved.
  Untested `scr_*` count: 78 → 72. Test-only pass; validator baseline
  unaffected by construction. Gate: build green, ctest 972/972 (968 + 4).
- Pass 332 (2026-07-22) — T3.3 dispatch-coverage slice completing
  `script_functions_action.c` (now zero untested functions), in new
  `ScriptActionSupportFunctions.cpp` (5 tests). The seven RTS speech
  setters (`scr_SetSpeech`, `SetMoveSpeech`, `SetSecondMoveSpeech`,
  `SetAttackSpeech`, `SetAssistSpeech`, `SetTerrainSpeech`,
  `SetSelectSpeech`) are pinned as accepted no-ops — true for a resolved
  self, false otherwise, no side effects — dispatch compatibility for
  legacy scripts. `scr_CallForHelp` publishes the caller as the team's
  caller-for-help and raises `ALERTIF_CALLEDFORHELP` on live non-hating
  others but never on the caller itself. `scr_DoActionOverride` starts a
  model-valid action even when the current animation is uninterruptible
  (probed via the `findValidAction` candidate idiom from
  `ObjectAccessors.cpp`; a wild out-of-range action index would probe
  `_actionMap` out of bounds, so the invalid-action path is deliberately
  not exercised). `scr_ShowTimer` sets the `timeron`/`timervalue`
  globals (`egoboo.h`). **Fixture lesson: `Team::callForHelp` walks the
  handler's ref iterator, and freshly spawned objects sit in the
  pending-add list until an iterator is constructed — tests must flush
  (the `flushObjectHandler` idiom from `ModuleUpdate.cpp`) before
  exercising handler-iterating code.** Untested `scr_*` count: 72 → 62.
  Test-only pass; validator baseline unaffected by construction.
  Gate: build green, ctest 977/977 (972 + 5).
- Pass 333 (2026-07-27) — T3.3 dispatch-coverage slice closing the state
  family: all 34 remaining functions across `script_functions_state.c`
  (21) and `script_functions_state_inventory.c` (13), in new
  `ScriptStateControlFunctions.cpp` (24 tests, standard fixture). Both
  state TUs now have zero untested functions. Pinned behaviors: the
  `IfStateIs0..15` literal ladder ignores `script_state_t` entirely
  (unlike the generic `IfStateIs`/`IfStateIsNot`, which compare
  `tmpargument` against `state` with plain signed equality);
  `IfTimeOut` is a strict raw unsigned `>` against
  `worldUpdateCount()` (equal counts do not time out); `SetTime` is a
  silent no-op that still returns TRUE for non-positive delay, and
  wraps in `uint32_t` for positive delay; `SetWeatherTime` writes both
  `timer_reset` and `time`, and its resolved-self guard runs before
  `activeModuleEnvironment()` (headless call with no module returns
  false instead of throwing); `scr_End` returns FALSE even on success
  while setting `terminate`; `scr_DoNothing` is the family's only
  guard-free function; the platform-operator checks report "not this
  platform" for an unresolved self even on the build's own platform;
  `IfStateIsOdd` pins C++ truncating modulo (`-3` is odd → TRUE, `-2` →
  FALSE). **Review lesson: guard characterization tests must choose
  operands that satisfy the underlying predicate — the first draft had
  four unresolved-self assertions whose comparison was false anyway
  (e.g. `x == y` while testing `IfXIsLessThanY`'s guard), passing
  regardless of whether the guard ran; an adversarial review round
  caught all four.** Untested `scr_*` count: 61 → 27 by direct
  re-measure (the logged 62 chain had drifted by one incidental test
  reference). Test-only pass; validator baseline unaffected by
  construction. Gate: build green, ctest 1001/1001 (977 + 24).
- Pass 334 (2026-07-27) — T3.3 dispatch-coverage slice closing the
  movement family: the 9 remaining functions across
  `script_functions_movement.c` (AddWaypoint, ClearWaypoints, Compass,
  IfAtWaypoint, IfAtLastWaypoint) and
  `script_functions_movement_physics.c` (AddXY, GetXY, SetXY,
  SetSpeedPercent), in new `ScriptMovementSupportFunctions.cpp` (9
  tests, standard fixture). Both TUs now have zero untested functions.
  Pinned behaviors: `AddWaypoint`'s cached `wp`/`wp_valid` refresh via
  `get_wp()` always reports the tail (oldest) waypoint, not the newest
  push, and once the list is full (`MAXWAY` 8) it silently overwrites
  the final slot forever while still returning true; `ClearWaypoints`
  resets `_head`/`_tail` but leaves the cached `wp`/`wp_valid` stale;
  `IfAtWaypoint`/`IfAtLastWaypoint` are pure alert-bit predicates
  independent of the waypoint list, and `ALERTIF_PUTAWAY` aliases the
  `ALERTIF_ATLASTWAYPOINT` bit; `Compass` subtracts a
  `facing >> 2`-quantized trig offset and truncates toward zero, and
  `Facing` canonicalization wraps by 65535 rather than 65536 (turn
  65536 reproduces turn 0's bucket; turn −1 and unwrapped turn 65535
  land in the same bucket); the "8-slot" STOR storage is actually 16
  slots addressed by `argument & 15` with plain signed masking (−1 →
  slot 15); `SetSpeedPercent` divides by 100 with a zero floor and no
  upper clamp. All nine leave state untouched behind the unresolved-self
  guard (sentinel-verified, non-vacuous operands). A source-comment
  hypothesis that `AddWaypoint`'s entry guard and `get_wp`'s internal
  liveness guard could diverge was investigated and debunked — both
  bottom out in the same `ObjectHandler::exists()` check for any real
  spawned object, so the non-diverging behavior is what got pinned.
  Untested `scr_*` count: 27 → 18. Test-only pass; validator baseline
  unaffected by construction. Gate: build green, ctest 1010/1010
  (1001 + 9), zero adversarial-review findings.
- Pass 335 (2026-07-27) — T3.3 dispatch-coverage slice closing the
  target family: the 12 remaining functions across
  `script_functions_target.c` (IfDistanceIsMoreThanTurn,
  IfTargetIsOldTarget, IfTargetIsSelf), `script_functions_target_orders.c`
  (CreateOrder, GetAttackTurn, GetDamageType, IssueOrder,
  OrderSpecialID, SetOldTarget), and `script_functions_target_select.c`
  (SetOwnerToTarget, SetTargetToNearestLifeform, SetTargetToSelf), in
  new `ScriptTargetSupportFunctions.cpp` (22 tests, standard fixture).
  All three TUs now have zero untested functions. Pinned behaviors:
  `IfDistanceIsMoreThanTurn` is a raw strict signed compare of the two
  registers (no Facing wrap, target ignored entirely);
  `IfTargetIsOldTarget` compares raw refs with no liveness check
  (never-set target and old-target read as equal — vacuous truth);
  **`CreateOrder` bug pinned as-is: the packed order accumulates in a
  `uint16_t`, so the target byte (`(ref & 0xFF) << 24`) is entirely
  lost and the x field keeps only the low 2 bits of `x >> 6` — the
  packed result is independent of the target ref**; `GetAttackTurn`
  publishes `directionlast` verbatim and returns true even for zero
  (unlike the GrogTime/DazeTime siblings); `GetDamageType` publishes
  the 0xFF "never hit" sentinel verbatim; `IssueOrder` broadcasts to
  live same-team members including the caller itself, overwrites
  already-ordered recipients (add_order's "was new" return is
  discarded), and skips terminated objects; `OrderSpecialID` matches
  the profile [SPEC] IDSZ with no team filter and no self-exclusion;
  `SetOldTarget`/`SetOwnerToTarget` copy the target ref verbatim with
  zero validation (Invalid and terminated refs included);
  `SetTargetToNearestLifeform` internally sets TARGET_ITEMS so ground
  items and invincible candidates are eligible "lifeforms", skips held
  items and dead candidates, fails while preserving the current target
  when nothing visible exists, and cannot see pending (unflushed)
  spawns. Fixture notes: order broadcasts iterate the handler — flush
  pending spawns first; the headless fixture has no LOS terrain, so
  the searching actor is made instance-invincible to take
  `chr_find_target`'s documented invincible-source LOS bypass. One
  adversarial-review fix round: a guard-test comment falsely claimed
  the nearest-lifeform sub-case distinguishes the resolveSelfContext
  guard (both that guard and `chr_find_target`'s internal source
  lookup derive from the same self ref, so they are forced to agree —
  second confirmed instance of the guard-divergence-is-unreachable
  pattern); assertions were already correct, comment rewritten
  honestly. Untested `scr_*` count: 18 → 6. Test-only pass; validator
  baseline unaffected by construction. Gate: build green, ctest
  1032/1032 (1010 + 22).
- Pass 336 (2026-07-28) — T3.3 dispatch-coverage FINAL slice: the last
  6 untested `scr_*` functions, a residual grab-bag across five TUs, in
  new `ScriptResidualFunctions.cpp` (24 tests, standard fixture, plus
  a `ScopedTestEngine` RAII and stub Controller/NonController game
  states). **Every one of the 404 `scr_*` dispatch functions now has
  test references — the coverage gap measured at ~110 on 2026-07-22 is
  closed.** Pinned behaviors: `ReaffirmCharacter` spawns into the
  particle handler's PENDING list (invisible to active-only counts
  until a flush) and raises `ALERTIF_REAFFIRMED` whenever below the
  profile's attached amount, but suppresses the alert at capacity;
  double-reaffirm before any flush double-spawns; `DisaffirmCharacter`
  raises `ALERTIF_DISAFFIRMED` unconditionally on any resolved self and
  synchronously terminates only ACTIVE attached particles —
  still-pending particles survive and get promoted to active by the
  call's own iterator teardown; `EndModule` is a silent true-returning
  no-op with no engine or a non-`IPlayingStateController` current
  state, and calls `endModuleInVictory()` exactly once on a controller
  state; `FindTileInPassage`'s docstring ("x/y set to 0 on failure")
  is false — registers are untouched on failure; its scan overscans
  one row/column past the passage's nominal span, honors `state.x`
  only on the first scanned row, re-finds the same tile when re-called
  with the returned center, and ignores the tile's upper `_img` bits;
  `SetVolumeNearestTeammate` is an empty stub beyond the guard (no
  registers, no audio call — the guard test documents that the guard
  is architecturally indistinguishable from a downstream failure
  because there is no downstream); `IfLeaderKilled` is a pure
  non-consuming alert-bit test (exactly `1<<13`), independent of
  actual team-leader state. **Fixture lessons (bit during authoring):
  (1) merely READING particle counts via
  `number_of_attached_particles()`/`iterator()` promotes pending
  particles to active (iterator destructor side effect) — raw
  `_activeParticles`/`_pendingParticles` peeks are needed to observe
  pending state; (2) `Object::respawn()` (part of normal spawn)
  already auto-reaffirms attached particles, so a freshly spawned
  torch has 1 pending particle and `ALERTIF_REAFFIRMED` set — clear
  both for an isolated baseline; (3) `EngineContext::clearEngine()`
  also clears ImageManager/PerkHandler/ProfileSystem/Audio/Particle —
  a scoped test engine must reinstall all five afterward to survive
  single-process (non-ctest) runs.** Untested `scr_*` count: 6 → 0.
  Test-only pass; validator baseline unaffected by construction.
  Gate: build green, ctest 1056/1056 (1032 + 24), zero
  adversarial-review findings.
- Pass 337 (2026-07-28) — T3.5 hud-widgets slice: `CharacterStatus`
  characterized in new `CharacterStatusWidget.cpp` (5 tests) after a
  two-agent feasibility scout over all four untested widgets. Pinned:
  construction is completely service-free (stores the ref,
  `make_shared<ProgressBar>`, default 32×32 Component bounds — the only
  widget of the four buildable with zero GUI infrastructure); the
  self-destroying lifecycle contract `PlayingState` relies on — `draw()`
  destroys the widget and detaches it from its parent `Container` when
  no session state is active or when the observed object does not
  resolve — and the deterministic headless wall: with a live object and
  no installed UIManager, `draw()` throws `std::logic_error` from
  `activeUIManager()` strictly before any `TextureManager` access
  (which would deadlock headless), leaving the widget undestroyed.
  **Finding: `tryObservedObject`'s `isTerminated()` re-check is
  unreachable dead code through this chain — `Object::requestTerminate()`
  synchronously erases the handler map entry
  (`ObjectHandler.cpp` `remove()`), so a "terminated but still
  resolvable" object cannot exist via ref lookup; missing and
  terminated refs collapse to the same nullptr path. The two draw()
  gates are also not black-box orderable (documented, not asserted).**
  Scout verdict recorded in the roadmap: the other three widgets are
  blocked at the constructor font/UIManager wall; cheapest unlocks are
  a `Button`/`Label` lazy text-layout seam (frees `ModuleSelector` and
  its latent <3-modules `size_t` underflow quirk) and extracting
  `LevelUpWindow::doLevelUp` into a pure function. Test-only pass;
  validator baseline unaffected by construction. Gate: build green,
  ctest 1061/1061 (1056 + 5), zero adversarial-review findings.
- Pass 338 (2026-07-28) — T3.5 runtime seam: headless text-widget
  construction. `Button::setText` and `Label`'s ctor/`setText` gained a
  `tryActiveUIManager()` presence guard with a `_textLayoutPending`
  flag and a draw()-time self-heal (`Button::draw`, `Label::draw`,
  `IconButton::draw`); `Label` keeps its requested font type so the
  font is fetched lazily once a manager appears, and `Label::setFont`
  clears the pending flag. With a manager installed the code path is
  behaviorally identical to the old eager path (same `getFont` /
  `layoutText` / `layoutTextBox` / `setSize` calls, same
  unconditional-font-deref crash semantics on a broken install), and
  the design scout PROVED the headless branch unreachable in
  production: the sole UIManager install (GameEngine initialization)
  precedes the earliest possible text-widget construction
  (`MainMenuState`), with a ~35-site census confirming `Label` sizes
  are read in state ctors (layout must stay eager under a manager) and
  Button text metrics are draw-only. New `GuiTextLayoutHeadless.cpp`
  (6 tests) pins the headless guarantees — Button/Label/default-Label/
  `OptionsButton` (by-value Label)/`ScrollableList` all construct
  without a manager; a deferred Label keeps Component's default 32×32
  bounds (not 0×0). **Finding that reshaped the pass: the planned
  ModuleSelector characterization is infeasible even with the seam.
  Its ctor builds text Buttons in the member-init list and calls
  `uiManager().getScreenWidth()` in the body — one continuous call, so
  no-manager throws in the body, while installing the raw-storage fake
  manager re-enables the eager `getFont`→`Font::layoutText` path,
  which is unconditionally GL-bound and segfaults on the
  never-constructed fake (gdb-verified; pre-existing hazard, not a
  seam regression). `ModuleSelectorWidget.cpp` (1 test) pins the clean
  `std::logic_error` construction failure and documents the full
  trace; the wheel-clamp underflow pins wait for a headlessly-
  constructible UIManager or an injectable layout engine.** Full
  runtime gate: build green, ctest 1068/1068 (1061 + 7), validator
  full 42/10/245 + test.mod 0/0, nm back-edge check clean (9 archives,
  zero back-edges). Adversarial reviews: zero critical findings (six
  minor notes; the `setFont` pending-flag clear was applied).
- Pass 339 (2026-07-28) — T3.5 extraction: the level-up gameplay
  computation moved out of the GUI. `LevelUpWindow::doLevelUp`'s
  gameplay half (~110 lines) moved VERBATIM — statement order and
  global-Random consumption order preserved — into the new GUI-free
  `Ego::applyCharacterLevelUp(Object&, const Perks::Perk&, playerList)`
  in `game/Logic/LevelUp.{hpp,cpp}` (egolib-library, beside
  `Player.cpp`; library archive 84 → 85 members): seed from
  `getLevelUpSeed()` first, the 8 float-interval attribute draws in
  `AttributeType` order, unconditional perk grant, +1 perk-type bonus,
  the 19-case flat-bonus switch (including the five
  immediate secondary-attribute mutations), level-index bump,
  `ALERTIF_LEVELUP`, player level-up-indicator clear, anti-save-scum
  `randomizeLevelUpSeed()`, the might→fat/size growth block (exact
  `!= 0` float compare), then the per-attribute
  read-displayed-value-then-apply loop. The function returns a
  `LevelUpReport { increase[8], displayedValue[8] }`; the widget makes
  one call and formats the report (the GUI displays pre-increase
  values — preserved via `displayedValue`). The unguarded
  `playerList[getPlayerNumber()]` index is preserved with a hazard
  comment (pre-existing; UB for non-players); the profile `[SEED]`
  override is parsed but never applied (dead code) — both flagged as
  candidate follow-up bugs. New `CharacterLevelUp.cpp` (9 tests,
  fixture registers the spawned follower as a player via `addPlayer` —
  mandatory for the indicator path) pins the computation with an
  RNG-replay oracle (never hardcoded mt19937 values): seeded
  determinism robust to generator pollution, the follower's
  profile gain intervals (LIFE_REGEN exactly [0,0] → SOLDIERS_FORTITUDE
  yields exactly +0.15), TOUGHNESS/GIGANTISM/BRUTE/POWER/ACROBATIC/
  NIGHT_VISION/default-case table rows, MAX_LIFE/MAX_MANA
  current-value coupling, level +1 with XP untouched, alert bit,
  indicator true→false, deterministic new seed, and fat delta exactly
  `getSizeGainPerMight() * 0.1f * increase[MIGHT]` + SIZETIME.
  **Scout-correction: the testplan's claim that follower's `[LEVL] 3`
  auto-levels through `checkLevelUp` at spawn (consuming RNG) is
  false — spawn does a plain `setExperienceLevelIndex` assignment;
  the oracle methodology was robust to this either way.** Full runtime
  gate: build green, ctest 1077/1077 (1068 + 9), validator 42/10/245 +
  test.mod 0/0, `ar t` 85 members no stray `.h.o`, nm back-edge clean.
  Adversarial reviews (old-vs-new statement walk + independent
  value re-derivation): zero critical findings, zero fix rounds.

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
