# ObjectGraphics Tint and Reflection Policy Pass

This document records the bounded `ObjectGraphics` tint/reflection-policy extraction completed on 2026-04-17 as the immediate follow-on to document 62.

## What changed

- extracted the render-tint policy that had remained inline inside `ObjectGraphics::getTint()` and `ObjectGraphics::getReflectionAlpha()` into private helper logic local to `ObjectGraphics.cpp`
- introduced a narrow internal tint-state carrier for effective alpha, light, sheen, and color-shift values
- split the old inline path into explicit internal stages:
  - derive base or reflection render state
  - apply local-player perception overrides
  - encode the final render tint for `CHR_ALPHA`, `CHR_LIGHT`, `CHR_PHONG`, and unchanged default cases
- kept the public render-facing `Object` surface unchanged:
  - `hasModelDescriptor()`
  - `getReflectionAlpha()`
  - `getTint(...)`
- extended the focused accessor regression coverage in `egolib/tests/egolib/tests/ObjectAccessors.cpp` with:
  - reflection tint/reflection-alpha coverage
  - local-player perception tint override coverage

## Why this pass next

Document 62 narrowed the remaining `ObjectGraphics` follow-on to two bounded options:

- split tint/reflection-policy helpers away from broader animation/update behavior, or
- narrow model-reset and animation-reset responsibilities inside `ObjectGraphics`

The tint/reflection branch was still the safer next seam:

- the renderer already consumes tint/reflection through stable `Object` forwarders rather than a public `ObjectGraphics` instance
- the logic is read-only and behaviorally coherent
- extracting it reduces `ObjectGraphics.cpp` policy density without widening into model lifecycle, animation ownership, or renderer redesign

## Scope constraints kept

- no gameplay or content behavior changes
- no new public `Object` or `ObjectGraphics` API surface
- no `setObjectProfile()` or animation-reset redesign in the same patch
- no renderer rewrite, render-pass redesign, or AI extraction
- no file-format or validator-schema changes

## Acceptance baseline

This pass is considered healthy when all of the following remain true:

- `Object` callers continue to use the existing render-facing forwarders unchanged
- tint/reflection behavior is preserved for baseline, reflection, and local-player perception cases
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

Continue the `ObjectGraphics` cleanup incrementally. The next bounded pass should stay inside `ObjectGraphics` and narrow model-reset / animation-reset responsibilities explicitly, for example by:

- separating the profile/model reset path inside `setObjectProfile()` from animation-start policy, or
- isolating animation reset/default-action selection from the broader profile application path

Do not mix that follow-on with renderer redesign, AI extraction, or broader role-interface work unless a separate pass first narrows those ownership seams.
