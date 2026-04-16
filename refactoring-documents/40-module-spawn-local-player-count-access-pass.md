# Module Spawn Local-Player Count Access Pass

This document records the spawn-time local-player-count accessor cleanup completed on 2026-04-16 after the first session local-player-count access pass.

## What changed

- migrated `GameModule::spawnObjectFromFileEntry()` away from the last direct spawn-time read of `local_stats.player_count`
- updated the spawn-realization plumbing in `Module_spawn.cpp` so `ops.currentLocalPlayerCount` now reads through `GameSessionContext::localPlayerCount()`
- kept `module_spawn_realization::decidePlayerBinding()` unchanged as the policy owner for zero-import local-device-slot assignment
- kept `module_player_startup` responsible for successful local-player bookkeeping writes to `local_stats.player_count`
- added focused regression coverage that keeps the zero-import spawn-realization path requesting incrementing local device slots across successive successful binds after the session-accessor migration

## Why this pass now

Document 39 introduced `GameSessionContext::localPlayerCount()` and migrated the first camera/load-state readers, but one live spawn-time consumer still reached directly into `local_stats.player_count`:

- `GameModule::spawnObjectFromFileEntry()` still supplied `currentLocalPlayerCount` from the legacy mirrored counter
- that kept zero-import spawn-time device-slot selection coupled to bookkeeping state instead of the active module/session player registration owner

The next safe step was to migrate that single spawn-time consumer without widening the pass into `game_loop.c` status logic or changing the existing player-binding policy.

## Scope constraints kept

- no gameplay behavior changes
- no changes to `GameModule::_playerList` ownership or registration order
- no changes to `SpawnRealizationOps` shape or `module_spawn_realization::decidePlayerBinding()` policy
- no changes to `GameModule::addPlayer(...)` public signatures
- no migration of `game_loop.c` or other non-targeted session-status readers
- no changes to `local_stats.player_count` write-side bookkeeping semantics

## Acceptance baseline

This pass is considered healthy when all of the following remain true:

- `GameModule::spawnObjectFromFileEntry()` no longer reads `local_stats.player_count` directly
- zero-import spawn binding still starts at local device slot 0 and advances across successive successful binds
- session local-player count still maps to the active module player list size once a module is live
- existing spawn-realization, module smoke, player-startup, load-player, and quest-log coverage still passes
- `test.mod` still validates with `egoboo-content-validator`

Validated command set for this pass:

```bash
cmake --build build -j4
```

```bash
ctest --test-dir build --output-on-failure -R 'ModuleSpawnRealizationFixture|ModuleLoadSmokeFixture|ModulePlayerStartupFixture|LoadPlayerElementFixture|PlayerQuestLogFixture'
```

```bash
HOME=/tmp/egoboo-home \
XDG_DATA_HOME=/tmp/egoboo-xdg \
./build/products/x64/bin/egoboo-content-validator --data-dir "$PWD/data" --module test.mod
```

## Follow-on recommendation

The next practical seam is the remaining direct `local_stats.player_count` read in `game_loop.c`:

- introduce a broader session-status abstraction for the `allpladead` calculation instead of treating it as just another count accessor migration
- keep `GameModule::_playerList`, `is_which_player`, `islocalplayer`, and the current bookkeeping writes unchanged until that broader gameplay-status seam is explicitly named
