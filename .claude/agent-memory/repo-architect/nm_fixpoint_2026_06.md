---
name: nm-fixpoint-2026-06
description: nm fixpoint on 138 true egolib-library TUs (post mesh-AI+QuestLog absorptions, 2026-06-10 state). 0-blocker list, seam-cut candidates, stale artifact warning.
metadata:
  type: project
---

## Archive layout (confirmed 2026-06-10 end-of-day)

`egolib-foundation-base` 119 TUs ← {`egolib-physics` 6, `egolib-renderer` 29} ← `egolib-library` 138 TUs.

Passes that landed on 2026-06-10 after the frontier absorption:
- Core/System.cpp bootstrap seam-swap → base (115→117 base after mesh-AI, then QuestLog to 119)
- mesh-AI terrain seam: AStar.cpp + LineOfSight.cpp → base (117→119 wait, above is sequential)
- QuestLog + PlayerQuestLog absorb → base (seam cut via Log::activeTarget())
- getMeshPointer() broader cleanup (17 sites migrated, not an absorb)

## IMPORTANT: Stale artifact warning

The nm analysis requires filtering against ALL lower-layer `.o` paths (base + physics + renderer).
`egolib-library.dir` contains 46 stale `.o` files from pre-move builds (older timestamps).
Use `comm -23 lib_rel.txt all_lower_rel.txt` to find the true 167→138 actual library set.
Using basename overlap (47 stale) instead of path overlap gives incorrect results.

The `/tmp/` files from the analysis session:
- `/tmp/lower_defined.txt` — symbols defined in base+physics (7781 symbols)
- `/tmp/true_library_only_symbols.txt` — symbols only in library (9286 symbols)
- `/tmp/all_lower_rel.txt` — relative .o paths in all lower layers (166 entries)

## 0-blocker TUs in egolib-library (pure CMake moves, no source edits)

Only ONE after removing stale artifacts and renderer-already-assigned TUs:

| TU | Notes |
|---|---|
| `game/mesh_fx.c` | 0 library-only blockers. Includes `game/mesh.h` + `EngineContext.hpp` but those symbols are all in lower layers. Can move to foundation-base as-is. |

All other `Graphics/` and `Renderer/` TUs showing as 0-blocker were stale artifacts from the egolib-renderer carve — they ARE already in egolib-renderer.

## 1-3 blocker TUs (seam-cut candidates)

| TU | Blockers | Blocker symbols |
|---|---|---|
| `game/Graphics/Billboard.cpp` | 1 | Object full type (isTerminated/isBeingHeld/isInsideInventory) |
| `game/GUI/Component.cpp` | 1 | `Ego::GUI::Container::bringComponentToFront` |
| `Profiles/ObjectProfile_export.cpp` | 1 | `Object::getBaseAttribute()` |
| `game/GUI/Layout.cpp` | 2 | `Component::getWidth/getHeight` |
| `game/Graphics/TileList.cpp` | 3 | (game-layer) |
| `game/Module/module_spawn.c` | 3 | (game-layer) |
| `game/Module/Module_spawn_plan.cpp` | 3 | game.h constants, EngineContext::logTarget, convert_spawn_file_load_name |

## Top seam-cut candidates (ranked)

### TIER 1 — Trivial 0-blocker absorb (pure CMake)
- `game/mesh_fx.c` → `egolib-foundation-base`. No source edits needed. The EngineContext.hpp include is present but all symbols resolved from lower layers.

### TIER 2 — Small seam cuts (1-3 changes each)
- `game/Module/Module_spawn_plan.cpp` (3 blockers): (a) Remove game.h from Module_spawn_plan.hpp (only uses MAX_IMPORT_PER_PLAYER — relocate to egolib_config.h); (b) replace EngineContext::logTarget() with Log::activeTarget() for 3 log calls; (c) convert_spawn_file_load_name as callback param. 1 caller (module_spawn.cpp) needs updating.
- `game/GUI/Component.cpp` (1 blocker): `Container::bringComponentToFront` — check if it can be moved to Container.hpp inline or forwardable.
- `game/GUI/Layout.cpp` (2 blockers): `Component::getWidth/getHeight` — both are in the Component interface; may need Component.hpp cleanup first.

### TIER 3 — Component.hpp cluster (medium, unlocks 2 TUs)
Remove from `Component.hpp`:
- `#include "egolib/game/Core/GameEngine.hpp"` — `engine()` helper has 0 callers in `game/GUI/*.cpp`
- `#include "egolib/game/graphic.h"` — 0 symbols used in Component.hpp body
Move `uiManager()` protected inline from Component.hpp to Container.hpp.
Net: Component.cpp + Layout.cpp become absorbable (2 TUs). Container.cpp remains blocked (uses EngineContext::uiManager() in drawAll()).

### TIER 4 — ObjectProfile_export (medium)
`Profiles/ObjectProfile_export.cpp` (1 blocker): `Object::getBaseAttribute()`. Remove gratuitous GameEngine.hpp from ObjectProfile_internal.h; still blocked by Object.hpp unless Object attrs get an interface.

## Key non-seam blockers (avoid)

- **game/CharacterMatrix.c** (10 blockers): GameSessionContext, graphic_mad.h, renderer_3d.h, Module.hpp
- **Object.hpp** (library-layer): by-value ObjectPhysics/ObjectGraphics/Inventory composition = flag-day
- **Container.cpp**: EngineContext::uiManager() blocks whole Container/GameState/Panel cluster

## ::get() census (current, 668 total)

| Singleton | Count |
|---|---|
| EngineContext | 472 |
| GameSessionContext | 123 |
| video_buffer_manager | 12 |
| InputSystem | 8 |
| GraphicsSystemNew | 6 |
| TLT | 5 |
| egoboo_config_t | 5 |
| TextureManager | 4 |
| Console | 4 |

EngineContext (472) and GameSessionContext (123) are the primary dams — 88% of all ::get() calls.
video_buffer_manager (12) is the next highest non-routed singleton.

## Split file status (2026-06-10)

- **mesh.c**: SPLIT into 5 TUs (mesh.c 423L, mesh_fx.c 281L, mesh_geometry.c 366L, mesh_loader.c 114L, mesh_query.c 259L). Total 1443L vs original ~1370L (slight growth from headers). mesh_fx.c is foundation-base-ready (0 blockers). mesh_loader.c has 5 blockers (ego_mesh_t ctor/dtor/finalize + EngineContext::logTarget). mesh_query.c has 5 blockers (g_meshStats + ego_tile_info_t ops).
- **fileutil.c**: NOT SPLIT. Still monolithic at 598L. ReadContext.cpp is a separate 752L TU.

## Next roadmap sequencing

After the 2026-06-10 completions (mesh-AI terrain seam, getMeshPointer cleanup, QuestLog absorb), the nm fixpoint shows:
1. mesh_fx.c absorption (trivial, 0 blockers, pure CMake)
2. Component.hpp gratuitous include cleanup (unlocks 2 GUI TUs)
3. Module_spawn_plan seam (3 targeted edits)
4. egoboo.h → fileutil.c split (fileutil.c is 598L, still large but not urgent — no 0-blocker gain)
5. Entities ownership inversion (123 blockers, flag-day, DO NOT attempt)
