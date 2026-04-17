# Object Attachment and Platform Encapsulation Pass

This document records the bounded `Object` attachment, inventory-placement, and platform-capability encapsulation cleanup completed on 2026-04-17 as the immediate follow-on to document 53.

## What changed

- added accessor methods on `Object` for a third selected state group:
  - holder ref / held-slot attachment state
  - inventory-holder placement state
  - platform / can-use-platforms flags
  - platform holding-weight bookkeeping
- moved those backing fields behind a private boundary in `Object.hpp`
- migrated non-`Object` callers in physics, collision, scripts, spawn/bootstrap, export, inventory, rendering, and targeting code to the accessor surface
- extended the focused accessor regression coverage in `egolib/tests/egolib/tests/ObjectAccessors.cpp`

## Why this pass next

Document 53 explicitly deferred the more coupled attachment and platform state:

- holder and inventory-placement refs still leaked into scripts, inventory, targeting, and rendering helpers
- platform capability flags still leaked into collision and export code
- platform holding-weight bookkeeping was still mutated directly from physics code

These fields were still high-traffic coupling points, but they were simple enough for another accessor-only pass before role interfaces or component extraction.

## Scope constraints kept

- no gameplay or content behavior changes
- no attachment/platform redesign in the same patch
- no `Object` component extraction
- no role-interface split yet
- no file-format or validator-schema changes
- no attempt to encapsulate `PhysicsData` platform-tracking fields in the same pass

## Acceptance baseline

This pass is considered healthy when all of the following remain true:

- the selected attachment/platform fields are private in `Object.hpp`
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

Continue the `Object` cleanup incrementally. With the highest-traffic public state now behind accessors, the next bounded pass can either finish shrinking the remaining public `Object` surface or start the first role-interface extraction for a specific consumer cluster without mixing that design work into the field-encapsulation sequence.
