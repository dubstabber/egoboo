# Validator Baseline

This document records the first non-UI content validation pass added during the refactor audit on 2026-04-12, the first report-mode update added on 2026-04-13, and the 2026-04-15 spawn-reconciliation remediation refresh.

It is not a playtest. It is a structural baseline for module/content health.

## 1. What was added

The repository now contains a lightweight validation tool:

- target: `egoboo-content-validator`
- source: `tools/egoboo-content-validator.cpp`
- machine-readable report mode: `--json`

It is wired into the root build through `tools/CMakeLists.txt` and the root `CMakeLists.txt`.

Related Linux build/run documentation was also added in:

- `doc/build-linux.md`

Related compatibility specs now exist in:

- `08-spawn-format-spec.md`
- `09-data-format-spec.md`

## 2. Validator scope

The validator currently checks:

- module discovery through `ProfileSystem::loadModuleProfiles()`
- required module files:
  - `mp_data/menu.txt`
  - `mp_data/spawn.txt`
  - `mp_data/level.mpd`
  - `mp_data/wawalite.txt`
- `level.mpd` parsing
- `wawalite.txt` parsing
- `spawn.txt` parsing
- local object enumeration under module `objects/`
- object profile loading through lightweight `ObjectProfile::loadFromFile(...)`
- presence of `data.txt` and `tris.md2`
- narrow semantic `data.txt` validation for raw `DRES`, `SKIN`, and `LEVL` tagged expansions plus loaded skin-override and ammo-vs-max-ammo invariants
- spawn-referenced object resolution against `mp_objects`
- object script compilation or fallback to `mp_data/script.txt`

When `--json` is enabled, the validator also emits:

- run summary totals
- per-module result rows
- categorized warning and error events
- aggregated unresolved spawn-object names per module

It does not yet check:

- first interactive frame
- actual object instantiation in world state
- combat/runtime behavior
- rendering correctness
- save/import/export

## 3. Runtime coupling discovered while building the validator

The validator was meant to be minimal, but it still had to initialize more runtime state than expected.

The current minimal startup path now requires:

- VFS initialization and base/module mount setup
- logging
- `ImageManager`
- `PerkHandler`
- `ProfileSystem`

That is an architectural finding, not just a tooling detail.

Why it matters:

- object-profile parsing is not isolated from gameplay/support singletons
- “content validation” is currently entangled with runtime service initialization
- future refactor work should separate pure data parsing from runtime service access

Concrete examples found during implementation:

- `ObjectProfile::loadDataFile()` touches `PerkHandler`
- texture existence checks depend on `ImageManager`
- lightweight profile loading still depends on config state for Gouraud shading behavior

## 4. Environment notes

### Standard source-tree use

The intended command is:

```bash
./build/products/x64/bin/egoboo-content-validator --data-dir "$PWD/data"
```

Machine-readable output:

```bash
./build/products/x64/bin/egoboo-content-validator --data-dir "$PWD/data" --json
```

### Sandboxed or CI-like environments

In this audit environment, SDL preference paths under the real home directory were read-only.

For sandboxed runs, the validator needed redirected user-data paths:

```bash
HOME=/tmp/egoboo-home \
XDG_DATA_HOME=/tmp/egoboo-xdg \
./build/products/x64/bin/egoboo-content-validator --data-dir "$PWD/data"
```

This is an execution-environment note, not a gameplay bug.

## 5. Latest full baseline results

**Last verified:** 2026-06-08. Regenerate these numbers with:

```bash
HOME=/tmp/egoboo-home \
XDG_DATA_HOME=/tmp/egoboo-xdg \
./build/products/x64/bin/egoboo-content-validator --data-dir "$PWD/data"
```

Full run command used for the original console baseline:

```bash
HOME=/tmp/egoboo-home \
XDG_DATA_HOME=/tmp/egoboo-xdg \
./build/products/x64/bin/egoboo-content-validator --data-dir "$PWD/data"
```

Full run command used for the 2026-04-13 report-mode refresh:

```bash
HOME=/tmp/egoboo-home \
XDG_DATA_HOME=/tmp/egoboo-xdg \
./build/products/x64/bin/egoboo-content-validator --data-dir "$PWD/data" --json
```

The 2026-04-13 JSON refresh produced the same top-level totals as the original console baseline.

A follow-up 2026-04-13 validator pass added narrow `data.txt` semantic checks for raw `DRES`, `SKIN`, and `LEVL` tagged expansions plus loaded skin-override and ammo-vs-max-ammo invariants. That pass also produced the same top-level totals as the original baseline, meaning those new warnings did not trigger on the shipped content set.

A later 2026-04-15 remediation pass extended reconciliation JSON output to schema version `3` and applied a conservative spawn-reference repair batch to `archmage.mod`, `abyss2.mod`, and `palwater.mod`. That reduced the global error count without changing parser behavior or warning totals.

Summary:

| Metric | Value |
| --- | ---: |
| Modules validated | 42 |
| Passing modules | 10 |
| Failing modules | 32 |
| Validator warnings | 10 |
| Validator errors | 245 |

Passing modules (the 10 modules whose run row shows `errors=0`):

- `archaeologist.mod`
- `imprisoned2.mod`
- `imprisoned3.mod`
- `imprisoned4.mod`
- `imprisoned5.mod`
- `palshad.mod`
- `palwater.mod`
- `rcars.mod`
- `test.mod`
- `valkyrie.mod`

