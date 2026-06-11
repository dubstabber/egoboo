# Pass 74 — egolib-game-graphics: the NINTH archive carve (plan + measured surface)

Status: **COMPLETE** (branch `refactor/egolib-game-graphics-carve`; commits `7495f2955` VM test,
`6a0a9808e` P1 relocate-down, `e35e9edaa` P2 CMake split). Realized: 15 reverse edges cut (as
predicted), egolib-game-graphics = 17 TUs, egolib-library 77 → 62, 0 forbidden edges across all 9
archives, ctest 876/876, validator 0/0. See `71-completed-passes-log.md` for the full landed record.
Paired with a VM compile→execute characterization test (safety-net, landed first).

## Goal

Carve the `game/Graphics` render-pass cluster out of `egolib-library` into a new above-library
archive `egolib-game-graphics`, shrinking `egolib-library` from 77 → 61 TUs and isolating the 3D
scene-rendering infrastructure from game-core logic. This is the 9th link-split, following the
proven above-library pattern (gamestates / scriptvm / hud-widgets): relocate-down + interface/hook
seam + injection-from-above.

## Measured reverse-edge surface (authoritative, archive-member nm)

Tooling note: the CMakeFiles `egolib-library.dir/` object directory holds **224 stale `.o`** from
before the upper carves; the real `libegolib-library.a` has **77 members**. Always measure against
the **archive**, not the `.dir` glob. Methodology validated by controls: library→{hud-widgets,
scriptvm, gamestates} = 0 (acyclic), library→foundation/lower = 427 (detector sees real edges).

Cluster G (16 TUs, currently in egolib-library): `Camera.cpp`, `CameraSystem.cpp`,
`BillboardSystem.cpp`, `RenderPasses.cpp`, `TextureAtlasManager.cpp`, and the 11
`RenderPasses/*.cpp` (Background, EntityReflections, EntityShadows, Foreground, Heightmap,
NonOpaqueEntities, NonReflectiveTiles, OpaqueEntities, ReflectiveTilesFirst, ReflectiveTilesSecond,
WaterTiles).

**15 reverse edges (library remainder → cluster), from exactly 2 TUs:**
- `graphic.c` → 14: the 11 render-pass ctors (via `GFX::GFX()` make_unique, lines 117-127) +
  `BillboardSystem::BillboardSystem()` (via `GameAppImpl::GameAppImpl()`, line 452) +
  `TextureAtlasManager::TextureAtlasManager()` (GameAppImpl ctor region) +
  `BillboardSystem::render_all(Camera&)` (via `GFX::renderBillboards()`, line 135).
- `GameEngine.cpp` → 1: `CameraSystem::CameraSystem()` (via the `idlib::singleton<CameraSystem>`
  `initialize()`/`get()` instantiated at `GameEngine.cpp:352-353`).

`graphic_scene.c:216` calls `renderBillboards` through the **IGFX vtable**
(`EngineContext::get().gfx().renderBillboards()`) — virtual dispatch, NOT a concrete symbol; nm
confirms it is not a reverse-edge source. `EntityList`/`TileList`/`ObjectGraphics`/`ParticleGraphics`
stay in egolib-library (Entities/GameSession coupling) and do NOT reference the cluster.
`cartman` and `content-validator` (link egolib-library only) reference **none** of the cluster.

## Why an injection HOOK (not move-the-call-to-Main.cpp)

`GFX`/`CameraSystem` are `idlib::singleton`s. `GFX::get()`/`initialize()`/`uninitialize()` and
`CameraSystem::get()`/`initialize()` are template methods instantiated in their callers; they
reference the concrete ctor/dtor. Once `GFX::GFX()` moves up (it must — it builds upper-layer render
passes), any library TU that triggers construction becomes a new reverse edge. So the construction
**trigger** must also leave library.

The trigger sits **mid-`GameEngine::initialize()`** (lines 347-354), order-dependent with neighbors
(`gfx_config_t::download` before; `gfx_system_init_all_graphics`, console-rect-from-GFX-window
after). Moving it to `Main.cpp` pre-`start()` would reorder it. So: keep the call **at the same
line** but route it through a registered `std::function` hook held in egolib-library; the upper
layer registers the concrete implementation at boot. Order is preserved exactly.

## Design

