# Refactoring Strategy And Roadmap

Snapshot date: 2026-07-21. This is the single forward-looking document: the
rules the refactor runs under, where the original phase plan stands, and what
is still left. It absorbs the former `04-refactoring-strategy.md`.
Completed-pass history lives in `71-completed-passes-log.md`; current metrics
and the measured pre-refactoring comparison live in
`CODEBASE-HEALTH-STATUS.md`.

## Goals

- Make the runtime understandable and reduce global/hidden coupling.
- Preserve shipped content behavior while modernizing the pipeline.
- Move remaining active runtime code from the C/C++ mix toward C++.
- Make Linux native, native Windows (open-source toolchain), and Linux-hosted
  Windows cross-builds first-class and as similar as practical; never
  reintroduce Visual Studio-only requirements.
- Treat compiler warnings and stale docs as debt; target warning-clean
  supported configurations.
- Stabilize the runtime through tests, validation, and playtesting before
  claiming platform support is healthy.

## Non-Negotiable Rules

1. **No flag-day rewrites.** The codebase is behavior-dense; the replacement
   cost is accumulated undocumented semantics, not code volume.
2. **Characterization tests before risky restructuring.**
3. **Preserve observable behavior** unless the task explicitly changes it.
4. **Do not mix data migration with engine decomposition** in one change set.
5. **Keep legacy loaders alive until parity is measurable** — especially module
   metadata, object profiles, spawn data, scripts, and mesh data.
6. Reduce coupling and ownership ambiguity before cosmetic cleanup.

## Phase Status

The original phase plan, with where each phase actually stands:

| Phase | Scope | Status |
| --- | --- | --- |
| 0 Baseline freeze | Canonical build docs, source-scope rules, portability capture | **Done** — `doc/build-linux.md`, `doc/build-windows.md`, these docs |
| 1 Observability | Validator, parser smoke tests, regression harness | **Done** — `egoboo-content-validator` with known baseline; 977 ctest cases |
| 2 Context extraction | Retire raw globals behind explicit contexts | **Done** — `_gameEngine`/`_currentModule`/`update_wld` gone from active code |
| 3 Engine/gameplay service split | Real boundaries between platform services, content, session, presentation | **Largely done** — nine-archive acyclic DAG, active `I*` seams, composition-root bootstraps; constructor injection is the remaining frontier |
| 4 File/subsystem decomposition | Break oversized hotspots | **Done for production code** — zero runtime files over 1,000 lines; mechanical split fronts are substantially exhausted |
| 5 Content pipeline normalization | Schemas/IR, importers, dual-load parity | **Not started** |
| 6 Scripting replacement prep | Documented script API surface, compatibility layer | **Not started** (deliberately deferred; EgoScript untouched by design) |
| 7 Compatibility cleanup | Retire legacy parsers, dead experiments, stale docs | **Partial** — legacy platform docs quarantined, uber-header deleted; parsers intentionally alive per Rule 5 |

## What Is Left

### Tier 1: Runtime Structure

- **T1.1 `Object` interface breadth.** `Object.hpp` (998 lines) is still the
  broadest runtime interface. Useful work is reducing multi-role call surfaces
  and keeping callers on role interfaces that express the real dependency —
  not mechanical line trimming. A by-value state-aggregate extraction was
  scoped and deliberately banked as flag-day-scale. Avoid broad `Object&`
  parameters in new code.
- **T1.2 Service-locator narrowing.** `EngineContext::get()` (272 sites) and
  `GameSessionContext::get()` (23 sites) are intentional seams but still
  flatten dependency visibility. The frontier is constructor/parameter
  injection where a call path already has the dependency; do not add new
  hidden globals. Remove remaining low-count direct singleton calls where a
  service seam already exists.
