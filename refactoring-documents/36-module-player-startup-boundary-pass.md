# Module Player-Startup Boundary Pass

This document records the player-startup boundary cleanup completed on 2026-04-16 after the startup-equipment hook pass.

## What changed

- kept `module_spawn_realization::realizeSpawnEntry()` responsible for spawn creation, attachment handling, XP adjustment, startup-equipment identification, and player-binding policy
- changed the internal player-binding operation to pass a small request object instead of a raw device index:
  - target local device slot
  - whether successful binding should identify the spawned player object
- moved the successful-bind identification side effect out of `bindSpawnedPlayer()` and into the module-side startup sink
- extracted a small internal `module_player_startup` helper boundary used by `GameModule::addPlayer(...)` for:
  - player-list registration
  - player index assignment on the spawned object
  - quest-log hydration from `quest.txt`
  - local-player flag and `local_stats` updates
  - optional spawned-player identification on successful startup binding
- kept the public `GameModule::addPlayer(const std::shared_ptr<Object>&, const Ego::Input::InputDevice&)` signature unchanged
- added direct characterization coverage for the module-side startup helper around:
  - null-object rejection without mutating player/module state
  - successful local-player startup preserving silent missing-`quest.txt` behavior

## Why this pass now

Document 35 isolated startup-equipment identification, but one player-startup side effect still crossed the seam awkwardly:

- `bindSpawnedPlayer()` still knew whether a successful local bind should identify the spawned player object
- `GameModule::addPlayer()` still mixed player registration, quest-log hydration, and local-player bookkeeping in one opaque sink

The safe next step was to keep binding policy in the spawn-realization layer while moving all bind-success side effects behind one narrower module-side startup boundary.

## Scope constraints kept

- no gameplay behavior changes
- no spawn-order changes
- no changes to `spawnAllObjects()` orchestration or parent tracking
- no change to startup-equipment identification for local-player child items
- no change to import-slot matching or the legacy no-op remote-input branch
- no new public API in `Module_spawn_realization.hpp`
- no new error reporting for missing `quest.txt`; the current silent best-effort hydration behavior remains intact

## Acceptance baseline

This pass is considered healthy when all of the following remain true:

- existing spawn-realization characterization tests still pass
- existing module smoke coverage still passes
- the new module-side startup helper characterization tests pass
- `test.mod` still validates with `egoboo-content-validator`

Validated command set used for this pass:

```bash
ctest --test-dir build --output-on-failure -R 'ModuleSpawnRealizationFixture|ModuleLoadSmokeFixture|ModulePlayerStartupFixture'
```

```bash
HOME=/tmp/egoboo-home \
XDG_DATA_HOME=/tmp/egoboo-xdg \
./build/products/x64/bin/egoboo-content-validator --data-dir "$PWD/data" --module test.mod
```

## Follow-on recommendation

The next practical seam is the remaining player-startup content concern around quest-log hydration:

- decide whether quest-log loading should remain a module-startup side effect or move behind a narrower player-profile/session service
- keep the current silent missing-`quest.txt` behavior until that boundary is explicitly redesigned
- continue preserving `spawnAllObjects()` orchestration, player-binding policy, and `game_load_profile_ai()` ordering while narrowing the player/session ownership boundary
