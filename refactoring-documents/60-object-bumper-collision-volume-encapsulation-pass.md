# Object Bumper and Collision-Volume Encapsulation Pass

This document records the bounded `Object` bumper / collision-volume encapsulation cleanup completed on 2026-04-17 as the immediate follow-on to document 59.

## What changed

- added accessor methods on `Object` for a ninth selected state group:
  - initial/profile-seeded bump state
  - current runtime bump state
  - saved/base bump state
  - downgraded loose bump state
  - min and max character collision volumes
  - per-slot collision volumes
- moved `bump_stt`, `bump`, `bump_save`, `bump_1`, `chr_max_cv`, `chr_min_cv`, and `slot_cv` behind a private boundary in `Object.hpp`
- added narrow write-side helpers so spawn/bootstrap and `ObjectPhysics` can seed or publish collision state without re-exposing mutable field access
- rewired `ObjectPhysics::updateCollisionSize()` to rebuild local collision data and publish it through the new accessor surface
- migrated non-`Object` callers in module spawn/bootstrap, physics, targeting, scripts, particles, camera tracking, and render/debug code to the accessor surface
- extended the focused accessor regression coverage in `egolib/tests/egolib/tests/ObjectAccessors.cpp`

## Why this pass next

Document 59 left two realistic follow-ons while staying out of role-interface extraction:

- a deliberate collision-shape pass around bumper / collision-volume state, or
- the broader render-facing pass around `inst`

The codebase still favored the collision-shape branch first:

- the remaining bumper / collision-volume data was still a coherent public seam in `Object.hpp`
- that state was still mostly mechanical and physics-facing enough to seal behind accessors without widening into render/animation ownership
- the `inst` branch still crosses rendering, animation, matrix-cache, combat, and particle placement code, which would have materially increased patch scope

## Scope constraints kept

- no gameplay or content behavior changes
- no `inst` or `ObjectGraphics` ownership work in the same patch
- no `ai` extraction or role-interface work
- no file-format or validator-schema changes
- no `phys.bumpdampen` or broader physics-state encapsulation in the same patch

## Acceptance baseline

This pass is considered healthy when all of the following remain true:

- the selected bumper / collision-volume fields are private in `Object.hpp`
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

Continue the `Object` cleanup incrementally. The next bounded pass should stay out of role-interface extraction and target the broader render-facing `inst` seam explicitly.

Do not mix that follow-on with `ai` extraction unless a separate pass first narrows that ownership boundary.