- **T1.3 `GameModule` ownership.** `GameModule` now owns the world and
  orchestrates; the logic halves have left the class.
  `ModuleLoadPhase`/`ModuleLoadContext` and the `GameModuleRuntime` provider
  are in place; as of Passes 326-327 the load helpers are `module_loading`
  free functions with explicit narrow inputs and `Passage` stores
  `ego_mesh_t&` + `ObjectHandler&` (no load step references the module
  object); as of Pass 328 the per-update steps are `module_update` free
  functions and a `PitsState` env-state struct with explicit inputs (no
  update step references the module object either — only the object
  handler, mesh, damage-tile config, and services). The spawn family was
  scouted and is already at its endpoint: the realization core
  (`module_spawn_realization::realizeSpawnEntry`) and spawn planning are
  extracted pure logic with dedicated tests, and what remains on the class is
  interface overrides (`spawnObjectRef`), by-design ops-wiring composition
  (`spawnObjectFromFileEntry`), and orchestration. T1.3 is complete at
  reasonable ROI; `GameModuleRuntime`'s `std::function` providers stay — they
  are a deliberate swappable seam that tests rely on.
- **T1.4 Error-handling policy.** `doc/error-handling-policy.md` is the active
  target. New code must not add silent failures; migrate the mixed
  exception/boolean/null-return styles only in bounded subsystem passes with
  tests.
- **T1.5 Future file splits** only when they improve ownership, navigation, or
  archive boundaries — the routine size-driven split queue is exhausted.
  Verify symbol ownership after archive moves; never let private headers
  become stray compiled `.h.o` archive members.

### Tier 2: Build And Platform

- **T2.1 Keep the nine-archive DAG acyclic.** Any source movement in
  `egolib/library/CMakeLists.txt` must preserve
  `foundation-base <- {physics, renderer <- gui} <- library <- game-graphics
  <- hud-widgets <- {scriptvm, gamestates}`. Verify with live-archive `nm`
  checks, not object-directory globs.
- **T2.2 Native Windows open-source build.** Still open. Add a native path
  (for example MSYS2/UCRT64) once cross-build assumptions are stable.
- **T2.3 Wine runtime stabilization.** The cross-build works, but Wine
  execution still needs the mipmap/audio compatibility defaults in
  `run-egoboo-windows.sh`. Not yet a credible runtime verification target.
- **T2.4 Retire legacy CI/project artifacts** (AppVeyor, Visual Studio
  remnants) once they stop serving a compatibility purpose.

### Tier 3: Content, Script, And Deeper Design

- **T3.1 Content pipeline normalization** (Phase 5, unstarted). Staged model:
  define schemas/IR → build importers+validators+exporters → dual-load and
  compare representative modules → only then retire legacy parsers.
  Precondition surfaced by the validator: a spawn-reference reconciliation
  pass (229 of 245 baseline errors are `missing_spawn_object`) and a
  per-module triage list. See `03-data-and-content-audit.md` §7–8 for design
  guidance.
- **T3.2 Model asset migration.** The glTF/GLB loader v1 accepts only a
  narrow static-frame subset, and virtually all shipped objects are still
  MD2 (`tris.md2`). Remaining: animated glTF support and an automated
  MD2→glTF conversion/validation path for the ~950 model assets.
- **T3.3 Scripting replacement preparation** (Phase 6, unstarted). Do not
  jump to Lua. First: document the script API surface, an event/command model
  independent of EgoScript syntax, and a compatibility layer. Dispatch already
  uses a registry table. **The dispatch-coverage campaign is COMPLETE:
  Passes 330–336 closed the measured gap from ~110 untested `scr_*`
  functions to zero — every one of the 404 `scr_*` dispatch functions now
  has test references** (family slices: alerts Pass 330, locomotion 331,
  action-support 332, state-control 333, movement-support 334,
  target-support 335, and the final residual grab-bag Pass 336,
  `ScriptResidualFunctions.cpp`; per-pass detail and pinned legacy quirks
  are in the pass log — notable pinned bugs include the `CreateOrder`
  packed-order truncation and the `FindTileInPassage` docstring
  contradiction). Remaining near-term value here: narrowing helper
  dependencies as role surfaces improve, and keeping the gap at zero for
  new functions (re-derive with the `rg -o`/`comm -23` recipe in the pass
  log).
