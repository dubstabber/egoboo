---
name: nm-fixpoint-2026-06
description: nm fixpoint analysis of all 138 egolib-library TUs — blocker counts, top seam-cut candidates, EngineContext/GameSessionContext as bottleneck pattern
metadata:
  type: project
---

## nm fixpoint analysis (2026-06-10)

Build: `/home/hwang/Projects/egoboo/build` (Linux, Ninja). Method: extracted defined symbols from each lower layer (`egolib-foundation-base` 6500 syms, `egolib-physics` 715, `egolib-renderer` 1784 = 7556 combined), then for each of the 138 exclusive library TUs counted how many of their undefined symbols land only in the library-only set (9834 symbols).

### Zero-blocker TU

Only one genuine zero-blocker among the 138: `game/GameStates/LoadPlayerElement.cpp` (71 lines). All its undefined symbols (`QuestLog`, `ObjectProfile::getIcon`, `RandomName`) resolve into `egolib-foundation-base`. This is directly absorbable NOW but it's a leaf with no reverse-absorption value.

### Key bottleneck symbols

- `EngineContext::get()` — defined in `game/Core/EngineContext.cpp` (9 blockers of its own: AudioSystem, ParticleHandler, GameEngine). **91 of 138 TUs** reference it; it IS the primary dam.
- `GameSessionContext::get()` — defined in `game/Core/GameSessionContext.cpp` (19 blockers). **74 of 138 TUs** reference it.
- `gfx` (BSS global, defined in `game/graphic.c`) — 15 TUs reference it.

### Top seam-cut candidates (1-3 blockers, excluding LoadPlayerElement)

| TU | Blockers | Blocking symbols | Assessment |
|---|---|---|---|
| `game/Graphics/Billboard.cpp` | 1 | `chr_getMatUp(Object*, Vector3f&)` | chr_getMatUp is in CharacterMatrix.c (10 blockers itself); extract a free-function shim or move chr_getMatUp to base |
| `game/GUI/Component.cpp` | 1 | `Container::bringComponentToFront()` | circular GUI dep; Component is base class, Container is subclass — forward-declare or split header |
| `Profiles/ObjectProfile_export.cpp` | 1 | `Object::getBaseAttribute()` | Object is deep library; pass attribute value as parameter to break dep |
| `game/Graphics/RenderPasses/NonReflectiveTilesRenderPass.cpp` | 2 | `TileListV2::render()`, `TileList::getMesh()` | both in RenderPasses.cpp/TileList.cpp (7-3 blockers); cluster move only |
| `game/GUI/Layout.cpp` | 2 | `Component::getWidth()`, `Component::getHeight()` | trivially inline; move getWidth/Height to Component header |
| `game/Module/AnimatedTiles.cpp` | 2 | `GameSessionContext::worldUpdateCount()`, `GameSessionContext::get()` | worldUpdateCount seam already done elsewhere; pass frame counter as param or inject IWorldTime |
| `game/Module/Fog.cpp` | 2 | `EngineContext::get()`, `EngineContext::config()` | config used for fog_enable flag; inject config at construction |
| `game/Module/Module_spawn_realization.cpp` | 2 | `EngineContext::get()`, `EngineContext::logTarget()` | logging only; inject ILogTarget |
| `Profiles/ObjectProfile_load.cpp` | 2 | `activeProfileSystem()`, `tryActiveAudioSystem()` | both singleton shims; pass as parameters |
| `game/GameStates/OptionsConfigActions.cpp` | 3 | `EngineContext::get()`, `::config()`, `::audioSystem()` | inject config+audio |
| `game/Graphics/TileList.cpp` | 3 | `GameSessionContext::get()`, `::mesh()`, `ego_tile_info_t::testFX()` | testFX defined in mesh.c; cluster move with mesh |
| `game/Module/Module_spawn_plan.cpp` | 3 | `convert_spawn_file_load_name()`, `EngineContext::get()`, `::logTarget()` | convert_spawn is in module_spawn.c (4 blockers) |
| `game/Module/Water.cpp` | 3 | `gfx`, `EngineContext::get()`, `::config()` | gfx is the dam; Water + Fog could move together if gfx is seamed |
| `game/script_compile.c` | 3 | `EngineContext::get()`, `::profileSystem()`, `::logTarget()` | inject IProfileSystem + ILogTarget |

### mesh.c specifically (4 blockers)

Blockers: `EngineContext::get()`, `EngineContext::logTarget()`, `GameSessionContext::get()`, `GameSessionContext::water()`.

- `EngineContext::logTarget()` is only for error messages in mesh load path — trivially injectable.
- `GameSessionContext::water()` is only at `getElevation()` (line 1329) for waterwalk elevation test — pass `water_instance_t*` as param or add an `IWaterElevation` seam.
- `getMeshPointer()` has 41 call sites scattered across: Object_appearance.cpp (4), Particle_core.cpp (4), Particle_spawn.cpp (2), graphic_scene.c (7), GameStates/MapEditorState.cpp (5), Module_spawn.cpp (1), script_implementation.c (6), graphic.c (1), GUI/MiniMap.cpp (2), Module/Passage.cpp (4), Module/Weather.cpp (2), GameSessionContext.cpp (1, it's the mesh() accessor itself). Most callers are deep library TUs with many other blockers — cleaning getMeshPointer doesn't unlock much on its own.

### Strategic observation

`EngineContext` and `GameSessionContext` are the true bottlenecks. Moving either to `egolib-foundation-base` would unblock 74-91 TUs, but both have their own complex blocker chains (AudioSystem, ParticleHandler, GameModule, etc.). The highest-value seam-cut strategy is to inject their individual services (logTarget, config, worldUpdateCount) at construction/call sites, converting from pull-singletons to pushed-interfaces — matching the pattern already used for ITerrainQuery and QuestLog.

**Why:** EngineContext::get() is referenced by 91/138 library TUs; it IS the dam blocking the entire next wave of absorptions.
**How to apply:** When scouting next seam, focus on EngineContext::logTarget injection (pure logging, no runtime deps) as a preparatory pass that would reduce blocker counts across the most TUs.
