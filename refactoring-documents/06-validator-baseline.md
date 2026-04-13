# Validator Baseline

This document records the first non-UI content validation pass added during the refactor audit on 2026-04-12 and the first report-mode update added on 2026-04-13.

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

The JSON refresh produced the same top-level totals as the original console baseline.

A follow-up 2026-04-13 validator pass added narrow post-load `data.txt` semantic checks for saved-character skin overrides, ammo-vs-max-ammo, and required particle-profile hooks. That pass also produced the same top-level totals as the original baseline, meaning those new warnings did not trigger on the shipped content set.

Summary:

| Metric | Value |
| --- | ---: |
| Modules validated | 42 |
| Passing modules | 9 |
| Failing modules | 33 |
| Validator warnings | 25 |
| Validator errors | 278 |

Passing modules:

- `archaeologist.mod`
- `imprisoned2.mod`
- `imprisoned3.mod`
- `imprisoned4.mod`
- `imprisoned5.mod`
- `rcars.mod`
- `spiderlair.mod`
- `test.mod`
- `valkyrie.mod`

## 6. Error composition

The error distribution is highly concentrated.

| Error category | Count |
| --- | ---: |
| spawn-referenced object missing from `mp_objects` | 277 |
| other hard validator errors | 1 |

The one non-spawn hard error was:

- `heist.mod`: `mp_objects/eyeballguard.obj/data.txt` missing

This means the dominant failure class is not parser breakage. It is content-reference integrity.

## 7. Warning composition

Current warning counts:

| Warning category | Count |
| --- | ---: |
| missing object script with fallback to `mp_data/script.txt` | 10 |
| object script fallback after compile/load failure | 15 |

These are useful signals, but they are secondary compared to the missing spawn references.

Stable report categories in the current JSON output:

- errors:
  - `missing_spawn_object`
  - `missing_required_file`
- warnings:
  - `script_missing`
  - `script_fallback`

The validator can now also emit `profile_field_invalid` warnings for a small set of post-load `data.txt` invariants, but the current shipped baseline did not surface any instances.

## 8. Highest-error modules

Top failing modules from the first baseline:

| Module | Errors | Warnings | Spawn Entries |
| --- | ---: | ---: | ---: |
| `archmage.mod` | 56 | 1 | 164 |
| `abyss2.mod` | 35 | 0 | 333 |
| `palwater.mod` | 18 | 0 | 123 |
| `zippy.mod` | 16 | 1 | 164 |
| `heist.mod` | 15 | 0 | 112 |
| `palash.mod` | 13 | 0 | 154 |
| `rogue.mod` | 13 | 0 | 135 |
| `palsand.mod` | 12 | 0 | 105 |
| `bishopiacity.mod` | 10 | 4 | 403 |
| `advent.mod` | 9 | 2 | 92 |
| `palshad.mod` | 9 | 0 | 133 |
| `soldier.mod` | 9 | 0 | 110 |

## 9. Most common unresolved spawn object names

The most repeated unresolved object names were:

| Object name | Count |
| --- | ---: |
| `unknown.obj` | 42 |
| `shutter.obj` | 25 |
| `dark glower.obj` | 15 |
| `object.obj` | 12 |
| `door.obj` | 7 |
| `randommagic.obj` | 6 |
| `guard.obj` | 6 |
| `.obj` | 5 |
| `blacklance.obj` | 5 |
| `gaschest.obj` | 5 |
| `treasurechest.obj` | 5 |
| `darkie.obj` | 5 |
| `tbunny.obj` | 5 |

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
   - `palwater.mod`
   - `zippy.mod`
   - `heist.mod`

## 12. Important limitation

This baseline still does not replace playtesting.

A module can pass the validator and still be broken at runtime.
A module can also fail here because of stale content references while remaining partially playable.

Use this document as a structural refactor baseline, not as a final quality judgment.
