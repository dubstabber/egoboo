# Object Appearance and Profile Scalar Encapsulation Pass

This document records the bounded `Object` appearance/profile scalar encapsulation cleanup completed on 2026-04-17 as the immediate follow-on to document 56.

## What changed

- added accessor methods on `Object` for a sixth selected state group:
  - current / base skin state
  - base-model reference state
  - overlay flag state
  - shadow-size baseline / saved / current state
- moved those backing fields behind a private boundary in `Object.hpp`
- migrated non-`Object` callers in spawn/bootstrap, scripts, GUI, enchantment, shadow rendering, and export code to the accessor surface
- extended the focused accessor regression coverage in `egolib/tests/egolib/tests/ObjectAccessors.cpp`

## Why this pass next

Document 56 left two realistic follow-ons: move into the appearance/profile scalar cluster, or keep shrinking the non-graphics stats/ammo surface first.

The current codebase favored the appearance/profile branch:

- `skin`, `skin_stt`, `basemodel_ref`, `is_overlay`, and `shadow_size*` still formed one coherent state cluster around spawn, polymorph, shadow rendering, and icon/skin selection
- those fields were still read directly outside `Object` by scripts, GUI, rendering, export, and enchantment code
- this pass keeps the accessor sequence mechanical without widening scope into ammo, level, or gender behavior across combat, inventory, and UI

## Scope constraints kept

- no gameplay or content behavior changes
- no `inst`, `ori`, or `ai` encapsulation in the same patch
- no stats/ammo/gender cleanup mixed into the same patch
- no role-interface extraction
- no file-format or validator-schema changes

## Acceptance baseline

This pass is considered healthy when all of the following remain true:

- the selected appearance/profile scalar fields are private in `Object.hpp`
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

Continue the `Object` cleanup incrementally. The next bounded pass should return to the remaining stats/ammo/gender and level-state scalar surface before attempting `inst`, `ori`, or any role-interface extraction.
