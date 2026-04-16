# Environment State Ownership Pass

This document records the environment/session ownership cleanup completed on 2026-04-16.

It follows the session-state ownership pass, the deferred audio leaf cleanup, and the entity-layer decomposition work by removing the remaining raw environment globals from active runtime code.

## What changed

- `GameModule` now owns:
  - weather state
  - fog state
  - animated-tile state
- `upload_wawalite(...)` now uploads into explicit state objects passed by the module instead of mutating file-scope globals.
- `GameSessionContext` now exposes accessors for module-owned weather, fog, and animated-tile state so scripting and render leaves can reach that state without depending on global storage.
- active runtime consumers now use those ownership seams in:
  - `game/Module/Module.cpp`
  - `game/graphic_fan.c`
  - `game/script_functions_state.c`
  - `game/script_functions_systems.c`
- the old `g_weatherState`, `fog`, and `g_animatedTilesState` exports were removed from `game.h` and `game_wawalite.c`.

## Why this pass now

The earlier session-state pass removed counters, import state, and active-module globals, but the environment state still sat outside the module/session seam even though it was module-local in behavior.

That left three problems:

- `GameModule::updateModuleServices()` still updated raw globals
- script functions still mutated fog/weather through `game.h`
- tile animation still depended on global animated-tile state

Moving those values under `GameModule` finishes the remaining ownership work that the module-runtime plan explicitly deferred.

## Scope constraints kept

- no gameplay behavior changes
- no loader rewrite
- no content-format or validator-schema changes
- no weather-system redesign
- no render-pipeline redesign

This is ownership cleanup only. Update order and wawalite semantics remain unchanged.

## Acceptance baseline

This pass is considered healthy when all of the following remain true:

- active runtime code no longer reads `g_weatherState`, `fog`, or `g_animatedTilesState` as globals
- module environment upload still runs during module construction
- touched code still builds cleanly
- existing tests still pass
- `test.mod` still validates against the current validator baseline

## Follow-on recommendation

The next recommended seam after this pass is a narrower `Module.cpp` file split that moves construction/environment/update responsibilities into dedicated translation units now that the remaining environment state is module-owned.
