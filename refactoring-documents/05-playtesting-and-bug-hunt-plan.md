# Playtesting And Bug Hunt Plan

## 1. Status of this document

This audit did not execute a runtime playtest inside the current environment. The plan below is therefore a risk-based playtesting strategy derived from code structure, content layout, and known architecture weaknesses.

It should be treated as:

- a starting playtest matrix
- a bug intake workflow
- a way to prioritize validation while refactoring

It should not be treated as a list of confirmed runtime bugs.

## 2. Why playtesting must be part of the refactor

Egoboo is not a pure library project. Many of its highest-risk defects are likely to be:

- data-driven
- timing-sensitive
- module-specific
- script-specific
- dependent on load order or runtime state

Those are exactly the kinds of failures that static reading alone will miss.

## 3. Highest-risk gameplay areas

### A. Startup and data-path resolution

Why high risk:

- current Linux workspace already required source changes
- VFS mount order is critical
- content overlay rules are implicit

Test:

- start from clean environment
- start with explicit `EGOBOO_DATA_DIR`
- start without it
- verify module discovery and basic asset loading

### B. Module loading

Why high risk:

- module constructor performs many operations in one path
- failures can originate from mesh, passages, profiles, spawn data, AI compilation, or VFS overlay

Test:

- load every module to first interactive frame
- record failures with module name and failing asset/file

### C. Import/export and saved characters

Why high risk:

- special slot handling
- import object paths and temporary directories
- exported characters write `data.txt` and `naming.txt`

Test:

- create/load/save/reload character
- finish module with export enabled
- abort module with export disabled

### D. Spawn and script behavior

Why high risk:

- `spawn.txt` controls initial world state
- script compilation and object spawning are tightly linked
- dynamic slots and dependencies are nontrivial

Test:

- module startup
- scripted NPC behavior
- spawned loot and treasure tables
- scripted deaths, messages, and quest triggers

### E. Physics, collision, and terrain interaction

Why high risk:

- collision logic spans legacy and newer systems
- terrain, water, passages, and movement all intersect here
- several large files and TODO comments point to instability or ambiguity

Test:

- ramps and uneven terrain
- pits
- water walking
- jumping
- projectile collisions
- large crowds and particle-heavy fights

### F. Inventory, equipment, and shop logic

Why high risk:

- inventory reaches deeply into `_currentModule`
- shop logic depends on passage ownership and item state
- import/export also touches inventory contents

Test:

- pick up, drop, equip, unequip
- stacked items
- money collection
- buying and selling
- module exit with carried items

### G. UI, camera, and input flow

Why high risk:

- `PlayingState`, `UIManager`, and camera systems all depend on global runtime state
- debug and inventory overlays interact with current module and player objects

Test:

- menu navigation
- module select
- in-game HUD
- character windows
- pause/menu transitions
- keyboard and mouse edge cases

## 4. Suggested first-wave module matrix

Use a small but diverse module set first.

| Module | Why include it |
| --- | --- |
| `test.mod` | minimal smoke and parser sanity |
| `advent.mod` or another starter module | early-game baseline |
| `heist.mod` | nontrivial scripted scenario |
| `palwater.mod` or another water-heavy module | environment and water behavior |
| `zombor.mod` | larger enemy/object variety |
| `worldmap.mod` | transition and meta-flow coverage if still active |

If any of these are broken or unsuitable, replace them, but keep the category coverage.

## 5. Suggested smoke checklist per module

For each selected module:

1. Can the module be discovered in the menu?
2. Can it load without hard failure?
3. Do players spawn correctly?
4. Do nearby scripted objects/NPCs behave at all?
5. Can the player move, attack, and interact?
6. Does at least one combat encounter resolve?
7. Can inventory/equipment interaction be exercised?
8. Can the module be exited, won, or aborted cleanly?
9. If export is allowed, does save/export work?
10. Does reloading after save/export behave as expected?

## 6. Bug capture format

Every bug report should capture:

- module name
- player class or imported character used
- exact action sequence
- whether the issue is deterministic
- latest relevant `log.txt` excerpt
- screenshots or save files if relevant
- whether it appeared before or after a refactor branch

Minimum structured bug categories:

- startup/path
- module load
- content parse
- script
- AI
- combat
- collision/physics
- inventory/shop
- save/import/export
- UI/input
- performance

## 7. Tooling that should be added early

### Tool 1: module load validator

Headless or near-headless check that reports:

- missing files
- parse failures
- bad slot references
- duplicate slot problems
- mesh load failures
- script compile failures

Status:

- first implementation now exists as `egoboo-content-validator`
- baseline results are recorded in `06-validator-baseline.md`
- current dominant failure class is missing spawn-referenced objects rather than parser crashes

### Tool 2: asset inventory checker

Reports:

- orphaned object directories
- missing mandatory files per object/module type
- invalid file naming patterns
- duplicate identifiers

### Tool 3: structured runtime logging for module load phases

Log distinct phases:

- VFS setup
- profile load
- mesh load
- passage load
- spawn parse
- AI compile
- first frame

### Tool 4: playtest result tracker

Keep a markdown or CSV matrix with:

- module
- date
- build/commit
- tester
- result
- bug links

## 8. Refactor-era playtesting cadence

Use two levels of playtesting:

### Per-change smoke

For every risky refactor touching runtime or content loading:

- boot game
- load `test.mod`
- load one representative real module
- verify one save/import/export path if touched

### Milestone regression

At the end of each major phase:

- full selected module matrix
- save/load/export checks
- combat and inventory checks
- environment checks

## 9. Suggested first bug-hunt priorities

Priority should follow architecture risk, not just player visibility.

### Priority 1

- startup and path handling
- module load failures
- save/import/export corruption
- script compile/runtime hard breaks

### Priority 2

- collision and movement blockers
- softlocks in modules
- inventory/shop corruption

### Priority 3

- rendering artifacts
- audio oddities
- cosmetic UI issues

## 10. Bottom line

The project needs playtesting as a development discipline, not as a final polish step.

The most useful immediate work is:

1. choose a stable module smoke matrix
2. add validation tooling around loaders
3. record results systematically
4. run smoke tests after every major refactor step
