# Object `inst` Transitional Boundary Pass

This document records the bounded `Object::inst` transitional boundary cleanup completed on 2026-04-17 as the immediate follow-on to document 60.

## What changed

- moved `Object::inst` behind a private boundary in `Object.hpp`
- added a transitional `Object` forwarding surface for the common non-render callers:
  - animation control and query helpers
  - render-state getters for alpha/light/sheen, color shift, and texture offsets
  - matrix, vertex, and lighting helper forwarding
  - explicit `graphics()` / `graphics() const` escape hatches for render-facing code that still needs `ObjectGraphics`
- sealed the remaining public `ObjectGraphics` data surface behind accessors:
  - alpha / light / sheen
  - color shift
  - texture offsets
  - matrix-cache copy and validity helpers
- migrated gameplay, script, module, UI, particle-placement, and physics callers off raw `inst` access
- rewired matrix and render code to use either the new `Object` forwarding methods or the explicit `graphics()` boundary
- extended the focused accessor regression coverage in `egolib/tests/egolib/tests/ObjectAccessors.cpp`

## Why this pass next

Document 60 recommended the broader render-facing `inst` seam as the next bounded follow-on after the bumper / collision-volume pass.

The current codebase still favored a transitional boundary rather than strict full encapsulation in one patch:

- `Object::inst` remained the last large public seam on `Object`
- many non-render callers only needed a small subset of animation, matrix, or render-state helpers
- `ObjectGraphics` behavior still crosses animation, matrix, combat-effect, and AI-adjacent flow, so a full role or ownership redesign would have widened scope materially
- the transitional `graphics()` escape hatch keeps render and matrix code compiling while still sealing the raw public field surface

## Scope constraints kept

- no gameplay or content behavior changes
- no `ai` extraction or animation-policy redesign
- no `ObjectGraphics` behavior split or render-pipeline redesign
- no file-format or validator-schema changes
- no attempt to remove every render-side `ObjectGraphics` dependency in the same patch

## Acceptance baseline

This pass is considered healthy when all of the following remain true:

- `Object::inst` is private in `Object.hpp`
- `ObjectGraphics` no longer exposes raw public alpha/light/sheen, color-shift, texture-offset, or matrix-cache fields
- gameplay, script, module, UI, and physics callers use `Object` forwarding helpers instead of raw `inst`
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

Continue the `Object` cleanup incrementally. The next bounded pass should use the new private `inst` boundary to narrow one of the remaining render-facing seams explicitly, for example:

- shrinking the `graphics()` escape hatch by moving more render-only queries onto stable `Object` helpers, or
- separating matrix-publication helpers from broader animation/render behavior inside `ObjectGraphics`

Do not mix that follow-on with `ai` extraction or a renderer rewrite unless a separate pass first narrows those ownership boundaries.
