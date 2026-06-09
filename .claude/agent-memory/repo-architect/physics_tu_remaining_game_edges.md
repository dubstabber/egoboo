---
name: physics-tu-remaining-game-edges
description: Complete edge inventory of remaining game/ coupling in Physics TUs after IObjectWorld seam merge (CollisionSystem, ObjectPhysics, ParticlePhysics, particle_collision)
metadata:
  type: project
---

Surveyed 2026-06-09. All four TUs are in egolib/library/src/egolib/game/Physics/.

## Confirmed clean (no game/ edges after IObjectWorld merge)
- ParticlePhysics.cpp — only game/ include is egolib/game/CharacterMatrix.h (for chr_getMatUp); header itself is clean (no game/ deps in CharacterMatrix.h), but CharacterMatrix.c implementation pulls graphic_mad.h + renderer_3d.h + Module.hpp

## Remaining game/ edges by TU

### CollisionSystem.cpp
1. `egolib/game/Core/GameSessionContext.hpp` — only symbol used: `GameSessionContext::get().worldUpdateCount()` at line 48, called at line 357
2. `egolib/game/physics.h` — symbols used: `phys_expand_chr_bb` (line 310), `phys_expand_prt_bb` (line 364), `phys_intersect_oct_bb` (lines 413, 446), `phys_expand_oct_bb` (lines 783–784), `phys_estimate_collision_normal` (line 786), `phys_estimate_pressure_normal` (line 791)

### ObjectPhysics.cpp
1. `egolib/game/Core/GameSessionContext.hpp` — symbol: `GameSessionContext::get().worldUpdateCount()` at line 55, called at lines 374, 516
2. `egolib/game/Shop.hpp` — symbol: `Shop::canGrabItem` at line 844
3. `egolib/game/CharacterMatrix.h` — symbols: `chr_matrix_valid` (line 91), `mat_getTranslate` (lines 93, 930), `set_weapongrip` (line 926), `chr_update_matrix` (lines 928, 1014), `chr_calc_grip_cv` (line 1089)

### ParticlePhysics.cpp
1. `egolib/game/CharacterMatrix.h` — symbol: `chr_getMatUp` (lines 426, 616)
   — No GameSessionContext, no game/physics.h, no Shop

