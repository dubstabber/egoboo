# ObjectGraphics Animation Control Policy Pass

This document records the bounded `ObjectGraphics` animation/control-policy extraction completed on 2026-04-17 as the immediate follow-on to document 64.

## What changed

- decomposed `ObjectGraphics::updateAnimationRate()` into narrower policy stages inside `ObjectGraphics.cpp`:
  - `shouldSkipAnimationRateUpdate()`
  - `applyMountedAnimationRatePolicy()`
  - file-local locomotion-decision derivation via `LocomotionAnimationDecision`
  - `applyIdleAnimationPolicy()`
  - `applyMovementAnimationPolicy(ModelAction, int)`
- kept `updateAnimationRate()` as the only orchestration entrypoint and preserved its caller contract and mutation order
- kept end-of-animation flow in `incrementFrame()` / `incrementAction()` unchanged in this pass
- extended focused accessor coverage in `egolib/tests/egolib/tests/ObjectAccessors.cpp` with characterization tests for:
  - mounted scenery animation-rate stop behavior
  - mounted holder-rate inheritance
  - bored idle alert/timer behavior
  - idle recovery from walking animation back to `ACTION_DA`
  - stealth movement choosing `ACTION_WA`
  - flying idle remap to flap/run animation policy

## Why this pass next

Document 64 narrowed the next follow-on to broader animation/control policy cleanup inside `ObjectGraphics`, especially around `updateAnimationRate()` and `incrementAction()`.

`updateAnimationRate()` was the safer next seam:

- it mixed guard logic, mounted-rate policy, grounded/flying locomotion selection, boredom handling, and action mutation in one method
- the work stays inside `ObjectGraphics` and does not reopen the public render-facing `Object` surface
- the extraction reduces policy density without changing renderer responsibilities, AI ownership, or end-of-animation transition behavior

## Scope constraints kept

- no gameplay or content behavior changes
- no public `Object` or `ObjectGraphics` API changes
- no renderer rewrite, render-pass redesign, or AI extraction
- no `incrementFrame()` / `incrementAction()` behavior changes in the same patch
- no file-format, validator-schema, or content-loader changes

## Acceptance baseline

This pass is considered healthy when all of the following remain true:

- `updateAnimationRate()` remains behavior-preserving while delegating to narrower policy helpers
- mounted animation-rate behavior remains unchanged for scenery and non-scenery holders
- idle/boredom, stealth locomotion, and flying locomotion policy remain unchanged
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

Continue the `ObjectGraphics` cleanup incrementally, but keep it bounded. The next pass should target the end-of-animation transition seam without reopening the public render surface, for example by:

- separating end-of-action transition policy from `incrementFrame()` loop/freeze bookkeeping, or
- isolating `incrementAction()` / `_nextAnimation` resolution from frame/cache maintenance responsibilities

Do not mix that follow-on with renderer redesign, role-interface extraction, or broader subsystem decomposition unless a separate pass first narrows those ownership seams.
