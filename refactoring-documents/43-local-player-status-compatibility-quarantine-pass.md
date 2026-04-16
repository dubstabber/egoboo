# Local-Player Status Compatibility Quarantine Pass

This document records the local-player status compatibility quarantine cleanup completed on 2026-04-16 after the session local-player status ownership pass.

## What changed

- kept `GameSessionContext` as the ownership surface for local-player count and liveness status:
  - `localPlayerCount()`
  - `localPlayerStatus()`
  - `hasLocalPlayers()`
  - `allLocalPlayersDead()`
- quarantined the legacy `local_stats` write-through for:
  - `player_count`
  - `noplayers`
  - `allpladead`
- moved the mirror publication into a file-local compatibility bridge inside `GameSessionContext.cpp`
- removed the old header-level `syncLegacyLocalPlayerState()` helper declaration so the compatibility path no longer appears in the session type surface
- annotated `local_stats_t` and the global `local_stats` definition as legacy compatibility state
- narrowed `ModulePlayerStartup` coverage so raw `local_stats` assertions remain only in focused compatibility tests for:
  - pre-module fallback mirroring
  - reset-time mirror clearing
  - published `all players dead` mirror updates

## Why this pass now

Document 42 moved ownership of local-player status to `GameSessionContext`, but the exported legacy mirrors still needed one final structural cleanup:

- production gameplay/UI reads were already off the raw fields
- the only remaining production writes lived in `GameSessionContext`
- tests still mixed ownership assertions with compatibility assertions

The safe next step was to quarantine the mirror writes behind one explicit implementation-only bridge while preserving the exported ABI and current behavior.

## Scope constraints kept

- no gameplay behavior changes
- no changes to `GameModule::_playerList` ownership or player registration order
- no changes to `GameModule::addPlayer(...)` public signatures
- no changes to spawn orchestration, player-binding policy, or import-slot matching
- no change to the current pre-first-frame `all players dead` timing
- no removal or layout change of `local_stats_t`
- no changes to non-player `local_stats` fields

## Acceptance baseline

This pass is considered healthy when all of the following remain true:

- `GameSessionContext` remains the only production owner of local-player status
- `local_stats.player_count`, `local_stats.noplayers`, and `local_stats.allpladead` are written only from the compatibility bridge in `GameSessionContext.cpp`
- `game_reset_players()` still clears local-player state through `GameSessionContext::resetLocalPlayerState()`
- reset-state behavior still reports no local players and not-all-dead before the first published frame snapshot
- direct raw-field assertions in tests remain limited to dedicated compatibility cases
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

The next practical seam is broader legacy-global quarantine beyond this local-player status slice:

- continue migrating other gameplay-owned `local_stats` concerns toward explicit session or subsystem ownership surfaces
- keep the exported `local_stats` ABI stable until all remaining direct consumers of each field group have an explicit replacement
