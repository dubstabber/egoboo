# Module Runtime Ownership Plan

This document records the next recommended refactoring phase after the runtime-context extraction passes and the `game.c` split completed on 2026-04-15.

The broad roadmap in `19-new-refactoring-plan.md` is still valid. This document narrows the immediate next step to the highest-yield seam visible in the current codebase.

## Why this seam now

The project already has useful runtime wrappers:

- `GameSessionContext`
- `EngineContext`

Those wrappers removed a large amount of direct `_currentModule` and `_gameEngine` usage from UI, gameplay shell, graphics shell, entity/physics shell, scripting shell, and inventory/commercial code.

However, the remaining architectural center of gravity is still the module/session runtime layer:

- `game/Module/Module.cpp` still owns module construction, content bootstrap, world-state initialization, and the main per-frame module update path.
- `GameSessionContext.cpp` still forwards several session-scoped values to legacy globals rather than owning them directly.
- small leaf systems that still reach raw globals are mostly module-adjacent:
  - `game/Module/Weather.cpp`
  - `game/Module/AnimatedTiles.cpp`
  - `Audio/AudioSystem.cpp`

That makes module-runtime ownership a better next seam than another presentation-only cleanup pass. It reduces real architectural coupling rather than just moving the last few call sites behind wrappers.

## Current constraints

Keep these constraints explicit:

- no gameplay behavior changes
- no content-format changes
- no loader rewrite beyond splitting existing logic into named phases
- no scripting redesign
- no audio-system redesign
- no validator schema change

The goal is ownership cleanup and seam creation, not feature work.

## Recommended checkpoint plan

### Checkpoint 1: Move session-owned counters and import state behind the session seam

Primary files:

- `egolib/library/src/egolib/game/Core/GameSessionContext.*`
- `egolib/library/src/egolib/game/Module/Module.cpp`
- `egolib/library/src/egolib/game/Module/Weather.cpp`
- `egolib/library/src/egolib/game/Module/AnimatedTiles.cpp`

Actions:

- make `GameSessionContext` the source of truth for:
  - world update count
  - character stat clock
  - enchant stat clock
  - import-list access
  - slot-override state
- remove raw reads of `_currentModule`, `update_wld`, `clock_chr_stat`, and related session globals from the touched module-adjacent files
- keep the bridge implementation local if full storage migration is too invasive for one pass

Why first:

- it finishes the lowest-risk ownership work around the current wrappers
- it removes the remaining module-leaf global access without touching scripts, rendering internals, or content semantics

### Checkpoint 2: Split `GameModule` construction into named phases

Primary file:

- `egolib/library/src/egolib/game/Module/Module.cpp`

Actions:

- extract constructor work into private helpers with stable names, for example:
  - module VFS and seed setup
  - texture/audio/global-profile bootstrap
  - wawalite and environment upload
  - profile and import loading
  - mesh, passages, and alliance load
  - debug logging and session-counter reset
- keep the constructor order unchanged
- do not change file formats or load rules

Why second:

- the constructor is currently the main hidden integration point
- named phases make later test coverage and fault isolation realistic

### Checkpoint 3: Split `GameModule::update()` into service and simulation helpers

Primary files:

- `egolib/library/src/egolib/game/Module/Module.cpp`
- optional new siblings under `egolib/library/src/egolib/game/Module/`

Actions:

- separate the current update path into helpers such as:
  - service updates
  - environment updates
  - AI/input gate
  - simulation updates
  - post-simulation camera/frame bookkeeping
- keep frame order unchanged
- route helper code through `GameSessionContext` where practical instead of raw globals

Why third:

- `GameModule::update()` is the runtime choke point after the `game.c` split
- this creates a better seam for later audio, weather, camera, and render-adjacent cleanup

### Checkpoint 4: Clean up module-leaf dependents after the boundary is stable

Primary files:

- `egolib/library/src/egolib/Audio/AudioSystem.cpp`
- any remaining small module-owned helpers discovered during the first three checkpoints

Actions:

- replace direct `_currentModule` lookups in audio loop ownership checks with session/object-handler accessors
- keep this as follow-on work, not the lead change

Why deferred:

- `AudioSystem.cpp` is a real remaining hotspot, but it is a leaf consumer of the module boundary rather than the boundary itself
- cleaning the module/session seam first reduces the chance of doing the audio cleanup twice

## Acceptance criteria

This phase is complete when:

- touched module-runtime files stop reading raw `_currentModule`, `update_wld`, and `clock_chr_stat` directly
- `GameModule` construction is broken into named phases without behavior drift
- `GameModule::update()` is no longer one undifferentiated lifecycle block
- existing `egolib` tests still pass
- `test.mod` still validates against the current validator baseline

## Why not jump to audio or rendering first

Audio and render-pass cleanup are both valid future steps, but neither is the best next one.

- audio is a smaller leaf seam and depends on module/object ownership being clearer
- render-pass work is broader and easier to destabilize
- the module runtime is the narrowest place where a small refactor reduces coupling across several downstream systems at once

## Follow-on options after this phase

If this phase lands cleanly, the next recommendations are:

1. finish the audio leaf cleanup
2. move weather and animated-tile state ownership fully under the module/session layer
3. start a narrower `Module.cpp` file split around construction, environment, and update responsibilities
