# New Refactoring Plan

This document defines concrete, prioritized refactoring tasks based on the codebase health assessment (document 17) and modularization analysis (document 18). It supersedes nothing — it builds on the strategy in document 04 with specific, actionable work items ordered by impact and safety.

## Principles

1. **No flag-day rewrites.** Every change must be incremental and verifiable.
2. **Tests before restructuring.** Add characterization tests around code before moving or splitting it.
3. **Build system changes first.** Making structure visible in the build is cheaper and safer than rewriting code.
4. **Decouple from globals before extracting subsystems.** Removing global state access is the prerequisite for meaningful modularization.
5. **Prioritize by coupling reduction, not cosmetic cleanup.**
6. **Keep platform support aligned with the long-term toolchain goals.** Windows support should move toward native compilation with fully open-source tooling; do not add new Visual Studio-only requirements.
7. **Treat warnings as quality signals, not background noise.** Repeated C++ warnings usually indicate portability debt, interface drift, or weak type boundaries.

---

## Phase A: Build System and Hygiene (Low Risk, High Visibility)

**Goal:** Make the project structure explicit, remove dead code, and improve developer onboarding.

### A1: Replace GLOB_RECURSE with explicit source lists

- **File:** `egolib/library/CMakeLists.txt`
- **Action:** Replace the single `GLOB_RECURSE` with per-directory `set()` calls listing sources explicitly.
- **Why:** Makes subsystem ownership visible, prevents stale file pickup, enables future module splitting.
- **Risk:** None. Same build output.
- **Effort:** Small (1–2 hours).

### A2: Remove dead code directories

- **Action:** Delete or quarantine:
  - `egolib/library/src/egolib/game/Lua/` (0 lines, abandoned)
  - `egolib/library/src/egolib/Network/` (0 lines, placeholder)
- **Why:** Reduces confusion for new contributors.
- **Risk:** None.

### A3: Clean up stale documentation

- **Action:**
  - Delete or mark as deprecated: `README.Linux`, `README.MinGW`, `README.OSX`, `README.Windows`, `README.VisualStudio`
  - Ensure `doc/build-linux.md`, `doc/build-windows.md`, and `README.md` are the canonical references.
  - Remove Visual Studio-specific build guidance from maintained docs and keep the documented Windows path centered on open-source tooling.
- **Why:** Contradictory build docs waste developer time.

### A4: Integrate cartman into the build (optional)

- **Action:** Add `cartman/` as an optional CMake target.
- **Why:** Prevents further bit-rot of the map editor.
- **Risk:** Low. Gate with a CMake option.

### A5: Simplify the documented submodule workflow

- **Action:** Document the normal Egoboo setup as root-submodules-only (`data`, `external`, `idlib`, `idlib-game-engine`) instead of a recursive submodule checkout.
- **Why:** The superproject already injects the top-level `idlib/` into `idlib-game-engine`, so requiring `idlib-game-engine/idlib` for standard builds adds repository noise without changing the active architecture.
- **Risk:** Low. This is a documentation and onboarding cleanup unless `idlib-game-engine` is later changed to require its nested checkout in superproject builds.

### A6: Formalize Windows build parity goals

- **Action:** Document the supported long-term build matrix as:
  - Linux native build
  - native Windows build
  - Linux-hosted Windows cross-build
- **Action:** Keep these paths as close as practical in CMake structure, dependency handling, and runtime assumptions.
- **Why:** Portability debt grows quickly when each platform evolves as a separate workflow.
- **Risk:** Low for documentation, medium once build logic changes begin.

### A7: Add compiler warning baselines and a ratchet

- **Action:** Record current warning baselines for the supported C++ toolchains and start reducing them intentionally.
- **Action:** Avoid introducing new warning classes in touched areas; where practical, make warning cleanup part of the same refactor.
- **Why:** The current warning volume is a symptom of portability and maintainability debt, not just cosmetic noise.
- **Risk:** Low-Medium. Some warning cleanup will expose real bugs or unclear ownership.

---

## Phase B: Test Infrastructure (Medium Risk, Critical Foundation)

**Goal:** Create a safety net before structural changes.

### B1: Add characterization tests for content parsers

- **Target parsers:**
  - `spawn.txt` (SpawnFileReaderImpl)
  - `data.txt` (ObjectProfile::loadDataFile)
  - `menu.txt` (ModuleProfile::loadFromFile)
  - `wawalite.txt` (read_wawalite_vfs)
  - `level.mpd` (map loading)
