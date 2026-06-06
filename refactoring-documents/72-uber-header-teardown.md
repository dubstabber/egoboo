# Uber-Header Teardown (reframed T3.3)

Snapshot date: 2026-06-06. Owner front: deep coupling reduction. Branch: `refactor/uber-header-teardown`.

## Why this supersedes the old T3.3 framing

The roadmap (T3.3, `19-refactoring-roadmap.md`) framed this as "shrink `egolib/egolib.h`'s
transitive reach and physically delete the uber-header." Scouting the live code corrected that:

- `egolib/egolib.h` (54 subsystem includes) has only ~24 direct includers; several are redundant no-ops.
- The **real** amplifier is `egolib/library/src/egolib/game/egoboo.h`. It is a *thin* header
  (gfx_rv alias, 7 game constants, 2 timer globals, `config_synch`) whose only structural harm is its
  first line `#include "egolib/egolib.h"`. ~60 files include `egoboo.h`, and **55 of them are headers**
  (`Object.hpp`, the 19 role interfaces, `mesh.h`, `graphic.h`, …) — so the 54-subsystem uber-header is
  propagated into essentially the whole game library transitively.

**Goal:** make every `egoboo.h`/`egolib.h` consumer self-sufficient in its egolib includes, then cut the
`egoboo.h → egolib.h` link (line is guarded today; see below). `egoboo.h` survives as a thin header that
only needs `typedef.h` + `egoboo_setup.h` + `<cstdint>` for its own body. Deleting `egolib.h` outright is
a later, optional stretch goal — far less valuable than cutting the `egoboo.h` propagation.

This is incremental and always-green: adding precise includes to a header is additive-safe while
`egolib.h` is still included by default; the payoff link-cut is the final pass.

## The probe + verification technique (reusable)

`egoboo.h` now carries a guard:

```cpp
#ifndef EGOBOO_NO_UBER_INCLUDE
#include "egolib/egolib.h"
#endif
```

- **Normal build** (no macro): `egolib.h` included → tree stays green during migration.
- **Scope probe / per-header check**: define `EGOBOO_NO_UBER_INCLUDE` to simulate the cut.

Per-header self-containment is checked in ~1s without a full rebuild, using the project's own flags
(`build/egolib/library/CMakeFiles/egolib-library.dir/flags.make`):

```bash
# /tmp/selfcheck.sh <header-rel-to egolib/library/src>   e.g. egolib/game/mesh.h
echo "#include \"$1\"" | g++ <CXX_FLAGS> <CXX_INCLUDES> -DEGOBOO_NO_UBER_INCLUDE -fsyntax-only -x c++ -
```

A header with `errors=0` under that command is self-contained for the post-cut world.

## Probe scope (2026-06-06, before Pass 1)

Full keep-going build with `egolib.h` neutralized: **185 of ~286 egolib TUs fail** — the true transitive
footprint. Root cause is **37 non-self-contained headers** cascading into those TUs, plus ~30 sources with
direct errors. Ranked by error count (blast radius):

| Header | errors | Header | errors |
|---|---|---|---|
| game/mesh.h | 2023 | game/CharacterMatrix.h | 40 |
| Entities/IParticleHandler.hpp ✅ | 520 | Logic/Team.hpp | 34 |
| game/graphic.h | 461 | game/GUI/UIManager.hpp | 34 |
| Entities/Object.hpp | 376 | game/GUI/Button.hpp | 32 |
| game/Graphics/Vertex.hpp ✅ | 276 | game/Graphics/Billboard.hpp | 32 |
| game/lighting.h | 180 | game/GUI/Material.hpp | 31 |
| Entities/ICharacterState.hpp ✅ | 147 | game/GUI/Label.hpp | 26 |
| Entities/IPhysical.hpp ✅ | 126 | game/graphic_prt.h | 25 |
| Entities/ParticleHandler.hpp | 121 | Entities/IRenderable.hpp ✅ | 20 |
| game/Inventory.hpp | 96 | game/Shop.hpp | 16 |
| Entities/IInventoryHolder.hpp ✅ | 72 | game/Graphics/ParticleGraphics.hpp | 14 |
| Entities/IAnimationControl.hpp ✅ | 58 | Entities/ITargetInfo.hpp ✅ | 14 |
| game/Graphics/TileList.hpp | 56 | game/graphic_mad.h | 12 |
| game/Graphics/RenderPass.hpp | 52 | game/Graphics/BillboardSystem.hpp | 11 |
| game/script_implementation.h | 48 | Entities/ITeamMember.hpp ✅ | 7 |
| game/Graphics/Camera.hpp | 42 | Passage/ObjectGraphics/script_compile/IBillboardSystem/link/graphic_fan | ≤4 |

(✅ = made self-contained in Pass 1.)

### Symbol → canonical (self-contained) home dictionary

The missing symbols concentrate in a few egolib subsystems:

