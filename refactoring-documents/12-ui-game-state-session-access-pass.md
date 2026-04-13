# UI and Game-State Session Access Pass

This document records the next refactoring pass after the `GameSessionContext` extraction work.

## Baseline for this pass

As of 2026-04-13:

- module/session load and teardown now flow through `GameSessionContext`
- validator bootstrap setup is shared through `ContentRuntimeBootstrap`
- the remaining easy `_currentModule` call sites are concentrated in `GameStates` and HUD/UI code

That makes presentation-layer session access the next bounded seam.

## Scope of this pass

- Keep gameplay behavior unchanged.
- Keep content formats, loaders, and validator output unchanged.
- Remove direct `_currentModule` reads from the touched game-state and HUD/UI files.
- Use `GameSessionContext::activeModule()` for immediate update/draw paths.
- Use `GameSessionContext::tryActiveModule()` for destructors, debug watches, and other callbacks that can outlive the active module.

## In-scope areas

- `PlayingState`
- `InGameMenuState`
- `VictoryScreen`
- HUD/UI widgets that read live module or player-session state:
  - `CharacterStatus`
  - `CharacterWindow`
  - `LevelUpWindow`
  - `MiniMap`

## Explicit non-goals

- no changes to `script_functions.c`, `game.c`, `graphic.c`, physics, or gameplay simulation
- no loader rewrite
- no validator feature work
- no content repair
- no broad `_gameEngine` replacement

## Acceptance criteria

- touched files do not add new raw `_currentModule` reads
- debug watches and teardown paths tolerate the module being absent
- menu, loading, in-module UI, restart-module flow, and victory flow behave the same
- `test.mod` still validates against the existing baseline

## Remaining deferred hotspots after this pass

The largest direct-global hotspots are still outside the presentation layer:

- `game/script_functions.c`
- `game/game.c`
- `Entities/Object.cpp`
- `Entities/Particle.cpp`
- physics and graphics runtime code that still read `_currentModule` or `update_wld`

Those should stay deferred until a later pass can extract a tighter simulation/runtime seam without mixing in UI work.
