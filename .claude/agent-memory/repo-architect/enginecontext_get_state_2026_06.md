---
name: enginecontext-get-state-2026-06
description: Current state of ::get() singleton call sites — counts by class, EngineContext coverage map, top reduction candidates as of 2026-06-10
metadata:
  type: project
---

## ::get() singleton call-site inventory (2026-06-10)

**Total:** 712 call sites across egolib/library/src/. Zero in foundation-base, physics, or renderer layers.

### Breakdown by class

| Class | Calls | In EngineContext? |
|---|---|---|
| EngineContext::get() | 397 | yes (is the context itself) |
| GameSessionContext::get() | 129 | no — separate session-scoped context |
| Renderer::get() | 87 | **no** — top raw singleton by call count |
| CameraSystem::get() | 17 | yes (ICameraSystem seam exists) |
| video_buffer_manager::get() | 12 | **no** — idlib layer, vertex buffer alloc |
| InputSystem::get() | 10 | yes (IInputSystem seam exists) |
| GraphicsSystemNew::get() | 9 | **no** — SDL display/window manager |
| egoboo_config_t::get() | 6 | yes (installConfig/config()) |
| TLT::get() | 5 | **no** — trig lookup table, math layer |
| AudioSystem::get() | 5 | yes (IAudioSystem seam exists) |
| Console::get() | 4 | **no** |
| TextureManager::get() | 4 | yes (ITextureManager seam exists) |
| ImageManager::get() | 4 | yes (IImageManager seam exists) |
| GFX::get() | 3 | yes (IGFX seam exists) — but one raw call in graphic_scene.c remains |
| Log::get() | 3 | yes (installLogTarget) — but 3 raw calls in System.cpp bootstrap |
| GraphicsSystem::get() | 2 | partial — App.cpp installs it into EngineContext; 1 raw use in App.cpp |
| Runtime::get() | 2 | **no** |
| parser_state_t::get() | 2 | **no** — script compiler internal |
| ProfileSystem::get() | 2 | yes (IProfileSystem seam exists) |

### EngineContext services already routed (15 seams)

AudioSystem, InputSystem, PerkHandler, ImageManager, FontManager, GraphicsSystem, TextureManager, ParticleHandler, ProfileSystem, CameraSystem, BillboardSystem, TextureAtlasManager, GFX, Config (egoboo_config_t), LogTarget (Log::Target).

### Top 5 raw singletons NOT yet in EngineContext

1. **Renderer::get() — 87 calls** — OpenGL renderer base class (`egolib/Renderer/Renderer.hpp`). Already has an abstract interface. Highest value target. Call sites span Font.cpp, Console.cpp, ogl_extensions.c, App.cpp, and render passes. Needs `installRenderer`/`renderer()` seam added to EngineContext.

2. **video_buffer_manager::get() — 12 calls** — defined in idlib layer. Used for vertex buffer creation in BillboardSystem, RenderPasses (Background, EntityShadows), Console, Font. Lives in `egolib/game/Graphics/`, `egolib/Console/`, `egolib/Graphics/`. Routing through EngineContext requires adding an idlib interface seam.

3. **GraphicsSystemNew::get() — 9 calls** — SDL display/window manager (`egolib/Graphics/GraphicsSystemNew.hpp`). Concrete singleton subclass pattern. Used for display mode queries.

4. **Console::get() — 4 calls** — 4 raw call sites. Console already partially uses Renderer::get() internally.

5. **TLT::get() — 5 calls** — trigonometric lookup table, math layer. Used only in Camera.cpp for swing/roll calculations. Very narrow interface; could be replaced with std::sin/std::cos or injected at Camera construction.

### Conversion pattern (from recent passes)

The established pattern (e.g., ITerrainQuery, IAudioSystem):
1. Extract an abstract interface `IFoo` with the minimal methods callers need.
2. Have the concrete class inherit `IFoo`.
3. Add `installFoo(IFoo&)` + `foo()` + `tryFoo()` + `clearFoo()` to EngineContext.
4. At bootstrap (ContentRuntimeBootstrap or GameEngine::initialize), call `EngineContext::get().installFoo(Foo::get())`.
5. Replace call sites from `Foo::get().method()` → `EngineContext::get().foo().method()`.

### Blocked keystones

- **EngineContext::get() itself (397 calls)** — cannot move EngineContext to lower layers because it depends on AudioSystem, ParticleHandler, GameEngine, PlayingState (all library-layer). The dam only breaks by injecting its individual services (logTarget, config, profileSystem) at call sites.
- **GameSessionContext::get() (129 calls)** — same pattern: depends on GameModule, water_instance_t, ego_mesh_t (19 blockers). Reduce by IWorldTime / IWaterElevation seams.

**Why:** Renderer::get() is the highest-value unrouted singleton; routing it through EngineContext is a clean win with no complex dependencies since IRenderer-style abstraction is already in place.
**How to apply:** When planning next ::get() reduction pass, start with Renderer (87 calls), then video_buffer_manager (12), then GraphicsSystemNew (9).
