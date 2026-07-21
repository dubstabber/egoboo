# Playtesting And Bug Hunt Plan

A risk-based playtesting strategy derived from code structure, content layout,
and known architecture weaknesses — a playtest matrix and bug-intake workflow,
not a list of confirmed bugs. Egoboo's highest-risk defects are data-driven,
timing-sensitive, module-specific, and script-specific: exactly what static
reading and unit tests miss. Playtesting is a development discipline here, not
final polish.

## 1. Highest-risk areas and what to test

| Area | Why risky | Core checks |
| --- | --- | --- |
| Startup / data paths | VFS mount order critical; overlay rules implicit | Clean-env start; with and without `EGOBOO_DATA_DIR`; module discovery |
| Module loading | One constructor path spans mesh, passages, profiles, spawn, AI compile, VFS | Load every module to first interactive frame; record failing asset per module |
| Import/export, saved chars | Special slots; temp dirs; export writes `data.txt`/`naming.txt` | Create/load/save/reload; finish with export on; abort with export off |
| Spawn and scripts | `spawn.txt` drives world state; compile and spawn tightly linked | Startup, scripted NPCs, loot/treasure tables, scripted deaths/messages/quests |
| Physics/collision/terrain | Legacy + new systems intersect at terrain, water, passages, movement | Ramps, pits, water walking, jumping, projectiles, particle-heavy crowds |
| Inventory/equipment/shops | Deep module reach; passage-owned shops; export touches inventory | Pick up/drop/equip, stacks, money, buy/sell, exit with carried items |
| UI/camera/input | States and camera depend on runtime state; overlays touch players | Menus, module select, HUD, character windows, pause transitions, input edge cases |

## 2. Module matrix

Small but diverse first wave; substitute equivalents if any are unsuitable but
keep the category coverage:

| Module | Coverage |
| --- | --- |
| `test.mod` | minimal smoke and parser sanity |
| `advent.mod` (or other starter) | early-game baseline |
| `heist.mod` | nontrivial scripted scenario |
| `palwater.mod` | water/environment behavior |
| `zombor.mod` | larger enemy/object variety |
| `worldmap.mod` | transition and meta-flow coverage |

## 3. Per-module smoke checklist

1. Discovered in menu? 2. Loads without hard failure? 3. Players spawn
correctly? 4. Scripted objects/NPCs behave at all? 5. Move/attack/interact
works? 6. One combat encounter resolves? 7. Inventory/equipment exercised?
8. Exits/wins/aborts cleanly? 9. Save/export works where allowed?
10. Reload after save/export behaves?

## 4. Bug capture

Record: module, player class/import used, exact action sequence, deterministic
or not, `log.txt` excerpt, screenshots/saves, and whether it predates the
current refactor branch. Categories: startup/path, module load, content parse,
script, AI, combat, collision/physics, inventory/shop, save/import/export,
UI/input, performance.

Known open runtime findings from past play-testing (example: wizard.mod
players fire homing missiles continuously without input, via the
`IfUsed`/`ALERTIF_USED` → `chr_do_latch_attack` latch chain) need runtime
debugging sessions, not static fixes.

## 5. Cadence

- **Per risky change**: boot, load `test.mod`, load one representative real
  module, exercise one save/import/export path if touched.
- **Per milestone**: the full module matrix, save/load/export, combat,
  inventory, and environment checks.

## 6. Priorities

1. Startup/paths, module load failures, save corruption, script hard breaks.
2. Collision/movement blockers, softlocks, inventory/shop corruption.
3. Rendering artifacts, audio oddities, cosmetic UI issues.

## 7. Supporting tooling status

- Module load validator: **exists** (`egoboo-content-validator`; baseline in
  `06-validator-baseline.md`). Dominant failure class is missing
  spawn-referenced objects, not parser crashes.
- Still wanted: an asset-inventory checker (orphaned directories, missing
  mandatory files, invalid naming, duplicate identifiers), structured
  phase-tagged module-load logging, and a persistent playtest result matrix
  (module / date / commit / result / bug links).
