# Session Local-Player Count Access Pass

This document records the local-player-count accessor cleanup completed on 2026-04-16 after the local-player bookkeeping pass.

## What changed

- added `GameSessionContext::localPlayerCount()` as the first narrow read-side accessor for local-player count
- implemented that accessor from the active module player registration state by returning `playerList().size()` when a module is live, with a fallback to the current mirrored legacy counter when no module is active yet
- migrated the first raw `local_stats.player_count` consumers to the session accessor:
  - camera setup in `LoadingState`
  - camera setup in `DebugModuleLoadingState`
  - single-player fast-turn gating in `Player::updateLatches()`
  - single-player autoturn gating in `Camera::readInput()`
- kept `module_player_startup` responsible for the existing successful local-bind bookkeeping writes to `local_stats.player_count`
- added focused regression coverage that checks the new session accessor against the current mirrored legacy counter in the module-player-startup path

## Why this pass now

Document 38 isolated the remaining successful local-player bookkeeping side effects, but the first read-side consumers still reached directly into `local_stats.player_count`.

The next safe step was to introduce one narrow session accessor and migrate the camera/load-state callers named in the follow-on recommendation, without widening the pass into spawn orchestration or game-loop ownership changes.

## Scope constraints kept

- no gameplay behavior changes
- no changes to `GameModule::_playerList` ownership or registration order
- no changes to `GameModule::addPlayer(...)` public signatures
- no changes to `spawnAllObjects()` orchestration, player-binding policy, or import-slot matching
- no migration of `game_loop.c` or other non-targeted `local_stats.player_count` readers
- no changes to `local_stats.player_count` write-side bookkeeping semantics

## Acceptance baseline

This pass is considered healthy when all of the following remain true:

- the targeted camera/load-state callers no longer read `local_stats.player_count` directly
- session local-player count still matches the mirrored legacy bookkeeping counter before an active module exists
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

The next practical seam is the next consumer migration away from raw `local_stats.player_count` reads outside the camera/load-state path:

- decide whether `GameModule::spawnObjectFromFileEntry()` should adopt the session accessor or a narrower module-local replacement for `currentLocalPlayerCount`
- then revisit the remaining `game_loop.c` reads separately, because those paths may want a broader session-status abstraction rather than a plain count accessor
- keep `GameModule::_playerList`, `is_which_player`, `islocalplayer`, and the current bookkeeping writes unchanged until those later consumers have an explicit replacement seam
