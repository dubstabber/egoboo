# Cartman Build Integration — Scouting (T3.5)

Snapshot date: 2026-06-07. Status: **scouting only — no code changed.** This document records a feasibility
assessment for roadmap item **T3.5** ("`cartman/` exists in-tree but is disconnected from the main CMake graph.
Gate it with a CMake option and add it to the build matrix.").

All numbers below are **compile-probe ground truth**, not estimates — each cartman TU/header was syntax-checked
(`g++ -fsyntax-only -x c++`) against the *current* egolib include set, with a temporary `egolib/egolib.h`
aggregate shim (the real one was deleted in Pass 226) placed in a `/tmp` probe dir so the legacy include resolves
without touching the working tree.

## Verdict

**Feasible, MEDIUM effort, low architectural risk.** Cartman is a port job, not a rewrite. The core data/math
model already compiles clean against modern egolib; the bit-rot is concentrated in a handful of headers + the
gfx/gui/input/main `.c` files and is **dominated by ~4 systematic, mechanical renames**. Recommend gating behind a
CMake option **OFF by default** and landing the port incrementally.

## What cartman is

- A standalone **map editor** for Egoboo (`SDL_main` entry at `cartman/src/cartman/cartman.c:1777`).
- **~9,254 LOC**, 35 files (9 `.c`, 7 `.cpp`, 12 `.h`, 7 `.hpp`) under `cartman/src/cartman/`.
- **No build files at all** (no `CMakeLists.txt`, no autotools/VS project — and none in git history). It was never
  wired into the modern CMake build; it predates that migration.
- **Last meaningful change: 2017-11-29** ("Update for latest versions of dependencies"). ~8.5 years of egolib API
  drift since. (`cartman/doc/cart.txt` is older still — references a "6/01" DOS version — not build-relevant.)
- Dependencies: **egolib** (9 distinct headers incl. the now-deleted `egolib/egolib.h`), **SDL** (2), **0 idlib**
  direct, **0 direct GL** (GL comes via egolib's `Extensions/ogl_extensions.h`).

## Bit-rot magnitude (compile-probe)

**719 raw errors across 11 of 16 source TUs**, but this number is misleading — it is dominated by **cascade** from
a few failing headers. Probing headers in isolation shows the true root surface is small.

| Source TU | errors | | Header (isolated) | root errors |
|---|---|---|---|---|
| cartman.c | 298 | | cartman_gui.h | 13 |
| cartman_gfx.c | 141 | | cartman_gfx.h | 6 |
| cartman_gui.c | 66 | | cartman_input.h | 6 |
| cartman_functions.c | 43 | | cartman_functions.h | 4 |
| cartman_input.c | 35 | | cartman_select.h | 1 |
| Views/SideView.cpp | 32 | | **(14 other headers)** | **0** |
| Views/VertexView.cpp | 30 | | | |
| View.cpp | 25 | | | |
| Views/FxView.cpp | 23 | | | |
| Views/TileView.cpp | 23 | | | |
| cartman_select.c | 3 | | | |
| **cartman_map.c, cartman_math.c, Clocks.c, Tile.cpp, Vertex.cpp** | **0** | | | |

**Key signal:** `cartman_input.h` has only **6** root errors in isolation, but `Cartman::Input` being left
incomplete cascades to **160** errors in `cartman.c`. So the 719 total collapses dramatically once ~30 header root
errors are fixed. The entire genuine header-root surface is **~30 errors across just 5 headers**; 14 of 19 headers
and the whole core data/math model (`cartman_map.c`, `cartman_math.c`, `Tile`, `Vertex`, `View`, all 4 `Views/*`)
are **already clean**.

## Bit-rot taxonomy (the genuine fix surface)

Three classes, the first three of which are **mechanical and systematic** (verified the rename targets all exist in
the current tree):

1. **`id::` → `idlib::`** — the foundation library namespace was renamed; `id::` is **fully dead** in live egolib
   (0 references). Cartman still writes `id::…`. Mechanical.
2. **bare `singleton<T>` → `idlib::singleton<T>`** — `idlib::singleton` lives at
   `idlib/library/src/idlib/singleton/singleton.hpp`; current egolib classes write `public idlib::singleton<T>`
   (e.g. `App.hpp`, `ProfileSystem.hpp`). Cartman's `Cartman::Input` / `Gui::Manager` use the bare form. Mechanical.
3. **bare math types → `Ego::`-qualified** — `Vector2f`/`Vector3f`/`Point2f`/`Rectangle2f` and
   `Ego::Math::Colour4f` → `Ego::Colour4f`. The `Ego::` aliases exist (`integrations/math.hpp`,
   `integrations/color.hpp`). Fixable by qualifying or adding `using` declarations. Mechanical.
4. **Genuine egolib API drift (the real porting residual)** — method/member renames on egolib classes that cartman
   calls, visible after the cascade clears: `Ego::GraphicsWindow::getSize()`/`getDrawableSize()` →
   `drawable_size`/`size`; cartman's own `Gui::Window` `size`/`position` members; `ImageManager`/gfx/mesh accessors.
   Concentrated in `cartman_gfx.c` (141), `cartman_gui.c` (66), `cartman.c` (residual after cascade). These need
   manual porting against current egolib APIs — estimated low-hundreds of edits, **not** rewrites.

## CMake / link design (trivial)

Cartman links exactly what the `egoboo` executable does — just `egolib-library`, which transitively provides SDL2,
idlib, GLEW, PhysFS, OpenGL, etc. Sketch:

```cmake
# cartman/CMakeLists.txt (new)
option(EGOBOO_BUILD_CARTMAN "Build the Cartman map editor (legacy, in port)" OFF)
if(EGOBOO_BUILD_CARTMAN)
  file(GLOB_RECURSE CARTMAN_SOURCES src/*.c src/*.cpp)
  add_executable(cartman ${CARTMAN_SOURCES})
  target_include_directories(cartman PRIVATE src)
  target_link_libraries(cartman egolib-library)
  if(WIN32)
    egoboo_stage_windows_runtime_libraries(cartman idlib-game-engine-library)
  endif()
endif()
```

Then `add_subdirectory(cartman)` in the root `CMakeLists.txt` (the `option()` keeps it out of the default build).
Mirrors `egoboo/CMakeLists.txt`. **No new third-party dependency** is introduced.

## Risks

- **No automated runtime verification.** Cartman is a GUI editor needing a display *and* a module to edit; there is
  no equivalent of the `--module test.mod` validator or a headless smoke-run. Compile-green ≠ works. Manual launch
  on a real display is the only functional check.
- **Semantic (not just compile) drift** is possible where cartman pokes egolib internals — mesh memory layout,
  immediate-mode GL (`glBegin`/`glEnd` via `ogl_extensions.h`, deprecated but still linkable), file-format structs.
  Some fixes may need behavioral judgement, not just renames.
- **The 4 dangling `egolib/egolib.h` includes** (cartman.c, cartman_config.h, cartman_gfx.c, Clocks.h) left by
  Pass 226 must be narrowed to precise headers as part of this work (same technique as Pass 226).
- **Legacy immediate-mode OpenGL** — cartman is GL 1.x style; fine for now (egolib still exposes it) but it will
  resist any future renderer modernization.

## Recommended incremental plan

Always-green, verifiable steps (mirrors the refactoring program's discipline):

1. **Wire the disconnected target first (compiles nothing yet):** add the gated `cartman/CMakeLists.txt` +
   `add_subdirectory`, `EGOBOO_BUILD_CARTMAN=ON` in a scratch configure. Establishes the build seam and the exact
   error list as the live metric.
2. **Mechanical rename sweep (headers first, leaf-upward):** `id::`→`idlib::`, `singleton<>`→`idlib::singleton<>`,
   bare math → `Ego::`-qualified, `Ego::Math::Colour4f`→`Ego::Colour4f`. Fix the 5 dirty headers → watch the `.c`
   cascade collapse. Re-narrow the 4 dangling `egolib.h` includes here too.
3. **Genuine API-drift port, by TU in ascending error count:** `cartman_select.c` (3) → `Views/*` → `cartman_gui.c`
   → `cartman_gfx.c` → `cartman.c`. Map each missing/renamed symbol to its current egolib API.
4. **Link + build the executable**, resolve link errors (likely few, since egolib-library is the sole dep).
5. **Manual functional smoke** on a real display against a small module; document what works.
6. Keep `EGOBOO_BUILD_CARTMAN` **OFF by default** until it builds + launches reliably, then flip the default and add
   to the build matrix / docs.

## How to resume / re-probe

- Recreate the probe shim: `git show <pre-Pass-226>:egolib/library/src/egolib/egolib.h > /tmp/cartman-probe/egolib/egolib.h`
  (or just supply the precise includes cartman actually needs).
- `cartcheck` recipe: `g++ -std=gnu++17 -D_GNU_SOURCE -fsyntax-only -x c++ <file>` with
  `-I/tmp/cartman-probe -Icartman/src` + the egolib-library `CXX_INCLUDES` from
  `build/egolib/library/CMakeFiles/egolib-library.dir/flags.make`.
- Effort estimate: **medium** — a focused multi-pass port (mechanical rename sweep clears the bulk; a residual of
  genuine egolib-API-drift fixes in gfx/gui/main remains), not a quick CMake wiring and not a rewrite.
