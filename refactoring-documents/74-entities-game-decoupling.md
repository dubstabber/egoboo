# 74 — Entities ↔ game include-decoupling front

> **STATUS: propagating-header slice COMPLETE — 2026-06-08; Collidable continuation COMPLETE — 2026-06-08;
> `.cpp`/internal-header slice COMPLETE — 2026-06-09.** Three sections below, newest last:
> (1) the propagating-header slice (6 passes); (2) the Collidable base→`Module.hpp` continuation
> (the deferred edge was cut + the T3.4 combat-integration net landed); (3) the `.cpp`/internal-header
> slice — `Object_internal.h` **12 → 2** and `Particle_internal.h` **5 → 2** game/ includes (branch
> `refactor/entities-game-cpp-decoupling`, passes A/E/F/G/H + the Pass C ownership note and Pass D
> `IBillboardSystem`-seam analysis).
> Build 0 / validator `test.mod` 0/0 / ctest **815/817** (the 2 perennial `ScriptLoaderFixture` cases) /
> render-path smoke-runs exit 124 every applicable pass.
> Kept as a technique/recipe reference; see [71-completed-passes-log.md](71-completed-passes-log.md)
> for the per-pass log entry and [19-refactoring-roadmap.md](19-refactoring-roadmap.md) (T3.7).

## Why this front

At the time of this front, `egolib-library` was still the monolithic gameplay archive.
The blocker to splitting it into idlib-shaped layered sub-libraries was **include
coupling**, not CMake. After the EngineContext service-hub fan-in was reduced to 8 leaves
(doc 71, service-hub slice), the next lib-split blocker shifted to the **Entities layer
reaching *up* into the game layer** via `#include`.

The structural harm is concentrated in **propagating headers** — a header's `game/`
include flows into *every* translation unit that includes it. The Entities layer has four
such headers, and three of them (`Object.hpp`, `Particle.hpp`, `Common.hpp`) are pulled by
huge swaths of the tree. Impl `.cpp`/`.c` includes are far less harmful (they don't
propagate) and were left for the deeper Entities↔game `.cpp` front.

## The load-bearing gotcha: dead-in-header ≠ safe-to-remove

A scouting workflow classified every `game/` include in the Entities headers by *usage in
the including header* (by-value member / base class / pointer / unused). That is necessary
but **not sufficient**: an include can be unused by the header itself yet serve as a
**transitive conduit** that downstream TUs free-ride on. Removing such an include compiles
the header fine but breaks the free-riders.

Examples hit in this front:
- `Object.hpp` → `graphic_mad.h` is dead in Object.hpp, but `graphic_mad.h` pulls the
  `game/egoboo.h` monolith, which was the only path feeding `ONESECOND` to 5 Entities TUs
  and `gfx_rv` etc. elsewhere.
- `Object.hpp` → `BillboardSystem.hpp` is dead in Object.hpp, but `GameEngine.cpp` /
  `graphic_scene.c` were getting the full `BillboardSystem` type through it.
- Splitting `physics.h` exposed `CollisionSystem.cpp` / `ObjectPhysics.cpp` /
  `ObjectProfile_export.cpp` (+ a test) that had been free-riding on `Object.hpp → physics.h`
  for the physics functions **and** `PhysicalConstants.hpp` symbols.

**Method:** after removing a propagating include, run a *keep-going* build
(`cmake --build build -j20 -- -k`) to enumerate the **complete** free-rider set in one pass
(per-TU first errors across all TUs, not just the first), then add the precise direct
include each free-rider actually uses (include-what-you-use). For game-layer free-riders
this is a clean `game → game` or `game → egolib-core` include — no new upward coupling.
Every free-rider in this front was a game-layer TU or a test; none were lower-layer.

## The three relocation patterns

When the "dead" include was actually a mislocated *primitive*, the right fix is to move the
primitive **down** a layer rather than re-include it sideways:

1. **Mislocated constant** — `ONESECOND` (50 UPS) lived in the heavy `game/egoboo.h` but is
   pure data needed by Entities timer code. Moved to `egolib/egolib_config.h` (already a
   low-layer compile-time-constants header, reached from egoboo.h transitively via
   `typedef.h`, so existing consumers were untouched). (Pass 2)
2. **Mislocated struct** — `GLvertex` (`game/Graphics/Vertex.hpp`) is a pure GL primitive
   (only `typedef.h` + `ogl_extensions.h`). `git mv`'d to `egolib/Graphics/Vertex.hpp`
   (beside `ModelDescriptor.hpp`/`VertexFormat.hpp`); 3 includers + 1 CMake line updated.
   `IRenderable.hpp` used it only as `const GLvertex&`, so it now **forward-declares**
   `struct GLvertex;` and is fully game-free. (Pass 4)
3. **Header with a primitive/function split** — `game/physics.h` mixed pure-math primitives
   (`orientation_t`, `apos_t`, `phys_data_t`, `PLATTOLERANCE`, `PLATFORM_STICKINESS`, the
   `PHYS_PLATFORM` enum) with game-aware free functions (`phys_expand_chr_bb` over
   `Object*`/`IPhysical*`/`Particle*`). Extracted the primitives to a new lower-layer
   header `egolib/PhysicsData.h` (pure idlib math: `bbox.h` + `_math.h`); `physics.h`
   includes it and keeps only the function declarations; the method definitions stay in
   `physics.c` (which sees the decls via `physics.h`). `Common.hpp` (uses `phys_data_t`)
   and `Object.hpp` (uses `orientation_t`) repoint at `PhysicsData.h`. (Pass 5)

## The 6 passes

| Pass | Change | Free-riders fixed (IWYU) |
|------|--------|--------------------------|
| 1 | Drop 4 dead game/ includes: `Common.hpp`→`mesh.h`, `Object.hpp`→`BillboardSystem.hpp`, `Object_internal.h`/`Particle_internal.h`→`CharacterMatrix.h` | `GameEngine.cpp`,`graphic_scene.c` (+BillboardSystem.hpp); `PlayingState.cpp`,`MiniMap.cpp`,`LevelUpWindow.cpp` (+`Time/Time.hpp`) |
| 2 | Relocate `ONESECOND` (50 UPS) → `egolib/egolib_config.h` | — (transparent via `typedef.h`) |
| 3 | Drop `Object.hpp`→`graphic_mad.h` + `Particle.hpp`→`graphic_prt.h` (each pulled the `egoboo.h` monolith) | `EntityReflectionsRenderPass.cpp` (+both), `EntityShadowsRenderPass.cpp` (+`graphic_prt.h`) |
| 4 | `git mv` `game/Graphics/Vertex.hpp` → `egolib/Graphics/Vertex.hpp`; forward-declare `GLvertex` in `IRenderable.hpp` | none |
| 5 | Split `physics.h` primitives → `egolib/PhysicsData.h`; repoint `Common.hpp`+`Object.hpp` | `CollisionSystem.cpp`,`ObjectPhysics.cpp`,`ObjectProfile_export.cpp`, `tests/ScriptMovementFunctions.cpp` |
| 6 | Drop `Object.hpp`'s redundant direct `Module.hpp` (flows via `Collidable.hpp` base) | none |

## Result

Propagating-header `game/` include edges: **15 → 7 (−53%)**.

| Header | Before | After |
|--------|:------:|:-----:|
| `Common.hpp` | 2 (`mesh.h`, `physics.h`) | **0 — game-free** |
| `IRenderable.hpp` | 1 (`Graphics/Vertex.hpp`) | **0 — game-free** |
| `Object.hpp` | 8 | 4 |
| `Particle.hpp` | 4 | 3 |