- **T3.4 `shared_ptr<Object>` discipline.** Public enumeration is ref-first;
  remaining shared handles are intentional ownership or weak-storage paths.
  Continue preferring `ObjectRef`/non-owning references where the handler
  guarantees lifetime.
- **T3.5 Rendering and GUI characterization.** Rendering correctness remains
  thin on tests. As of Pass 329 the `GameEngine` state-stack transition
  semantics are pinned headless (`GameStateStackTransitions.cpp`: push/begin,
  ended-state fallthrough with re-entry `beginState()`, deferred
  `setGameState` clear, main-menu-factory fallback). Pass 337 characterized
  `CharacterStatus` (`CharacterStatusWidget.cpp`): the self-destroying HUD
  widget lifecycle (destroy + parent detach on lost session or unresolvable
  observed object) and the deterministic `std::logic_error` headless wall at
  `activeUIManager()`. The other three untested widgets are blocked at a
  common wall (scouted, Pass 337): any text-bearing widget fetches a font
  from the active UIManager in its constructor
  (`TitleBar`/`Label`/`Button`), a real UIManager is not headlessly
  constructible (its constructor deadlocks in `TextureManager` without a GL
  context and font atlases need the renderer), and the raw-storage fake
  UIManager idiom is pointer-identity-only (UB if `getFont` runs). Cheapest
  unlocks were assessed in that order, and Pass 338 landed the first: a
  `tryActiveUIManager()` presence guard with a pending-layout flag and
  draw-time self-heal in `Button`/`Label` (production path provably
  unchanged — the sole UIManager install precedes the earliest possible
  text-widget construction in the boot order), making every text-bearing
  widget headlessly *constructible* with no manager installed
  (`GuiTextLayoutHeadless.cpp` pins the guarantees). Pass 338 also
  discovered the seam is NOT sufficient for `ModuleSelector`: its ctor
  body calls `uiManager().getScreenWidth()` (throws with no manager),
  while installing the raw-storage fake manager re-enables the eager
  `getFont`→`Font::layoutText` path, which is unconditionally GL-bound
  and segfaults on the never-constructed fake (gdb-verified,
  pre-existing hazard; `ModuleSelectorWidget.cpp` pins the clean
  construction-requires-a-manager failure and documents the trace). The
  real unlock for ModuleSelector's wheel-clamp `size_t` underflow quirk
  (<3 modules) and the rest of the family is a headlessly-constructible
  UIManager or an injectable text-layout engine. Pass 339 landed the
  other high-value move: `LevelUpWindow::doLevelUp`'s gameplay half
  (seeded attribute draws, perk grant + flat-bonus table, level bump,
  `ALERTIF_LEVELUP`, indicator clear, anti-save-scum reseed, might→fat
  growth, attribute application) moved verbatim into the GUI-free
  `Ego::applyCharacterLevelUp` (`game/Logic/LevelUp.{hpp,cpp}`,
  egolib-library — RNG stream and mutation order provably identical;
  the widget formats a returned report), characterized by
  `CharacterLevelUp.cpp` with an RNG-replay oracle. Two latent hazards
  documented, not fixed: the unguarded `playerList[getPlayerNumber()]`
  index for non-players, and the profile `[SEED]` override being parsed
  but never applied. Pass 342 extracted `Ego::IFont`/`ILaidTextRenderer`
  (the real font seam) from `Font`. Pass 343 finished the ModuleSelector
  unlock: extracted `Ego::GUI::IUIManager` from `UIManager` (`UIManager`
  now the sole production implementation, resolved through the same
  `activeUIManager()`/`Component::uiManager()` seam; `GameEngine`
  ownership of the concrete `UIManager` is unchanged — only the seam
  publishes the interface) and added `Ego::Test::HeadlessUIManager`, a
  properly-constructed, GL/SDL_ttf-free `IUIManager` stub serving a
  deterministic-metrics `IFont`. This retired the raw-storage,
  never-constructed fake `UIManager` idiom (UB if `getFont` ran) from both
  `ScriptSystemsFunctions.cpp` and `ScriptActionFunctions.cpp`, and made
  `ModuleSelector` — previously constructible in *no* manager-installed
  state without throwing — fully constructible headlessly with a manager
  installed (`GuiHeadlessUIManagerStub.cpp`; `ModuleSelectorWidget.cpp`
  keeps the no-manager-throws variant). Pass 344 completed the arc:
  all three previously-blocked widgets are now characterized under
  `HeadlessUIManager` — `ModuleSelectorWidget.cpp` pins the full
  wheel/click interaction machine including the <3-modules `size_t`
  underflow that defeats the blocking gate (rescued only by
  implementation-defined narrowing, with the "next" button spuriously
  enabled), the fresh-state both-buttons-enabled anomaly, the
  prev-click underflow to `SIZE_MAX`, and the
  `notifyModuleListUpdated`-vs-wheel enable-condition asymmetry;
  `CharacterWindowWidget.cpp` pins construction, the tab machine, the
  enchant merge-by-name/xN-prefix/underscore logic, and the level-up
  button lifecycle (including the no-parent self-destroy branch);
  `LevelUpWindowWidget.cpp` pins the shell — perk-offer seeded draws
  (3 vs 5 for JACK_OF_ALL_TRADES) and the `setHoverPerk` state
  machine. The hud-widgets layer now has zero untested widgets.
  Remaining otherwise: render passes and camera (blocked on a GL-free
  harness that does not exist).
