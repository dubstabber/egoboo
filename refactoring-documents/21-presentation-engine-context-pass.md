# Presentation Engine-Context Pass

This document records the next refactoring pass after the inventory and commerce cleanup work.

## Baseline for this pass

As of 2026-04-15:

- module/session load and teardown already flow through `GameSessionContext`
- inventory-facing runtime helpers such as `CharacterMatrix.c` and `Inventory.cpp` already use session accessors instead of raw `_currentModule`
- the highest-density remaining direct-global hotspot is now the presentation layer:
  - `game/GUI/*`
  - `game/GameStates/*`
- those files mostly reach `_gameEngine` for UI manager access, screen metrics, state transitions, and cursor control rather than gameplay simulation

That makes presentation-layer engine access the next safe seam before touching audio, render passes, camera internals, or module-owned runtime state.

## Scope of this pass

- Keep gameplay behavior unchanged.
- Keep content formats, loaders, and validator output unchanged.
- Remove direct `_gameEngine` reads from `game/GUI/*` and `game/GameStates/*`.
- Extend `EngineContext` with direct UI-manager accessors.
- Reuse thin presentation-layer helpers so widgets and states call `engine()` / `uiManager()` instead of reaching the global engine singleton directly.

## In-scope areas

- `game/Core/EngineContext.*`
- `game/GUI/Component.hpp`
- `game/GUI/*`
- `game/GameStates/*`

## Explicit non-goals

- no `_currentModule` cleanup outside already-touched presentation files
- no audio-system cleanup
- no render-pass, camera, weather, or gameplay-simulation cleanup
- no loader rewrite
- no validator feature work
- no content repair
- no UI-system redesign beyond swapping access paths

## Acceptance criteria

- `game/GUI/*` and `game/GameStates/*` no longer read raw `_gameEngine`
- touched presentation code uses `EngineContext`-backed `engine()` / `uiManager()` access instead of the global singleton directly
- build succeeds with the presentation seam in place
- existing `egolib` tests still pass
- `test.mod` validation remains the required runtime/content check and still passes
- main menu, options, module/player selection, minimap interaction, level-up flow, in-game menu flow, victory flow, and debug presentation states behave the same

## Remaining deferred hotspots after this pass

The largest direct-global hotspots after this pass sit outside the presentation layer:

- `Audio/AudioSystem.cpp`
- `game/Graphics/*` render-pass and camera helpers
- `game/Module/Module.cpp`
- weather and other runtime helpers that still read module-owned globals

Those should remain deferred until a later pass can pick another bounded seam without mixing presentation cleanup with deeper engine-service ownership changes.
