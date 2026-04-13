# Spawn.txt Format Spec

This document records the current `spawn.txt` contract as implemented in the active loader on 2026-04-13.

It is a compatibility spec for the existing runtime, not a proposal for a future replacement format.

## 1. Location and loader path

- Source file in content: `<module>/gamedat/spawn.txt`
- Runtime VFS path: `mp_data/spawn.txt`
- Primary parser: `egolib/library/src/egolib/FileFormats/SpawnFile/SpawnFileReaderImpl.cpp`
- Load-name normalization: `egolib/library/src/egolib/game/Module/module_spawn.c`
- Live module activation: `egolib/library/src/egolib/game/Module/Module.cpp`

The file is parsed after module VFS setup and before live object spawn/attachment.

## 2. Accepted entry forms

The loader accepts two kinds of entries.

### Spawn entries

General shape:

```text
<load_name>: <spawn_name> <slot> <x> <y> <z> <facing_or_attach> <money> <skin_or_?> <passage> <content> <level> <stat> <ghost> <team>
```

Meaning in current code:

- `load_name`
  The left-hand token before `:`. Despite the legacy field name `spawn_comment`, this is the source token used to resolve which object profile to load.
- `spawn_name`
  The runtime instance name passed to `spawnObject(...)`. If it is `NONE`, the runtime replaces it with an empty string.
- `slot`
  Requested object profile slot. Import-reserved slots are treated specially by module load logic.
- `x y z`
  Position in tile/grid units. The parser multiplies each value by `Info<float>::Grid::Size()` before storing world coordinates.
- `facing_or_attach`
  One printable character. `N`, `S`, `E`, `W`, and `?` mean facing. `L`, `R`, and `I` mean attach-left, attach-right, and attach-inventory.
- `money`
  Integer money override for the spawned object.
- `skin_or_?`
  Integer skin override, or `?` for `ObjectProfile::NO_SKIN_OVERRIDE`.
- `passage`
  Passage index associated with the spawn.
- `content`
  Content/state payload stored on the spawned object AI state.
- `level`
  Spawn level override.
- `stat`
  Boolean parsed through `readBool()`.
- `ghost`
  One printable character consumed by the loader and ignored. The code comments mark it as a bad unused legacy value.
- `team`
  One printable letter converted to a team index with `(chr - 'A') % Team::TEAM_MAX`.

### Dependency entries

General shape:

```text
#dependency <load_name_or_%treasure_table> <slot>
```

Meaning in current code:

- The loader records the dependency target in `spawn_comment`.
- `do_spawn` is false, so the entry reserves or preloads the profile without directly spawning an object instance.
- The validator still checks that the referenced object resolves correctly through `mp_objects`.

## 3. Lexical rules and tolerated quirks

- Leading whitespace and blank lines are ignored.
- Single-line comments beginning with `/` are skipped by the parser.
- Spawn-entry left-hand names may begin with alphabetic characters, `%`, or `_`.
- A missing `:` after the left-hand token is a syntax error.
- The parser accepts `%table_name` references in `load_name`.
- Tabs or formatting normalization are not specified separately from the loader; compatibility depends on the current `ReadContext` behavior.

## 4. Load-name normalization

After parsing, module load calls `convert_spawn_file_load_name(...)`.

Normalization rules:

- trim surrounding whitespace
- if the name begins with `%`, resolve it through `randomtreasure.txt`
- append `.obj` if the name does not already end with `.obj`
- lowercase the final result

This means validator results should be interpreted against the normalized object name, not only the raw token written in `spawn.txt`.

## 5. Runtime implications

- Resolved objects are looked up through `mp_objects`, not through a fixed physical path.
- Because `mp_objects` is built from module-local objects plus mounted global object folders, the same spawn name can resolve differently depending on VFS overlay state.
- `facing_or_attach` is overloaded. An attachment directive replaces facing rather than coexisting with it.
- `#dependency` lines affect profile loading even when they do not create a live object.
- Import-reserved slots are part of module-load semantics and should not be treated as ordinary free slots by migration tooling.

## 6. Validator contract

The current validator checks:

- `spawn.txt` parses successfully
- normalized spawn load names resolve to `mp_objects/<name>.obj`
- resolved object profiles can load their `data.txt`, `tris.md2`, and script data

The validator currently classifies missing resolved objects as `missing_spawn_object` and aggregates repeated unresolved names per module.

## 7. Refactor implications

- Any future structured replacement must preserve the distinction between the resolved load target and the spawned runtime instance name.
- Treasure-table expansion and VFS overlay semantics are part of the format contract, not incidental loader details.
- The unused `ghost` field still occupies a positional slot and cannot be removed safely without a compatibility layer.
- Alias reconciliation work should operate on normalized names while still preserving the original source token for migration traceability.
