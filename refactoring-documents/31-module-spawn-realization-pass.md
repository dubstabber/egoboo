# Module Spawn Realization Pass

This document records the spawn-realization extraction completed on 2026-04-16.

## What changed

- extracted the live `spawn.txt` realization branches from `GameModule::spawnObjectFromFileEntry()` into a dedicated internal helper
- kept `GameModule::spawnAllObjects()` as the orchestration layer for profile activation, per-entry realization, parent tracking, terrain tilt, and deferred AI loading
- preserved the existing runtime behavior for:
  - missing-parent rejection for attached spawns
  - `ATTACH_NONE` matrix setup
  - inventory attachment and merge-termination handling
  - left/right grip attachment
  - startup-equipment identification for local player gear
  - single-player and import-based local-player binding
- added direct characterization tests for the new realization helper covering attachment and player-binding branches

## Why this pass now

Document 30 isolated the planning half of module spawning. That left one behavior-heavy function as the remaining seam:

- it still mixed object creation, attachment branches, post-spawn state updates, and player/import bookkeeping
- it was only covered indirectly by coarse module-load smoke coverage
- it remained on the startup critical path because `GameSessionContext::beginModule()` still calls `spawnAllObjects()` immediately after module construction

This pass narrows that live behavior into a testable helper without changing the surrounding module-load flow.

## Scope constraints kept

- no gameplay behavior changes
- no spawn-order changes
- no loader-rule changes
- no content-format or validator-schema changes
- no attempt to redesign remote-input handling
- no changes to `GameModule::spawnObject()`

## Acceptance baseline

This pass is considered healthy when all of the following remain true:

- existing module-load smoke coverage still passes
- existing spawn-planning tests still pass
- new realization-helper tests pass
- `test.mod` still validates against the current validator baseline
- player/import attachment behavior remains unchanged in runtime flow

## Follow-on recommendation

The next practical seam is a narrower cleanup inside the new realization helper:

- isolate import-slot matching and local-player binding into a smaller policy helper
- decide whether startup-equipment identification should remain coupled to spawn realization or move behind a more explicit player-startup hook
- keep `spawnAllObjects()` as orchestration unless a later pass also extracts parent-tracking semantics