The **7 remaining edges are all genuine by-value compositions / base classes** — the
deferred hard core:
- `Object.hpp`: `Inventory.hpp` (member `_inventory`), `Physics/Collidable.hpp` (base),
  `Physics/ObjectPhysics.hpp` (member `_objectPhysics`), `Graphics/ObjectGraphics.hpp`
  (member `inst`).
- `Particle.hpp`: `Graphics/ParticleGraphics.hpp` (member `inst`), `Physics/Collidable.hpp`
  (base), `Physics/ParticlePhysics.hpp` (member).

## Deferred / out of scope (do NOT treat as easy follow-ons)

- **The by-value composition core.** `ObjectPhysics`/`ObjectGraphics`/`ParticleGraphics`
  take `Object&`/`Particle&` in their constructors (bidirectional); `Inventory` manages
  game-layer slots. These cannot be header-decoupled without relocating those game service
  classes a layer down or inverting ownership — **flag-day scale**, outside this front's
  incremental ceiling.
- **The `Collidable` base class** (on both `Object` and `Particle`) transitively pulls
  `game/Module/Module.hpp`. Reducible only via extracting a lower-layer collision interface
  (`ICollidable`) — and that must be **gated behind T3.4 live-fixture combat/collision
  characterization tests** first, since Object/Particle both spawn through this path and
  observable collision behaviour must be pinned before the surgery.
- **The internal headers + impl `.cpp`s** (`Object_internal.h`, `Particle_internal.h`,
  `Object_attributes.cpp`, `Object_combat.cpp`, `Enchant.cpp`, `ParticleHandler.cpp`) still
  reach `game/`. These are **non-propagating** (impl-only) and belong to the deeper, larger
  Entities↔game `.cpp` coupling front, not this propagating-header slice.

## Continuation (2026-06-08): the `Collidable` base edge was cut, and the T3.4 net landed

The deferred bullet above said the `Collidable` base → `Module.hpp` edge was "reducible **only** via
an `ICollidable` extraction gated behind T3.4 collision characterization tests." A re-scout found
that **partially stale** and the edge was cut the same day — see the two
[71-completed-passes-log.md](71-completed-passes-log.md) entries
("Collidable base-class `Module.hpp` conduit-cut" and "combat damage *integration*"). Summary:

- **The edge did not need `ICollidable`.** `Collidable.hpp`'s interface never names `GameModule`
  (only `Index1D`/`mesh_wall_data_t`/idlib vectors, all from `game/mesh.h`, which does not pull
  `Module.hpp`); the `GameModule` use is confined to `Collidable.cpp`. Swapping `Collidable.hpp`'s
  `Module.hpp` include → `mesh.h` (and adding the direct `Module.hpp` to `Collidable.cpp`) cut the
  conduit with **zero behavior change**.
- **The free-rider blast radius was much larger than the propagating slice's** — 32 TUs, because
  the conduit fed `Object.hpp`/`Particle.hpp` (near-tree-wide), and `Module.hpp` had also been the
  transitive carrier of `fileutil.h`/`Renderer`/`AudioSystem` for those TUs. All 32 IWYU-fixed with
  the precise include each uses (the doc-74 keep-going method). Measured: `Object.hpp` game/ closure
  **14→7**, `Particle.hpp` **13→6** — the whole `game/Module/` subtree gone from both.
- **The T3.4 gate is now in place.** `egolib/tests/egolib/tests/CombatDamageIntegration.cpp` (6
  cases) pins the integrated `Object::damage`/`kill` side-effect chain on a live module-spawned
  follower, complementing the existing `ObjectAccessors`/`ScriptMovementFunctions` position and
  `PhysicsCollisionNormal`/`PhysicsIntersection` wall-math fixtures.
