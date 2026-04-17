# ObjectGraphics Frame Publication Pass

This document records the bounded `ObjectGraphics` frame-publication cleanup completed on 2026-04-17 as the immediate follow-on to document 67.

## What changed

- renamed the shared frame-state write helper inside `ObjectGraphics.cpp` from `commitFrameState(...)` to `publishFrameState(...)` to make the animation-state publication seam explicit
- routed both public frame-publication entrypoints through that helper:
  - `setFrameFull(...)`
  - `removeInterpolation()`
- kept `setFrameFull(...)` behavior unchanged:
  - still heals `_currentAnimation` before resolving frame bounds
  - still preserves the prior source frame
  - still wraps `frame_along` inside the action-local frame range
  - still publishes only frame/interpolation state
- kept `removeInterpolation()` behavior unchanged:
  - still no-ops when source and target already match
  - still collapses interpolation by snapping source to target and zeroing interpolation progress
  - still leaves action-selection state untouched
- extended focused accessor coverage in `egolib/tests/egolib/tests/ObjectAccessors.cpp` with characterization tests for:
  - `removeInterpolation()` snapping to the current target frame without mutating action state
  - `removeInterpolation()` remaining a no-op when interpolation is already collapsed

## Why this pass next

Document 67 narrowed the next follow-on to animation-state publication, especially around `setFrameFull()`, `removeInterpolation()`, and the remaining interpolation/cache publication assumptions.

This was the smaller safe half of that seam:

- `setFrameFull()` and `removeInterpolation()` were the two direct public frame-publication entrypoints reached from gameplay/script call paths
- both methods manually published overlapping pieces of `_sourceFrameIndex`, `_targetFrameIndex`, `_animationProgressInteger`, and `_animationProgress`
- the work stayed inside `ObjectGraphics` and preserved the public `Object` caller surface
- it avoided mixing a small publication cleanup with the broader `updateAnimation()` loop and vertex-cache sequencing

## Scope constraints kept

- no gameplay or content behavior changes
- no public `Object` or `ObjectGraphics` API changes
- no renderer rewrite, render-pass redesign, or AI extraction
- no `updateAnimation()` / `incrementFrame()` / `updateVertexCache()` logic changes in the same patch
- no file-format, validator-schema, or content-loader changes

## Acceptance baseline

This pass is considered healthy when all of the following remain true:

- `setFrameFull()` and `removeInterpolation()` remain behavior-preserving while delegating to the explicit frame-publication helper
- `setFrameFull()` still heals invalid current actions before frame lookup and preserves the prior source frame
- `removeInterpolation()` still snaps source to target only when the two frames differ
- `removeInterpolation()` still leaves `_currentAnimation`, `_nextAnimation`, and interruptibility unchanged
- focused accessor tests remain green, including the new interpolation-collapse characterization coverage
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

Continue the `ObjectGraphics` cleanup incrementally, but keep it bounded. The next pass should move into the remaining animation/cache publication seam rather than reopening renderer or caller-surface work, for example by:

- characterizing and isolating the interpolation-step publication loop inside `updateAnimation()`, or
- separating frame-step publication assumptions from vertex-cache validity and child-invalidation sequencing

Do not mix that follow-on with renderer redesign, role-interface extraction, or broader subsystem decomposition unless a separate pass first narrows those ownership seams.
