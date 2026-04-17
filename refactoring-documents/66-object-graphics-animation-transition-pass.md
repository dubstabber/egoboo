# ObjectGraphics Animation Transition Pass

This document records the bounded `ObjectGraphics` end-of-animation transition extraction completed on 2026-04-17 as the immediate follow-on to document 65.

## What changed

- decomposed `ObjectGraphics::incrementFrame()` into narrower transition helpers inside `ObjectGraphics.cpp`:
  - `handleFrozenAnimationEnd(int)`
  - `handleLoopedAnimationEnd()`
  - `handleQueuedAnimationEnd()`
  - `resolveMountedLoopAnimation() const`
- kept `incrementFrame()` as the only frame-advance orchestration entrypoint and preserved its frame publication order and child-instance invalidation path
- kept `incrementAction()` as the only helper that resolves `_nextAnimation` into a concrete follow-on action
- preserved mounted loop substitution behavior while isolating it from broader frame bookkeeping
- extended focused accessor coverage in `egolib/tests/egolib/tests/ObjectAccessors.cpp` with characterization tests for:
  - freeze-at-last-frame rollover
  - looped animation wrap to first frame
  - mounted loop substitution for item-holding riders
  - mounted loop substitution for empty-handed riders
  - queued next-action transition at end of animation

## Why this pass next

Document 65 narrowed the next follow-on to the end-of-animation transition seam inside `ObjectGraphics`, especially around `incrementFrame()` and `incrementAction()`.

That seam was still the right next move:

- `incrementFrame()` mixed interpolation normalization, end-of-action policy, loop/mount substitution, queued-action handoff, and frame publication in one method
- the work stays inside `ObjectGraphics` and does not reopen the public render-facing `Object` surface
- the extraction reduces transition-policy density without changing renderer responsibilities, AI ownership, or broader animation-rate policy

## Scope constraints kept

- no gameplay or content behavior changes
- no public `Object` or `ObjectGraphics` API changes
- no renderer rewrite, render-pass redesign, or AI extraction
- no broader animation-system redesign beyond the end-of-animation seam
- no file-format, validator-schema, or content-loader changes

## Acceptance baseline

This pass is considered healthy when all of the following remain true:

- `incrementFrame()` remains behavior-preserving while delegating to narrower transition helpers
- freeze-at-last-frame behavior still keeps the last frame and marks the action interruptible
- looped actions still wrap to the first frame of the current action
- mounted loop substitution still remaps to `ACTION_MH` for riders holding an item and `ACTION_MI` otherwise
- queued next-action transitions still advance through `incrementAction()` without introducing new fallback policy
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

Continue the `ObjectGraphics` cleanup incrementally, but keep it bounded. The next pass should stay inside animation-state mutation and frame bookkeeping, for example by:

- separating animation-state mutation from frame/cache maintenance helpers more explicitly, or
- narrowing `startAnimation()` / `setAction()` / `setFrame()` responsibilities without widening the public caller surface

Do not mix that follow-on with renderer redesign, role-interface extraction, or broader subsystem decomposition unless a separate pass first narrows those ownership seams.