- **`Collidable.hpp` is now fully game-include-free (the `ICollidable` header decoupling — DONE).**
  The residual `game/mesh.h` edge (the last `game/` include on the base) was cut the same day: the
  header needs only mesh *primitives* (`Index1D` from the game-free `egolib/Mesh/Info.hpp`, the
  idlib vector aliases, `BIT_FIELD`), and `mesh_wall_data_t` — the one game-mesh type, used only by
  reference in `hit_wall` — is **forward-declared** (it embeds a `const ego_mesh_t*`, so it can't
  relocate down). 3 free-rider TUs IWYU-fixed. Result: `Object.hpp` game/ closure **7 → 5**,
  `Particle.hpp` **6 → 4** (both shed `mesh.h` *and* `lighting.h`, reachable from these headers only
  via the `Collidable` base). See the [71-completed-passes-log.md](71-completed-passes-log.md)
  entry "Collidable header made fully game-include-free."
- **`Collidable` is now a fully lower-layer component (DONE).** The `activeModule()` runtime
  coupling was decoupled via a lower-layer `Ego::Physics::ICollisionWorld` DIP seam
  (`egolib/Physics/ICollisionWorld.{hpp,cpp}`; 2 methods `isInside` + `getTileIndex`, installed
  `active*()` accessor in the Log/service-hub keystone style). `GameModule` implements it and the
  game session installs/clears it across the `_activeModule` lifetime (beginModule before spawn /
  quitModule). `Collidable.{hpp,cpp}` then `git mv`'d to `egolib/Physics/` (beside `ICollisionWorld`,
  the seed of a future `egolib-physics` sub-library); `Collidable.cpp` now references **no game
  symbol** and has **0 `game/` includes**. `Object.hpp` game/ closure **5 → 4**, `Particle.hpp`
  **4 → 3** (the base header left the game/ count). Behavior-identical (ctest 815/817, position/
  respawn + combat-integration fixtures green, menu smoke-run clean). See the
  [71-completed-passes-log.md](71-completed-passes-log.md) entry "Collidable decoupled from
  GameModule + relocated to a lower layer."
- **Across the whole Collidable front:** `Object.hpp` game/ transitive closure **14 → 4**,
  `Particle.hpp` **13 → 3**. The remaining edges are exactly the by-value composition members +
  `CharacterMatrix.h`/`egoboo.h` — the deferred hard core. **Next:** `Collidable`'s sibling physics
  TUs (`CollisionSystem`/`ObjectPhysics`/`ParticlePhysics`/`particle_collision`) are still
  game-coupled; an actual `egolib-physics` link-target split remains gated on the broader
  Entities↔game `.cpp` coupling.

## The `.cpp` / internal-header slice — DONE (2026-06-09)

The "broader Entities↔game `.cpp` coupling" the propagating-header + Collidable
sections kept naming as *next*. This slice attacks the **shared internal infrastructure
headers** `Object_internal.h` and `Particle_internal.h` — the headers `#include`d by the
split `Object_*.cpp` (×7) and `Particle_*.cpp` (×4) implementation files. They are
**semi-propagating**: not tree-wide like `Object.hpp`, but a `game/` include in
`Object_internal.h` still flows into all seven `Object_*.cpp`, so every consumer pays for
includes only a few use. Branch `refactor/entities-game-cpp-decoupling`, 6 verified passes.

### The recurring pattern: header-used vs. conduit-only includes

The load-bearing distinction (generalising the propagating-slice's "dead-in-header ≠
safe-to-remove" gotcha): a shared header's `game/` includes split into

- **header-used** — referenced by the header's own inline helpers (e.g. `Object_internal.h`'s
  `object_detail::gameSession()` needs `GameSessionContext.hpp`; `activeModule()`/`selfHandle()`
  need `Module.hpp`). These must stay.
- **conduit-only** — not used by the header body at all; present only so the consumer `.cpp`s
  can free-ride. These are the reduction target: **push each down to the precise consumer that
  uses it** (include-what-you-use), enumerated with a keep-going build (`cmake --build -- -k 0`).