`palshad.mod` and `palwater.mod` now pass (`palwater.mod` was repaired by the 2026-04-15 spawn-reference batch and reached `errors=0`). `spiderlair.mod` dropped out of the passing set: it now reports 1 error (its `throne.obj` script fails to compile — see the `script_compile_failure` category below).

## 6. Error composition

The error distribution is highly concentrated.

| Error category | JSON id | Count |
| --- | --- | ---: |
| spawn-referenced object missing from `mp_objects` | `missing_spawn_object` | 229 |
| object script failed to compile (incl. fallback) | `script_compile_failure` | 15 |
| missing required object data file | `missing_required_file` | 1 |
| **Total** | | **245** |

The one `missing_required_file` error was:

- `heist.mod`: `mp_objects/eyeballguard.obj/data.txt` missing

`script_compile_failure` (15) is now emitted as an **error** rather than a warning; the failing-to-compile object scripts are no longer demoted to a soft signal. This is why one previously-passing module (`spiderlair.mod`) now fails on a single such error.

The dominant failure class is still not parser breakage. It is content-reference integrity: `missing_spawn_object` alone accounts for 229 of 245 errors (~93%).

## 7. Warning composition

Current warning counts:

| Warning category | JSON id | Count |
| --- | --- | ---: |
| missing object script with fallback to `mp_data/script.txt` | `script_missing` | 10 |
| **Total** | | **10** |

`script_missing` is the only warning category that fires on the shipped content set. The "object script fallback after compile/load failure" condition that previously contributed 15 warnings is now reported as the `script_compile_failure` **error** category (see Section 6), so it no longer counts toward warnings.

These are useful signals, but they are secondary compared to the missing spawn references.

Stable report categories in the current JSON output:

- errors:
  - `missing_spawn_object`
  - `script_compile_failure`
  - `missing_required_file`
- warnings:
  - `script_missing`

The validator can now also emit `profile_field_invalid` warnings for a small set of post-load `data.txt` invariants, but the current shipped baseline did not surface any instances.

## 8. Highest-error modules

Top failing modules from the 2026-06-08 refresh:

| Module | Errors | Warnings | Spawn Entries |
| --- | ---: | ---: | ---: |
| `archmage.mod` | 40 | 0 | 164 |
| `abyss2.mod` | 33 | 0 | 333 |
| `zippy.mod` | 17 | 0 | 164 |
| `heist.mod` | 15 | 0 | 112 |
| `palash.mod` | 13 | 0 | 154 |
| `bishopiacity.mod` | 13 | 1 | 403 |
| `palsand.mod` | 12 | 0 | 105 |
| `rogue.mod` | 11 | 0 | 135 |
| `advent.mod` | 10 | 1 | 92 |
| `soldier.mod` | 9 | 0 | 110 |

`palwater.mod` is no longer in this table: it now passes with `errors=0` after the 2026-04-15 spawn-reference repair batch.

Per-module error and warning counts above are aggregated from the validator's detailed `error [module]` / `warning [module]` event lines (these sum exactly to the 245/10 run totals). The current validator build only prints an `[ok]`/`[fail]` summary row — and therefore a live `spawn_entries` figure — for 21 of the 42 modules; `archmage.mod`, `zippy.mod`, and `advent.mod` are not among them, so their `Spawn Entries` values here are carried over from the prior baseline (their `spawn.txt` content is unchanged) and are not re-confirmed by the 2026-06-08 run.

## 9. Most common unresolved spawn object names

The most repeated unresolved object names (2026-06-08 run) were:

| Object name | Count |
| --- | ---: |
| `unknown.obj` | 42 |
| `shutter.obj` | 18 |
| `dark glower.obj` | 15 |
| `object.obj` | 12 |
| `door.obj` | 7 |
| `randommagic.obj` | 6 |
| `guard.obj` | 6 |
| `.obj` | 5 |
| `blacklance.obj` | 5 |
| `treasurechest.obj` | 5 |
| `tbunny.obj` | 5 |
| `ulna2.obj` | 4 |
| `tbutton.obj` | 4 |
| `magiccage.obj` | 4 |
| `button.obj` | 4 |

Interpretation:

- some missing names are probably typos or stale renames
- some are placeholder values that were never replaced
- some likely depend on historical aliasing or content overlays that no longer exist
- at least a few contain malformed or empty names, e.g. `.obj`

The validator now emits these counts directly through the `unresolved_spawn_references` JSON array with per-module aggregation.

## 10. Immediate refactor implications

The validator results change the order of operations for the larger refactor.

Before a JSON migration or scripting-language replacement, the project needs:

1. A content-reference normalization pass for `spawn.txt` object names.
2. A generated inventory of real object directory names and likely aliases.
3. A per-module triage list for high-error modules instead of treating the whole content set as equally healthy.
4. Clear separation between:
   - missing asset/reference bugs
   - parser bugs
   - runtime logic bugs

The first pass strongly suggests that content integrity debt is larger than parser debt.

## 11. Suggested next steps from this baseline

1. Use the validator JSON report as the source of truth for future baseline refreshes.
2. Build an object-name reconciliation workflow on top of the aggregated unresolved-spawn output:
   - spawn name
   - module
   - resolved virtual path
   - repeated occurrence count
3. Use the validator as a gate before touching legacy formats.
4. Start triage with the worst modules:
   - `archmage.mod`
   - `abyss2.mod`
   - `zippy.mod`
   - `heist.mod`
   - `palash.mod`

## 12. Important limitation

This baseline still does not replace playtesting.

A module can pass the validator and still be broken at runtime.
A module can also fail here because of stale content references while remaining partially playable.

Use this document as a structural refactor baseline, not as a final quality judgment.
