# Validator Baseline

Structural content-health baseline for `egoboo-content-validator`
(`tools/egoboo-content-validator.cpp`, `--json` for machine-readable output).
Not a playtest: a module can pass validation and still be broken at runtime,
or fail on stale references while remaining partially playable.

**Read this before treating validator failures as regressions.** The shipped
legacy content set is not internally consistent; the full run exits nonzero by
design. Treat parser crashes, new error categories, or baseline count changes
as suspicious — not the standing 230 legacy errors themselves.

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

Last verified 2026-08-06 (the original baseline dates to 2026-04-12 with a
spawn-reference repair batch to `archmage.mod`/`abyss2.mod`/`palwater.mod`
applied 2026-04-15):

| Metric | Value |
| --- | ---: |
| Modules validated | 42 |
| Passing modules | 11 |
| Warnings | 20 |
| Errors | 230 |

Category breakdown (`--json`), which is the thing to compare rather than the
totals:

| Category | Count | Severity |
| --- | ---: | --- |
| `missing_spawn_object` | 229 | error |
| `missing_required_file` | 1 | error |
| `script_fallback` | 10 | warning |
| `script_missing` | 10 | warning |

**Measure this on a clean user directory.** See §3.1 — the baseline is
sensitive to leftover files in `EGOBOO_USER_DIR`, which shadow `mp_data`.

### Baseline change 2026-07-29: 245 -> 240

Five errors were removed, in two content fixes plus an engine change.

**Two** were the same defect: `wizard.obj/script.txt` and
`archwizard.obj/script.txt` wrote a five-character IDSZ literal, `[STAFF]`. An
IDSZ is exactly four characters, so the lexer threw `invalid IDSZ` and the
engine silently substituted the default do-nothing script, leaving both wizard
classes with no working script at all. Fixed to `[STAF]`, the Parent ID that
`qstaff`/`mstaff`/`blackstaff` already declare. These were the only two
malformed IDSZ literals in the content tree. The same pass made
`load_ai_script_vfs0` log compilation errors at WARNING instead of letting them
masquerade as "unable to load script file", so a recurrence is visible.

**Three more** were a second instance of the same class: `marcus.obj` and
`archghost.obj` (both live content in `bishopiacity.mod`) and `santa.obj` called
`AddTargetQuest`, which is not an opcode. The intended function is `AddQuest`,
whose `tmpargument` = quest IDSZ / `tmpdistance` = level signature every call
site already set up. No script in the content tree fails to compile any more.

Compilation is now transactional: it builds into a local script and publishes it
only after `parse_jumps` has run, so a partially compiled stream with unresolved
fail-jumps can never reach a caller, and a present-but-broken script is reported
at WARNING rather than sharing the "unable to load" wording used for an absent one.

Passing modules (`errors=0`) as of the 2026-08-06 correction, 11 of them:
`archaeologist.mod`, `imprisoned2–5.mod`, `palshad.mod`, `palwater.mod`,
`rcars.mod`, `spiderlair.mod`, `test.mod`, `valkyrie.mod`. `spiderlair.mod`
joined the list in that correction — its former errors were the leaked-script
artifact, not content faults.

### 3.1 Baseline correction 2026-08-06: 42/10/240 -> 42/20/230

The previously documented 10 warnings / 240 errors was **not a property of the
content**. It was a property of the measuring machine.

`ScriptLoader.cpp` had two tests that deny the default-script fallback by
writing a deliberately uncompilable five-byte `mp_data/script.txt`, and neither
removed it. `vfs_writeEntireFile` writes to the PhysFS write directory, which is
the user directory, and `setup_init_base_vfs_paths` mounts the user directory
*ahead* of the data directory — so the corrupt file shadowed the real default AI
script at `data/basicdat/mp_data/script.txt` for every later run against that
user directory, including runs of `egoboo-content-validator`.

The effect is a clean ten-item swap. Ten objects fail to compile their own
script and fall back to the default. With a valid default that is ten
`script_fallback` **warnings**; with the corrupt one it is ten
`script_compile_failure` **errors**. Hence 20/230 becomes 10/240.

Under `ctest` the leak is contained, because each test process gets its own
per-PID `EGOBOO_USER_DIR` that is removed afterwards. It escapes when
`egolib-tests-executable` is run directly — an ordinary thing to do while
debugging one case — and then persists indefinitely.

This is also the explanation for the note this section used to carry, that "a
transient alternate classification (25 warnings / 230 errors) has been observed
from the console path in some environments". It was neither transient nor
environmental: it was whether the machine had a leaked `script.txt`.

The leak is fixed (`ScopedUncompilableDefaultScript` in `ScriptLoader.cpp`
installs and removes it as a scoped resource). **If you measure 42/10/240, your
user directory predates that fix.** Check for and delete
`<EGOBOO_USER_DIR>/mp_data/script.txt`; the default is
`.egoboo-runtime/user/mp_data/script.txt`.

Lesson worth keeping: a test that writes into the user directory can silently
move a project-wide measurement, because the user directory shadows `mp_data`.
Treat any test write outside a test-owned subdirectory as a scoped resource.

## 4. Error and warning composition

| Category | JSON id | Count | Severity |
| --- | --- | ---: | --- |
| Spawn-referenced object missing from `mp_objects` | `missing_spawn_object` | 229 | error |
| Missing required object `data.txt` (`heist.mod` `eyeballguard.obj`) | `missing_required_file` | 1 | error |
| Object script present but not compilable, falls back | `script_fallback` | 10 | warning |
| Missing object script, falls back to `mp_data/script.txt` | `script_missing` | 10 | warning |

`script_compile_failure` no longer appears. It was the category the ten
`script_fallback` items were misreported as while a corrupt `mp_data/script.txt`
shadowed the real default script (§3.1).

The dominant failure class is content-reference integrity, not parser
breakage: `missing_spawn_object` is ~93.5% of all errors. The validator can
also emit `profile_field_invalid` and `parse_failure`, which do not trigger on
the current shipped set.

Highest-error modules: `archmage.mod` (39), `abyss2.mod` (33), `zippy.mod`
(16), `heist.mod` (15), `palash.mod` (13), `palsand.mod` (12), `rogue.mod`
(11), `bishopiacity.mod` (10), `advent.mod` (9), `soldier.mod` (9).

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
