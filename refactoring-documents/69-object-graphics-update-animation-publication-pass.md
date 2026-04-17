# ObjectGraphics UpdateAnimation Publication Pass

This document records the bounded `ObjectGraphics` `updateAnimation()` publication cleanup completed on 2026-04-17 as the immediate follow-on to document 68.

## What changed

- extracted the remaining interpolation-state publication seam inside `ObjectGraphics.cpp` into two narrow private helpers:
  - `publishInterpolationState(...)`
  - `applyPublishedInterpolationStep()`
- rewrote `updateAnimation()` to delegate quarter-step and residual-progress publication through those helpers while preserving the existing order:
  - consume full quarter-step crossings first
  - publish any remaining fractional interpolation progress second
  - update animation-rate policy last
- kept frame-publication responsibilities separate:
  - `publishFrameState(...)` still owns direct frame/interpolation publication for `setFrameFull(...)` and `removeInterpolation()`
  - the new interpolation helper only publishes the live `(ilip, flip)` pair used by `updateAnimation()`
- routed `incrementFrame()` child invalidation through `invalidateChildInstancesIfCacheInvalid()` instead of open-coding the cache-validity check
- extended focused accessor coverage in `egolib/tests/egolib/tests/ObjectAccessors.cpp` with characterization tests for:
  - a single quarter-step interpolation advance that leaves source/target frames unchanged
  - a frame-boundary crossing that advances to the next frame while preserving the old target as the new source
  - a mixed frame-boundary-plus-residual-progress update that preserves the current loop order and final published interpolation state

## Why this pass next

Document 68 narrowed the next follow-on to the remaining animation/cache publication seam, especially around `updateAnimation()` and the coupling between frame advancement and cache invalidation.

That seam was still the right next move:

- `updateAnimation()` still mixed interpolation-state publication and step-trigger side effects inline
- `incrementFrame()` still open-coded the child-invalidation tail even though the equivalent helper already existed
- the work stayed entirely inside `ObjectGraphics` and preserved the existing `Object` caller surface
- it kept the pass bounded by avoiding renderer changes, vertex-cache redesign, or animation-policy replacement

## Scope constraints kept

- no gameplay or content behavior changes
- no public `Object` or `ObjectGraphics` API changes
- no renderer rewrite, render-pass redesign, or AI extraction
- no `updateAnimationRate()` policy changes
- no `updateVertexCache()` / `needs_update()` / `isVertexCacheValid()` semantic changes
- no file-format, validator-schema, or content-loader changes

## Acceptance baseline

This pass is considered healthy when all of the following remain true:

- `updateAnimation()` remains behavior-preserving while delegating interpolation publication to explicit helpers
- quarter-step publication still updates interpolation before triggering frame-FX and frame-advance side effects
- a frame-boundary crossing still promotes the prior target frame to the new source frame
- residual interpolation progress after a frame advance still lands on the new frame pair
- `incrementFrame()` still invalidates child instances on the same effective conditions as before
- focused accessor tests remain green, including the new `updateAnimation()` characterization coverage
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

Continue the `ObjectGraphics` cleanup incrementally, but keep it bounded. The next pass should stay inside the remaining animation/cache publication residue rather than reopening renderer or caller-surface work, for example by:

- characterizing and narrowing the remaining cache-validity assumptions around `isVertexCacheValid()` versus `needs_update()`, or
- separating vertex-cache publication bookkeeping from the broader dirty-range merge logic in `updateVertexCache()`

Do not mix that follow-on with renderer redesign, role-interface extraction, or broader subsystem decomposition unless a separate pass first narrows those ownership seams.
