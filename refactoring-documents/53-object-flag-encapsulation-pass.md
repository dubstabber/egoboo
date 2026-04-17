# Object Flag Encapsulation Pass

This document records the bounded `Object` flag and player-binding encapsulation cleanup completed on 2026-04-17 as the immediate follow-on to document 52.

## What changed

- added accessor methods on `Object` for a second selected state group:
  - player number / local-player binding
  - name-known / ammo-known state
  - invincible / kursed / hit-ready / equipped flags
  - item / shop-item / crushable flags
  - sparkle state
- moved those backing fields behind a private boundary in `Object.hpp`
- migrated non-`Object` callers in scripts, spawn/bootstrap, export, combat, physics, UI, shop, and passage code to the accessor surface
- extended the focused accessor regression coverage in `egolib/tests/egolib/tests/ObjectAccessors.cpp`

## Why this pass next

Document 52 intentionally stopped after team, held/equipment, jump, size-transition, and damage-type state. The next highest-traffic public `Object` state was the mutable flag cluster:

- player registration state was still written directly outside `Object`
- knowledge and kurse flags were read and mutated by scripts, inventory, export, and UI code
- item / shop / crushable flags were still exposed to module and passage logic
- sparkle state still leaked directly into GUI and script code

These fields are simple state, widely read, and good candidates for accessor-only cleanup before tackling more coupled attachment/platform mechanics.

## Scope constraints kept

- no gameplay or content behavior changes
- no attachment/platform redesign in the same patch
- no `Object` component extraction
- no role-interface split yet
- no file-format or validator-schema changes

## Acceptance baseline

This pass is considered healthy when all of the following remain true:

- the selected flag and player-binding fields are private in `Object.hpp`
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

Continue the `Object` cleanup incrementally. The next bounded pass should target attachment/platform state and remaining holder/inventory-placement flags, because those still account for the largest cross-subsystem direct coupling after the mutable flag cluster is sealed off.
