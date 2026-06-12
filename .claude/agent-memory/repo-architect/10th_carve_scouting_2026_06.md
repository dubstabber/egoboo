---
name: 10th-carve-scouting-2026-06
description: 10th archive carve feasibility scouting (all 7 candidates measured; verdict PARK as a NEW archive); PARTIALLY SUPERSEDED 2026-06-12 by a CMakeLists-only relocate of GameState + ActiveGameEngine into egolib-gui (no new archive; existing 9 archives stay; gui 22→24, library 62→60)
metadata:
  type: project
---

## Scouting result: PARK (as a NEW 10th archive)

Conducted 2026-06-12. All seven candidate options assessed against the live archives.
No 10th archive candidate is tractable at acceptable cost/payoff ratio.

## ⚠ PARTIAL SUPERSESSION — 2026-06-12, same-day follow-on

The PARK verdict above stands ONLY for *adding a 10th archive*. A separate ultracode scout the same day
identified a **CMakeLists-only relocate** that needs no new archive: the abstract `GameState` base + the
`ActiveGameEngine` ownership-move seam moved INTO the existing **egolib-gui** archive. The 4 ActiveGameEngine
functions had been pre-seamed on 2026-06-11 (commit `3ef9c2b79`, the `activeGameEngine()` seam) but the carve
itself was not attempted until today. nm verification on the live `.a` extracts:

- `ActiveGameEngine.cpp.o`: zero library U-syms (only libstdc++/`__cxa_*` unwinding).
- `GameState.cpp.o`: one library U-sym = `activeGameEngine()` (moves WITH the carve); other U-syms = `Ego::GUI::Container::*` / `Component::*` / `InputListener::*` (intra-gui).
- All 3 below-gui archives reference zero of the moved symbols.

Result: `gui` 22→24 TUs, `library` 62→60. Branch `refactor/gamestate-activegameengine-to-gui`. Zero source-file
edits, 3 CMakeLists block edits + 1 stale doxygen comment in `GameState.hpp` + 2 stale source-group comments.
Gates: ctest 877/877, build clean, nm acyclicity check (38 forbidden back-edges = 0 across all 9 archives,
positive controls intact).

**REFRAME of the older "move-only into LOWER layers is EXHAUSTED" note** (in `egolib-modularization-fronts.md`):
tightly-decoupled top-of-call-graph fragments whose U-sym closure already lives inside the target lower layer
can still cross a boundary cleanly with NO seam-cutting. The previous "exhausted" assertion implicitly assumed
the move would require breaking dependencies; when the dependencies are already seam-broken (as here, via the
2026-06-11 `activeGameEngine()` work), the relocate is mechanical.

## Current DAG state (confirmed live 2026-06-12, post-relocate)

`base 146 ◄ {physics 5, renderer 28 ◄ gui 24} ◄ library 60 ◄ game-graphics 17 ◄ hud-widgets 6 ◄ {scriptvm 17, gamestates 19}`

(Pre-relocate: `gui 22 ◄ library 62`. Note the prior `library 63 vs AGENTS 62` 1-TU discrepancy was likely
the same CMake-conditional that resolved during a fresh from-scratch rebuild.)

## Candidate assessments

### 1. Audio carve (egolib-audio, 1 TU: AudioSystem.cpp)

**Reverse edges: 5 concrete symbols from 3 TUs**
- `egoboo.c`: `AudioSystem::upload/download` (2 method calls, concrete singleton type)
- `game_loop.c`: `AudioSystem::DEFAULT_MAX_DISTANCE` (static float data)
- `GameEngine.cpp`: `AudioSystemCreateFunctor::operator()()` + `AudioSystemDestroyFunctor::operator()()` (from idlib::singleton template instantiation)

**Seam cost:**
1. Add `upload/download` virtuals to `IAudioSystem` (2 new methods)
2. `game_loop.c`: inline `1280.0f` constant (1 line)
3. `AudioBootstrap` `std::function` hook (same pattern as GraphicsBootstrap, ~1 new file)
4. Add `computeMinSoundDistance` + `computeSoundAttenuation` virtuals to `ICameraSystem`
   (AudioSystem uses concrete `Camera::getCenter/getPosition/getTurnZ_turns` — Camera is in game-graphics)
5. `CameraSystem.cpp` (in game-graphics) implements the 2 new ICameraSystem methods
6. `AudioSystem.cpp`: use ICameraSystem virtuals instead of Camera directly (~3 lines)

