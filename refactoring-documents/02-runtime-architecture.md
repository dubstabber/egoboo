# Runtime Architecture

Last refreshed: 2026-07-21. Volatile counts live in
`CODEBASE-HEALTH-STATUS.md`.

## 1. Boot path

1. `egoboo/src/game/Main.cpp`
2. `Ego::Core::System::initialize(argv[0])` — VFS and search paths, logging,
   configuration (`setup.txt`), SDL timer/event/video/audio/input services
3. `EngineContext::get().setEngine(std::make_unique<GameEngine>())`
4. install the main-menu factory, the default script system
   (`egolib-scriptvm`), and the default graphics bootstrap
   (`egolib-game-graphics`)
5. `engine().start()`, then `EngineContext::get().clearEngine()` on shutdown

The executable layer is the composition root for the systems that live above
`egolib-library`: it installs upper-layer services before handing control to
`GameEngine`, which still triggers the graphics bootstrap at the original
order-sensitive point. Filesystem and configuration setup happen before most
gameplay systems exist; VFS mount points are part of core runtime identity.

## 2. GameEngine

`egolib/game/Core/GameEngine.*` is the central orchestrator: startup/shutdown
sequencing, the fixed-rate loop, SDL event polling, the game-state stack,
preload UI rendering, screenshot handling, and saved-character/module profile
loading.

Ordered subsystem lifecycles are increasingly encapsulated in RAII
composition-root members — `ContentRuntimeBootstrap` (profile/content),
`GameplaySubsystemsBootstrap` (audio + particle), `ConsoleBootstrap`
(developer console) — but `GameEngine::initialize()` still directly
orchestrates the remaining concrete systems (gfx hook, collision), and
initialization order is effectively the architecture spec: if the order is
wrong, the game breaks.

Main loop: fixed update at 50 UPS, fixed render at 60 FPS, max frameskip 10.
Per iteration: panic-button check (`Ctrl+Q`), zero or more update frames, one
render frame if due, idle sleep, FPS/UPS estimation.

## 3. Game states

A stack of `GameState` instances: `MainMenuState`, `PlayingState`,
options/selection states, debug loading states, victory and in-game menu
states. The stack itself is healthy; the weakness is that states resolve
dependencies through context singletons and active seams rather than receiving
an isolated session object.

## 4. Global runtime state

The historical architecture was defined by three large mutable globals —
`_currentModule` (592 pre-refactor references), `_gameEngine` (266), and
`update_wld` (65) — reached from everywhere. That boundary is dismantled:

- `_currentModule`, `_gameEngine` — 0 active references; access routes through
  `GameSessionContext`/`GameModule` and `EngineContext`.
- `update_wld` — variable removed; 4 comment/debug-label artifacts remain.
- The old secondary session globals (`clock_chr_stat`, `clock_enc_stat`,
  `overrideslots`, `g_importList`) are gone; weather, fog, and animated-tile
  state are owned through module/session surfaces.

Remaining risk: coupling was migrated, not eliminated. Subsystems now reach
into `EngineContext::get()` / `GameSessionContext::get()` and the active `I*`
seams instead of raw globals. A service-interface layer is partially in place,
but constructor injection does not exist broadly — see the roadmap.

## 5. Module runtime

`GameModule` (`egolib/game/Module/Module.cpp`) is the live gameplay session
model for one loaded module. It owns module profile metadata, the object
handler and player list, teams, water/damage-tile state, passages, mesh,
tile/water textures, import/export validity, and the random seed.

Constructor-driven load sequence (now orchestrated through the named
`ModuleLoadPhase`/`ModuleLoadContext` boundary, with services provided by
`GameModuleRuntime`): reconfigure module VFS mounts → seed RNG → init teams →
load textures → global sounds/particles → `wawalite.txt` → object profiles →
mesh → passages → alliances → spawn from `spawn.txt` → compile AI scripts.
This is still a large amount of behavior for one constructor-driven lifecycle;
continued decomposition into explicit phases is roadmap work.

## 6. Content conventions are runtime assumptions

The runtime expects legacy conventions directly: slot numbers from `data.txt`,
hardcoded object filenames, legacy spawn-slot semantics, module VFS overlays,
`script.txt` loading, and implicit asset enumeration (`sound0..29`,
`part0..29`). Any content-format refactor is therefore also a runtime
refactor.

## 7. Mixed subsystem styles

`egolib` mixes older C-style systems (`game.c` descendants, `mesh.c`, the
fourteen `script_functions_*.c` files split from the former 8,153-line
`script_functions.c`) with newer C++ systems (`GameModule`, GUI, render
passes, players, camera, profiles). The deeper issue is mixed *ownership*
style: RAII classes coexist with procedural sequencing and cross-subsystem
reach, and newer code still often reaches for concrete singletons rather than
role interfaces.

Subsystem map:

| Area | Directories |
| --- | --- |
| Core/runtime services | `Core/`, `Configuration/`, `Log/`, `VFS/`, platform filesystem code |
| Gameplay runtime | `game/`, `Entities/`, `Profiles/`, `Logic/`, `Inventory`, `Shop` |
| Presentation / IO | `Graphics/`, `Renderer/`, `Image/`, `Audio/`, `InputControl/`, `game/GUI/` |
| File and content formats | `FileFormats/`, `Script/` |

Gameplay logic is spread across `game/`, `Entities/`, `Profiles/`,
`FileFormats/`, `Script/`, and GUI states that manipulate runtime state
directly.

## 8. Target boundaries

The architecture is converging on five explicit boundaries:

- **A. Platform/runtime services** — logging, filesystem, window/audio/input
  bootstrap, renderer backend.
- **B. Content repositories** — module metadata, object/particle definitions,
  script assets, mesh data.
- **C. Game session runtime** — active module, players, objects, teams,
  quest/runtime state.
- **D. Presentation** — menus, HUD, camera, render passes.
- **E. Script/rules execution** — EgoScript compatibility layer now, adapter
  for a future engine.

The first move — replacing raw global access with `GameSessionContext` /
`EngineContext` wrappers — is complete. The next frontier is replacing those
context singletons with constructor-injected service interfaces so subsystems
receive dependencies instead of reaching for a wrapper.