- **T3.6 Content-pipeline/runtime separation.** Profile parsing, model
  loading, script compilation, and validator startup still require runtime
  services (`ImageManager`, `PerkHandler`, config). Keep separating pure data
  parsing from runtime service access where tests can prove behavior.
- **T3.7 Cartman follow-ups.** Builds and launches behind
  `EGOBOO_BUILD_CARTMAN=OFF`. Keep it off the default build until broader
  module smoke coverage exists and the pre-existing no-argument shutdown
  crash is fixed.
- **T3.8 Playtesting discipline.** The plan in
  `05-playtesting-and-bug-hunt-plan.md` is still mostly unexecuted; known
  open runtime findings (for example the wizard.mod continuous-firing latch
  bug) need runtime debugging, not static fixes.

### Deferred By Design

Not early targets unless a blocker demands them: renderer modernization,
large-scale gameplay rebalance, asset visual upgrades, networking, save-format
replacement.

## Risks To Manage

| Risk | Mitigation |
| --- | --- |
| Accidentally changing content semantics | Dual-load comparison, golden assets, validator gate |
| Deleting useful legacy behavior because it looks ugly | Document first; attach behavior notes to loader/script migrations |
| Refactoring giant files without adding seams | Seam interfaces and targeted tests before splits |
| Portability regressions during cleanup | Preserve the documented Fedora/Linux behavior; keep the three build paths aligned |
| Warning debt masking portability problems | Keep warning baselines; treat new warning classes as regressions |
| Architecture work outpacing playtesting | Every risky change gets a smoke target; no large structural merges without validation |

## Definition Of Success

- A new contributor can build and launch reliably from one document.
- Gameplay code reads without chasing globals through unrelated systems.
- Content can be validated without starting the full game.
- Content semantics live in schemas and code, not folklore.
- Scripting has a stable API boundary.
- Regressions are caught by repeatable tests and playtests.
- Linux and Windows builds are close enough that portability fixes are shared
  work, and the Windows artifact runs natively as well as cross-built.
- Supported C++ configurations are free of routine warning noise.

## Fronts To Treat As Closed

- Runtime globals retired through `EngineContext` / `GameSessionContext`.
- `egolib/egolib.h` uber-header deleted.
- Nine-archive acyclic link layout carved and verified.
- Production monoliths split; zero runtime files over 1,000 lines.
- glTF/GLB loading landed behind the current static-mesh subset.
- Public object enumeration is ref-first through `ObjectRef`.
- `cartman` wired into CMake behind `EGOBOO_BUILD_CARTMAN`.
- Full validator baseline stable at 42 modules / 10 warnings / 245 known
  legacy content errors.