| Symbols | Include |
|---|---|
| `Facing`, `Matrix4f4f` | `egolib/_math.h` |
| `Vector3f`, `Vector2f`, `AxisAlignedBox2f/3f`, `Ego::Math::constrain` | `egolib/_math.h` / `egolib/Math/_Include.hpp` |
| `oct_bb_t`, `bumper_t`, `normal_cache_t` | `egolib/bbox.h` |
| `slot_t`, `grip_offset_t` | `egolib/Logic/ObjectSlot.hpp` |
| `GLvertex`, `GLfloat`, `GLint`, `GLXvector*` | `egolib/game/Graphics/Vertex.hpp` / `egolib/Extensions/ogl_extensions.h` |
| `map_t`, `tile_dictionary_t`, `MAP_*`, `TILE_*`, `Index1D` | `egolib/FileFormats/map_file.h`, `map_tile_dictionary.h` (+ grid) |
| `ParticleProfileRef`, refs, `ENC_REF`, `PRO_REF`, `SKIN_T`, `ObjectRef`, `SFP8_T` | `egolib/typedef.h` |
| `LocalParticleProfileRef` | `egolib/Profiles/LocalParticleProfileRef.hpp` |
| `ModelAction` | `egolib/Graphics/ModelDescriptor.hpp` |
| `IDSZ2` | `egolib/IDSZ.hpp` |
| `XPType` | `egolib/Profiles/_Include.hpp` (NOT `ObjectProfile.hpp` — it has an `#error` direct-include guard) |
| `DamageType` | `egolib/Logic/Damage.hpp` |
| `Gender` | `egolib/Logic/Gender.hpp` |
| `Ego::Attribute` / `Ego::Perks` | `egolib/Logic/Attribute.hpp` / `egolib/Logic/Perk.hpp` |
| `egoboo_config_t` | `egolib/egoboo_setup.h` |

`override` / `Ego` / `float` / `pos` / `nrm` counts in the probe are cascade noise (secondary to a failed
base type), not real targets.

## Pass 1 — Entities role-interface cluster (2026-06-06, Pass 221)

Made all **19 role-interface headers** (`Entities/I*.hpp`) self-contained and removed their
`#include "egolib/game/egoboo.h"`, plus their two egolib leaf dependencies `Logic/ObjectSlot.hpp` and
`game/Graphics/Vertex.hpp`. Verified: each `errors=0` under `EGOBOO_NO_UBER_INCLUDE`; normal build clean;
`test.mod` warnings=0 errors=0; ctest 736/738 (only pre-existing #526/#527).

Note: this is preparatory — `Object.hpp` still pulls `egoboo.h` (directly + via other game headers), so no
TU's transitive set shrinks yet. The transitive payoff lands only when the link is cut (final pass).

## Remaining work-list (suggested leaf-upward order)

1. **Low-level egolib/game leaf headers**: `lighting.h`, `mesh.h` (keystone, 2023), `graphic.h`,
   `CharacterMatrix.h`, `graphic_mad.h`, `graphic_prt.h`, `graphic_fan.h`. Need Math + bbox + map-format +
   GL-vertex includes.
2. **Logic/Profiles**: `Team.hpp` (XPType already self-contained home), `Shop.hpp`.
3. **Entities**: `Object.hpp`, `ParticleHandler.hpp`, `Inventory.hpp`, `Passage.hpp`.
4. **Graphics/GUI**: `Vertex`-dependent and camera/billboard/material/label/button/UIManager headers,
   `TileList.hpp`, `RenderPass.hpp`, `ParticleGraphics.hpp`, `ObjectGraphics.hpp`, `BillboardSystem.hpp`,
   `IBillboardSystem.hpp`.
5. **Script**: `script_implementation.h`, `script_compile.h`, `link.h`.
6. **~30 source TUs** with direct errors (`mesh.c`, `ObjectGraphics.cpp`, `CharacterMatrix.c`,
   `ParticleGraphics.cpp`, `Material.cpp`, `RenderPasses.cpp`, … ) — narrow each `egoboo.h`/`egolib.h`
   include to precise includes.
7. **Final payoff pass**: remove the `#ifndef EGOBOO_NO_UBER_INCLUDE` guard + `#include "egolib/egolib.h"`
   from `egoboo.h`. Then narrow the ~24 direct `egolib.h` includers and shrink/delete `egolib.h`.
   Full build + validator + ctest + **smoke-run** (engine-init path is untested by the verify loop).

## How to resume

1. Rebuild the checker from `flags.make` (recipe above) → `/tmp/selfcheck.sh`.
2. Re-run the probe (neutralize the guarded include, keep-going build) to get the *current* reduced
   failure count and refresh the work-list — Pass 1 should have removed the role-interface cascade.
3. Take the next leaf header, add precise includes from the dictionary, drive its `selfcheck` to 0, then
   the next; keep the normal build green; commit per pass with a `71-completed-passes-log.md` entry.