### Phase A — library-side seam: `egolib/game/Graphics/GraphicsBootstrap.{hpp,cpp}` (egolib-library)
```
namespace Ego::Graphics {
  using GraphicsBootstrapHook = std::function<void()>;
  void registerGraphicsBootstrap(GraphicsBootstrapHook init, GraphicsBootstrapHook teardown);
  void runGraphicsBootstrapInit();      // null-safe no-op if unregistered (tests never register)
  void runGraphicsBootstrapTeardown();
  void installDefaultGraphicsSystems(); // DEFINED upper (graphic_init.cpp), declared here
  void clearDefaultGraphicsSystems();
}
```
Holds two static `std::function`. Mirrors `IScriptSystem.hpp` (declares both the lower-layer
accessor and the upper-defined `installDefault*`).

### Phase B — upper-layer TU: `egolib/game/Graphics/graphic_init.cpp` (NEW, in egolib-game-graphics)
Relocate-down from `graphic.c`: `GFX::GFX()`, `GFX::~GFX()`, `GFX::renderBillboards()`,
`GameAppImpl::GameAppImpl()`, `GameAppImpl::~GameAppImpl()`, `GameAppImpl::getDynalist()`,
`getBillboardSystem()`, `getMd2ModelRenderer()`. Define `installDefaultGraphicsSystems()` →
`registerGraphicsBootstrap(initLambda, teardownLambda)`:
- init: `GFX::initialize(); installGFX(GFX::get()); installBillboardSystem(GFX::get().getBillboardSystem()); CameraSystem::initialize(); installCameraSystem(CameraSystem::get()); cameraSystem().getCameraOptions().turnMode = config().camera_control.getValue();`
- teardown (mirror GameEngine 533-536 exactly): `clearCameraSystem(); clearBillboardSystem(); clearGFX(); GFX::uninitialize();`

### Phase C — `GameEngine.cpp`
Replace bootstrap (347-354) with `Ego::Graphics::runGraphicsBootstrapInit();` and teardown
(533-536) with `Ego::Graphics::runGraphicsBootstrapTeardown();`. Drop now-unused includes
(`CameraSystem.hpp`, etc.).

### Phase D — `egoboo/src/game/Main.cpp`
`#include "egolib/game/Graphics/GraphicsBootstrap.hpp"`; call
`Ego::Graphics::installDefaultGraphicsSystems();` next to `installDefaultScriptSystem()` (before
`engine().start()`). Tests do NOT register (they never run `GameEngine::initialize()`); the hook is
null-safe.

### Phase E — `graphic.c`
Remove the moved bodies and the now-unused cluster includes (lines 30-44: 11 RenderPass headers,
`DefaultMd2ModelRenderer.hpp`, `BillboardSystem.hpp`, `TextureAtlasManager.hpp`). Build is the gate.

### Phase F — CMake (`egolib/library/CMakeLists.txt`)
`EGOLIB_GAME_GRAPHICS_LAYER_SOURCES` = the 16 cluster `.cpp` (+ `.hpp`) + `graphic_init.cpp`. Add to
the `REMOVE_ITEM SOURCE_FILES` line and the C-as-C++ coercion `foreach`. `add_library(
egolib-game-graphics STATIC ...)` linking `egolib-library` (+ lower). Insert linearly into the DAG:
**`egolib-library ◄ egolib-game-graphics ◄ egolib-hud-widgets ◄ {egolib-scriptvm, egolib-gamestates}`**
— re-point `egolib-hud-widgets` to link `egolib-game-graphics` (giving scriptvm/gamestates it
transitively). `cartman` + `content-validator` stay on `egolib-library` only.

## Verification gate (all must pass)
1. nm archive-member check: library→game-graphics = 0; game-graphics→{hud,scriptvm,gamestates} = 0;
   controls hold.
2. Clean build: egoboo, cartman, content-validator, tests.
3. `ctest -j20` = 875/875 (+ the new VM characterization test).
4. `content-validator --module test.mod` smoke.
5. Commit per phase; `--no-ff` merge on opt-in; no push (user reserves pushes).
6. Update CLAUDE.md/AGENTS.md DAG description, `71-completed-passes-log.md`, repo-architect memory,
   auto-memory `MEMORY.md`.

## Risk notes
GL-heavy cluster but the carve is link-topology only (render passes aren't in the test suite, no new
GL context). Watch `GFX`/`GameAppImpl` construction/destruction ORDER when the bodies move TUs —
keep make_unique order and the teardown sequence byte-identical. If the post-move nm count does not
collapse to 0, STOP and re-scope (signal the cluster is more coupled than measured).
