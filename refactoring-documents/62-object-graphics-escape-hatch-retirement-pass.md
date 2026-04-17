# Object Graphics Escape-Hatch Retirement Pass

This document records the bounded `Object::graphics()` escape-hatch retirement completed on 2026-04-17 as the immediate follow-on to document 61.

## What changed

- added stable `Object` render-facing forwarders for the last `ObjectGraphics` queries still needed by callers:
  - `hasModelDescriptor()`
  - `getReflectionAlpha()`
  - `getTint(...)`
- made `ObjectGraphics::getTint(...)` `const` so those forwarders stay read-only
- removed the public `Object::graphics()` / `graphics() const` escape hatch from `Object.hpp`
- migrated the remaining renderer and attachment/debug callers to the `Object` surface:
  - `graphic_mad.c`
  - `graphic_prt.c`
  - `ParticleGraphics.cpp`
- kept `ObjectGraphics` behavior ownership intact; this pass only retired the public escape hatch
- extended the focused accessor regression coverage in `egolib/tests/egolib/tests/ObjectAccessors.cpp`

## Why this pass next

Document 61 left two bounded follow-on options:

- shrink the `graphics()` escape hatch by promoting a few stable `Object` helpers, or
- split matrix-publication helpers away from broader `ObjectGraphics` behavior

The codebase was already using `Object` forwarding for matrix, vertex, and matrix-cache work. The remaining public dependency was narrower:

- render-path model presence checks
- tint calculation
- reflection alpha queries
- debug attachment/grip helpers

That made caller migration the lower-risk, higher-value step. Splitting `ObjectGraphics` internals first would have widened scope without materially reducing the public seam.

## Scope constraints kept

- no gameplay or content behavior changes
- no animation-policy or AI extraction
- no `ObjectGraphics` internal decomposition
- no renderer rewrite or render-pass redesign
- no file-format or validator-schema changes

## Acceptance baseline

This pass is considered healthy when all of the following remain true:

- `Object.hpp` no longer exposes public `graphics()` accessors
- in-repo render and attachment callers use the `Object` forwarding surface instead of a public `ObjectGraphics` instance
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

Continue the `Object` cleanup incrementally. The next bounded pass should stay inside `ObjectGraphics` rather than re-opening a public escape hatch, for example:

- splitting tint/reflection-policy helpers from the broader animation/update behavior, or
- narrowing model-reset and animation-reset responsibilities inside `ObjectGraphics`

Do not mix that follow-on with renderer redesign, AI extraction, or broader role-interface work unless a separate pass first narrows those ownership seams.
