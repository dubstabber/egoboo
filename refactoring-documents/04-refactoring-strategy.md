# Refactoring Strategy

## 1. Goals

This strategy is designed for a multi-month refactor where stability matters more than speed.

Primary goals:

- make the runtime understandable
- reduce global coupling
- make content semantics explicit
- preserve shipped content while modernizing the pipeline
- create a path toward modern scripting and better playtesting
- move the remaining active runtime code from the current C/C++ split toward C++ as the primary implementation language
- make native Windows compilation a real project target instead of relying on Wine-driven workflows as the practical fallback
- make Linux-hosted Windows cross-compilation a real project target as well, not a fragile side path
- make Linux builds, native Windows builds, and Linux-hosted Windows builds follow as similar a CMake workflow as practical
- keep the Windows toolchain fully open source and remove Visual Studio-specific build requirements from the maintained development path
- reduce platform-specific assumptions, old dependency friction, and portability-specific code paths so the codebase becomes genuinely cross-platform
- treat C++ compiler warnings as portability and maintainability debt; the long-term target is warning-clean code in supported configurations
- improve the currently buggy and incomplete runtime toward a stable baseline before claiming platform support is healthy

## 2. Rules for the refactor

### Rule 1: no flag-day rewrite

The codebase is too behavior-dense for a one-shot rewrite. The replacement cost is not just code volume. It is accumulated undocumented semantics.

### Rule 2: protect behavior before changing architecture

Characterization tests, content validators, and smoke tools must come before deep subsystem replacement.

### Rule 3: do not mix data migration with engine decomposition in the same change set

Those are separate risk classes and should be staged independently.

### Rule 4: replace globals with explicit context before replacing subsystems

If `_currentModule` remains globally reachable, every subsystem extraction will keep leaking.

### Rule 5: keep legacy loaders alive until parity is measurable

This is especially important for:

- module metadata
- object profiles
- spawn data
- scripts
- mesh/environment data

## 3. Recommended phase plan

## Phase 0: baseline freeze

Deliverables:

- canonical build/run documents for Linux and Windows
- clear source-scope rules for active code versus archive/generated/vendor content
- these audit docs
- explicit capture of existing Fedora/Linux portability edits
- explicit statement that Wine is a compatibility aid for current debugging, not the long-term Windows support target
- explicit capture of current Windows cross-build and Wine-runtime failures so later fixes can be measured against a baseline

Do first because:

- it prevents everyone from debugging a different local setup
- it turns "tribal knowledge" into written constraints

## Phase 1: observability and safety net

Add tooling before architecture changes:

- content load validator CLI
- parser smoke tests for `menu.txt`, `spawn.txt`, `data.txt`, `wawalite.txt`, `level.mpd`
- module load smoke list
- structured log capture for startup and module load failures
- regression harness for a few representative modules

Recommended representative modules:

- `test.mod` for minimal loading
- one starter module
- one module with heavy combat
- one module with water/environment complexity
- one module with imports/exports or progression complexity

## Phase 2: runtime context extraction

Introduce explicit context objects without changing behavior yet. The first
version of this phase has landed: active engine access goes through
`EngineContext`, and active module/session access goes through
`GameSessionContext` and `GameModule` surfaces.

Minimum targets:

- `EngineContext`
- `GameSessionContext`
- `ModuleContentContext`

Responsibilities to move behind explicit interfaces:

- active module access
- update clocks
- configuration access
- service access
- message/log dispatch

Success criteria:

- new code does not reintroduce `_currentModule` or `_gameEngine`
- context access is gradually narrowed toward installed service interfaces
- new subsystem code declares dependencies where practical instead of adding
  hidden singleton access

## Phase 3: split engine services from gameplay services

Create a real boundary between:

- platform/runtime services
- game content repositories
- gameplay session logic
- presentation/UI

Likely extraction candidates:

- VFS and filesystem wrappers
- content repositories and loaders
- render backend and graphics services
- audio/input bootstrapping

Do not start with rendering rewrite. Start with dependency boundaries.

## Phase 4: file and subsystem decomposition

