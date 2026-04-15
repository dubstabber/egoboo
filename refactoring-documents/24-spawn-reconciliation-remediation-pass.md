# Spawn Reconciliation Remediation Pass

This document records the 2026-04-15 follow-up pass after the earlier spawn reconciliation tooling work.

The earlier pass established reusable spawn-name normalization and validator-side reconciliation reporting. This pass finishes the remediation-facing report shape and uses it for a small, conservative content repair batch.

## What changed

### Validator report

`egoboo-content-validator --json --emit-reconciliation` now emits schema version `3`.

Each reconciliation row now includes:

- `placeholder_like`
- `suggested_matches`

Each `suggested_matches` row includes:

- `object_name`
- `resolved_virtual_path`
- `source_kind`
- `origin_path`
- `match_reason`
- `score`

The ranking policy is intentionally deterministic:

- exact canonical-key matches first
- then prefix, suffix, and close contains-style matches
- then edit-distance matches up to distance `3`
- module-local objects rank above shipped globals
- non-`work_in_progress` roots rank above `work_in_progress`

This is still a tooling aid, not an auto-fix system. Ambiguous rows are left for manual triage.

### Tests

`SpawnName` tests now also cover:

- placeholder-like token detection
- exact-match ordering over broader partial matches
- module-local preference within the same match class
- a truncation case (`chimepuzl` -> `chimepuzzle`)

## Conservative repair batch

This pass only touched rows with strong local evidence:

- `archmage.mod`
  - `chime1` .. `chime4` -> `chime`
  - `chimepuzl` -> `chimepuzzle`
  - black-side chess pieces now use `blacktower`, `blackbishop`, `blackqueen`, `blackking`
  - white-side chess pieces now use `whitetower`, `whitebishop`, `whitequeen`
  - `Viking` -> `vikingchallenge`
- `abyss2.mod`
  - `betrayer` -> `thebetrayer`
  - `arrowtrap2` -> `arrowtrap`
- `palwater.mod`
  - `Shutter` -> `shutdoor`
  - `Trigger` -> `trigdoor`

The pass deliberately did **not** rewrite placeholder-like or weakly matched rows such as:

- `unknown`
- `Object` / `object`
- `maintainer`
- `treasurechest`
- `FemFaerie`
- `Wierd`
- `tbutton`
- several high-error `abyss2.mod` boss/object names

Those remain manual follow-up items because the best suggestion was either absent or not strong enough to justify changing shipped content blindly.

## Validation results

Targeted module deltas from the new reconciliation-guided batch:

- `archmage.mod`: `56 -> 39` errors
- `abyss2.mod`: `35 -> 33` errors
- `palwater.mod`: `18 -> 9` errors
- `zippy.mod`: unchanged at `16` errors
- `heist.mod`: unchanged at `15` errors

Full validator refresh after the batch:

- modules validated: `42`
- passing modules: `9`
- failing modules: `33`
- warnings: `25`
- errors: `250`
- `missing_spawn_object`: `249`

Compared with the 2026-04-13 baseline, this pass reduced:

- total errors: `278 -> 250`
- missing spawn-object errors: `277 -> 249`

## Remaining follow-up

The next content-integrity work should stay focused on rows where the new report still points at unresolved ambiguity:

1. review placeholder-like rows separately from typo/alias rows
2. triage `zippy.mod` and `heist.mod` by inspecting intended object semantics, not just string similarity
3. revisit `abyss2.mod` boss/object names with local content knowledge before renaming more spawn rows
4. consider teaching the suggestion ranking about local abbreviations only if a clear, low-risk rule emerges from repeated data