Method per pass: classify each include by header self-use (grep the header body), remove the
conduit-only ones, run `-- -k 0` to get the **complete** per-TU free-rider set, then add the
precise direct include each free-rider actually uses. Watch for the **within-TU cascade**: in
`Object_appearance.cpp` the `CameraSystem`-not-declared error suppressed a following
`TileList` incomplete-type error, which only surfaced after `CameraSystem` was fixed (a second
keep-going iteration caught it).

### The passes

| Pass | Change | Result |
|------|--------|--------|
| A | Route the four active `GameEngine::GAME_TARGET_UPS` (=50) Entities sites onto the existing low-layer `ONESECOND` seam (`egolib_config.h`, relocated there by propagating-slice Pass 2); drop `Object.hpp`-adjacent `GameEngine.hpp` from `Enchant.cpp`. Also unified an in-file inconsistency (`Object_update.cpp` used both names for 50). | `Enchant.cpp` game-engine-free; `GameEngine` no longer referenced anywhere in `Entities/` (it became a *dead* conduit edge, enabling E). |
| E | Confine the **minimap reveal chain** out of `Object_internal.h`: move the `tryActivePlayingState()` helper (sole consumer `Object_update.cpp`) into that TU; drop `EngineContext.hpp` (only that helper used it), `GameEngine.hpp` (dead post-A), `PlayingState.hpp`, `MiniMap.hpp` from the conduit. | `Object_internal.h` **12 → 8** game/ includes; 0 free-riders (the two billboard `.cpp`s already self-included `EngineContext`). |
| F | Push the remaining conduit-only includes down: `CameraSystem`/`TileList` → `Object_appearance`; `Billboard` → attributes/combat; `Player` → combat; plus the hidden-transitive `Physics/PhysicalConstants.hpp` (`CHR_INFINITE_WEIGHT`/`CHR_MAX_WEIGHT`) to appearance/attributes/update. `script_implementation.h` + (header-side) `TileList.hpp` were dead/redundant (0 free-riders). | `Object_internal.h` **8 → 3**. |
| G | Push the **`game.h` gravity-well** (272-line game-core free-function header) down to the five consumers that call its functions (`chr_do_latch_attack`, `disaffirm_attached_particles`, `DisplayMsg_printf`, `chr_stoppedby_tests`, …) + `Shop.hpp` (`Shop::drop`, which had free-ridden through `game.h`'s transitive tail) to interaction. `Object.cpp`/`Object_update.cpp` use neither and are freed. | `Object_internal.h` **3 → 2** — now exactly `GameSessionContext.hpp` + `Module.hpp` (its inline helpers' genuine deps). |
| H | Apply the same treatment to the sibling `Particle_internal.h`: drop `GameEngine.hpp` (dead post-A), `game.h` (→ spawn/update/combat), `Physics/PhysicalConstants.hpp` (`g_environment` → spawn). `Particle_core.cpp` needed none. | `Particle_internal.h` **5 → 2** (same minimal core). |

### Result

| Shared header | game/ includes before | after | propagates to |
|---------------|:---------------------:|:-----:|:-------------:|
| `Object_internal.h`   | 12 | **2** | 7 `Object_*.cpp` |
| `Particle_internal.h` | 5  | **2** | 4 `Particle_*.cpp` |

