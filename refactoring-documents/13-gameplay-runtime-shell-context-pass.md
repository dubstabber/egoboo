# Gameplay Runtime Shell Context Pass

This document records the next refactoring pass after the UI/session-access cleanup work.

## Baseline for this pass

As of 2026-04-13:

- module/session load and teardown already flow through `GameSessionContext`
- UI and game-state code now use session accessors instead of reading `_currentModule` directly
- the largest remaining lower-risk direct-global seam is `game.c`, not `script_functions.c` or the entity/runtime internals

That makes the gameplay runtime shell the next bounded seam.

## Scope of this pass

- Keep gameplay behavior unchanged.
- Keep content formats, loaders, and validator output unchanged.
- Remove direct `_currentModule`, `_gameEngine`, and `update_wld` reads from touched gameplay-shell helpers in `game.c`.
- Use `GameSessionContext` for module/session state and world/stat clocks.
- Introduce a narrow `EngineContext` wrapper for the few `PlayingState` lookups that still reached `_gameEngine`.

## In-scope areas

- `game.c`
- `GameSessionContext`
- new `EngineContext`
- gameplay-shell helpers for:
  - import/export flow
  - player/session helpers
  - particle/object helper entrypoints in `game.c`
  - status/minimap/message-log access that depends on the active `PlayingState`

## Explicit non-goals

- no changes to `script_functions.c`, `graphic.c`, `Object.cpp`, `Particle.cpp`, physics, or render passes
- no loader rewrite
- no validator feature work
- no content repair
- no broad removal of legacy globals from untouched files

## Acceptance criteria

- touched gameplay-shell code uses `GameSessionContext` and `EngineContext` instead of raw `_currentModule`, `_gameEngine`, or `update_wld`
- build succeeds with the new runtime-core seam in place
- existing `egolib` tests still pass
- `test.mod` validation remains the required runtime/content check once the environment can provide the validator's expected `/debug` log path

## Remaining deferred hotspots after this pass

The largest direct-global hotspots still sit in higher-risk runtime code:

- `game/script_functions.c`
- `game/graphic.c`
- `Entities/Object.cpp`
- `Entities/Particle.cpp`
- physics and graphics code that still read `_currentModule` or frame/update globals

Those should remain deferred until a later pass can isolate simulation and rendering ownership without mixing in scripting or entity-lifetime changes.
