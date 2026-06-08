# 74 — Entities ↔ game include-decoupling front

> **STATUS: COMPLETE (incremental ceiling reached) — 2026-06-08.**
> Branch `refactor/entities-game-decoupling`, 6 verified passes. Build 0 / validator
> `test.mod` 0/0 / ctest **809/811** (the 2 perennial `ScriptLoaderFixture`
> PrimaryScript-fallback cases) / render-path smoke-runs exit 124 every applicable pass.
> Kept as a technique/recipe reference; see [71-completed-passes-log.md](71-completed-passes-log.md)
> for the per-pass log entry and [19-refactoring-roadmap.md](19-refactoring-roadmap.md) (T3.7).

## Why this front

`egolib-library` is one monolithic `add_library` (`egolib/library/CMakeLists.txt`). The
blocker to splitting it into idlib-shaped layered sub-libraries is **include coupling**,
not CMake. After the EngineContext service-hub fan-in was reduced to 8 leaves (doc 71,
service-hub slice), the next lib-split blocker shifted to the **Entities layer reaching
*up* into the game layer** via `#include`.

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
(`cmake --build build -j4 -- -k`) to enumerate the **complete** free-rider set in one pass
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