- **Method:** Golden-file tests: parse a known input file, compare output struct fields against expected values.
- **Why:** These parsers are high-risk and currently unprotected.
- **Effort:** Medium (2–3 days).

### B2: Add module load/unload smoke tests

- **Action:** Create a test that loads a module through `game_begin_module()` and verifies key postconditions (object count, passage count, mesh loaded).
- **Target modules:** `test.mod`, one starter module.
- **Why:** Module loading is the most complex lifecycle path and has zero test coverage.

### B3: Expand validator to cover profile parsing

- **Action:** Extend `egoboo-content-validator` from pure parse/load coverage into narrow semantic `data.txt` validation first, then widen toward broader field-range and enchant integrity checks.
- **Why:** The validator currently only checks file presence and spawn-name resolution.

### B4: Add a build-time compile test for each proposed module boundary

- **Action:** Create a test source per proposed module (from document 18) that includes only that module's headers and verifies compilation.
- **Why:** This catches hidden cross-module dependencies cheaply.

### B5: Add platform-smoke coverage for Windows artifacts

- **Action:** Add a narrow smoke path for the Linux-hosted Windows build that at least exercises startup, content discovery, and early subsystem initialization under the current emulator path.
- **Action:** Keep `debug-output.txt` style failure captures as comparable artifacts until the Windows runtime reaches a stable baseline.
- **Why:** The current Windows-on-Linux path is known to be unplayable; stabilization needs repeatable evidence, not one-off manual runs.

---

## Phase C: Global State Reduction (High Risk, Highest Value)

**Goal:** Break the `_currentModule` / `_gameEngine` dependency chains that prevent modular reasoning.

### C1: Introduce GameSessionContext

- **Action:** Create a `GameSessionContext` struct/class that holds:
  - Active module reference
  - Object handler
  - World clock (`update_wld`)
  - Stat clocks (`clock_chr_stat`, `clock_enc_stat`)
  - Import list
  - Weather/fog/animated tile state
- **Pass it explicitly** to game-loop functions, physics, and entity updates.
- **First migration targets:** Functions in `game.c` that currently read `_currentModule` directly.
- **Why:** This is the single highest-impact architectural change. It enables testing, headless validation, and future multi-session support.
- **Risk:** High. Requires careful incremental migration.

### C2: Wrap \_gameEngine access behind accessor functions

- **Action:** Replace direct `_gameEngine->` calls with a small set of accessor functions (e.g., `getUIManager()`, `getActiveGameState()`, `getCurrentUpdateFrame()`).
- **Why:** Reduces coupling surface. Later these accessors can be converted to interface methods.
- **First targets:** GUI code and game state transitions.

### C3: Eliminate extern globals in game.h

- **Action:** Move `g_weatherState`, `fog`, `g_animatedTilesState`, `g_endText`, `overrideslots`, `g_importList`, `clock_enc_stat`, `clock_chr_stat`, `update_wld` into `GameSessionContext` or the module that owns them.
- **Why:** These are all session-scoped state masquerading as globals.

### C4: Break Entity ↔ Module circular dependency

- **Action:**
  - `Object.hpp` should not include `Module/Module.hpp`.
  - Introduce a forward-declared interface or accessor for module services that `Object` needs.
  - Move `Object` creation out of `Module.cpp` constructor into a factory or loader.
- **Why:** This circular dependency blocks module extraction.

---

## Phase D: File Splitting (Medium Risk, Medium Value)

**Goal:** Break oversized files into testable, reviewable units.

### D1: Split script_functions.c (8,183 lines)

- **Proposed split:**
  - `script_functions_movement.c` — movement/position functions
  - `script_functions_combat.c` — damage, attack, defense
  - `script_functions_inventory.c` — item, money, inventory
  - `script_functions_targeting.c` — target finding, checking
  - `script_functions_state.c` — state changes, alerts, timers
  - `script_functions_misc.c` — everything else
- **Why:** Largest file in the project. Currently untestable as a unit.

### D2: Split game.c (2,456 lines)

- **Completed split (2026-04-15):**
  - `game_internal.h` — shared includes and context accessors for split translation units
  - `game_export.c` — character export/import and save management
  - `game_combat.c` — latch attack, swipe execution, attachment lookup
  - `game_targeting.c` — particle and AI target finding/validation
  - `game_wawalite.c` — wawalite read/write/upload and environment state globals
  - `game_loop.c` — main loop helpers, stat display, particle reaffirmation, message log bridge
  - residual `game.c` — module lifecycle, end text, escape expansion, terrain queries, zeitgeist/menu music