**Topological position after carve:**
- AudioSystem must be at or above game-graphics (Camera is in game-graphics)
- Could be sibling of game-graphics if ICameraSystem seam eliminates Camera dep
- CMakeLists already has `EGOLIB_AUDIO_SOURCES` grouped separately (pre-anticipated)
- 9th carve memo noted this explicitly as "TOO SMALL / TOO COUPLED"

**Verdict: tractable but LOW PAYOFF (1 TU, 6 seam edits). Not worth a standalone archive.**

### 2. App.cpp + Console.cpp (potential "bootstrap" archive)

**Reverse edges: 0** for each (confirmed nm measurement)
**Forward deps:** both call `EngineContext::get()` (in library), so they sit above library naturally.

- `App.cpp` (87 lines): installs renderer/font/graphics/texture services via EngineContext
  - Deps: foundation-base (FontManager, ImageManager, TextureManager) + renderer (GraphicsSystem) + library (EngineContext)
- `Console.cpp` (419 lines): in-game dev console overlay
  - Deps: foundation-base (activeVideoBufferManager) + library (EngineContext + font/graphics/input services)

**Problem:** these 2 TUs are NOT cohesive (bootstrap installer vs dev console). They share only
the "no reverse edges" property. The roadmap notes App/System as "bootstrap installers" at the
right level. No coherent cluster justification. 0 payoff beyond removing 2 TUs from library.

**Verdict: NOT WORTH A CARVE. Park.**

### 3. Profiles carve

- `ProfileSystem.cpp` and `ObjectProfile_export.cpp` remain in library.
- Other profile TUs already in foundation-base.
- `ProfileSystem.cpp` has 1 reverse edge (ContentRuntimeBootstrap calls its ctor) but many forward deps
  (EngineContext, GameSessionContext, LoadPlayerElement, ObjectProfile::loadFromFile).
- `ObjectProfile_export.cpp`: 1 blocker (`Object::getBaseAttribute()` via `Entities/_Include.hpp`).
- Only 2 TUs remaining = not worth an archive.

### 4. Entities carve

123 blockers (flag-day). Not feasible incrementally. See [[physics-tu-remaining-game-edges]].

### 5. Module/GameSession carve

GameModule has 30+ reverse edge call sites from ~20 library TUs. Not tractable.

### 6. Physics middle carve (particle_collision, ObjectPhysics, CharacterMatrix)

ObjectPhysics: Shop.hpp + CharacterMatrix.h = genuinely-game blockers.
CharacterMatrix.c: 10+ blockers, graphic_mad.h + renderer_3d.h + Module.hpp.
particle_collision.c: CharacterParticleOps.h + Billboard impl = game-layer.
Not a clean cluster. See [[physics-tu-remaining-game-edges]].

### 7. Other clustering

- **graphic*.c cluster** (6 C files still in library): 9 reverse edges from GameEngine/egoboo/Module_bootstrap.
  ObjectGraphics is entity-coupled. "graphics-render FULL is NOT carve-able (45 reverse edges)" per health status.
- **Weather.cpp**: 0 reverse edges but 3+ forward deps into library (EngineContext, GameSessionContext, GameModule).
  Alone: 1 TU, no payoff.
- **game_combat.c, game_loop.c, game_targeting.c**: deeply interconnected with Entities cluster. No clean cut.

## Why no 10th carve

The library's 63 remaining TUs ARE the game-core "glue" — intentionally co-resident because they form
tight mutual dependencies (Entities ↔ Module ↔ Physics ↔ GameSession ↔ Graphics). The 9 prior carves
extracted every natural cluster. What's left is:
1. Low-TU isolated files (AudioSystem=1, App=1, Console=1) — too few for a meaningful archive
2. Large interlinked clusters (Entities, Physics, Module) — too coupled to carve without flag-day work

**The correct next step is NOT a 10th archive carve.** The roadmap points to:
- T1.2: Object role interface extraction (multi-role decoupling)
- T1.3: Continued service-interface layer expansion (remaining singletons)
- Deeper seam work to reduce the "genuine game-core" coupling (future Entities↔game decoupling front)

## Key observations

- `EGOLIB_AUDIO_SOURCES` was deliberately grouped in CMakeLists at line 34 — the maintainer
  pre-anticipated an audio carve but it was explicitly parked in the 9th carve memo.
- `EGOLIB_CONSOLE_SOURCES` similarly pre-grouped.
- Audio: `activeCameraSystem()` is in foundation-base; the Camera type is in game-graphics.
  The ICameraSystem seam would be needed to abstract spatial audio position queries.
- AudioSystem has exactly 3 library TUs calling concrete-type symbols (5 symbol sites total).
  This is the LOWEST reverse-edge count of any remaining library TU, but single-TU payoff.
