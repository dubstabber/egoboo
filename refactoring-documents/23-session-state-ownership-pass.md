# Session-State Ownership Pass

This document records the session-state ownership cleanup completed on 2026-04-15.

It follows the presentation and module-runtime cleanup passes by turning `GameSessionContext` from a thin forwarding wrapper into the source of truth for active-module and session-frame state.

## What changed

- `GameSessionContext` now owns:
  - the active `GameModule`
  - import-list state
  - slot-override state
  - world update count
  - character stat clock
  - enchant stat clock
- legacy storage exports were removed from:
  - `game.h`
  - `game.c`
  - `game_export.c`
  - `game_loop.c`
  - `Module.hpp`
  - `GameEngine.hpp`
- the remaining direct `_currentModule` reads in active runtime code were replaced with session accessors in:
  - `Logic/Team.cpp`
  - `Entities/Enchant.cpp`
  - `game/link.c`
  - `game/mesh.c`
- the remaining direct session-frame and slot-override reads were replaced with session accessors in:
  - `Profiles/ProfileSystem.cpp`
  - `game/GUI/InventorySlot.cpp`
  - `game/GUI/CharacterStatus.cpp`
  - `game/Graphics/ObjectGraphics.cpp`

## Scope constraints kept

- no gameplay behavior changes
- no content-format or validator-schema changes
- no loader rewrite
- no audio-system redesign
- no render-pipeline redesign

The change is ownership cleanup only. Construction order, module spawn timing, and per-frame update order remain unchanged.

## Acceptance baseline

This pass is considered healthy when all of the following remain true:

- the codebase no longer has active runtime reads of raw `_currentModule`
- `GameSessionContext` is the only owner of the moved session state
- the build succeeds
- parser and module smoke tests still pass
- `test.mod` still validates cleanly

## Remaining deferred hotspots

This pass removes the last raw session globals, but it does not finish runtime decoupling.

The main follow-on seams are now:

- `Audio/AudioSystem.cpp` module/session ownership cleanup
- further `Module.cpp` decomposition around construction and update responsibilities
- narrowing broad `GameSessionContext` reach in render/UI leaves once more local frame/runtime interfaces exist

Those should stay bounded follow-up passes rather than being folded into this ownership migration.
