# Object Movement and Collision-Mask Encapsulation Pass

This document records the bounded `Object` movement/collision-mask encapsulation cleanup completed on 2026-04-17 as the immediate follow-on to document 55.

## What changed

- added accessor methods on `Object` for a fifth selected state group:
  - collision stop-mask state
  - movement turn-mode state
  - bump-list next ref bookkeeping
- moved those backing fields behind a private boundary in `Object.hpp`
- migrated non-`Object` callers in scripts, spawn/bootstrap, physics, targeting, and export code to the accessor surface
- extended the focused accessor regression coverage in `egolib/tests/egolib/tests/ObjectAccessors.cpp`

## Why this pass next

Document 55 left two realistic follow-ons: keep shrinking the remaining non-graphics `Object` public surface, or jump into the graphics/appearance cluster.

The codebase still favored one more small accessor pass first:

- `stoppedby` remained a live collision, pathfinding, and line-of-sight coupling seam across scripts, targeting, and export
- `turnmode` still leaked from movement scripts into physics-facing turning logic
- `bumplist_next` was a trivial leftover public ref that could be sealed off without widening scope

This pass keeps the sequence mechanical and avoids mixing render-facing appearance state into the same patch.

## Scope constraints kept

- no gameplay or content behavior changes
- no graphics/appearance-state encapsulation in the same patch
- no role-interface extraction in the same patch
- no `Object` component extraction
- no file-format or validator-schema changes

## Acceptance baseline

This pass is considered healthy when all of the following remain true:

- the selected movement/collision-mask fields are private in `Object.hpp`
- non-`Object` callers use accessors instead of direct field access
- focused accessor tests remain green
- module spawn / startup / realization tests remain green
- validator behavior on `test.mod` remains unchanged

Validated command set for this pass:

```bash
cmake --build build -j4
```

```bash
ctest --test-dir build --output-on-failure -R ObjectAccessor
```

```bash
ctest --test-dir build --output-on-failure -R ModuleSpawnRealizationFixture
```

```bash
ctest --test-dir build --output-on-failure -R ModulePlayerStartupFixture
```

```bash
ctest --test-dir build --output-on-failure -R ModuleLoadSmokeFixture
```

```bash
HOME=/tmp/egoboo-home \
XDG_DATA_HOME=/tmp/egoboo-xdg \
./build/products/x64/bin/egoboo-content-validator --data-dir "$PWD/data" --module test.mod
```

## Follow-on recommendation

Continue the `Object` cleanup incrementally. The next bounded pass should either:

- tackle the remaining appearance/profile scalar cluster (`skin`, `skin_stt`, `basemodel_ref`, `is_overlay`, `shadow_size*`), or
- start the stats/ammo/gender scalar cleanup if the goal is to keep avoiding render-facing state for another pass

Do not mix either follow-on with the larger bumper/CV structs, `ori`, or `inst`.