Break oversized hotspots into smaller modules before behavior rewrites.

Top candidates:

- `game/script_functions.c`
- `game/game.c`
- `game/graphic.c`
- `vfs.c`
- `Entities/Object.cpp`
- `Profiles/ObjectProfile.cpp`
- `game/Module/Module.cpp`

Typical split styles:

- public API versus internal implementation
- lifecycle versus behavior
- parsing versus validation
- content definition versus runtime instance

## Phase 5: content pipeline normalization

Build a content IR and migration toolchain.

Recommended order:

1. module metadata
2. spawn data
3. object definitions
4. particle definitions
5. environment data
6. save/import/export formats

Requirements:

- importer from legacy assets
- validator
- diffable output
- round-trip or at least source traceability

## Phase 6: scripting replacement preparation

Do not jump straight from EgoScript to Lua.

First create:

- a documented game scripting API surface
- an event and command model independent of EgoScript syntax
- a compatibility layer for current scripts

Only then choose implementation:

- embedded Lua
- a custom data-driven rules layer
- hybrid scripting plus declarative behaviors

### Why Lua is still a sensible candidate

- mature ecosystem
- accessible for modders
- easy embedding

### Why Lua should not be introduced immediately

- current script behavior is entangled with object state, globals, timing, and legacy helper functions
- without API stabilization, Lua becomes a second layer of chaos

## Phase 7: compatibility and cleanup

Only after dual-load parity and better tests:

- retire legacy parsers
- remove old direct-global access
- delete dead migration experiments
- delete or quarantine stale docs

## 4. First concrete refactor tasks

The early setup tasks have mostly landed: current build docs live under `doc/`,
the validator is integrated, and the former runtime globals are retired from
active code. Current high-value work that still fits this strategy:

1. Keep `GameModule` loading moving toward named phases and explicit inputs.
2. Continue splitting object profile/model/script loading only when tests cover
   the behavior being moved.
3. Convert hardcoded loader conventions into internal manifest-building helpers
   without changing the external legacy content format yet.
4. Add focused tests around module load, model load, script fallback, and
   high-risk state transitions before changing those paths.
5. Keep architecture notes compact: append small pass entries to
   `71-completed-passes-log.md` unless a new boundary needs its own document.

## 5. Areas to defer

These should not be early refactor targets unless required by a blocker:

- renderer modernization
- large-scale gameplay rebalance
- asset visual upgrades
- network ambitions
- save format replacement

They are real future work, but they are not the first maintainability win.

## 6. Risks to manage explicitly

### Risk 1: changing content semantics accidentally

Mitigation:

- dual-load
- golden assets
- validator tooling

### Risk 2: deleting useful legacy behavior because it looks ugly

Mitigation:

- document first
- attach behavior notes to loader and script migrations

### Risk 3: refactoring inside giant files without adding seams

Mitigation:

- file splitting
- wrapper interfaces
- targeted tests first

### Risk 4: portability regressions during cleanup

Mitigation:

- preserve and formalize the current Fedora/Linux fixes
- test with explicit env vars and without them
- keep Linux-native, native-Windows, and Linux-hosted Windows builds aligned instead of letting each platform drift into separate assumptions

### Risk 5: warning debt masking portability problems

Mitigation:

- record warning baselines for supported toolchains
- remove warnings steadily instead of normalizing them
- treat new warning classes as regressions when practical

### Risk 6: architecture work outpacing playtesting

Mitigation:

- every phase needs a small playtest target list
- do not merge large structural changes without smoke validation

## 7. Definition of success

This refactor effort is succeeding when:

- a new contributor can build and launch reliably from one document
- gameplay code can be read without chasing globals through unrelated systems
- module and object content can be validated without starting the full game
- content semantics live in schemas and code, not in scattered folklore
- scripting has a stable API boundary
- module loading and gameplay regressions are caught by repeatable tests and playtests
- supported Linux and Windows build paths are close enough that portability fixes are shared work rather than platform-specific rewrites
- the Windows build is usable both natively and when cross-built from Linux
- the supported C++ build configurations are free of routine warning noise
