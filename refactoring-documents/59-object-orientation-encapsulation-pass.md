# Object Orientation Encapsulation Pass

This document records the bounded `Object` orientation encapsulation cleanup completed on 2026-04-17 as the immediate follow-on to document 58.

## What changed

- added accessor methods on `Object` for an eighth selected state group:
  - current facing state
  - map-twist facing state on the X and Y axes
  - previous-frame facing state
- moved `ori` and `ori_old` behind a private boundary in `Object.hpp`
- migrated non-`Object` callers in spawn/bootstrap, physics, scripts, combat, particles, enchantment, and matrix-update code to the accessor surface
- extended the focused accessor regression coverage in `egolib/tests/egolib/tests/ObjectAccessors.cpp`

## Why this pass next

Document 58 recommended two realistic follow-ons while staying out of role-interface extraction:

- a narrower mechanical pass around `ori` and adjacent orientation flow, or
- a broader render-facing pass around `inst`

The codebase still favored the orientation branch first:

- `ori` and `ori_old` remained a high-traffic public seam across spawn, physics, scripting, particle logic, and matrix construction
- the orientation state was still scalar and mechanical enough to seal behind accessors without widening into graphics ownership
- the `inst` branch would have crossed the broader render/animation/matrix-cache boundary and materially increased patch scope

## Scope constraints kept

- no gameplay or content behavior changes
- no `inst` or `ObjectGraphics` ownership work in the same patch
- no bumper / collision-volume encapsulation in the same patch
- no `ai` extraction or role-interface work
- no file-format or validator-schema changes

## Acceptance baseline

This pass is considered healthy when all of the following remain true:

- `ori` and `ori_old` are private in `Object.hpp`
- non-`Object` callers use orientation accessors instead of direct field access
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

Continue the `Object` cleanup incrementally. The next bounded pass should stay out of role-interface extraction and choose one of:

- a deliberate collision-shape pass around the remaining bumper / CV structs if the goal stays on mechanical non-graphics state, or
- the broader `inst` accessor pass once the team is ready to cross the render/animation boundary explicitly

Do not mix either follow-on with `ai` extraction unless a separate pass first narrows that ownership seam.
