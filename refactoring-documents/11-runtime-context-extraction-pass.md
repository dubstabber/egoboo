# Runtime Context Extraction Pass

This document records the next refactoring pass after the spawn-reconciliation tooling work.

## Baseline for this pass

As of 2026-04-13:

- spawn-name normalization is shared between runtime and validator code
- the validator can emit reconciliation rows and reachable-object inventories
- the dominant validation failure class is still missing spawn-object references, not parser crashes

That means the next useful pass is no longer another spawn-format/tooling slice. The stronger seam is runtime-session access.

## Scope of this pass

- Keep gameplay behavior unchanged.
- Keep legacy parsers and content formats unchanged.
- Introduce explicit runtime/session access points for:
  - active module ownership
  - import-list ownership
  - slot-override state
  - world and stat clocks
- Reuse a shared non-UI content bootstrap path between the main runtime and the validator.

## In-scope caller migration

This pass only migrates the module/session load path:

- `game_begin_module`, `game_quit_module`, `game_finish_module`
- `LoadingState`
- `DebugModuleLoadingState`
- `MapEditorState`
- `GameEngine` frame-count and shutdown cleanup
- validator bootstrap setup

## Explicit non-goals

- no broad `_currentModule` replacement across scripts, GUI, physics, or rendering
- no content repair or alias tables
- no loader rewrites
- no scripting replacement
- no data migration

## Acceptance criteria

- the validator still loads module profiles and emits the existing JSON report structure
- `test.mod` still validates
- full validator totals stay aligned with the 2026-04-13 baseline unless a separately explained bug fix changes them
- normal module loading, debug module loading, and map editor loading still work
- new or edited load-path code uses the session/bootstrap seam instead of raw globals
