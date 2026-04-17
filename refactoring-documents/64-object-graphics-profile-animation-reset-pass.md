# ObjectGraphics Profile / Animation Reset Pass

This document records the bounded `ObjectGraphics` profile/model-reset and animation-reset split completed on 2026-04-17 as the immediate follow-on to document 63.

## What changed

- decomposed `ObjectGraphics::setObjectProfile()` into explicit private stages inside `ObjectGraphics.cpp`:
  - `resetProfileApplicationState()`
  - `applyProfileRenderDefaults(const ObjectProfile&)`
  - `initializeProfileAnimation(const ObjectProfile&)`
- kept `setObjectProfile()` as the only public profile-application entrypoint and reduced it to a short orchestrator
- preserved the caller contract in `Object` lifecycle and polymorph paths:
  - `Object::respawn()`
  - `Object::polymorphObject(...)`
- extended focused accessor coverage in `egolib/tests/egolib/tests/ObjectAccessors.cpp` with characterization tests for:
  - direct `ObjectGraphics::setObjectProfile()` reset of profile-derived render defaults
  - matrix-cache invalidation during direct profile reset
  - live-object reset to idle animation policy
  - dead-object reset to `K`-family death animation policy

## Why this pass next

Document 63 narrowed the immediate follow-on to the remaining bounded seam inside `ObjectGraphics`:

- separate the profile/model reset path inside `setObjectProfile()` from animation-start policy, or
- isolate animation reset/default-action selection from the broader profile application path

That seam was still the right next move:

- `setObjectProfile()` remained the live coupling point for matrix/cache reset, model binding, render default restoration, and initial action selection
- the public `Object` render-facing surface was already stable after documents 61-63, so the next cleanup could stay internal
- this split reduces responsibility density in `ObjectGraphics` without widening into renderer redesign, AI extraction, or broader `Object` role-interface work

## Scope constraints kept

- no gameplay or content behavior changes
- no public `Object` or `ObjectGraphics` API changes
- no caller migration outside the existing `Object` lifecycle/polymorph paths
- no renderer rewrite, render-pass redesign, or AI extraction
- no file-format, validator-schema, or content-loader changes

## Acceptance baseline

This pass is considered healthy when all of the following remain true:

- `setObjectProfile()` remains behavior-preserving while delegating to narrower private helpers
- live objects still reset to idle `ACTION_DA` animation policy after profile application
- dead objects still reset to a `K`-family death animation and freeze behavior after profile application
- focused accessor tests remain green, including the new polymorph characterization coverage
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

Continue the `ObjectGraphics` cleanup incrementally, but keep it bounded. The next pass should stay inside animation/control policy rather than reopening the public render surface, for example by:

- separating default-action selection and progression policy from the broader `updateAnimationRate()` / `incrementAction()` path, or
- isolating animation-state mutation from frame/cache maintenance helpers

Do not mix that follow-on with renderer redesign, entity-interface extraction, or broader subsystem decomposition unless a separate pass first narrows those ownership seams.
