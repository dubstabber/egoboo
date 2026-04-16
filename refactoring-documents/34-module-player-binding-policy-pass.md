# Module Player-Binding Policy Pass

This document records the player-binding policy cleanup completed on 2026-04-16 after the spawn-realization extraction pass.

## What changed

- kept `module_spawn_realization::realizeSpawnEntry()` as the live spawn helper for object creation, attachment handling, XP adjustment, and startup-equipment identification
- isolated the player-binding decision into a narrower internal policy step that decides whether a spawned stat entry should:
  - bind to the next local device slot in non-import runs
  - bind to an imported local player number in import-backed runs
  - skip binding entirely
- kept the existing `ops.addPlayer(...)` plumbing in `GameModule::spawnObjectFromFileEntry()` unchanged
- preserved the current no-op behavior when import-slot matching fails or reaches the legacy remote-input branch
- extended characterization coverage for:
  - non-stat spawns skipping player binding
  - zero-import player binding only identifying the spawn when `addPlayer(...)` succeeds
  - import-profile bounds/mismatch cases returning normally without binding a player

## Why this pass now

Document 31 narrowed live spawn realization into a testable helper, but the last policy-heavy branch still mixed two concerns:

- deciding whether the spawned object should become a local player
- applying that decision through the existing `addPlayer(...)` runtime wiring

That decision logic was smaller and lower risk than changing attachment semantics, startup-equipment handling, or parent tracking, so it was the next safe seam to isolate.

## Scope constraints kept

- no gameplay behavior changes
- no spawn-order changes
- no attachment or parent-threading changes
- no change to startup-equipment identification for child items spawned on local players
- no change to `spawnAllObjects()` orchestration
- no new public API for module spawn realization

## Acceptance baseline

This pass is considered healthy when all of the following remain true:

- existing spawn-realization characterization tests still pass
- the new stat-gating and zero-import add-player outcome tests pass
- module smoke coverage around reserved import slots and concrete spawn resolution still passes
- `test.mod` still validates with `egoboo-content-validator`

## Follow-on recommendation

The next practical seam is to decide whether startup-equipment identification should stay coupled to live spawn realization or move behind a smaller player-startup hook.

That follow-up should keep attachment behavior and `spawnAllObjects()` ordering fixed while isolating only the player-startup side effects.