- **Why:** The original file mixed session lifecycle, combat, targeting, export, environment, and update-loop responsibilities in one translation unit.

### D3: Split graphic.c (2,257 lines)

- **Proposed split:**
  - `graphic_lighting.c` — grid illumination
  - `graphic_rendering.c` — main render passes
  - `graphic_debug.c` — debug overlays
- **Why:** Rendering, lighting, and debug are independent concerns.

### D4: Split Object.cpp (3,201 lines)

- **Proposed split:**
  - `Object_lifecycle.cpp` — construction, initialization, termination
  - `Object_update.cpp` — update loop, AI driving
  - `Object_combat.cpp` — damage, defense, death handling
  - `Object_interaction.cpp` — mounting, inventory interaction
- **Why:** The Object class has too many responsibilities.

### D5: Split ObjectProfile.cpp (1,468 lines)

- **Proposed split:**
  - `ObjectProfile_load.cpp` — loadDataFile (493 lines alone)
  - `ObjectProfile_export.cpp` — exportCharacterToFile (350 lines)
  - `ObjectProfile_core.cpp` — everything else

---

## Phase E: Error Handling Normalization (Low Risk, Medium Value)

**Goal:** Converge on a single error handling strategy.

### E1: Audit and categorize error handling patterns

- **Action:** Create a categorized list of which files use exceptions, return codes, or silent failure.
- **Why:** Three competing strategies make error propagation unpredictable.

### E2: Migrate egolib_rv return codes to exceptions in C++ code

- **Action:** In C++ files that already use try/catch, replace `egolib_rv` returns with exceptions.
- **Leave C files alone** until they are converted to C++.

### E3: Replace raw new/delete with smart pointers

- **Action:** Audit the 232 raw `new`/`delete` call sites. Convert to `std::unique_ptr` or `std::make_shared` where ownership is clear.
- **Priority targets:** Files that also use `shared_ptr` (mixed patterns are worst).

### E4: Standardize warning-sensitive C++ boundaries

- **Action:** Prefer explicit ownership, typed enums, modern casts, and narrower interfaces in touched C++ code so warning cleanup is structural rather than cosmetic.
- **Why:** Portability problems often surface first as warnings around conversions, ownership, and implicit platform assumptions.

---

## Phase F: Namespace and Header Cleanup (Low Risk, Low-Medium Value)

### F1: Move free functions into namespaces

- **Action:** Wrap the 60+ free functions in `game.h` into `namespace Ego::Game` or appropriate sub-namespaces.
- **Why:** Reduces global namespace pollution and improves discoverability.

### F2: Eliminate egolib.h uber-header

- **Action:** Replace `#include "egolib/egolib.h"` (57 includes) with specific includes in each consuming file.
- **Why:** Reduces compilation coupling and build times.

### F3: Replace C-style casts with C++ casts

- **Action:** Replace the 211 C-style casts with `static_cast`, `reinterpret_cast`, or `const_cast` as appropriate.
- **Why:** Type safety and searchability.

---

## Phase G: Content Validation Expansion (Low Risk, High Value)

### G1: Per-module triage based on validator output

- **Action:** Use the validator JSON report to create a per-module health dashboard.
- **Categorize modules as:** clean, fixable (typos/renames), broken (missing dependencies).

### G2: Object-name reconciliation

- **Action:** Build an automated workflow that:
  1. Extracts all spawn-referenced object names
  2. Matches against actual object directories
  3. Reports likely aliases, typos, and missing assets
- **Why:** 277 of 278 validator errors are missing spawn references.

### G3: Automated content regression tests

- **Action:** Run the full validator as a CI step. Fail the build if error counts increase above the baseline.
- **Why:** Prevents content regressions during refactoring.

---

## Priority Matrix

| Phase                     | Risk   | Impact     | Effort       | Recommended Order |
| ------------------------- | ------ | ---------- | ------------ | ----------------: |
| A: Build & Hygiene        | Low    | Medium     | Small        |           **1st** |
| B: Test Infrastructure    | Medium | High       | Medium       |           **2nd** |
| G: Content Validation     | Low    | High       | Small-Medium |           **3rd** |
| C: Global State Reduction | High   | Highest    | Large        |           **4th** |
| D: File Splitting         | Medium | Medium     | Medium       |           **5th** |
| F: Namespace & Header     | Low    | Low-Medium | Small-Medium |           **6th** |
| E: Error Handling         | Low    | Medium     | Medium       |           **7th** |

