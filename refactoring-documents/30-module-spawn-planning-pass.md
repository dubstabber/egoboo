# Module Spawn Planning Pass

This document records the spawn-planning extraction completed on 2026-04-16.

## What changed

- extracted the `spawn.txt` planning work from `GameModule::spawnAllObjects()` into a dedicated internal helper
- isolated parse and normalization, static-slot reservation, dynamic-slot reservation, and slot assignment into one planning step
- kept profile activation, object realization, parent tracking, terrain tilt, and deferred AI loading in `GameModule::spawnAllObjects()` unchanged
- added direct tests for the new planning helper and for `activate_spawn_file_load_object()`

## Why this pass now

Document 29 identified the spawn/import realization seam as the next practical extraction target after the `GameModule` translation-unit split and the added boundary smoke coverage.

The safe boundary in that seam was the planning half of `spawnAllObjects()`:

- it already had a clear input: parsed `spawn.txt` entries plus treasure-table resolution
- it already had a clear output: normalized entries with reserved concrete profile slots
- it had lower behavior risk than changing live object realization, attachment flow, or player binding

This pass keeps the side-effectful runtime behavior in place while making the planning logic smaller, testable, and easier to reason about.

## Scope constraints kept

- no gameplay behavior changes
- no attachment or player-binding rewrite
- no spawn-order changes
- no loader-rule changes
- no validator-schema or content-format changes
- no attempt to make dynamic slot assignment more deterministic yet

## Acceptance baseline

This pass is considered healthy when all of the following remain true:

- `GameModule::spawnAllObjects()` still realizes objects in the same order
- dynamic spawn entries still resolve through the same slot-reservation policy
- `activate_spawn_file_load_object()` behavior remains unchanged
- existing module-load smoke coverage still passes
- new planning/helper tests pass

## Follow-on recommendation

The next recommended seam is the live realization half of module spawning:

- `spawnObjectFromFileEntry()`
- attachment handling for inventory and left/right wield slots
- player binding and import-slot matching

That pass should start by adding characterization coverage around missing-parent failures, attachment branches, and local-player bookkeeping before moving behavior.
