# Uber-Header Teardown (reframed T3.3)

**STATUS: COMPLETE (2026-06-07) — historical record + reusable technique reference.** The `egolib.h` uber-header was deleted; see `71-completed-passes-log.md` and `19-refactoring-roadmap.md` T3.3. The `EGOBOO_NO_UBER_INCLUDE` selfcheck technique and the symbol→home dictionary below remain useful reference.

Snapshot date: 2026-06-06. Owner front: deep coupling reduction. Work merged to `master` (the original `refactor/uber-header-teardown` branch is no longer the live source of truth).

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

## Pass 2 — graphics/logic/script leaf headers (2026-06-06, Pass 222)

Made 11 more headers self-contained: `game/lighting.h`, `game/mesh.h` (the keystone), `game/graphic.h`,
`game/graphic_fan.h`, `game/graphic_mad.h`, `game/graphic_prt.h`, `Logic/Team.hpp`, `game/Shop.hpp`,
`game/link.h`, `game/script_compile.h`, `game/script_implementation.h`. Dropped `egoboo.h` from 9; kept a
thin `egoboo.h` in `graphic_mad.h`/`graphic_prt.h` (they use `gfx_rv`). Used `class Object;` /
`class ObjectProfile;` forward decls where only pointer/`shared_ptr`/`ObjectRef` is used.

