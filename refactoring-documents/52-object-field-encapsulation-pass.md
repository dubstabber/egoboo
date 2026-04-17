# Object Field Encapsulation Pass

This document records the bounded `Object` encapsulation cleanup completed on 2026-04-17 after the engine-context ownership pass.

## What changed

- added explicit accessor methods on `Object` for the first selected field groups:
  - held/equipped object refs
  - current/base team refs
  - jump state
  - size transition state
  - damage-target / reaffirm damage types
  - damage threshold
- moved those backing fields behind a private boundary in `Object.hpp`
- migrated non-`Object` callers in gameplay, physics, rendering, export, UI, and module-spawn code to the accessor surface
- added focused regression coverage for the new accessor seam in `egolib/tests/egolib/tests/ObjectAccessors.cpp`

## Why this pass now

Document 51 retired the last in-repo raw engine-global ownership seam:

- `_gameEngine` is now owned through `EngineContext`
- `_currentModule` had already been retired
- the remaining `update_wld` mentions are wording residue, not live coupling

That left `Object` as the next active maintainability hotspot:

- large public state surface
- many direct field reads/writes across unrelated subsystems
- weak invariants around simple state transitions

This pass narrows that surface without redesigning entity behavior.

## Scope constraints kept

- no gameplay or content behavior changes
- no `Object` component extraction
- no role-interface work yet
- no enum conversion or service-layer cleanup mixed into the same patch
- no full public-field retirement; only the selected first groups moved behind accessors

## Acceptance baseline

This pass is considered healthy when all of the following remain true:

- the selected `Object` field groups are private in `Object.hpp`
- non-`Object` callers use the new accessor surface instead of direct field access
- focused accessor tests pass
- module spawn / startup / realization tests remain green
- validator behavior on `test.mod` remains unchanged

Validated command set for this pass:

```bash
cmake --build build -j4
```

```bash
ctest --test-dir build --output-on-failure -R ObjectAccessorFixture
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

Keep the `Object` cleanup incremental. The next pass should continue shrinking the public `Object` surface with another bounded accessor migration, or start defining role-specific interfaces once the remaining high-traffic field groups are no longer exposed directly.