Both shared infrastructure headers now `#include` from `game/` **only the two headers their
own inline helpers use** (`GameSessionContext.hpp` + `Module.hpp`). All ten conduit-only edges
that used to flow through `Object_internal.h`
(`GameEngine`/`PlayingState`/`MiniMap`/`EngineContext`/`game.h`/`Player`/`script_implementation`/
`CameraSystem`/`TileList`/`Billboard`) and the three through `Particle_internal.h`
(`GameEngine`/`game.h`/`PhysicalConstants`) are gone from the propagating surface; the consumers
that genuinely use those symbols now carry **precise, non-propagating** impl includes. Compiled
code is byte-identical (pure include relocation, plus A's value-identical `50==50` constant swap).
All passes green: build `-- -k 0` 0-errors, validator `test.mod` 0/0, ctest -j1 **815/817** (the 2
perennial `ScriptLoaderFixture` cases), menu smoke-run clean OpenGL boot + graceful shutdown.

### Pass C — the genuine ownership residue (not a conduit)

After the conduit push-downs, the `.cpp` consumers' remaining `game/` includes are **genuine
service/ownership coupling**, not removable by include hygiene: `Enchant.cpp` and the `Object_*`/
`Particle_*` impls reach `GameSessionContext::activeModule().getObjectHandler()` /`spawnObject()`
for object lifetime + overlay-spawn, and `EngineContext::get()` for billboard/perk/config/log
services. Decoupling these needs **lower-layer DIP seams or ownership inversion**, not relocation —
the deferred hard core (see below), tracked separately from this include-hygiene slice.

### Pass D (analysis) — the `IBillboardSystem` seam is ready, but belongs to the EngineContext fan-in front

Scoped the billboard coupling (`Object_combat` ×5 + `Object_attributes` ×1
`EngineContext::get().billboardSystem()`/`tryBillboardSystem()`). Findings:

- **The interface already exists and is layer-portable.** `game/Graphics/IBillboardSystem.hpp`
  has *only* lower-layer includes (`egolib/integrations/color.hpp`, `egolib/typedef.h`, `<memory>`,
  `<string>`), so it can `git mv` cleanly to `egolib/Graphics/IBillboardSystem.hpp` with no
  game/ includes to drag. `EngineContext` already publishes the billboard system *through this
  interface* (`installBillboardSystem(GFX::get().getBillboardSystem())` at `GameEngine.cpp:333`).
- **Recipe is the proven Log/ICollisionWorld ownership-move keystone**, not a singleton swap:
  add a lower-layer `activeBillboardSystem()`/`tryActiveBillboardSystem()` free-function accessor
  (a `g_activeBillboardSystem` pointer + install/clear/try in a lower-layer TU), relocate the
  installed-pointer ownership out of `EngineContext` into it, leave `EngineContext`'s billboard
  methods as thin delegators. The keystone is **mandatory** because tests install a
  `StubBillboardSystem` *through* `EngineContext` and assert routing (`ScriptActionFunctions.cpp`
  `DrawBillboardUsesInstalledBillboardSystemForValidMessages`, `CombatDamageIntegration.cpp`) — a
  `BillboardSystem::get()` swap would bypass the stub.
- **But the seam alone frees no TU, so it belongs to the EngineContext fan-in front, not this
  slice.** `Object_attributes` also uses `EngineContext` for `logTarget` (×3) + `perkHandler` (×1);
  `Object_combat` for `config` (×1). Note `logTarget` and `config` *already* have lower-layer
  accessors (`Log::activeTarget()` from the logging slice; `activeConfig()` from the service-hub
  slice). So the achievable target is concrete: **`Object_combat.cpp` becomes `EngineContext.hpp`-free
  with exactly two moves — the billboard keystone + migrating its one `config()` call to
  `activeConfig()`**; `Object_attributes.cpp` additionally needs the `perkHandler` keystone (the
  low-ROI stub-hazard service the service-hub slice already flagged). Recommended as the next
  EngineContext-fan-in pass, sequenced after this slice.

## Deferred hard core (unchanged, restated for this slice)

The internal-header slice deliberately stops at include hygiene. The genuine Entities→game
*runtime* coupling that remains in the impl `.cpp`s — `activeModule().getObjectHandler()` object
lifetime, `spawnObject()` overlay-spawn, `EngineContext` billboard/perk services, the by-value
`ObjectPhysics`/`ObjectGraphics`/`ParticleGraphics` composition members — is **ownership-inversion /
DIP-seam scale**, not relocation, and is the next frontier toward an actual `egolib`-shaped
sub-library split.
