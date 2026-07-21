# Validator Baseline

Structural content-health baseline for `egoboo-content-validator`
(`tools/egoboo-content-validator.cpp`, `--json` for machine-readable output).
Not a playtest: a module can pass validation and still be broken at runtime,
or fail on stale references while remaining partially playable.

**Read this before treating validator failures as regressions.** The shipped
legacy content set is not internally consistent; the full run exits nonzero by
design. Treat parser crashes, new error categories, or baseline count changes
as suspicious — not the standing 245 legacy errors themselves.

## 1. Scope

The validator checks: module discovery via `ProfileSystem::loadModuleProfiles()`;
required module files (`menu.txt`, `spawn.txt`, `level.mpd`, `wawalite.txt`)
and their parsing; local object enumeration; lightweight
`ObjectProfile::loadFromFile(...)` loading; presence of `data.txt` and one
model candidate (`tris.gltf`/`tris.glb`/`tris.md2`, loadable through
`ObjectModelLoader`); narrow `data.txt` semantic checks (raw `DRES`/`SKIN`/
`LEVL` expansions, skin-override and ammo-vs-max-ammo invariants);
spawn-referenced object resolution against `mp_objects`; and object script
compilation with the `mp_data/script.txt` fallback. JSON mode adds run totals,
per-module rows, categorized events, and per-module
`unresolved_spawn_references` aggregation.

It does **not** check first interactive frame, world-state instantiation,
combat/runtime behavior, rendering, or save/import/export.

Architectural finding from building it: "minimal" validation still requires
VFS, logging, `ImageManager`, `PerkHandler`, and `ProfileSystem` startup —
profile parsing is not isolated from runtime services (see roadmap T3.6).

## 2. Running it

```bash
./build/products/x64/bin/egoboo-content-validator --data-dir "$PWD/data"            # full
./build/products/x64/bin/egoboo-content-validator --data-dir "$PWD/data" --json     # report
./build/products/x64/bin/egoboo-content-validator --data-dir "$PWD/data" --module test.mod  # smoke
```

In sandboxed/read-only-home environments prefix
`HOME=/tmp/egoboo-home XDG_DATA_HOME=/tmp/egoboo-xdg`.

## 3. Current full baseline

Last verified 2026-07-15 (stable since the 2026-06-23 recheck; the original
baseline dates to 2026-04-12 with a spawn-reference repair batch to
`archmage.mod`/`abyss2.mod`/`palwater.mod` applied 2026-04-15):

| Metric | Value |
| --- | ---: |
| Modules validated | 42 |
| Passing modules | 10 |
| Warnings | 10 |
| Errors | 245 |

Passing modules (`errors=0`): `archaeologist.mod`, `imprisoned2–5.mod`,
`palshad.mod`, `palwater.mod`, `rcars.mod`, `test.mod`, `valkyrie.mod`.

A transient alternate classification (25 warnings / 230 errors) has been
observed from the console path in some environments; the JSON baseline above
is the maintained reference.

## 4. Error and warning composition

| Category | JSON id | Count |
| --- | --- | ---: |
| Spawn-referenced object missing from `mp_objects` | `missing_spawn_object` | 229 |
| Object + fallback script compile failure | `script_compile_failure` | 15 |
| Missing required object `data.txt` (`heist.mod` `eyeballguard.obj`) | `missing_required_file` | 1 |
| Missing object script, falls back to `mp_data/script.txt` (warning) | `script_missing` | 10 |

The dominant failure class is content-reference integrity, not parser
breakage: `missing_spawn_object` is ~93.5% of all errors. The validator can
also emit `profile_field_invalid` and `parse_failure`, which do not trigger on
the current shipped set.

Highest-error modules: `archmage.mod` (40), `abyss2.mod` (33), `zippy.mod`
(17), `heist.mod` (15), `bishopiacity.mod` (13), `palash.mod` (13),
`palsand.mod` (12), `rogue.mod` (11), `advent.mod` (10), `soldier.mod` (9).

Most-repeated unresolved spawn names: `unknown.obj` (42), `shutter.obj` (18),
`dark glower.obj` (15), `object.obj` (12), `door.obj` (7) — a mix of typos,
stale renames, never-replaced placeholders, lost overlay aliases, and
malformed names (e.g. bare `.obj`).

## 5. Implications

Before any structured-content migration or scripting replacement:

1. A spawn-reference normalization/reconciliation pass, driven by the
   `unresolved_spawn_references` JSON output.
2. A generated inventory of real object directory names and likely aliases.
3. Per-module triage starting with the highest-error modules above, instead of
   treating the content set as uniformly healthy.
4. Keep the categories distinct: missing-reference bugs versus parser bugs
   versus runtime logic bugs.

Use the JSON report as the source of truth for future baseline refreshes, and
use the validator as a gate before touching legacy formats.