**Lesson (important):** per-header `-fsyntax-only` selfcheck is necessary but NOT sufficient — it cannot see
a *source* file that was leeching transitive includes through a header you just narrowed. After narrowing,
run a **keep-going full build** (`cmake --build build -j4 -- -k`) and grep real `error:` lines to catch
those. Pass 2 caught exactly one: `FileFormats/SpawnFile/SpawnFileReaderImpl.cpp` lost `Ego::trim_ws`
(→`Core/StringUtilities.hpp`) and `Info<float>::Grid::Size()` (→`FileFormats/map_file.h`, the grid `Info`
template specialization — NOT `Mesh/Info.hpp`'s `Ego::MeshInfo`) via `Team.hpp`. Two common leech symbols:
`Ego::trim_ws` and the unqualified grid `Info<float>`/`Info<int>` template.

`CharacterMatrix.h` was deferred from Pass 2 (it has inline SDL/matrix logic: `matrix_cache_t`,
`EulerFacing`, `SDLK_LEFT`, grip helpers — needs more includes than a leaf header).

## Pass 3 — camera/module/physics/inventory + leaf interface headers (2026-06-06, Pass 223)

Narrowed 15 more headers off `egoboo.h` (kept thin `egoboo.h` only in `TileList.hpp` for `gfx_rv`):
`game/Graphics/{Camera,CameraSystem,EntityList,TileList,IBillboardSystem,ICameraSystem,Md2ModelRenderer}.hpp`,
`game/{game.h,game_internal.h,Inventory.hpp,physics.h,script_functions.h}`,
`game/Module/{Passage,Module}.hpp`, `game/GameStates/LoadPlayerElement.hpp`. Leech-fix:
`DefaultMd2ModelRenderer.hpp` (+`integrations/video.hpp` for `idlib::vertex_descriptor`, +`<vector>`).
Cascade win: `Object.hpp` is now down to a single `#error` (direct-include guard), not real coupling.

**Common leech-fix dictionary (add when a narrowed header's source consumers break):**
`Ego::trim_ws`→`Core/StringUtilities.hpp`; grid `Info<float/int>`→`FileFormats/map_file.h`;
`idlib::vertex_descriptor`→`integrations/video.hpp`; Ref types (`ObjectRef`,`BIT_FIELD`,`SKIN_T`,…)→`typedef.h`.

## Status after Pass 3 — headers still pulling `egoboo.h`

*(historical — front complete; all headers below were since narrowed and the link cut in Passes 224–226.)*

**Legitimate thin-`egoboo.h` keeps (use `gfx_rv`; leave until gfx_rv is retired or rehomed):**
`TileList.hpp`, `graphic_mad.h`, `graphic_prt.h`, `Graphics/IGFX.hpp`, `Graphics/ParticleGraphics.hpp`.

**Still need narrowing before the link can be cut (with their known needs):**
- `Entities/Object.hpp`, `Entities/ObjectHandler.hpp` — have an `#error` direct-include guard, so verify via
  `Entities/_Include.hpp`, not standalone. Object.hpp's real coupling is already ~gone (1 guard error only).
- `Entities/Particle.hpp`, `Entities/ParticleHandler.hpp` — need `prt_ori_t` (in `Profiles/ParticleProfile.hpp`,
  which itself has an `#error` guard → reach it via `Profiles/_Include.hpp`; verify the alias resolves).
- `Graphics/ParticleGraphics.hpp` — keeps `egoboo.h` for gfx_rv but also needs `Matrix4f4f`/`Vector3f`/`prt_ori_t`.
- `Graphics/Billboard.hpp`, `Graphics/BillboardSystem.hpp` — `Colour3f/4f`→`integrations/color.hpp`,
  `Texture`/`RefKind::Texture`→`typedef.h` + fwd-decl `Ego::Texture`, `Object`/`ObjectRef`→fwd-decl + `typedef.h`,
  `Vector3f/4f`/`Time`→`_math.h`/`Time/Time.hpp`.
- `GUI/UIManager.hpp`, `GUI/Material.hpp` — `Colour4f`→`integrations/color.hpp`, `Point2f`/`Rectangle2f`/`Vector2f`→
  `_math.h`/Math, `ego_frect_t`/`Texture`/`RefKind::Texture`→find homes + `typedef.h`.
- `game/CharacterMatrix.h` — `EulerFacing`/`Facing`/`Vector3f`/`float_t`→`_math.h`, `oct_bb_t`→`bbox.h`,
  `slot_t`/`GRIP_VERTS`/`SLOT_LEFT`→`Logic/ObjectSlot.hpp`, `SDLK_LEFT`→an SDL include; `matrix_cache_t` is
  self-defined (cascade). The messiest remaining header.

## Pass 224 + Pass 225 — last headers, then CUT (2026-06-06)

**Pass 224** self-contained the final 10 consumer headers (the 4 `#error`-guarded Entities headers verified
via `Entities/_Include.hpp`; Billboard/BillboardSystem/UIManager/Material via `integrations/{color,math,video}.hpp`
+ `Time/Time.hpp` + forward decls; CharacterMatrix via `_math.h`/`bbox.h`/`ObjectSlot.hpp`/`<array>`). Leech-fixes:
`ObjectGraphics.hpp` (+`Profiles/_Include.hpp`), `Material.cpp` (+`Renderer/Renderer.hpp`).

**Pass 225 — THE LINK IS CUT.** Removed `#include "egolib/egolib.h"` + the `EGOBOO_NO_UBER_INCLUDE` guard from
`game/egoboo.h`; it is now a thin header. A keep-going build surfaced ~20 sites still leeching egolib types
through the egoboo.h chain — fixed with precise includes (no code changed):
- 16 source TUs via a 4-agent parallel workflow (source files are include-graph leaves → safe to fix in parallel,
  each verified with `g++ -fsyntax-only`): `egoboo_setup.c`, `RenderPass.cpp`, 3 RenderPasses TUs, 5 GameStates,
  5 GUI.
- 3 headers that were NOT egoboo.h includers but leeched transitively (so they never showed up in the
  "includes egoboo.h" scan — a blind spot): `Graphics/RenderPass.hpp` (+`Clock.hpp`), `GUI/Button.hpp` +
  `GUI/Label.hpp` (+`Graphics/Font.hpp`, needed COMPLETE `Ego::Font` for a `LaidTextRenderer` member).
- executable `egoboo/src/game/Main.cpp` (+`Core/System.hpp`).

**Lesson:** narrowing only headers that *include* egoboo.h missed headers that leeched egolib.h transitively
*through* an egoboo.h-including header. The keep-going build after the cut is what caught them — always cut +
keep-going-build to find the true tail.

**State: DONE.** egoboo.h has 0 `egolib.h` dependency; build/validator/ctest 736/738 green across all targets
(`egoboo`, content-validator, tests). Optional follow-on: `egolib.h` still has ~22 direct includers; narrowing
them + deleting `egolib.h` is separate and lower-value. A menu smoke-run is advisable as a final boot-path check
(the cut is pure include hygiene — no runtime behavior changed).

## Pass 226 — `egolib.h` DELETED — FRONT COMPLETE (2026-06-07)

The "optional follow-on" was executed. `egolib.h` had **18 direct includers** at this point (12 headers + 6 sources —
fewer than the ~22 quoted above because Passes 221–224 had already narrowed several). Same technique, one level up:
a temporary `#ifndef EGOLIB_NO_UBER_INCLUDE` guard around `egolib.h`'s body made each includer probe-able hermetically.

- **Phase A — narrow the 18 includers** (precise includes, no code changed). The reusable insight reappeared:
  put each include at the **lowest correct header** so consumers inherit it — `game/game.h` (uses
  `wawalite_camera/graphics/data/physics_t` + `add_linebreak_cpp`) got `FileFormats/wawalite_file.h` + `strutil.h`
  + a `struct script_state_t;` fwd-decl; `Module/module_spawn.h` (declares fns taking `spawn_file_info_t`) got
  `SpawnFile/spawn_file.h`. Fixing those two cascaded `game_internal.h` + `module_spawn.c` to 0. Found a **pure-leech
  header** `Module/damagetile_instance.h` with *zero* includes (100% reliant on egolib.h) → 7 precise includes.
- **Phase B — the cut**: removed all 18 includes; the keep-going build surfaced the true transitive-leech tail —
  **139 errors across 30 TUs** that pulled egolib types *through* a narrowed header without including egolib.h
  themselves (same blind spot as Pass 225, scaled up). Fixed with precise includes (no code changed): 1 header
  (`LoadPlayerElement.hpp` → `Renderer/DeferredTexture.hpp`, cascade-fixed its `.cpp`); **28 source TUs via a
  28-agent parallel workflow** (each probes with `srccheck`, maps via a shared symbol→home dictionary, self-verifies
  `errors=0`); plus `tools/egoboo-content-validator.cpp` (→ `spawn_file.h`). New dictionary entries this pass:
  GL prims + `GL_DEBUG` → `Extensions/ogl_extensions.h`; legacy bitmap-font globals (`fontyspacing`/`asciitofont`/
  `TABADD`) → `font_bmp.h`; `twist_to_normal`/`XX,YY,ZZ` → `map_functions.h`; `ReadContext` → `fileutil.h`;
  `Ego::isspace`/`iscntrl`/`isalpha`/`isdigit` → `Core/StringUtilities.hpp`; `ModuleProfile`/`ObjectProfile`
  complete types → `Profiles/_Include.hpp`; `Ego::OpenGL::Utilities` → `Renderer/OpenGL/Utilities.hpp`.
- **Delete**: removed `egolib.h` + its `egolib/library/CMakeLists.txt` source-list entry; reconfigure + full build green.

**Verified:** build all 4 targets = 0 errors; `test.mod` warnings=0 errors=0; full validator 42 modules
errors=245 (pre-existing legacy-content baseline, unchanged by include-only edits); ctest 736/738 (only #526/#527);
menu smoke-run exit 124, clean boot, error-scan empty.

**The uber-header pattern is fully eliminated from the live codebase** — both `egoboo.h`'s aggregate link (Pass 225)
and `egolib.h` itself (Pass 226). **This front is complete.** The only remaining `egolib.h` references are in the
disconnected, unbuildable **cartman** (4 files) and **utilities/migrator** (1 file) — no `CMakeLists.txt`, not in
the CMake graph, already bit-rotted (tracked under roadmap T3.5). Their dangling includes were intentionally left
as-is (the user confirmed the delete); they will need include work when/if those tools are rewired into the build.

## Remaining work-list (HISTORICAL — superseded by Pass 226, front complete)

*(historical — front complete; every item below was executed by Pass 226.)*

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

*(historical — front complete; retained only as a reusable recipe for the next header-narrowing front.)*

1. Rebuild the checker from `flags.make` (recipe above) → `/tmp/selfcheck.sh`.
2. Re-run the probe (neutralize the guarded include, keep-going build) to get the *current* reduced
   failure count and refresh the work-list — Pass 1 should have removed the role-interface cascade.
3. Take the next leaf header, add precise includes from the dictionary, drive its `selfcheck` to 0, then
   the next; keep the normal build green; commit per pass with a `71-completed-passes-log.md` entry.
