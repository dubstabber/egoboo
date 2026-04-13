# Scripting Runtime Shell Context Pass

This document records the next refactoring pass after the entity and physics runtime shell cleanup work.

## Baseline for this pass

As of 2026-04-13:

- module/session load and teardown already flow through `GameSessionContext`
- gameplay, graphics, entity, and physics shell helpers already use runtime wrappers instead of direct-global access
- the largest remaining direct-global hotspot is the script runtime cluster:
  - `game/script_functions.c`
  - `Script/script.c`
  - `game/script_implementation.c`
  - `game/script_variables.c`

That makes the scripting runtime shell the next bounded seam.

## Scope of this pass

- Keep script behavior unchanged.
- Keep content formats, loaders, and validator output unchanged.
- Remove direct `_currentModule`, `_gameEngine`, and `update_wld` reads from the touched script runtime files.
- Reuse `GameSessionContext` for active-module, object-handler, team, passage, mesh, player, and world-update access.
- Reuse `EngineContext` for script-triggered `PlayingState`, UI-manager, and game-state interactions.
- Keep the seam file-local where possible instead of widening public APIs.

## In-scope areas

- `game/script_functions.c`
- `Script/script.c`
- `game/script_implementation.c`
- `game/script_variables.c`

## Explicit non-goals

- no Lua migration
- no EgoScript syntax changes
- no scripting API redesign
- no loader rewrite
- no validator feature work
- no content repair
- no broad cleanup of `CharacterMatrix.c`, `Object.cpp`, `Particle.cpp`, or deeper gameplay simulation code

## Acceptance criteria

- touched script runtime files no longer read raw `_currentModule`, `_gameEngine`, or `update_wld`
- build succeeds with the scripting shell seam in place
- existing `egolib` tests still pass
- `test.mod` validation remains the required runtime/content check and still passes
- script-driven AI, passage queries, scripted spawn/enchant helpers, minimap visibility, status-monitor hooks, victory flow, and screenshot hooks behave the same

## Remaining deferred hotspots after this pass

The largest direct-global hotspots after this pass sit outside the script runtime shell:

- `game/CharacterMatrix.c`
- `game/Shop.cpp`
- `Audio/AudioSystem.cpp`
- remaining graphics, inventory, and module helpers that still read direct globals

Those should remain deferred until a later pass can pick another bounded runtime seam without mixing in script-language replacement or content work.
