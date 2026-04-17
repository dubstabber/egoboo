# ObjectGraphics Animation State / Frame Bookkeeping Pass

This document records the bounded `ObjectGraphics` animation-state mutation and frame-bookkeeping cleanup completed on 2026-04-17 as the immediate follow-on to document 66.

## What changed

- decomposed the remaining shared animation mutation path inside `ObjectGraphics.cpp` into narrower private helpers:
  - `tryCommitActionState(...)`
  - `tryCommitFrameState(int)`
  - `tryRestartAnimationAtActionStart(...)`
  - `normalizeCurrentAnimationForFrameMutation()`
  - `commitFrameState(...)`
  - `invalidateChildInstancesIfCacheInvalid()`
  - `restartMovementAnimation(ModelAction, int)`
- kept the public `ObjectGraphics` caller surface unchanged:
  - `startAnimation(...)`
  - `setAction(...)`
  - `setFrame(...)`
  - `setFrameFull(...)`
- made the responsibility split explicit:
  - action helpers own `_currentAnimation`, `_nextAnimation`, and `_canBeInterrupted`
  - frame helpers own `_sourceFrameIndex`, `_targetFrameIndex`, `_animationProgressInteger`, and `_animationProgress`
- preserved the existing locomotion-transition mutation order by isolating the old `setAction()` + `setFrame()` + `startAnimation()` sequence inside `restartMovementAnimation(...)`
- extended focused accessor coverage in `egolib/tests/egolib/tests/ObjectAccessors.cpp` with characterization tests for:
  - `setAction()` leaving frame bookkeeping unchanged
  - `setFrame()` leaving action state unchanged
  - `startAnimation()` restarting from the action’s first frame while keeping the prior target frame as the new source frame
  - `setFrameFull()` healing invalid current actions while preserving source-frame state
  - locomotion animation changes preserving the mapped walk-frame interpolation source before the restart to the new action

## Why this pass next

Document 66 narrowed the next follow-on to animation-state mutation and frame bookkeeping inside `ObjectGraphics`, especially around `startAnimation()`, `setAction()`, and `setFrame()`.

That seam was still the right next move:

- `startAnimation()` still mixed action validation/commit, frame reset, and child-instance invalidation in one method
- `setAction()` and `setFrame()` mutated different halves of the same animation state, but the boundary was only implicit
- movement-policy transitions still depended on a fragile three-call sequence whose intermediate frame mutation affected the final interpolation source
- the work stayed internal to `ObjectGraphics` and did not reopen the public `Object` render-facing surface

## Scope constraints kept

- no gameplay or content behavior changes
- no public `Object` or `ObjectGraphics` API changes
- no renderer rewrite, render-pass redesign, or AI extraction
- no `incrementFrame()` / `incrementAction()` redesign beyond preserving their existing interactions with the restarted animation state
- no file-format, validator-schema, or content-loader changes

## Acceptance baseline

This pass is considered healthy when all of the following remain true:

- `startAnimation()`, `setAction()`, `setFrame()`, and `setFrameFull()` remain behavior-preserving while delegating to narrower helpers
- action-state mutation and frame/interpolation mutation are separated internally without changing caller-visible semantics
- locomotion transitions still preserve the staged walk-frame source used for interpolation before the restarted action begins at frame 0
- focused accessor tests remain green, including the new animation-state characterization coverage
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

Continue the `ObjectGraphics` cleanup incrementally, but keep it bounded. The next pass should stay within animation-state publication rather than widening back into renderer or caller-surface work, for example by:

- narrowing `setFrameFull()` and `removeInterpolation()` around a more explicit frame-publication helper, or
- characterizing and isolating the remaining interpolation/cache publication assumptions around `updateAnimation()` and vertex-cache validity

Do not mix that follow-on with renderer redesign, role-interface extraction, or broader subsystem decomposition unless a separate pass first narrows those ownership seams.
