# Module Translation Unit Split Pass

This document records the `GameModule` file split completed on 2026-04-16.

It follows the environment-state ownership pass by turning the remaining monolithic `Module.cpp` implementation into focused translation units without changing behavior.

## What changed

- added `game/Module/Module_internal.h` as the shared private header for split `GameModule` implementation files
- moved constructor, teardown, and bootstrap phases into `game/Module/Module_bootstrap.cpp`
- moved profile/passage/alliance loading and slot-usage reporting into `game/Module/Module_loading.cpp`
- moved spawn realization, player attachment, and terrain-tilt work into `game/Module/Module_spawn.cpp`
- moved per-frame services, simulation, and terrain/environment updates into `game/Module/Module_update.cpp`
- reduced `game/Module/Module.cpp` to lightweight accessors and simple queries
- updated `egolib/library/CMakeLists.txt` to list the new module files explicitly

## Why this pass now

The earlier ownership passes already created the right seam:

- `GameModule` construction had named bootstrap phases
- update flow was already split into service, simulation, and finalize helpers
- environment state was module-owned instead of global

That meant the remaining problem in `Module.cpp` was structural rather than semantic: one translation unit still mixed bootstrap, loading, spawn, passage/shop helpers, runtime update, and trivial accessors.

Splitting it now makes the next passes narrower and easier to verify without reopening the earlier ownership work.

## Scope constraints kept

- no gameplay behavior changes
- no content-format or validator-schema changes
- no loader-rule changes
- no spawn-order changes
- no update-order changes

This is mechanical code redistribution only.

## Acceptance baseline

This pass is considered healthy when all of the following remain true:

- `GameModule` construction order is unchanged
- `GameModule::update()` order is unchanged
- touched code still builds cleanly
- existing tests still pass
- `test.mod` still validates against the current validator baseline

## Follow-on recommendation

The next recommended seam after this split is to add boundary-focused coverage around the now-separated module runtime files, then decide whether the next extraction should target the spawn/import path or another remaining large runtime hotspot.
