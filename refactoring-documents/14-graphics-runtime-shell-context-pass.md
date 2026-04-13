# Graphics Runtime Shell Context Pass

This document records the next refactoring pass after the gameplay runtime shell cleanup work.

## Baseline for this pass

As of 2026-04-13:

- module/session load and teardown already flow through `GameSessionContext`
- gameplay-shell helpers in `game.c` now use `GameSessionContext` and `EngineContext`
- the remaining lower-risk direct-global seam is the graphics shell in `graphic.c`, not scripting or entity/runtime internals

That makes the gameplay rendering shell the next bounded seam.

## Scope of this pass

- Keep rendering behavior unchanged.
- Keep content formats, loaders, and validator output unchanged.
- Remove direct `_currentModule`, `_gameEngine`, and `update_wld` reads from touched graphics-shell helpers in `graphic.c`.
- Use `GameSessionContext` for active-module and world-update access.
- Use `EngineContext` for UI-manager and rendered-frame access.

## In-scope areas

- `graphic.c`
- file-local rendering-shell helpers for:
  - HUD and overlay drawing
  - debug/status display
  - cursor rendering
  - passage debug drawing
  - tile/entity list assembly
  - flashing and object/particle instance update gates

## Explicit non-goals

- no changes to `game/Graphics/*`, render passes, camera internals, or graphics algorithms
- no changes to `script_functions.c`, `Object.cpp`, `Particle.cpp`, physics, or gameplay simulation
- no loader rewrite
- no validator feature work
- no content repair
- no broad removal of legacy globals from untouched files

## Acceptance criteria

- touched graphics-shell code uses `GameSessionContext` and `EngineContext` instead of raw `_currentModule`, `_gameEngine`, or `update_wld`
- build succeeds with the rendering-shell seam in place
- existing `egolib` tests still pass
- `test.mod` validation remains the required runtime/content check

## Remaining deferred hotspots after this pass

The largest direct-global hotspots still sit in higher-risk runtime code:

- `game/script_functions.c`
- `Entities/Object.cpp`
- `Entities/Particle.cpp`
- `game/Graphics/*` helpers and render-pass code that still read module or frame globals
- physics code that still reads `_currentModule` or update globals

Those should remain deferred until a later pass can isolate deeper simulation and graphics ownership without mixing in scripting or entity-lifetime changes.
