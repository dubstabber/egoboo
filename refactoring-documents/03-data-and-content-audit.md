# Data And Content Audit

Last refreshed: 2026-07-21. Format contracts live in `08-spawn-format-spec.md`
and `09-data-format-spec.md`; validator results in `06-validator-baseline.md`.

## 1. The content system is VFS-driven, not path-driven

The runtime constructs a layered virtual filesystem. Base mounts (from
`egoboo_setup.c`): `data/basicdat` → `mp_data`, user+data `modules` →
`mp_modules`, user `players`/`import`/`remote` → `mp_players`/`mp_import`/
`mp_remote`. When a module loads, module-related mounts are cleared and
rebuilt: the module's `objects` directory plus many global object category
folders onto `mp_objects`, module `gamedat` onto `mp_data`, then
`basicdat/globalparticles` appended to `mp_data`.

Consequences: a runtime path like `mp_data/foo` does not identify one physical
file; override precedence is encoded in mount order; any future asset pipeline
must explicitly model source precedence, module overrides, and user-versus-
shipped data. Content migration cannot be planned as "convert folders to JSON"
without preserving overlay semantics.

## 2. Content inventory

Audit-time snapshot (2026-04); re-derive with `find data ...` for exact
numbers: 42 modules, ~968 object directories (431 global, ~536 module-local),
~6,400 `.txt` files, ~950 `tris.md2` models, thousands of `.wav`/`.bmp`/`.png`
assets. Large enough that content conversion must be automated and validated,
never done by hand.

## 3. Module and object structure

A module: `gamedat/` with `menu.txt`, `spawn.txt`, `level.mpd`,
`wawalite.txt`, `passage.txt`, `alliance.txt`, plus tiles/title/plan images
and optional module-local `objects/*.obj`.

An object directory: `data.txt`, `script.txt`, `message.txt`, `naming.txt`,
optional `enchant.txt`, `part0.txt`… , `sound0.wav`… , one model candidate
(`tris.gltf`/`tris.glb`/`tris.md2`), `tris0.bmp|png`, `icon0.bmp`. Most files
are expected by convention, not by manifest.

## 4. Format characteristics

Most text formats are positional and parser-driven: field meaning depends on
order, comments/formatting are partly significant, validation is weak, and
safe tool-based editing is difficult.

- `menu.txt` — parsed sequentially by `ModuleProfile::loadFromFile()`;
  eight fixed summary lines; optional tagged `[TYPE]`-style extensions.
- `spawn.txt` — line-based `SpawnFileReaderImpl`; legacy slots, attachments,
  an unused "ghost" field, `#dependency` entries, `%` treasure-table refs.
  Contract: `08-spawn-format-spec.md`.
- `data.txt` — positional `ObjectProfile` payload plus tagged expansions.
  Contract: `09-data-format-spec.md`.
- `script.txt` — EgoScript; indentation-sensitive, line-oriented, compiled by
  the in-tree compiler.
- `level.mpd` — binary map format, loader supports versions 1–4.

## 5. Object model loading

Discovery prefers `tris.gltf`, then `tris.glb`, then `tris.md2`. A preferred
glTF/GLB asset is authoritative: if it exists and fails to load, the loader
reports it instead of silently falling back to MD2. Runtime and validator
discovery both go through `ObjectModelAsset` and `ObjectModelLoader`. The
runtime consumes `Ego::Graphics::AnimatedModel` regardless of format;
`MD2Model` and `GltfModel` are loader adapters and `ModelDescriptor` remains
the facade. `ModelAnimationMetadata` owns action ranges, frame effects,
walk-lip tables, and `copy.txt` healing behavior — MD2 derives it from legacy
frame names; glTF supplies it through `extras.egoboo`, with a single-frame
`DA` fallback for minimal static assets.

glTF/GLB loader v1 intentionally accepts a narrow static-frame subset:
triangle primitives (indexed or unindexed), `POSITION` VEC3/FLOAT, optional
`NORMAL` and `TEXCOORD_0`, optional 8/16/32-bit scalar indices, one glTF mesh
per Egoboo animation frame with identical vertex counts. It rejects required
extensions, skins, morph targets, Draco/meshopt compression, sparse accessors,
non-triangle primitives, missing meshes, and non-identity node transforms.
Virtually all shipped content is still MD2; asset migration is roadmap work.

## 6. Naming conventions and the script system

Fixed naming patterns are part of the runtime contract: `sound0..29`,
`part0..29.txt`, `icon0..29`, `tris0..29`. A structured-format migration alone
does not remove this debt unless the runtime changes too.

EgoScript is active and core to gameplay: compiler in
`game/script_compile.c`, runtime functions across fourteen
`script_functions_*.c` TUs, support under `Script/`, legacy docs in
`data/doc/AiDocs.txt`. Scripts are per-object, indentation-sensitive (tabs
invalid), run at 50 Hz, and fall back to `mp_data/script.txt` when an object
script fails to load. Do not replace EgoScript before capturing its execution
model, expected object API, control-flow semantics, and timing assumptions —
otherwise a migration becomes a behavior rewrite.

## 7. Recommended migration model

1. **Schemas/IR** — machine-readable schemas for module metadata, environment,
   spawn lists, object/particle definitions, message and naming tables.
2. **Importers before replacement** — legacy → IR importer, IR validator,
   IR → structured exporter.
3. **Adapters** — the first modern runtime loads the IR, not raw JSON.
4. **Dual-load and compare** representative modules (legacy versus migrated).
5. **Retire legacy parsers only after parity** — never delete `menu.txt`,
   `data.txt`, or `spawn.txt` parsing before parity tooling exists.

## 8. Design guidance for a structured format

Do not mirror the positional files one-to-one. The modern model needs stable
IDs (not slot-number-as-identity), explicit manifests (not suffix
enumeration), explicit inheritance/composition (not hidden "copy" semantics),
declarative references (not mount-order tricks) where possible, and preserved
metadata for round-trip debugging.

## 9. Prior migration attempts

The repo has tried to escape this model before, which proves the need is real
but landing is hard:

- `doc/legacy/ego2xml/` — 2015 XML proposal, archived.
- `utilities/migrator/` — tool shell with mostly empty migrator
  implementations and a stale README.
- `game/Lua/` — Lua/SWIG experiment, since deleted from the active tree
  (survives only under `backup-copy/`); never in the build graph.

Fragmented historical documentation (`data/doc/`, `.odt` manuals — see
`07-historical-docs-audit.md`) preserves intent but is not tied to current
code.

## 10. Bottom line

The content system is old, implicit, layered, and deeply embedded in runtime
assumptions. Safe order: document → inventory → formalize → validate →
dual-load → only then replace.
