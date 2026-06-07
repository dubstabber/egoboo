# Cartman Build Integration — Scouting (T3.5)

Snapshot date: 2026-06-07. Status: **Phase 2 executed AND runtime-verified — cartman compiles, links, RUNS, and
renders** (`EGOBOO_BUILD_CARTMAN=ON` → 0 compile + 0 link errors, 89 MB executable; GUI launch against `test.mod`
boots an OpenGL 4.6 context and renders the full editor — see "Phase 3 — RUNTIME VERIFIED"). Gated OFF by default so
the standard build is untouched. Phase 1 landed the CMake seam + mechanical rename sweep (719→60); Phase 2 ported the
60 genuine API-drift errors and resolved the link-time ODR collisions (see "Phase 2 — EXECUTED"). **The only remaining
decision is whether to flip the default ON / add to the build matrix** (deferred — see Phase 3). This
document records the original feasibility assessment for roadmap item **T3.5** ("`cartman/` exists in-tree but is disconnected from the main CMake graph.
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

## Phase 1 — EXECUTED (2026-06-07)

Steps 1–2 of the plan are done (CMake seam + mechanical rename sweep + dangling-`egolib.h` removal). Always-green:
the option is **OFF by default**, so the standard build is unchanged (verified: reconfigure OFF → no `cartman`
target → egolib-library/egoboo/content-validator/tests all build green).

- **CMake seam**: new gated `cartman/CMakeLists.txt` (`option(EGOBOO_BUILD_CARTMAN OFF)` with an early `return()`
  when OFF; `add_executable(cartman …)` linking only `egolib-library`); `add_subdirectory(cartman)` in root.
- **Mechanical rename sweep** (verified-safe, applied across `cartman/src`): `id::`→`idlib::` (20 sites);
  `Ego::Math::Colour*`→`Ego::Colour*` (26 sites; `Ego::Math::constrain` correctly preserved);
  `Ego::{fill,set_pixel,blit}`→`idlib::…` (8 sites, image-pixel ops that moved to idlib). Added the bare-math-type
  `using` block (`Ego::Vector2f/Vector3f/Point2f/Rectangle2f/Matrix4f4f`) + `integrations/math.hpp` to the central
  `cartman_typedef.h` so the legacy unqualified math names resolve with a minimal diff.
- **`egolib/egolib.h` removed** from all 4 cartman includers. `cartman_config.h` (included editor-wide via
  `cartman_typedef.h`) now carries cartman's curated egolib **core include surface** (platform, typedef, idlib,
  color/math integrations, Renderer, GraphicsSystem/Window, Font, ImageManager, Time, Mesh/Info, FileFormats/map_file);
  `Clocks.h` → `Time/Time.hpp`; `cartman.c`/`cartman_gfx.c` drop it (reached transitively).

**Error trajectory** (real `EGOBOO_BUILD_CARTMAN=ON` build, ground truth): **719 → 60**. The rename sweep + core
include surface collapsed essentially all of the cascade. The remaining **60 are genuine egolib API-drift**, the
**Phase 2 worklist**, concentrated in 5 TUs (cartman_gfx.c 17, cartman.c 15, cartman_gui.c 2, View.cpp 1,
cartman_input.c 1):

| Genuine-drift symbol | count | likely current API |
|---|---|---|
| `Ego::Math::Transform` (+ its `<`-token cascade, ~24) | 7 (+24) | moved/renamed — find the current transform type |
| `Ego::GraphicsWindow::getSize()` / `getDrawableSize()` | 7 / 5 | accessor renamed (`drawable_size` suggested; **no** `getSize`/`size` in the header — needs the base-class/idlib-game-engine window API) |
| `Ego::Core` not declared | 6 | needs `Console/Console.hpp` / `Core/System.hpp` or a rename |
| `Ego::Matrix4f4f::identity` | 5 | `idlib::matrix` identity is a free function now (`idlib::identity<…>()`?) |
| incomplete `struct GFX` / `Ego::App<GFX>` | 4 | cartman's app object inherits egolib `App<T>`; the App template API drifted |
| `idlib::axis_aligned_box(point, {brace-init})` | 1 | `Rectangle2f` 2-arg construction changed |

### Bugs / questionable code spotted (for future iterations, per the maintainer's note that cartman was buggy)

Not touched in this port (behavior-preserving include/rename work only) — flagged for a later cleanup pass:
- **Legacy immediate-mode OpenGL** throughout `cartman_gfx.c` (`glBegin`/`glEnd`/`glVertex*`) — works via
  `ogl_extensions.h` but is deprecated and will block any renderer modernization.
- **Pervasive global mutable state** (`camera_t cam;`, the `damagetile*`/`animtile*` globals in `cartman_gfx.c`,
  the singletons `Input`/`Manager`/`Resources`) and **C-style `NULL`/raw-pointer/`( type )cast` idioms** — the
  "spaghetti" character the editor was known for; candidates for a modernization pass once it compiles + runs.