### particle_collision.c
1. `egolib/game/Core/GameSessionContext.hpp` — symbol: `GameSessionContext::get().worldUpdateCount()` at line 50, called at line 1358
2. `egolib/game/physics.h` — symbols: `phys_estimate_pressure_normal` (lines 436, 475), `phys_expand_oct_bb` (lines 459, 460, 497, 498), `phys_estimate_collision_normal` (lines 463, 502), `phys_intersect_oct_bb` not used here (that's in CollisionSystem)
3. `egolib/game/graphic.h` — **NO direct symbols used**; included solely for its transitive chain: graphic.h → EngineContext.hpp → IAudioSystem.hpp → GSND_ enum + `activeAudioSystem()`. This entire include can be replaced with `#include "egolib/Audio/IAudioSystem.hpp"` directly.
4. `egolib/game/CharacterParticleOps.h` — symbols: `chr_get_lowest_attachment` (lines 949–950), `reaffirm_attached_particles` (line 1260). Header itself is clean (no game/ deps). Implementations live in game_combat.c and game_loop.c.
5. `egolib/game/Graphics/Billboard.hpp` — symbol: `Ego::Graphics::Billboard::Flags::All` (BIT_FIELD constant used as opt_bits in activeBillboardSystem().makeBillboard calls). Header itself has no game/ deps (only integrations/color, _math.h, typedef.h).
6. `egolib/game/Graphics/BillboardSystem.hpp` — appears to be transitively included but BillboardSystem.hpp only includes IBillboardSystem.hpp + lower-layer types; the concrete `Ego::Graphics::activeBillboardSystem()` is declared in `egolib/Graphics/IBillboardSystem.hpp` which is already directly included.

## Classification of each edge

### worldUpdateCount() strand (3 TUs: CollisionSystem, ObjectPhysics, particle_collision)
**Classification: seamable**
Pattern: exactly mirrors how IObjectWorld/ICollisionWorld were built. Solution: add `virtual uint32_t worldUpdateCount() const = 0` to IObjectWorld (or create a new `IWorldClock` single-method interface), install/clear alongside the existing IObjectWorld. The three file-local `worldUpdateCount()` wrappers (lines CollisionSystem:46–49, ObjectPhysics:53–56, particle_collision:48–51) would call `activeObjectWorld().worldUpdateCount()` or `activeWorldClock().worldUpdateCount()` instead of `GameSessionContext::get().worldUpdateCount()`, and the `#include "egolib/game/Core/GameSessionContext.hpp"` would be dropped from all three TUs.

### game/physics.h edge (CollisionSystem + particle_collision)
**Classification: seamable — by header relocation**
physics.h itself already only includes bbox.h, PhysicsData.h, PhysicalConstants.hpp — ALL lower-layer. The 6 function declarations are pure oct_bb_t math with no game-layer deps. physics.c includes game.h and mesh.h as dead includes (no symbols from either are actually used). Plan: (a) move physics.h to egolib/Physics/OctBBGeometry.h (or similar), (b) remove dead game.h + mesh.h includes from physics.c, (c) update the two TU includes to the new path. This completely eliminates the game/ edge with a 3-file relocation.

### game/CharacterMatrix.h edge (ObjectPhysics + ParticlePhysics)
**Classification: genuinely-game** (header is clean but impl pulls graphic_mad.h + renderer_3d.h + Module.hpp + GameSessionContext.hpp)
CharacterMatrix.c's chr_update_matrix calls into renderer/graphics for matrix updates. The functions chr_matrix_valid/set_weapongrip/chr_update_matrix/chr_calc_grip_cv are deeply coupled to the rendering matrix pipeline. This edge cannot be moved to a lower layer without a significant refactor of the rendering/matrix separation. Hard-blocker tier.

### game/Shop.hpp edge (ObjectPhysics only)
**Classification: genuinely-game**
Shop.cpp includes GameSessionContext.hpp + Module.hpp + Passage.hpp + game.h. Shop::canGrabItem checks shop passages and money. The shop logic is intrinsically tied to game modules. Not seamable without extracting a pure IShopPolicy interface. This is the grabStuff() method — it's fundamentally gameplay, not physics.

### game/graphic.h edge (particle_collision only)
**Classification: seamable — trivial removal**
graphic.h is included solely for the transitive IAudioSystem.hpp chain. No graphic.h-declared symbol is used. Replace with `#include "egolib/Audio/IAudioSystem.hpp"` and drop the graphic.h include. One-line fix.

### game/Graphics/Billboard.hpp edge (particle_collision only)
**Classification: already-clean** (header has no game/ deps)
`Billboard::Flags::All` is a BIT_FIELD constant (FULL_BIT_FIELD). The header does not include any game/ headers. The game/ path prefix is cosmetic; the content is layer-neutral.

### game/CharacterParticleOps.h edge (particle_collision only)
**Classification: already-clean** (header has no game/ deps)
CharacterParticleOps.h includes only typedef.h, ObjectSlot.hpp, _math.h, integrations/math.hpp. Implementations (chr_get_lowest_attachment in game_combat.c, reaffirm_attached_particles in game_loop.c) are game-layer, but the header boundary is clean.

## Immediate next steps (in priority order)
1. **worldUpdateCount seam** (3 TU impact): Add `worldUpdateCount()` to IObjectWorld (cheapest, mirrors existing pattern). GameModule already implements IObjectWorld; add `uint32_t worldUpdateCount() const override` that calls `GameSessionContext::get().worldUpdateCount()` — no new .cpp file needed. Removes GameSessionContext.hpp from CollisionSystem.cpp, ObjectPhysics.cpp, particle_collision.c.
2. **physics.h relocation** (2 TU impact): Move egolib/game/physics.h → egolib/Physics/OctBBGeometry.h, strip dead includes from physics.c. Removes the physics.h game/ edge from CollisionSystem + particle_collision.
3. **graphic.h → IAudioSystem.hpp** (1 TU, 1 line): In particle_collision.c replace `#include "egolib/game/graphic.h"` with `#include "egolib/Audio/IAudioSystem.hpp"`. Trivial.

**Why:** After steps 1+2+3, particle_collision.c and CollisionSystem.cpp would have ZERO game/ includes. ObjectPhysics.cpp would retain only Shop.hpp + CharacterMatrix.h (both genuinely-game).