---

## Definition of Done per Phase

### Phase A complete when:

- `CMakeLists.txt` lists sources explicitly
- Dead directories removed
- Single canonical build doc exists

### Phase B complete when:

- Golden-file tests exist for 5 core parsers
- Module load smoke test passes for `test.mod`
- Validator covers profile field ranges

### Phase C complete when:

- `GameSessionContext` exists and is passed to game loop
- No new code reads `_currentModule` or `_gameEngine` directly
- `game.h` exports zero `extern` globals

### Phase D complete when:

- No active source file exceeds 2,000 lines
- No function exceeds 200 lines
- All split files compile and link correctly

### Phase E complete when:

- C++ code uses only exceptions for error reporting
- Raw `new`/`delete` count reduced by 80%+

### Phase F complete when:

- `egolib.h` is deleted or reduced to <10 includes
- All game-layer free functions are namespaced

### Phase G complete when:

- Validator runs in CI
- Error count is tracked against baseline
- Object-name reconciliation report exists

---

## Dependencies Between Phases

```
A (build hygiene)
  └─ B (tests) ─── requires clean build structure
       └─ C (globals) ─── requires test safety net
            └─ D (file splits) ─── easier after globals are wrapped
                 └─ E, F (cleanup) ─── polish after structure is stable
  └─ G (content validation) ─── independent, can run in parallel with B–C
```

## Estimated Timeline (single developer, part-time)

| Phase     | Estimated duration |
| --------- | ------------------ |
| A         | 1–2 days           |
| B         | 1–2 weeks          |
| G         | 1 week             |
| C         | 2–4 weeks          |
| D         | 2–3 weeks          |
| E         | 1–2 weeks          |
| F         | 1 week             |
| **Total** | **~2–3 months**    |

---

## Progress Log

### Phase A: Build System and Hygiene — ✅ COMPLETE

- **A1** ✅ Explicit per-subsystem source lists in `egolib/library/CMakeLists.txt`
- **A2** ✅ Dead directories removed (`game/Lua/`, `Network/`)
- **A3** ✅ Stale READMEs already carry deprecation notices pointing to `doc/build-linux.md` and `README.md`
- **A4** ⏸ Deferred (optional cartman integration)

### Phase B: Test Infrastructure — 🟡 IN PROGRESS

- **B1** ✅ Golden-file characterization tests for all 5 core parsers (2026-04-13):
  - `spawn.txt` — entry count, first-entry fields, attachment types
  - `menu.txt` / ModuleProfile — name, imports, exporting, max players
  - `wawalite.txt` — load success, gravity range
  - `data.txt` / ObjectProfile — class name, gender, physical attributes, flags
  - `level.mpd` / map_t — load success, dimensions, tile/vertex memory, engine-limit bounds
  - Test bootstrap now forces `EGOBOO_DATA_DIR` to the repository `data/` tree so `test.mod` is discoverable under both direct runs and `ctest`
- **B2** 🟡 Headless module load smoke tests for `test.mod` are integrated and passing (2026-04-13):
  - required file presence
  - mesh load
  - `wawalite.txt` parse
  - `spawn.txt` parse
  - local object lightweight profile loads
  - spawn-reference resolution against `mp_objects`
  - end-to-end structural load sequence
  - Verified with 27 focused passing tests via both `egolib-tests-executable` and `ctest`
  - Full `game_begin_module()` / `game_quit_module()` lifecycle unload coverage remains pending because the live module path is still coupled to audio/graphics runtime startup
- **B3** 🟡 Validator semantic profile checks started (2026-04-13):
  - validator now warns on a small set of post-load `data.txt` invariants when encountered:
    - raw `DRES` expansion values must reference a valid skin slot
    - raw saved-character `SKIN` expansions must resolve to a valid skin index or accepted sentinel
    - raw saved-character `LEVL` expansions must stay within `0..MAXLEVEL-1`
    - saved-character `SKIN` override must resolve to a valid skin or sentinel
    - current ammo must not exceed max ammo
  - current full baseline totals remain unchanged, so these checks did not surface new issues in shipped content yet
- **B4** ❌ Build-time compile test per module boundary

### Phase C: Global State Reduction — 🟡 STARTED

