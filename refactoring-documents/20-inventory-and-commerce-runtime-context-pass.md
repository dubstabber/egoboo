# Inventory and Commerce Runtime Context Pass

This document records the next refactoring pass after the scripting runtime shell cleanup work.

## Baseline for this pass

As of 2026-04-13:

- module/session load and teardown already flow through `GameSessionContext`
- gameplay, graphics, entity, physics, and scripting shell helpers already use runtime wrappers instead of direct-global access
- the next bounded gameplay hotspot is the inventory and commerce interaction cluster:
  - `game/CharacterMatrix.c`
  - `game/Inventory.cpp`
  - `game/Shop.cpp`

That makes inventory, attachment, and shop ownership the next safe seam before broader UI, audio, or rendering cleanup.

## Scope of this pass

- Keep gameplay behavior unchanged.
- Keep content formats, loaders, and validator output unchanged.
- Remove direct `_currentModule` reads from the touched inventory and commerce runtime files.
- Reuse `GameSessionContext` for active-module access.
- Keep the seam file-local where possible instead of widening public APIs.

## In-scope areas

- `game/CharacterMatrix.c`
- `game/Inventory.cpp`
- `game/Shop.cpp`

## Explicit non-goals

- no `_gameEngine` cleanup in HUD or other GUI files
- no audio-system cleanup
- no render-pass or camera cleanup
- no loader rewrite
- no validator feature work
- no content repair
- no broad removal of legacy globals from untouched files

## Acceptance criteria

- touched inventory and commerce runtime files no longer read raw `_currentModule`
- build succeeds with the inventory/commerce seam in place
- existing `egolib` tests still pass
- `test.mod` validation remains the required runtime/content check and still passes
- stack merging, inventory swap/remove flow, shop buy/sell/steal handling, and held-item matrix updates behave the same

## Remaining deferred hotspots after this pass

The largest direct-global hotspots after this pass sit outside the inventory and commerce seam:

- `game/CharacterMatrix.c` adjacent UI and gameplay paths still coupled through other globals outside this pass
- `Audio/AudioSystem.cpp`
- remaining GUI `_gameEngine` call sites
- camera, render-pass, and weather helpers that still read module globals

Those should remain deferred until a later pass can pick another bounded seam without mixing inventory/combat behavior changes with UI, audio, or rendering work.
