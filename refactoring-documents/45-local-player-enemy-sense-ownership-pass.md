# Local-Player Enemy-Sense Ownership Pass

This document records the enemy-sense ownership cleanup completed on 2026-04-16 after the local-player perception ownership pass.

## What changed

- made `GameSessionContext` the ownership surface for the minimap enemy-sense filter by adding:
  - `enemySense()`
  - `publishEnemySense(...)`
  - `resetEnemySense()`
- added a shared `EnemySenseState` snapshot carrying:
  - `sense_enemies_team`
  - `sense_enemies_idsz`
- moved the legacy `local_stats` write-through for those fields into a file-local compatibility bridge inside `GameSessionContext.cpp`
- updated the active production writers to publish through the session seam instead of mutating `local_stats` directly:
  - perk-driven minimap reveals in `Entities/Object_update.cpp`
  - scripted enemy-blip publication in `game/script_functions_systems.c`
- updated `MiniMap::draw()` to read the session-owned enemy-sense state instead of the raw legacy global
- updated `game_reset_players()` and module-begin reset flow to clear the session-owned enemy-sense state and its compatibility mirrors
- added focused regression coverage for:
  - compatibility mirror publication
  - reset-time mirror clearing back to the disabled sentinel

## Why this pass now

Document 44 left two practical `local_stats` seams:

- the minimap enemy-sensing filter
- the respawn cooldown

The safer next slice was the enemy-sensing filter because:

- it is presentation-facing rather than gameplay-timing critical
- its production surface is narrow: two writers, one reset path, and one consumer
- it matches the existing status/perception quarantine pattern cleanly

This keeps the `local_stats` cleanup incremental while preserving the exported ABI.

## Scope constraints kept

- no gameplay behavior changes
- no changes to respawn timing or `local_stats.revivetimer`
- no changes to perk evaluation order or script-visible `AddBlipAllEnemies()` behavior
- no changes to team-hate or IDSZ matching semantics in the minimap filter
- no removal or layout change of `local_stats_t`

## Acceptance baseline

This pass is considered healthy when all of the following remain true:

- `GameSessionContext` owns the read-side enemy-sense surface
- `local_stats.sense_enemies_team` and `local_stats.sense_enemies_idsz` are written only from the compatibility bridge in `GameSessionContext.cpp`
- the disabled sentinel remains `Team::TEAM_MAX` plus `IDSZ2::None`
- danger-sense and sense-undead perk updates still reveal the same enemy sets on the minimap
- scripted `AddBlipAllEnemies()` publication still preserves last-writer-wins behavior within the existing frame/update order
- existing local-player status and perception compatibility tests still pass
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

The next practical legacy-global seam is now the respawn cooldown:

- move `local_stats.revivetimer` behind an explicit session-owned respawn state
- preserve current kill-time publication, frame-by-frame countdown, and respawn eligibility timing
- add dedicated tests before changing the ownership boundary because this seam is gameplay-timing sensitive
