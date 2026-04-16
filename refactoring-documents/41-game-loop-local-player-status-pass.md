# Game Loop Local-Player Status Pass

This document records the local-player-status helper cleanup completed on 2026-04-16 after the spawn-time local-player-count accessor pass.

## What changed

- added a new `LocalPlayerStatus` helper in `egolib/game/Core` for read-side local-player liveness aggregation
- added `collectLocalPlayerStatus(...)` to derive:
  - registered local-player count from the current player registration list
  - alive local-player count from non-terminated alive player objects
  - dead local-player count from non-terminated dead player objects
  - `allPlayersDead()` from those aggregated counts
- migrated `MainLoop::updateLocalStats()` away from the last direct gameplay read of `local_stats.player_count`
- kept `MainLoop::readPlayerInput()` consuming the mirrored `local_stats.allpladead` flag exactly as before
- added focused regression coverage for:
  - empty registration semantics
  - alive-only, dead-only, and mixed alive/dead local-player registration
  - null-object and terminated-object registrations staying excluded from alive/dead counts while still contributing to the registration count

## Why this pass now

Document 40 migrated the last spawn-time `local_stats.player_count` consumer, but one gameplay-loop read still remained in `game_loop.c`:

- `MainLoop::updateLocalStats()` still decided `local_stats.allpladead` from `numdead >= local_stats.player_count`
- that left the gameplay loop coupled to mirrored bookkeeping state even though it already iterated the registered local-player list directly

The next safe step was to name that read-side liveness calculation explicitly without widening the pass into player-registration ownership changes or respawn-UI behavior changes.

## Scope constraints kept

- no gameplay behavior changes
- no changes to `GameModule::_playerList` ownership or registration order
- no changes to `GameModule::addPlayer(...)` public signatures
- no changes to `module_player_startup` write-side bookkeeping for `local_stats.player_count` or `local_stats.noplayers`
- no changes to `spawnAllObjects()` orchestration, player-binding policy, or import-slot matching
- no changes to `local_stats.allpladead` consumers outside `MainLoop::updateLocalStats()`
- no pre-module replacement for the existing `GameSessionContext::localPlayerCount()` fallback to `local_stats.player_count`

## Acceptance baseline

This pass is considered healthy when all of the following remain true:

- `game_loop.c` no longer reads `local_stats.player_count` directly
- `MainLoop::updateLocalStats()` still reports all-players-dead using the active local-player registration list
- zero registered local players still produce the current `all players dead` result
- null-object and terminated-object player registrations do not contribute to alive/dead liveness totals
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

The next practical seam is the remaining write-side mirroring around local-player session status:

- decide whether `local_stats.player_count`, `local_stats.noplayers`, and `local_stats.allpladead` should remain long-term mirrors or move behind a broader session-status surface
- keep `GameModule::_playerList`, `is_which_player`, `islocalplayer`, and current spawn/player-startup bookkeeping unchanged until that broader status ownership pass is explicitly named
