# Module Startup-Equipment Hook Pass

This document records the startup-equipment hook cleanup completed on 2026-04-16 after the player-binding policy pass.

## What changed

- extracted the startup-equipment identification branch from `module_spawn_realization::realizeSpawnEntry()` into a smaller internal helper
- kept the hook translation-unit-local inside `Module_spawn_realization.cpp`
- preserved the existing runtime behavior:
  - only non-import startup spawns with a player parent identify the spawned object
  - the hook still clears `iskursed` and marks the object name as known
  - the hook still runs after attachment handling and XP adjustment
  - the hook still runs before player-binding side effects
- extended characterization coverage for:
  - attached child equipment on a local-player parent
  - import-backed startup spawns skipping identification
  - non-player parents skipping identification

## Why this pass now

Document 34 isolated the player-binding decision, but `realizeSpawnEntry()` still contained one separate startup-only side effect:

- identifying child equipment for local player starts
- clearing the spawned item's curse state during that same startup path

That branch was already narrow, already covered by one direct characterization test, and did not require changing spawn order, attachment behavior, or player-binding semantics. It was the next safe seam to extract without broadening the scope of the spawn startup path.

## Scope constraints kept

- no gameplay behavior changes
- no spawn-order changes
- no changes to parent tracking in `spawnAllObjects()`
- no changes to attachment handling, import-slot matching, or player-binding policy
- no change to `GameModule::addPlayer()` side effects
- no new public API in `Module_spawn_realization.hpp`

## Acceptance baseline

This pass is considered healthy when all of the following remain true:

- existing spawn-planning tests still pass
- expanded spawn-realization characterization tests still pass
- module smoke coverage around concrete spawn resolution and end-to-end test-module loading still passes
- `test.mod` still validates with `egoboo-content-validator`

## Follow-on recommendation

The next practical seam is the remaining player-startup side-effect boundary around local player creation:

- decide whether `bindSpawnedPlayer()` should stay responsible for the spawn-identification side effect after successful local-player binding
- keep `GameModule::addPlayer()` and quest-log loading behavior unchanged until that boundary is explicitly characterized
- continue preserving `spawnAllObjects()` orchestration and `game_load_profile_ai()` ordering
