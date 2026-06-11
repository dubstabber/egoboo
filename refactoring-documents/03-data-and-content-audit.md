# Data And Content Audit

## 1. The content system is VFS-driven, not path-driven

Egoboo does not simply read files from fixed folders. It constructs a layered virtual filesystem at runtime.

### Base mount points

From `egoboo_setup.c`:

- `data/basicdat` -> `mp_data`
- user `modules` -> `mp_modules`
- data `modules` -> `mp_modules`
- user `players` -> `mp_players`
- user `import` -> `mp_import`
- user `remote` -> `mp_remote`

### Module-specific remapping

When a module loads, the runtime:

- clears module-related mount points
- mounts the selected module's `objects` directory onto `mp_objects`
- mounts many global object category folders onto `mp_objects`
- mounts module `gamedat` onto `mp_data`
- appends `basicdat/globalparticles` onto `mp_data`

### Why this matters

This means:

- the runtime path `mp_data/foo` does not identify a single physical file source
- override precedence is encoded in mount order
- content migration cannot be planned as "convert folders to JSON" without preserving overlay semantics

Any future asset pipeline must explicitly model:

- source precedence
- module overrides
- user data versus shipped data

## 2. Current content inventory

> Note: the counts below are an audit-time snapshot and have drifted slightly as content was added. Re-derive with `find data ...` for exact current numbers.

### Modules and objects

- 42 module directories under `data/modules`
- 968 object directories under `data/`
- 431 global object directories under `data/basicdat/globalobjects`
- 536 module-local object directories

### File type counts

| Type | Count |
| --- | ---: |
| `data.txt` | 946 |
| `script.txt` | 951 |
| `message.txt` | 786 |
| `naming.txt` | 350 |
| `enchant.txt` | 206 |
| `menu.txt` | 43 |
| `spawn.txt` | 43 |
| `passage.txt` | 38 |
| `alliance.txt` | 40 |
| `wawalite.txt` | 43 |
| `level.mpd` | 43 |
| `tris.md2` | 953 |
| `*.txt` total | 6366 |
| `*.wav` | 2808 |
| `*.ogg` | 400 |
| `*.bmp` | 2270 |
| `*.png` | 1899 |

This is large enough that content conversion must be automated and validated, not done by hand.

## 3. Module structure today

A typical module looks like this:

- `gamedat/menu.txt`
- `gamedat/spawn.txt`
- `gamedat/level.mpd`
- `gamedat/wawalite.txt`
- `gamedat/passage.txt`
- `gamedat/alliance.txt`
- tiles, title image, plan/minimap, weather/splash/ripple data
- optional module-local `objects/*.obj`

Example semantics in `menu.txt`:

- module metadata
- reference directory
- unlock IDSZ
- import/export rules
- player count rules
- respawn rules
- summary text
- extra tagged extensions such as `[TYPE]`

## 4. Object structure today

A typical object directory contains:

- `data.txt`
- `script.txt`
- `message.txt`
- `naming.txt`
- `enchant.txt` optional
- `part0.txt` and friends
- `sound0.wav` and friends
- `tris.md2`
- `tris0.bmp` or `tris0.png`
- `icon0.bmp`

The runtime expects many of these files by convention rather than explicit manifest.

## 5. The file formats are not strongly schema-driven

Most text formats are positional and parser-driven rather than schema-driven.

### Consequences

- field meaning depends on order
- comments and formatting are partly significant
- documentation is split across code and old manuals
- format validation is weak compared to modern structured formats
- safe editing by tooling is difficult

### Examples

#### `menu.txt`

- parsed sequentially by `ModuleProfile::loadFromFile()`
- summary uses eight fixed lines
- extensions are read later as optional tagged suffixes

#### `spawn.txt`

- parsed line-by-line by `SpawnFileReaderImpl`
- still encodes legacy concepts such as slots, attachments, and a now-unused "ghost" value
- supports `#dependency` entries
- supports treasure-table references using `%`

#### `data.txt`

- parsed into `ObjectProfile`
- contains core stats, flags, IDSZ values, collision info, graphics flags, item behavior, XP tables, sound IDs, and tagged expansions

#### `script.txt`

- custom EgoScript / AI scripting language
- indentation-sensitive
- line-oriented
- compiled by the in-tree script compiler

#### `level.mpd`

- binary map format
- loader supports versions 1 through 4

#### `tris.md2`

- Quake-era MD2 model format, still central to the object pipeline

## 6. Hardcoded naming conventions are part of the runtime contract

