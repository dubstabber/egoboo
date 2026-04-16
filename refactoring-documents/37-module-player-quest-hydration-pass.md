# Module Player Quest Hydration Pass

This document records the shared quest-hydration boundary cleanup completed on 2026-04-16 after the module player-startup boundary pass.

## What changed

- added a shared `Ego::loadPlayerQuestLog(...)` helper in `game/Logic` for best-effort player quest hydration from a profile directory
- updated `module_player_startup::finalizeLocalPlayerStartup()` to use that helper instead of calling `QuestLog::loadFromFile(...)` directly
- updated `LoadPlayerElement` to use the same helper for menu-time quest gating
- kept `Ego::QuestLog::loadFromFile(...)` unchanged as the parser and persistence owner
- added direct helper tests plus characterization coverage for:
  - module startup loading quest progress from an isolated copied profile
  - menu-time player loading preserving silent missing-`quest.txt` behavior

## Why this pass now

Document 36 isolated the module-side player-startup sink, but one content concern still crossed the boundary awkwardly:

- local-player startup still knew how to hydrate quest state from `quest.txt`
- menu-time player loading duplicated that same best-effort load step

The safe next step was to move that shared content concern behind one narrower helper while keeping spawn policy, player registration, and session bookkeeping unchanged.

## Scope constraints kept

- no gameplay behavior changes
- no changes to `spawnAllObjects()` orchestration
- no changes to player-binding policy, import-slot matching, or `game_load_profile_ai()` ordering
- no changes to `QuestLog` parsing rules, file format, or export behavior
- no new logging or error reporting for missing `quest.txt`
- no broader player-profile/session service redesign

## Acceptance baseline

This pass is considered healthy when all of the following remain true:

- existing spawn-realization and module smoke coverage still passes
- the shared quest-hydration helper tests pass
- module player-startup tests still cover null-object rejection and silent missing-`quest.txt` behavior
- `LoadPlayerElement` still handles missing `quest.txt` without surfacing a new failure path
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

The next practical seam is the remaining local-player bookkeeping ownership inside `module_player_startup`:

- decide whether `local_stats` mutation and `islocalplayer` stamping should remain module-startup details or move behind a narrower player/session service
- keep the shared quest-hydration helper and silent missing-`quest.txt` behavior unchanged until that broader ownership boundary is explicitly redesigned