- `GFX` ties the whole editor lifecycle to `Ego::App<GFX>`; the App API drift (above) is the riskiest Phase 2 item
  because it touches startup/teardown and can't be checked without a runtime launch.

## Phase 2 — EXECUTED (2026-06-07)

**cartman now compiles AND links** against current egolib (`EGOBOO_BUILD_CARTMAN=ON`): the editor is a working build
target after ~8.5 years of bit-rot. Error trajectory: **60 → 0 compile errors → link green** (89 MB executable).
Always-green discipline held: gated OFF by default, so reconfiguring `EGOBOO_BUILD_CARTMAN=OFF` rebuilds the standard
targets clean and the standard build is untouched.

### The 60 compile errors — 5 researched fix categories (all behavior-preserving)

Each category was mapped to the *current* egolib/idlib API by reading how egolib's own runtime uses it today
(UIManager.cpp / Console.cpp / Camera.cpp / GameEngine.cpp / graphic.c), then applied:

1. **`Ego::App<GFX>` template (the 24-error header cascade + 4 incomplete-GFX):** NOT a rename — `Ego::App<T>` still
   exists (`egolib/App.hpp`, `template<typename T> struct App : idlib::singleton<T>` with the same `(title, version)`
   ctor). Pure **missing include**: added `#include "egolib/App.hpp"` to `cartman_gfx.h`. (Egolib's own `GFX` moved to
   the heavyweight `GameApp<GFX>`; the editor correctly stays on the minimal `App<GFX>`.)
2. **`GraphicsWindow::getSize()`/`getDrawableSize()` (~13 sites):** accessors renamed to `size()` / `drawable_size()`
   (cartman holds an `Ego::GraphicsWindow*` → base `idlib::window` virtuals; return type still supports `.x()/.y()`
   and `operator()(0/1)`). Mechanical rename.
3. **`Ego::Math::Transform::{ortho,scaling,lookAt}` (7 sites):** the old matrix-builder facade is gone; replaced with
   the idlib free functions `idlib::orthographic_projection_matrix` / `idlib::scaling_matrix` / `idlib::look_at_matrix`
   (arg order/semantics preserved). Added `#include "idlib/math.hpp"` to `cartman_config.h`.
4. **`Matrix4f4f::identity()` (5 sites):** the static member is gone; replaced with `idlib::identity<Matrix4f4f>()`.
5. **`Ego::Core::System` / `Ego::Core::Console` (6 sites):** no API drift — pure **missing includes** (they were
   reached via the deleted `egolib/egolib.h`). Added `egolib/Core/System.hpp` + `egolib/Console/Console.hpp` to
   `cartman_config.h`. The `Rectangle2f` 2-arg ctor "error" was collateral from the unresolved `getDrawableSize()`
   accessor — once renamed, the brace-init construction is byte-identical to egolib's own `GameEngine.cpp` and compiles.
   (One more missing-include surfaced after the cascade cleared: `Ego::FontManager` → `egolib/Graphics/FontManager.hpp`
   in `cartman_gfx.c`.)

### The link phase — ODR collisions (the doc's anticipated "resolve link errors")

Linking the full `egolib-library` surfaced symbol clashes between cartman's standalone-program globals and egolib's.
An **upfront `nm` symbol-diff** (cartman `.o` strong defs ∩ `libegolib-library.a` strong defs) proved there were
**exactly 4 logical collisions** — no rebuild-discover loop:

- **`main`:** cartman declared `int SDL_main(...)`; with no `SDL2main` linked, nothing provided `main`. egolib's
  `platform.h` does `#undef main` after `<SDL.h>`, so the fix mirrors `egoboo/src/game/Main.cpp` exactly: rename to a
  plain `int main(...)`.
- **`GFX::GFX()` / `GFX::~GFX()`:** cartman's editor `struct GFX` (global namespace) collided with egolib's game
  `struct GFX`. Both are CRTP singletons, so the clash also threatened `idlib::singleton<GFX>` identity. Fixed by
  namespacing cartman's type as **`Cartman::GFX`** (distinct type → distinct singleton); call sites qualified.
- **`config_download` / `config_upload`:** cartman's were dead thin `Ego::Setup::download/upload` wrappers, never
  called anywhere in cartman, duplicating egolib's real (heavier) versions. **Deleted** as dead duplicate code.

### Verification

