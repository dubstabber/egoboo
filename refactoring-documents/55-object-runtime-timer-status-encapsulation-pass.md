# Object Runtime Timer and Status Encapsulation Pass

This document records the bounded `Object` runtime-timer and status encapsulation cleanup completed on 2026-04-17 as the immediate follow-on to document 54.

## What changed

- added accessor methods on `Object` for a fourth selected state group:
  - reload / damage cooldown timers
  - grog / daze / boredom / careful timers
  - dismount timer and dismount-source object ref
  - in-water state
  - draw-icon flag
- moved those backing fields behind a private boundary in `Object.hpp`
- migrated non-`Object` callers in gameplay, physics, scripts, module bootstrap, export, UI, and session-perception code to the accessor surface
- extended the focused accessor regression coverage in `egolib/tests/egolib/tests/ObjectAccessors.cpp`

## Why this pass next

Document 54 left two possible follow-ons: keep shrinking the remaining public `Object` surface, or attempt the first role-interface extraction.

The codebase state still favored another bounded accessor pass:

- `ai`, `inst`, and `ori` remain broad, cross-subsystem coupling surfaces
- the runtime timer/status fields were still directly touched by gameplay, physics, scripts, UI, and export code
- those fields are simple mutable state and low-risk candidates for another accessor-only migration

This keeps the `Object` cleanup incremental without jumping into a wider interface design while the public state surface is still too large.

## Scope constraints kept

- no gameplay or content behavior changes
- no role-interface extraction in the same patch
- no graphics-surface encapsulation in the same patch
- no `Object` component extraction
- no file-format or validator-schema changes

## Acceptance baseline

This pass is considered healthy when all of the following remain true:

- the selected timer/status fields are private in `Object.hpp`
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

- finish another non-graphics public-state cluster such as remaining scalar movement/collision state, or
- prepare a later graphics-facing encapsulation pass around `inst`

Do not mix that follow-on with role-interface extraction unless the remaining high-traffic public field groups are first narrowed further.
