# Entity and Physics Runtime Context Pass

This document records the next refactoring pass after the graphics runtime shell cleanup work.

## Baseline for this pass

As of 2026-04-13:

- module/session load and teardown already flow through `GameSessionContext`
- gameplay-shell helpers in `game.c` now use `GameSessionContext` and `EngineContext`
- graphics-shell helpers in `graphic.c` now use those same runtime wrappers
- the remaining lower-risk direct-global seam is concentrated in the entity and physics runtime, not in scripting or deeper graphics internals

That makes the entity/physics runtime shell the next bounded seam.

## Scope of this pass

- Keep gameplay behavior unchanged.
- Keep content formats, loaders, and validator output unchanged.
- Remove direct `_currentModule`, `_gameEngine`, `update_wld`, and `clock_chr_stat` reads from the touched entity/physics runtime files.
- Reuse `GameSessionContext` for active-module access and world/stat clocks.
- Reuse `EngineContext` for the small remaining engine and playing-state lookups in the touched code.
- Move header-level global access behind out-of-line implementations where needed instead of widening the public API.

## In-scope areas

- `Entities/Object.*`
- `Entities/Particle.cpp`
- `Entities/ParticleHandler.cpp`
- `game/Physics/ObjectPhysics.cpp`
- `game/Physics/ParticlePhysics.cpp`
- `game/Physics/particle_collision.c`
- `game/Logic/Player.cpp`

## Explicit non-goals

- no changes to `game/script_functions.c`, `game/script_implementation.c`, `Script/script.c`, or `game/CharacterMatrix.c`
- no render-pass, camera, or graphics-algorithm cleanup beyond the small engine-state bridges already in the touched files
- no loader rewrite
- no validator feature work
- no content repair
- no broad removal of legacy globals from untouched files

## Acceptance criteria

- touched files no longer read raw `_currentModule`, `_gameEngine`, `update_wld`, or `clock_chr_stat`
- build succeeds with the entity/physics runtime seam in place
- existing `egolib` tests still pass
- `test.mod` validation remains the required runtime/content check and still passes
- object removal, particle collision/attachment, water ripple timing, and player inventory/charge timing behave the same

## Remaining deferred hotspots after this pass

The largest direct-global hotspots still sit in higher-risk runtime code:

- `game/script_functions.c`
- `Script/script.c`
- `game/script_implementation.c`
- `game/CharacterMatrix.c`
- graphics, audio, and other runtime helpers outside this pass that still read module or frame globals

Those should remain deferred until a later pass can isolate scripting and lower-level simulation helpers without mixing in content, loader, or UI changes.
