# Local-Player Perception Ownership Pass

This document records the local-player perception ownership cleanup completed on 2026-04-16 after the local-player status compatibility quarantine pass.

## What changed

- made `GameSessionContext` the ownership surface for the local-player perception aggregate by adding:
  - `localPlayerPerception()`
  - `publishLocalPlayerPerception(...)`
  - `resetLocalPlayerPerception()`
- added a shared `LocalPlayerPerceptionState` snapshot plus `collectLocalPlayerPerception(...)` to gather:
  - `seeinvis_level`
  - `seeinvis_mag`
  - `seedark_level`
  - `seedark_mag`
  - `seekurse_level`
  - `grog_level`
  - `daze_level`
- moved the legacy `local_stats` write-through for those fields into a file-local compatibility bridge inside `GameSessionContext.cpp`
- updated `MainLoop::updateLocalStats()` to publish both local-player status and local-player perception snapshots through the session seam
- updated active presentation consumers to read the session-owned perception state instead of raw `local_stats` fields:
  - `Graphics/Camera.cpp`
  - `Graphics/ObjectGraphics.cpp`
  - `graphic_lighting.c`
  - `graphic_scene.c`
- updated `game_reset_players()` to clear the session-owned perception state and its compatibility mirrors
- preserved map-editor invisibility reveal behavior by publishing the override through `GameSessionContext` rather than mutating `local_stats` directly
- added focused regression coverage for:
  - alive-player-only perception aggregation
  - exact magnitude derivation
  - compatibility mirror publication
  - reset-time mirror clearing

## Why this pass now

Document 43 quarantined the local-player status mirrors, but the rest of `local_stats` still mixed distinct concerns behind one legacy global.

The safest next slice was the local-player perception aggregate because:

- it already had one centralized production write path in `MainLoop::updateLocalStats()`
- the values are derived from the active local-player list rather than independently owned gameplay state
- the consumers are presentation-oriented reads, not mutation-heavy gameplay logic

This keeps the ownership cleanup incremental while preserving the exported `local_stats` ABI.

## Scope constraints kept

- no gameplay behavior changes
- no changes to respawn timing or `local_stats.revivetimer`
- no changes to minimap enemy-sensing behavior or `sense_enemies_*`
- no changes to player registration, spawn orchestration, or local-player status ownership
- no removal or layout change of `local_stats_t`

## Acceptance baseline

This pass is considered healthy when all of the following remain true:

- `GameSessionContext` owns the read-side local-player perception surface
- migrated perception fields in `local_stats` are written only from the compatibility bridge in `GameSessionContext.cpp`
- local-player perception still averages only alive, non-terminated local players
- `seeinvis_mag` and `seedark_mag` still derive from `exp(0.32f * level)`
- map editor mode still forces invisibility reveal
- camera motion, object tinting, ambient minimum, and curse flashing keep their current behavior
- existing local-player status compatibility tests still pass
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

The next practical legacy-global seams are now:

- the minimap enemy-sensing filter (`sense_enemies_team`, `sense_enemies_idsz`)
- the respawn cooldown (`revivetimer`)

The safer follow-on is the enemy-sensing filter because it remains presentation-facing, while `revivetimer` is more tightly coupled to kill/respawn gameplay timing.
