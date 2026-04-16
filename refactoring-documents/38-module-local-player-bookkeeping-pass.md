# Module Local-Player Bookkeeping Pass

This document records the local-player bookkeeping cleanup completed on 2026-04-16 after the shared quest-hydration pass.

## What changed

- kept `GameModule::_playerList` as the owner of runtime player registration
- kept `module_player_startup::registerPlayerBinding()` responsible for:
  - constructing `Ego::Player`
  - pushing the player into the module player list
  - assigning `object->is_which_player` immediately after registration
- extracted a narrower `applySuccessfulLocalPlayerBookkeeping()` helper in `game/Module` for the remaining successful local-bind side effects:
  - `object->islocalplayer = true`
  - `local_stats.player_count++`
  - `local_stats.noplayers = false`
  - optional `object->nameknown = true` when startup binding requests identification
- kept `finalizeLocalPlayerStartup()` as the small orchestrator that:
  - hydrates the player's quest log from the profile path
  - applies the local-player bookkeeping helper
- extended characterization coverage for:
  - stable registration-order assignment of `is_which_player`
  - successful startup identification when `identifySpawnOnSuccess` is enabled

## Why this pass now

Document 37 isolated shared quest hydration, but one mixed ownership seam still remained in the module startup path:

- quest hydration and local-player bookkeeping still lived in the same helper
- `_playerList` ownership was already module-local, while the rest of the successful local-bind side effects were not named separately

The safe next step was to isolate the remaining bookkeeping side effects without moving player registration into `GameSessionContext` or changing the existing backing state.

## Scope constraints kept

- no gameplay behavior changes
- no changes to `GameModule::addPlayer(...)` public signatures
- no changes to `spawnAllObjects()` orchestration, player-binding policy, or import-slot matching
- no migration of external consumers away from `local_stats.player_count`
- no change to `Object::isPlayer()`, `Object::islocalplayer`, or `Object::is_which_player` semantics
- no new logging or error reporting for missing `quest.txt`

## Acceptance baseline

This pass is considered healthy when all of the following remain true:

- player registration order still determines `object->is_which_player`
- successful local-player startup still marks the object as local and updates `local_stats`
- optional spawned-player identification still only affects successful startup binds that request it
- missing `quest.txt` remains silent
- existing spawn-realization and module smoke coverage still passes
- `test.mod` still validates with `egoboo-content-validator`

Validated command set for this pass:

```bash
ctest --test-dir build --output-on-failure -R 'ModuleSpawnRealizationFixture|ModuleLoadSmokeFixture|ModulePlayerStartup|LoadPlayerElement|PlayerQuestLog'
```

```bash
HOME=/tmp/egoboo-home \
XDG_DATA_HOME=/tmp/egoboo-xdg \
./build/products/x64/bin/egoboo-content-validator --data-dir "$PWD/data" --module test.mod
```

## Follow-on recommendation

The next practical seam is the first consumer migration away from raw `local_stats.player_count` reads:

- decide whether camera/load-state callers should move to a narrower module or session accessor for local-player count
- keep `GameModule::_playerList`, `is_which_player`, and `islocalplayer` semantics unchanged until those consumers have an explicit replacement seam