Build all (`EGOBOO_BUILD_CARTMAN=ON`) = 0 compile + 0 link errors, cartman binary produced. Reconfigure OFF → default
build (egolib-library/egoboo/content-validator/tests) exit 0; `test.mod` warnings=0 errors=0; ctest 736/738 (only the
pre-existing #526/#527). **Total diff: 6 files, +50/-37, all under `cartman/src/cartman/`** — no egolib change.

### Still open / flagged (Phase 3+)

- **Runtime verification — DONE (the former TOP RISK is cleared):** see "Phase 3 — RUNTIME VERIFIED" below. cartman
  boots an OpenGL 4.6 context and renders the full editor (all four viewports + HUD) against `test.mod`.
- **Pre-existing bug found (NOT from this port):** the no-arg / bad-args path **core-dumps** — `atexit(main_end)` is
  registered *before* `Ego::Core::System::initialize()`, so on the early `return EXIT_FAILURE` (missing module arg)
  `main_end` → `setup_clear_base_vfs_paths()` calls VFS cleanup on an uninitialized VFS and throws
  (`vfs_remove_mount_point: ... VFS was not initialized`) → `terminate`. The normal path (valid module) initializes VFS
  first, so this only hits error exits. A one-line guard (register `atexit` after `System::initialize`, or null-check
  the VFS in cleanup) would fix it — deferred as a behavior change, flagged here per the maintainer's "expect bugs" note.
- Legacy immediate-mode OpenGL + pervasive global mutable state + C-style idioms remain (documented under Phase 1's
  "Bugs / questionable code" above) for a future modernization pass.
- `EGOBOO_BUILD_CARTMAN` stays **OFF by default** until a manual functional smoke confirms the editor runs; only then
  flip the default / add to the build matrix / docs.

## Phase 3 — RUNTIME VERIFIED (2026-06-07)

**cartman not only compiles + links, it RUNS and renders the editor.** Launched the GUI against `test.mod` on a real
display (X11/XWayland) and captured a mid-run screenshot — the former top risk ("compile-green ≠ works") is cleared.

**Launch recipe** (cartman mirrors the egoboo smoke-run env + adds two positional args `<egoboo_path> <module>`;
`sys_fs_init` honors `EGOBOO_DATA_DIR`, file_linux.c:173, so data discovery matches the game):
```
HOME=/tmp/egoboo-home XDG_DATA_HOME=/tmp/egoboo-xdg \
DISPLAY=:0 SDL_VIDEODRIVER=x11 EGOBOO_DATA_DIR="$PWD/data" EGOBOO_DISABLE_AUDIO=1 \
timeout -k 3 -s TERM 12 ./build/products/x64/bin/cartman "$PWD/data" test
```
(module name is WITHOUT `.mod`; cartman grabs mouse/keyboard for the duration — briefly disruptive. cartman has no
SIGTERM handler that quits its `for(;;)` loop, so on timeout it's killed → exit 124/137 = "survived the full run";
any *other* early non-zero exit = a crash. Screenshot mid-run via a backgrounded launch→wait→`spectacle -bnf -o`→kill.)

**Observed (clean boot log + screenshot):** filesystem init (Data dir = `$PWD/data`), SDL timer/events/video/audio/
input, **OpenGL 4.6 Compatibility context created** (Mesa, AMD Radeon Vega 8), ImageManager (SDL_image 2.8.12),
FontManager (SDL_ttf 2.24.0); then it ran the full duration with **zero error/exception/crash lines** and the
screenshot shows the editor fully rendered: the **four viewport windows** (Vertex = wireframe mesh + blue vertex
markers; Tile/FX = textured tiles incl. the red floor + arrow border; Side = wireframe side projection) plus both HUD
text blocks (tile-info + minimap triangle top-left; "Brush size / Direct / Ambient / Vertices 16758740" + the
S/H/I/B/A/D/R/O key legend bottom-left). This visually exercises **every ported path**: `App<GFX>` startup (window+GL),
`FontManager::loadFont` (all HUD text), the four matrix builders (`idlib::orthographic_projection_matrix`/`look_at_matrix`/
`scaling_matrix`/`identity` — the correctly-projected views), `GraphicsWindow::size()`/`drawable_size()` (viewport + HUD
layout), module load (mesh + textures), and Console init.

**Decision deferred to the maintainer — flip `EGOBOO_BUILD_CARTMAN` ON by default?** Runtime is now verified, so the
plan's step-6 default-flip is unblocked. Left OFF for now (conservative): it keeps the default build fast (cartman is an
89 MB link) and matches the program's gated-incremental philosophy; only `test.mod` on one platform (Linux/Mesa) has
been exercised, and the pre-existing no-arg `atexit`/VFS crash is still unfixed. Flip it (and add to the build matrix +
`doc/build-*.md`) whenever broader module/platform coverage and that bug-fix are desired.

## How to resume / re-probe

- Recreate the probe shim: `git show <pre-Pass-226>:egolib/library/src/egolib/egolib.h > /tmp/cartman-probe/egolib/egolib.h`
  (or just supply the precise includes cartman actually needs).
- `cartcheck` recipe: `g++ -std=gnu++17 -D_GNU_SOURCE -fsyntax-only -x c++ <file>` with
  `-I/tmp/cartman-probe -Icartman/src` + the egolib-library `CXX_INCLUDES` from
  `build/egolib/library/CMakeFiles/egolib-library.dir/flags.make`.
- Effort estimate: **medium** — a focused multi-pass port (mechanical rename sweep clears the bulk; a residual of
  genuine egolib-API-drift fixes in gfx/gui/main remains), not a quick CMake wiring and not a rewrite.