The loader relies on fixed file naming patterns:

- `sound0` to `sound29`
- `part0.txt` to `part29.txt`
- `icon0` to `icon29`
- `tris0` to `tris29`

This matters because a JSON migration alone does not remove the convention debt unless the runtime is also changed.

## 7. Script system status

The current scripting path is still active and core to gameplay.

### What is present

- custom compiler in `game/script_compile.c`
- runtime function layer, split across fourteen TUs: `game/script_functions_{action,alerts,appearance,bitwise,combat,commerce,enchant,movement,quests,spawn,state,stat_gifts,target,target_select}.c`
- additional script support under `Script/`
- legacy docs in `data/doc/AiDocs.txt`

### Operational characteristics

- each object has a `script.txt`
- scripts are indentation-sensitive
- tabs are considered invalid
- script errors are logged
- old docs say scripts run 50 times per second
- loader can fall back to `mp_data/script.txt` if an object script fails to load

### Refactor implication

Do not replace EgoScript first. First capture:

- its execution model
- the object API it expects
- its control-flow semantics
- its save/load and timing assumptions

Otherwise a Lua migration will become a behavior rewrite, not just a scripting migration.

## 8. Existing migration attempts already in the repo

This repository has already tried to escape the current content model several times.

### Attempt A: `doc/ego2xml/` (now archived to `doc/legacy/ego2xml/`)

- old XML-based proposals and example schemas
- proposal documents date back to 2015
- evidence that structured content has been a known need for years

### Attempt B: `utilities/migrator/`

The tool executable can register:

- `ConvertPaletted`
- `DataMigrator`
- `EnchantMigrator`
- `EnvironmentMigrator`
- `ScriptMigrator`

But current implementation quality is uneven:

- `DataMigrator::run(path)` is empty
- `EnchantMigrator::run(path)` is empty
- `EnvironmentMigrator::run(path)` is empty
- `ScriptMigrator` parses files and writes a `...txt2` output, but is not integrated into the main workflow
- `utilities/migrator/README.md` is stale and currently documents a different tool entirely

### Attempt C: `game/Lua/` (since-removed Lua/SWIG experiment)

- was a Lua/SWIG scripting experiment containing a SWIG interface file and helper scripts
- the `game/Lua/` directory has since been deleted from the active tree; it now survives only under `backup-copy/` (`backup-copy/egolib/library/src/egolib/game/Lua`)
- was never integrated into the build graph, and included headers that were already out of date relative to the source tree

### Conclusion

The project already has evidence of repeated migration intent but no completed migration path. That is useful history: the need is real, but previous attempts did not land.

## 9. Content documentation already exists, but is fragmented and aged

Examples:

- `data/doc/AiDocs.txt`
- `data/doc/version-1.0/*`
- `doc/legacy/ego2xml/*` (archived 2015 XML proposal)
- PDFs and ODT manuals under `doc/` and `data/doc/`

The issue is not "no documentation exists". The issue is:

- too much of it is old
- it is not tied to current code
- it is spread across formats and eras

## 10. Recommended migration model

### Stage 1: define canonical schemas and IR

Create machine-readable schemas for:

- module metadata
- module environment
- spawn lists
- object definitions
- particle definitions
- message tables
- naming tables

### Stage 2: build importers and exporters before replacing runtime loaders

- importer: legacy text/binary -> canonical IR
- validator: IR -> warnings/errors
- exporter: IR -> structured representation such as JSON

### Stage 3: preserve old content behavior through adapters

The first modern runtime should load the new IR, not necessarily raw JSON directly.

### Stage 4: dual-load and compare

For representative modules:

- load legacy content
- load migrated content
- compare key runtime outputs

### Stage 5: retire legacy parsers only after parity

Do not delete `menu.txt`, `data.txt`, or `spawn.txt` parsing before parity tooling exists.

## 11. Specific design guidance for a JSON migration

If JSON is chosen, it should not simply mirror the old positional files one-to-one.

The modern model should include:

- stable object/module IDs instead of slot-number-as-identity
- explicit manifests instead of file enumeration by suffix
- explicit inheritance or composition instead of hidden "copy" semantics
- declarative references instead of mount-order tricks where possible
- preserved metadata for round-trip or legacy debugging

## 12. Bottom line

The content system is not just old. It is old, implicit, layered, and deeply embedded in runtime assumptions.

The safe migration strategy is:

1. document
2. inventory
3. formalize
4. validate
5. dual-load
6. only then replace