- Context wrapper headers and implementations exist:
  - `GameSessionContext` wraps `_currentModule`, `update_wld`, clocks, imports
  - `EngineContext` wraps `_gameEngine`
- Callers not yet migrated — wrappers still delegate to globals

### Phase D: File Splitting — 🟡 IN PROGRESS

- **D1** ✅ Split `script_functions.c` (8,183 lines → 7 files, 2026-04-15):
  - `script_functions_internal.h` — shared infrastructure (macros, templates, accessors)
  - `script_functions_bitwise.c` — 12 functions (216 lines)
  - `script_functions_state.c` — 91 functions (1,551 lines)
  - `script_functions_target.c` — 81 functions (1,777 lines)
  - `script_functions_movement.c` — 34 functions (644 lines)
  - `script_functions_action.c` — 46 functions (968 lines)
  - `script_functions_spawn.c` — 44 functions (1,182 lines)
  - `script_functions_systems.c` — 96 functions (2,128 lines)
  - All 404 functions accounted for, zero behavior change
  - 253 tests pass, validator baseline unchanged
- **D2** ✅ Split `game.c` (2,456 lines → 6 files + shared header, 2026-04-15):
  - `game_internal.h` — shared split infrastructure
  - `game_export.c` — export/import logic
  - `game_combat.c` — combat helpers
  - `game_targeting.c` — targeting helpers
  - `game_wawalite.c` — environment/wawalite helpers
  - `game_loop.c` — update-loop and UI message helpers
  - residual `game.c` keeps the remaining lifecycle, terrain, and zeitgeist helpers
  - verification: `cmake --build build -j4`, `ctest --test-dir build --output-on-failure -j4`, and validator `test.mod` all pass
- **D3** ❌ Split `graphic.c`
- **D4** ✅ Split `Object.cpp` (3,201 lines → 6 files + shared header, 2026-04-16):
  - `Object_internal.h` — shared entity/profile includes and runtime accessors
  - `Object_lifecycle.cpp` — construction, teardown, respawn, termination, trivial getters
  - `Object_combat.cpp` — damage, healing, death, resistance, experience, invictus handling
  - `Object_update.cpp` — per-frame update and resize handling
  - `Object_interaction.cpp` — holder, inventory, drop, money, and input-latch behavior
  - `Object_attributes.cpp` — attributes, perks, enchantments, stealth, team, polymorph
  - `Object_appearance.cpp` — skin, visibility, collision/physics wrappers, identity queries
  - residual `Object.cpp` now holds only the static definitions shared by the split translation units
- **D5** ✅ Split `ObjectProfile.cpp` (1,483 lines → 3 files + shared header, 2026-04-16):
  - `ObjectProfile_internal.h` — shared includes and `INVALID_SKIN`
  - `ObjectProfile_core.cpp` — constructor/destructor, accessors, random-name/message helpers, XP/perk utilities
  - `ObjectProfile_load.cpp` — texture/message loading, `data.txt` parsing, and profile load orchestration
  - `ObjectProfile_export.cpp` — character/profile serialization to legacy `data.txt`
  - deviation from the draft plan: `vfs_get_next_object_profile_ref(...)` stayed local to `ObjectProfile_load.cpp` because it is only used by parsing
  - verification: `cmake --build build -j4`, `ctest --test-dir build --output-on-failure -j4`, and validator `test.mod` all pass with 258 tests passing and `test.mod` at 0 warnings / 0 errors
- Additional entity split completed outside the original D1-D5 list, 2026-04-16:
  - `Particle.cpp` (1,447 lines → 4 files + shared header)
  - `Particle_internal.h` — shared entity includes and session/module accessors
  - `Particle_spawn.cpp` — reset, initialize, attach, and vertex-placement logic
  - `Particle_update.cpp` — per-frame update, water interaction, animation, dynamic lighting, continuous spawning
  - `Particle_combat.cpp` — attached damage and destroy/end-spawn behavior
  - `Particle_core.cpp` — environment struct, queries, collision wrappers, sound, ownership helpers
  - verification: `cmake --build build -j4`, `ctest --test-dir build --output-on-failure -j4`, and validator `test.mod` all pass with 258 tests passing and `test.mod` at 0 warnings / 0 errors

### Phase G: Content Validation — ✅ BASELINE COMPLETE

- Validator tool built and baselined (42 modules, 9 passing, 278 errors)
- JSON report mode available
