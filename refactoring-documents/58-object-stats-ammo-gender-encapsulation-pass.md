# Object Stats, Ammo, and Gender Encapsulation Pass

This document records the bounded `Object` stats/ammo/gender scalar encapsulation cleanup completed on 2026-04-17 as the immediate follow-on to document 57.

## What changed

- added accessor methods on `Object` for a seventh selected state group:
  - gender state
  - experience scalar state
  - raw experience-level index state
  - ammo maximum / current scalar state
- moved those backing fields behind a private boundary in `Object.hpp`
- migrated non-`Object` callers in spawn/bootstrap, spawn realization, scripts, combat, inventory, GUI, message formatting, and export code to the accessor surface
- extended the focused accessor regression coverage in `egolib/tests/egolib/tests/ObjectAccessors.cpp`

## Why this pass next

Document 57 recommended returning to the remaining stats/ammo/gender and level-state scalar surface before attempting `inst`, `ori`, or any role-interface extraction.

The current codebase still favored that bounded scalar branch:

- `gender`, `experience`, `experiencelevel`, `ammomax`, and `ammo` were the last small, high-traffic public scalar cluster outside the broader `ai`, `inst`, and collision/orientation seams
- those fields were still read or written directly by spawn/bootstrap, scripts, combat, inventory stacking, GUI, export, and message-formatting code
- this pass keeps the sequence mechanical and behavior-preserving without widening scope into render-facing or physics-facing structures

## Scope constraints kept

- no gameplay or content behavior changes
- no `ai`, `inst`, `ori`, bumper, or collision-volume encapsulation in the same patch
- no role-interface extraction
- no file-format or validator-schema changes
- no ammo or experience policy redesign beyond replacing direct field access

## Acceptance baseline

This pass is considered healthy when all of the following remain true:

- the selected stats/ammo/gender fields are private in `Object.hpp`
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

Continue the `Object` cleanup incrementally. The next bounded pass should stay out of role-interface extraction and focus on either:

- another clearly isolated remaining public-state seam around `ori` / collision-adjacent state if the goal stays mechanical, or
- a deliberate graphics-facing accessor pass around `inst` once the team is ready to cross that broader render/animation boundary

Do not mix either follow-on with `ai` extraction unless a separate pass first defines a narrower ownership seam for that state object.
